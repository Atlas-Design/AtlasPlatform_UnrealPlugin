#if WITH_DEV_AUTOMATION_TESTS
#include "AtlasWorkflowAsset.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasIOSchemaContracts, "Atlas.IO.SchemaContracts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAtlasIOSchemaContracts::RunTest(const FString&)
{
    const FString Root = IPluginManager::Get().FindPlugin(TEXT("AtlasWorkflow"))->GetBaseDir() / TEXT("Tests/Fixtures/IOContracts");
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(Root / TEXT("AgentCreated*.json")), true, false);
    TestEqual(TEXT("All 19 normalized IO exports present"), Files.Num(), 19);
    for (const auto& File : Files)
    {
        FString Json, Error; FAtlasWorkflowSchema Schema;
        FFileHelper::LoadFileToString(Json, *(Root / File));
        if (!TestTrue(File, UAtlasWorkflowAsset::ParseSchemaFromJson(Json, Schema, Error))) { AddError(Error); continue; }
        TSharedPtr<FJsonObject> Original;
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Original);
        TestEqual(TEXT("Input count retained"), Schema.Inputs.Num(), Original->GetArrayField(TEXT("inputs")).Num());
        TestEqual(TEXT("Output count retained"), Schema.Outputs.Num(), Original->GetArrayField(TEXT("outputs")).Num());
        for (int32 I = 0; I < Schema.Inputs.Num(); ++I)
        {
            const auto& Def = Schema.Inputs[I];
            const auto Input = Original->GetArrayField(TEXT("inputs"))[I]->AsObject();
            TestEqual(TEXT("Original type retained"), Def.OriginalType, Input->GetStringField(TEXT("type")));
            TestTrue(TEXT("Required presence preserved"), Def.bHasRequiredMetadata);
            TestEqual(TEXT("Required value preserved"), Def.bRequired, Input->GetBoolField(TEXT("required")));
            bool Array = false; Input->TryGetBoolField(TEXT("isArray"), Array);
            TestEqual(TEXT("Array metadata preserved"), Def.bIsArray, Array);
            TestTrue(TEXT("Null and non-null default presence retained"), Def.bHasDefaultValue);
            TestEqual(TEXT("Null is distinct"), Def.bDefaultIsNull, Input->GetField<EJson::None>(TEXT("default_value"))->Type == EJson::Null);
        }
    }
    FString Json, Error; FAtlasWorkflowSchema Legacy;
    FFileHelper::LoadFileToString(Json, *(Root / TEXT("LegacyAllTypes.json")));
    TestTrue(TEXT("Original all-types export parses"), UAtlasWorkflowAsset::ParseSchemaFromJson(Json, Legacy, Error));
    TestEqual(TEXT("Original input count"), Legacy.Inputs.Num(), 15);
    TestEqual(TEXT("Original output count"), Legacy.Outputs.Num(), 21);
    for (const auto& Def : Legacy.Inputs) TestTrue(TEXT("Legacy absent metadata retains required behavior"), Def.bRequired && !Def.bHasRequiredMetadata);
    const FString Prefix = TEXT(R"({"api_id":"test","version":"0.2","base_url":"https://api.dev.atlas.design","inputs":)");
    auto Parse = [&](const FString& Definitions, FAtlasWorkflowSchema& Out) { return UAtlasWorkflowAsset::ParseSchemaFromJson(Prefix + Definitions + TEXT("}"), Out, Error); };
    FAtlasWorkflowSchema Schema;
    TestTrue(TEXT("Presence fixture parses"), Parse(TEXT(R"([{"id":"required","type":"boolean","required":true},{"id":"text","type":"string","required":false,"default_value":""},{"id":"number","type":"number","required":false,"default_value":0},{"id":"flag","type":"boolean","required":false,"default_value":false},{"id":"file","type":"agent_context","required":false,"default_value":"92543afc-e8be-4c81-aa23-5205a331e7fd"},{"id":"null","type":"image","required":false,"default_value":null}])"), Schema));
    auto Defaults = Schema.CreateDefaultInputs();
    TestEqual(TEXT("Defaults preserve false zero empty and file ID; omit null and absent"), Defaults.Num(), 4);
    TestTrue(TEXT("Context is a file asset"), Schema.Inputs[4].Type == EAtlasValueType::File);
    TestEqual(TEXT("Compound format retained"), Schema.Inputs[4].Format, FString(TEXT("atlas-context.json")));
    TestTrue(TEXT("File default becomes reusable ID"), Defaults.Values[TEXT("file")].Type == EAtlasValueType::FileId);
    TestTrue(TEXT("Empty text default is typed text"), Defaults.Values[TEXT("text")].Type == EAtlasValueType::String);
    FAtlasWorkflowInputs Inputs; TArray<FString> Errors;
    TestFalse(TEXT("Missing required rejected"), Schema.ValidateInputs(Inputs, Errors));
    Inputs.SetValue(TEXT("required"), FAtlasValue::MakeBool(false));
    TestTrue(TEXT("Optional fields can be omitted; explicit false remains valid"), Schema.ValidateInputs(Inputs, Errors));
    Inputs.SetValue(TEXT("null"), FAtlasValue());
    TestFalse(TEXT("Explicit invalid value is not omission"), Schema.ValidateInputs(Inputs, Errors));
    Inputs.Values.Remove(TEXT("null")); Inputs.SetValue(TEXT("unknown"), FAtlasValue::MakeString(TEXT("x")));
    TestFalse(TEXT("Unknown keys rejected"), Schema.ValidateInputs(Inputs, Errors));
    for (const FString Bad : {TEXT(R"([{"id":"x","type":"image","required":"false"}])"), TEXT(R"([{"id":"x","type":"image","isArray":"true"}])"), TEXT(R"([{"id":"x","type":"image","isArray":true,"default_value":[true]}])")})
        TestFalse(TEXT("Malformed contract rejected"), Parse(Bad, Schema));
    TestTrue(TEXT("Unknown type remains visible"),Parse(TEXT(R"([{"id":"x","type":"typo"}])"),Schema));
    TestTrue(TEXT("Unknown type stays unsupported"),Schema.Inputs[0].Type==EAtlasValueType::None && Schema.Inputs[0].OriginalType==TEXT("typo"));
    TestTrue(TEXT("File array schema parses"), Parse(TEXT(R"([{"id":"x","type":"image","isArray":true,"default_value":[]}])"), Schema));
    TestEqual(TEXT("Empty list default preserved as JSON"), Schema.Inputs[0].DefaultValueJson, FString(TEXT("[]")));
    Inputs.Values.Reset(); Inputs.SetValue(TEXT("x"), FAtlasValue::MakeFileId(TEXT("an-id")));
    TestFalse(TEXT("Array input rejects scalar"), Schema.ValidateInputs(Inputs, Errors));
    Inputs=Schema.CreateDefaultInputs();
    TestTrue(TEXT("Explicit empty default is a typed list"), Inputs.Values.FindChecked(TEXT("x")).bIsArray);
    TestTrue(TEXT("Empty list executes without becoming missing"), Schema.ValidateInputs(Inputs, Errors));
    Schema.Inputs[0].bRequired=false; Inputs.Values.Empty();
    TestTrue(TEXT("Optional list may be omitted"), Schema.ValidateInputs(Inputs,Errors));
    return true;
}
#endif
