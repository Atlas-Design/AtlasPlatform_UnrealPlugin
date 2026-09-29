#if WITH_DEV_AUTOMATION_TESTS
#include "AtlasArchiveSafety.h"
#include "AtlasHistoryManager.h"
#include "AtlasOutputManager.h"
#include "AtlasSDKSettings.h"
#include "AtlasRuntimeSettings.h"
#include "AtlasJob.h"
#include "AtlasWorkflowAsset.h"
#include "AtlasFileManager.h"
#include "AtlasFileWork.h"
#include "AtlasPlatformAuth.h"
#include "AtlasJsonObject.h"
#include "AtlasHttpLibrary.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

struct FAtlasNonBatchTestAccess
{
 static void Directory(UAtlasHistoryManager* M, const FString& P) { M->TestHistoryDirectory=P; }
 static bool Parse(UAtlasJob* J, UAtlasJsonObject* R) { return J->ParseStatusResponse(R); }
 static FString Key(const FString& Url) { return UAtlasFileManager::CacheKeyFor(Url, TEXT("same-file")); }
 static void Seed(UAtlasFileManager* M, double Time) { M->UploadCache.Add(TEXT("k"), TEXT("id")); M->UploadCacheTimes.Add(TEXT("k"), Time); }
 static void Prune(UAtlasFileManager* M) { M->PruneCaches(); }
};
namespace { constexpr auto Flags=EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasArchiveContracts,"Atlas.NonBatch.Archives",Flags)
bool FAtlasArchiveContracts::RunTest(const FString&)
{
 auto* S=GetMutableDefault<UAtlasSDKSettings>(); const auto Old=S->OutputFolder; const bool Limit=S->bLimitVisibleHistory; const int32 Count=S->MaxHistoryRecordsPerWorkflow;
 const FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Automation/NonBatch")/FGuid::NewGuid().ToString(EGuidFormats::Digits));
 S->OutputFolder.Path=Root/TEXT("Output");
 ON_SCOPE_EXIT { S->OutputFolder=Old; S->bLimitVisibleHistory=Limit; S->MaxHistoryRecordsPerWorkflow=Count; };
 TestFalse(TEXT("Traversal rejected"),AtlasArchive::IsContained(Root,Root/TEXT("../escape")));
 TestFalse(TEXT("Sibling prefix rejected"),AtlasArchive::IsContained(Root,Root+TEXT("other/file")));
 TestFalse(TEXT("Alternate stream rejected"),AtlasArchive::IsContained(Root,Root/TEXT("f:stream")));
 TestNotEqual(TEXT("Sanitization cannot alias"),AtlasArchive::Component(TEXT("a/b")),AtlasArchive::Component(TEXT("a_b")));
 TestFalse(TEXT("Case variants remain distinct on Windows"),AtlasArchive::Component(TEXT("A")).Equals(AtlasArchive::Component(TEXT("a")),ESearchCase::IgnoreCase));
 const FString Path=Root/TEXT("atomic.json");
 TestTrue(TEXT("Initial atomic write"),AtlasArchive::WriteText(Path,TEXT("old")));
 TestTrue(TEXT("Atomic replacement"),AtlasArchive::WriteText(Path,TEXT("new")));
 FString Contents; FFileHelper::LoadFileToString(Contents,*Path); TestEqual(TEXT("Replacement bytes"),Contents,FString(TEXT("new")));
 const FString LongPath=Root/FString::ChrN(80,TEXT('a'))/FString::ChrN(80,TEXT('b'))/TEXT("long.json");
 TestTrue(TEXT("Atomic archive supports long Windows paths"),AtlasArchive::WriteText(LongPath,TEXT("long-path")));
 TStrongObjectPtr<UAtlasHistoryManager> H(NewObject<UAtlasHistoryManager>()); FAtlasNonBatchTestAccess::Directory(H.Get(),Root/TEXT("History"));
 FAtlasJobHistoryRecord A; A.JobId=FGuid::NewGuid(); A.ApiId=TEXT("../unsafe-id"); A.WorkflowName=TEXT("Demo"); A.StartedAt=A.CompletedAt=FDateTime::UtcNow();
 A.TimeBasis=TEXT("UTC"); A.OutputWarnings.Add(TEXT("download missing"));
 TestTrue(TEXT("Save authoritative record"),H->SaveRecord(A));
 auto B=A; B.JobId=FGuid::NewGuid(); B.CompletedAt+=FTimespan::FromSeconds(1); TestTrue(TEXT("Second record"),H->SaveRecord(B));
 S->bLimitVisibleHistory=true; S->MaxHistoryRecordsPerWorkflow=1;
 TestEqual(TEXT("Retention limits display"),H->GetAllHistory().Num(),1);
 H->ClearHistory(); S->bLimitVisibleHistory=false; H->RefreshHistory();
 TestEqual(TEXT("Clear also hides older retained records"),H->GetAllHistory().Num(),0);
 TestTrue(TEXT("Late writer can persist archive"),H->SaveRecord(A));
 TestEqual(TEXT("Tombstone prevents resurrection"),H->GetAllHistory().Num(),0);
 TStrongObjectPtr<UAtlasOutputManager> O(NewObject<UAtlasOutputManager>());
 A.OutputFilePaths.Add(TEXT("x"),Path);
 TestTrue(TEXT("Foreign archive path is not returned"),O->GetOutputFilePathForJobFromHistory(A,TEXT("x")).IsEmpty());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasResponseContracts,"Atlas.NonBatch.Contracts",Flags)
bool FAtlasResponseContracts::RunTest(const FString&)
{
 FAtlasWorkflowSchema Schema; FString Error;
 const FString Prefix=TEXT("{\"api_id\":\"id\",\"base_url\":\"https://api.dev.atlas.design\",\"version\":\"0.2\",");
 TestTrue(TEXT("One-sided bound parses"),UAtlasWorkflowAsset::ParseSchemaFromJson(Prefix+TEXT("\"inputs\":[{\"name\":\"n\",\"type\":\"number\",\"min\":2}],\"outputs\":[]}"),Schema,Error));
 TestTrue(TEXT("Absent upper bound does not become one"),Schema.Inputs[0].ValidateValue(FAtlasValue::MakeNumber(100),Error));
 TestFalse(TEXT("Lower bound enforced"),Schema.Inputs[0].ValidateValue(FAtlasValue::MakeNumber(1),Error));
 TestFalse(TEXT("Bad array rejected"),UAtlasWorkflowAsset::ParseSchemaFromJson(Prefix+TEXT("\"inputs\":{}}"),Schema,Error));
 TestFalse(TEXT("Unknown parameter type rejected"),UAtlasWorkflowAsset::ParseSchemaFromJson(Prefix+TEXT("\"inputs\":[{\"name\":\"x\",\"type\":\"typo\"}]}"),Schema,Error));
 FAtlasValue File; File.Type=EAtlasValueType::Image; File.FileId=TEXT("file-1"); TestEqual(TEXT("Typed file getter"),File.GetFileId(),File.FileId);
 const FString Unicode=TEXT("caf\u00e9/\u4e16\u754c"); TestEqual(TEXT("UTF8 URL round trip"),UAtlasHttpLibrary::PercentDecode(UAtlasHttpLibrary::PercentEncode(Unicode)),Unicode);
 const FAtlasError Http=FAtlasError::FromHttpStatus(429,TEXT("{\"error\":{\"message\":\"quota detail\",\"node_name\":\"node\"}}"));
 TestTrue(TEXT("HTTP category preserved"),Http.Code==EAtlasErrorCode::RateLimited); TestEqual(TEXT("Backend detail preserved"),Http.Message,FString(TEXT("quota detail")));
 TStrongObjectPtr<UAtlasWorkflowAsset> Workflow(NewObject<UAtlasWorkflowAsset>()); FAtlasParameterDef Def; Def.Name=TEXT("result"); Def.Type=EAtlasValueType::Json; Workflow->Schema.Outputs.Add(Def);
 TStrongObjectPtr<UAtlasJob> Job(NewObject<UAtlasJob>()); Job->SetWorkflowAsset(Workflow.Get()); TStrongObjectPtr<UAtlasJsonObject> Response(NewObject<UAtlasJsonObject>());
 Response->DecodeJson(TEXT("{\"result\":{\"outputs\":{\"result\":[1,{\"a\":true}]}}}"));
 TestTrue(TEXT("JSON arrays accepted"),FAtlasNonBatchTestAccess::Parse(Job.Get(),Response.Get()));
 TestTrue(TEXT("Array remains JSON"),Job->Outputs.Values[TEXT("result")].JsonValue.StartsWith(TEXT("[")));
 Response->DecodeJson(TEXT("{\"result\":{\"outputs\":{}}}")); TestFalse(TEXT("Missing output rejected"),FAtlasNonBatchTestAccess::Parse(Job.Get(),Response.Get()));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasCacheContracts,"Atlas.NonBatch.CredentialsAndCaches",Flags)
bool FAtlasCacheContracts::RunTest(const FString&)
{
 auto* S=GetMutableDefault<UAtlasSDKSettings>(); const auto Origins=S->TrustedApiOrigins; const bool Enabled=S->bEnableUploadCache; const int32 Age=S->CacheMaxAgeHours;
 ON_SCOPE_EXIT { S->TrustedApiOrigins=Origins; S->bEnableUploadCache=Enabled; S->CacheMaxAgeHours=Age; };
 S->TrustedApiOrigins={TEXT("https://trusted.example")};
 TestTrue(TEXT("Exact HTTPS origin permitted"),FAtlasPlatformAuth::IsTrustedDestination(TEXT("https://trusted.example:443/path")));
 for (const FString Url : {TEXT("http://trusted.example"),TEXT("https://trusted.example.evil/path"),TEXT("https://trusted.example@evil/path"),TEXT("https://trusted.example:444/path")}) TestFalse(TEXT("Untrusted origin rejected"),FAtlasPlatformAuth::IsTrustedDestination(Url));
 auto* Runtime=GetMutableDefault<UAtlasRuntimeSettings>(); const bool Override=Runtime->bUseRuntimeOverrides; const float Timeout=Runtime->RequestTimeoutSeconds;
 const float LegacyTimeout=S->RequestTimeoutSeconds;
 Runtime->bUseRuntimeOverrides=true; Runtime->RequestTimeoutSeconds=87.f;
 TestEqual(TEXT("Runtime override resolved"),UAtlasSDKSettings::Get()->RequestTimeoutSeconds,87.f);
 Runtime->bUseRuntimeOverrides=false; TestEqual(TEXT("Disabling overrides restores legacy setting"),UAtlasSDKSettings::Get()->RequestTimeoutSeconds,LegacyTimeout);
 Runtime->bUseRuntimeOverrides=Override; Runtime->RequestTimeoutSeconds=Timeout;
 TestNotEqual(TEXT("Cache endpoint scopes differ"),FAtlasNonBatchTestAccess::Key(TEXT("https://a/upload")),FAtlasNonBatchTestAccess::Key(TEXT("https://b/upload")));
 TStrongObjectPtr<UAtlasFileManager> M(NewObject<UAtlasFileManager>());
 S->bEnableUploadCache=true; S->CacheMaxAgeHours=1; FAtlasNonBatchTestAccess::Seed(M.Get(),FPlatformTime::Seconds()-7200); FAtlasNonBatchTestAccess::Prune(M.Get()); TestEqual(TEXT("Expired entry removed"),M->GetUploadCacheSize(),0);
 FAtlasNonBatchTestAccess::Seed(M.Get(),FPlatformTime::Seconds()); S->bEnableUploadCache=false; FAtlasNonBatchTestAccess::Prune(M.Get()); TestEqual(TEXT("Disabled cache emptied"),M->GetUploadCacheSize(),0);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasPreviewContracts,"Atlas.NonBatch.BackgroundPreview",Flags)
bool FAtlasPreviewContracts::RunTest(const FString&)
{
 auto* S=GetMutableDefault<UAtlasSDKSettings>(); const int32 Dimension=S->MaxPreviewDimension; S->MaxPreviewDimension=64; ON_SCOPE_EXIT { S->MaxPreviewDimension=Dimension; };
 auto& Module=FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper")); auto Wrapper=Module.CreateImageWrapper(EImageFormat::PNG);
 TArray<uint8> Raw; Raw.Init(128,128*96*4); TestTrue(TEXT("Create PNG fixture"),Wrapper->SetRaw(Raw.GetData(),Raw.Num(),128,96,ERGBFormat::BGRA,8));
 const auto& Png=Wrapper->GetCompressed(); const FString Path=FPaths::ProjectSavedDir()/TEXT("Automation/preview.png"); IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true); TestTrue(TEXT("Write fixture"),FFileHelper::SaveArrayToFile(Png,*Path));
 TStrongObjectPtr<UAtlasOutputManager> Manager(NewObject<UAtlasOutputManager>()); bool Called=false; UTexture2D* First=nullptr;
 Manager->LoadPreviewAsync(Path,[&](UTexture2D* Texture) { Called=true; First=Texture; }); AtlasFileWork::Drain();
 TestTrue(TEXT("Background callback completes"),Called); TestNotNull(TEXT("Texture created"),First); if (!First) return false;
 TestEqual(TEXT("Preview dimension bounded"),First->GetSizeX(),64); TestEqual(TEXT("Aspect ratio preserved"),First->GetSizeY(),48);
 Manager->LoadPreviewAsync(Path,[&](UTexture2D* Texture) { TestEqual(TEXT("Unchanged file reuses texture"),Texture,First); }); AtlasFileWork::Drain();
 return true;
}
#endif
