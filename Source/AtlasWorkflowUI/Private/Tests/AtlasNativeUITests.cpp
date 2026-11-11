#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "AtlasNativeInputWidgets.h"
#include "AtlasFileWidgets.h"
#include "AtlasNativeWorkflowWidgets.h"
#include "AtlasHistoryFilterWidget.h"
#include "AtlasEditorSubsystem.h"
#include "AtlasJobManager.h"
#include "AtlasHistoryManager.h"
#include "AtlasSDKSettings.h"
#include "Misc/ScopeExit.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditorUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "K2Node.h"
#include "AtlasWorkflowAsset.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "AtlasNativeUITestObserver.h"
#include "Components/CheckBox.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/MultiLineEditableText.h"
#include "Components/WidgetSwitcher.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasNativeUIInputs, "Atlas.NativeUI.InputControls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAtlasNativeUIInputs::RunTest(const FString&)
{
    UClass* Class = LoadClass<UAtlasInputFieldWidget>(nullptr, TEXT("/AtlasWorkflow/Core/UI/Editor/Utilities/EUW_InputField.EUW_InputField_C"));
    if (!TestNotNull(TEXT("Migrated input widget loads with native parent"), Class)) return false;
    UWorld* World = GEditor->GetEditorWorldContext().World();
    TStrongObjectPtr<UAtlasInputFieldWidget> Field(CreateWidget<UAtlasInputFieldWidget>(World, Class));
    if (!TestNotNull(TEXT("Create designer input"), Field.Get())) return false;
    Field->currentDefParameter.Name = TEXT("prompt");
    Field->currentDefParameter.DisplayName = TEXT("Prompt label");
    Field->currentDefParameter.Type = EAtlasValueType::String;
    Field->defaultValue = FAtlasValue::MakeString(TEXT("{\"prompt\":\"ordinary text\"}"));
    TSharedPtr<SWidget> SlateWidget = Field->TakeWidget();
    FString Name; FAtlasValue Value;
    Field->CollectValue(Name, Value);
    TestEqual(TEXT("Wire key preserved"), Name, FString(TEXT("prompt")));
    TestEqual(TEXT("JSON-looking text remains text"), Value.StringValue, Field->defaultValue.StringValue);
    auto* Label = Cast<UTextBlock>(Field->GetWidgetFromName(TEXT("InputName")));
    TestEqual(TEXT("Display label set without property-binding graph"), Label->GetText().ToString(), FString(TEXT("Prompt label")));
    auto* Text = Cast<UMultiLineEditableText>(Field->GetWidgetFromName(TEXT("EditableText_string")));
    Text->SetText(FText::FromString(TEXT("Edited value")));
    // Existing Blueprint signature remains usable by the unmigrated workflow panel.
    UFunction* GetValue = Field->FindFunction(TEXT("GetValue"));
    if (!TestNotNull(TEXT("Compatibility GetValue exists"), GetValue)) return false;
    FStructOnScope Params(GetValue);
    Field->ProcessEvent(GetValue, Params.GetStructMemory());
    auto* Out = FindFProperty<FStructProperty>(GetValue, TEXT("value"));
    if (!TestNotNull(TEXT("Original output pin preserved"), Out)) return false;
    TestEqual(TEXT("Blueprint caller reads native value"), Out->ContainerPtrToValuePtr<FAtlasValue>(Params.GetStructMemory())->StringValue, FString(TEXT("Edited value")));
    TStrongObjectPtr<UAtlasNativeUITestObserver> Observer(NewObject<UAtlasNativeUITestObserver>());
    Field->OnInputUpdated.AddDynamic(Observer.Get(), &UAtlasNativeUITestObserver::Changed);
    FAtlasParameterDef Def; Def.Name = TEXT("flag"); Def.Type = EAtlasValueType::Boolean; Def.bHasDefaultValue=true; Def.DefaultValue=TEXT("true");
    Field->ConfigureInput(Def);
    TestEqual(TEXT("Boolean default label matches value"),Field->BooleanValueLabel->GetText().ToString(),FString(TEXT("True")));
    auto* Check = Cast<UCheckBox>(Field->GetWidgetFromName(TEXT("CheckBox_Bool")));
    Check->SetIsChecked(true);
    Field->CollectValue(Name, Value);
    TestTrue(TEXT("Boolean collected"), Value.BoolValue);
    const int32 Before = Observer->Changes;
    Check->OnCheckStateChanged.Broadcast(true);
    TestEqual(TEXT("One notification per interaction"), Observer->Changes, Before + 1);
    // Reconstruction must not reset user values or multiply native bindings.
    SlateWidget.Reset(); Field->ReleaseSlateResources(true); SlateWidget = Field->TakeWidget();
    Field->CollectValue(Name, Value); TestTrue(TEXT("Selection survives reconstruction"), Value.BoolValue);
    const int32 BeforeRebuilt = Observer->Changes;
    Check->OnCheckStateChanged.Broadcast(true);
    TestEqual(TEXT("No duplicate subscription after reconstruction"), Observer->Changes, BeforeRebuilt + 1);
    Def.Name = TEXT("amount"); Def.Type = EAtlasValueType::Number; Def.bHasMinimum = true; Def.MinValue = -10;
    Field->ConfigureInput(Def);
    auto* Spin = Cast<USpinBox>(Field->GetWidgetFromName(TEXT("EditorUtilitySpinBox_260")));
    Spin->SetValue(42.5f); Field->CollectValue(Name, Value);
    TestEqual(TEXT("Min-only definition does not invent max=1"), Value.NumberValue, 42.5f);
    Def.Name = TEXT("image"); Def.Type = EAtlasValueType::Image;
    Field->ConfigureInput(Def);
    auto* Picker = Cast<UAtlasFilePickerWidget>(Field->GetWidgetFromName(TEXT("EUW_FilePicker_Image")));
    auto* MeshPicker = Cast<UAtlasFilePickerWidget>(Field->GetWidgetFromName(TEXT("EUW_FilePicker_Mesh")));
    if (!TestNotNull(TEXT("Embedded picker has native parent"), Picker)) return false;
    TestFalse(TEXT("Mesh instance remains a mesh picker"), MeshPicker->isTexture);
    const FString Fixture = FPaths::ProjectSavedDir() / (FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".png"));
    FFileHelper::SaveStringToFile(TEXT("Path-selection fixture, no image decoding"), *Fixture);
    Picker->SelectFile(Fixture); Field->CollectValue(Name, Value);
    TestEqual(TEXT("File input collected"), Value.FilePath, Fixture);
    TestTrue(TEXT("Typed image value"), Value.Type == EAtlasValueType::Image);
    const int32 BeforeClear = Observer->Changes;
    Picker->SelectFile(TEXT("")); Field->CollectValue(Name, Value);
    TestTrue(TEXT("Cleared file is missing input"), Value.Type == EAtlasValueType::None);
    TestEqual(TEXT("Clear propagates once"), Observer->Changes, BeforeClear + 1);
    Field->OnInputUpdated.RemoveDynamic(Observer.Get(), &UAtlasNativeUITestObserver::Changed);
    SlateWidget.Reset(); Field->ReleaseSlateResources(true);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasSingleFiles, "Atlas.NativeUI.SingleFiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAtlasSingleFiles::RunTest(const FString&)
{
    UWorld* World = GEditor->GetEditorWorldContext().World();
    UClass* InputClass = LoadClass<UAtlasInputFieldWidget>(nullptr, TEXT("/AtlasWorkflow/Core/UI/Editor/Utilities/EUW_InputField.EUW_InputField_C"));
    UClass* RowClass = LoadClass<UAtlasParameterRowWidget>(nullptr, TEXT("/AtlasWorkflow/Core/UI/Editor/Utilities/EUW_ParameterRow.EUW_ParameterRow_C"));
    if (!TestNotNull(TEXT("Input class"), InputClass) || !TestNotNull(TEXT("Row class"), RowClass)) return false;
    TStrongObjectPtr<UAtlasInputFieldWidget> Field(CreateWidget<UAtlasInputFieldWidget>(World, InputClass));
    FAtlasParameterDef Def; Def.Name = TEXT("context"); Def.Type = EAtlasValueType::File;
    Def.OriginalType = TEXT("agent_context"); Def.Format = TEXT("atlas-context.json"); Def.AllowedExtensions = {Def.Format};
    Def.bHasDefaultValue = true; Def.DefaultValue = TEXT("a439f5b9-9b37-4a52-8d9d-441c142291c5");
    Field->currentDefParameter = Def; Field->defaultValue = Def.GetDefaultValueAsAtlasValue();
    TSharedPtr<SWidget> Slate = Field->TakeWidget();
    FString Name; FAtlasValue Value; Field->CollectValue(Name, Value);
    TestEqual(TEXT("File default reaches picker"), Value.FileId, Def.DefaultValue);
    TestTrue(TEXT("Existing ID avoids local upload"), Value.FilePath.IsEmpty());
    TestEqual(TEXT("Original type retained"), Value.OriginalType, Def.OriginalType);
    auto* Picker = Cast<UAtlasFilePickerWidget>(Field->GetWidgetFromName(TEXT("EUW_FilePicker_Image")));
    auto* Id = Picker ? Cast<UEditableTextBox>(Picker->GetWidgetFromName(TEXT("AssetIdInput"))) : nullptr;
    if (!TestNotNull(TEXT("Visible asset ID control exists"), Id)) return false;
    Id->SetText(FText::FromString(TEXT("invalid"))); Id->OnTextChanged.Broadcast(Id->GetText()); Field->CollectValue(Name, Value);
    TestEqual(TEXT("Rejected ID preserves committed selection"), Value.FileId, Def.DefaultValue);
    Id->SetText(FText::FromString(Def.DefaultValue)); Id->OnTextChanged.Broadcast(Id->GetText()); Field->CollectValue(Name, Value);
    TestEqual(TEXT("Typing restores valid ID"), Value.FileId, Def.DefaultValue);
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/SingleFiles") / FGuid::NewGuid().ToString(EGuidFormats::Digits));
    IFileManager::Get().MakeDirectory(*Root, true);
    const FString Source = Root / TEXT("context.atlas-context.json");
    const FString Contents = TEXT("{\"conversation\":[],\"fixture\":true}");
    TestTrue(TEXT("Create file fixture"), FFileHelper::SaveStringToFile(Contents, *Source));
    Picker->SelectFile(Source); Field->CollectValue(Name, Value);
    TestEqual(TEXT("Compound suffix allowed"), Value.FilePath, Source);
    TestTrue(TEXT("Local selection clears ID"), Value.FileId.IsEmpty());
    Picker->ConfigurePicker(EAtlasValueType::Image, {TEXT("exr")});
    auto* FromProject = Picker->GetWidgetFromName(TEXT("EditorUtilityButton_FromProject"));
    TestTrue(TEXT("EXR does not offer PNG project export"), FromProject->GetVisibility() == ESlateVisibility::Collapsed);
    auto* Settings = GetMutableDefault<UAtlasSDKSettings>(); const auto Old = Settings->OutputFolder;
    Settings->OutputFolder.Path = Root;
    ON_SCOPE_EXIT { Settings->OutputFolder = Old; };
    Value.FileId = Def.DefaultValue; Value.FileName = FPaths::GetCleanFilename(Source);
    FAtlasJobHistoryRecord Record; Record.JobId = FGuid::NewGuid(); Record.ApiId = TEXT("single-files");
    Record.StartedAt = Record.CompletedAt = FDateTime::UtcNow(); Record.Outputs.Values.Add(TEXT("context"), Value);
    Record.OutputFilePaths.Add(TEXT("context"), Source);
    TStrongObjectPtr<UAtlasParameterRowWidget> Row(CreateWidget<UAtlasParameterRowWidget>(World, RowClass));
    Row->ConfigureParameter(false, TEXT("context"), Value, true, Record);
    TSharedPtr<SWidget> RowSlate = Row->TakeWidget();
    TestFalse(TEXT("Context has no native importer"), Row->CanImportOutput());
    auto* Actions = Row->GetWidgetFromName(TEXT("FileActions"));
    if (!TestNotNull(TEXT("Designer-owned file actions exist"), Actions)) return false;
    TestTrue(TEXT("Legacy actions hidden behind shared card"), Actions->GetVisibility() == ESlateVisibility::Collapsed);
    TestTrue(TEXT("Shared output card reachable"),Row->FilePresentation && Row->FilePresentation->GetVisibility()==ESlateVisibility::Visible);
    TestTrue(TEXT("Copy original file"), Row->SaveOutputCopy(Root / TEXT("copy.atlas-context.json")));
    TestFalse(TEXT("Existing destination protected"), Row->SaveOutputCopy(Root / TEXT("copy.atlas-context.json")));
    TArray<uint8> Original, Copy;
    FFileHelper::LoadFileToArray(Original, *Source); FFileHelper::LoadFileToArray(Copy, *(Root / TEXT("copy.atlas-context.json")));
    TestTrue(TEXT("Original bytes preserved"), Original == Copy);
    RowSlate.Reset(); Row->ReleaseSlateResources(true); Slate.Reset(); Field->ReleaseSlateResources(true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasNativeUIWorkflow, "Atlas.NativeUI.WorkflowCoordination",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAtlasNativeUIWorkflow::RunTest(const FString&)
{
    UWorld* World = GEditor->GetEditorWorldContext().World();
    UClass* Class = LoadClass<UAtlasCurrentWorkflowWidget>(nullptr, TEXT("/AtlasWorkflow/Core/UI/Editor/SingleWorkflow/EUW_CurrentWorkflow.EUW_CurrentWorkflow_C"));
    UClass* RowClass = LoadClass<UAtlasParameterRowWidget>(nullptr, TEXT("/AtlasWorkflow/Core/UI/Editor/Utilities/EUW_ParameterRow.EUW_ParameterRow_C"));
    if (!TestNotNull(TEXT("Workflow native class"), Class) || !TestNotNull(TEXT("Row native class"), RowClass)) return false;
    TStrongObjectPtr<UAtlasCurrentWorkflowWidget> Panel(CreateWidget<UAtlasCurrentWorkflowWidget>(World, Class));
    TStrongObjectPtr<UAtlasWorkflowAsset> Workflow(NewObject<UAtlasWorkflowAsset>());
    Workflow->Schema.ApiId = TEXT("native-ui-fixture"); Workflow->Schema.Name = TEXT("Fixture workflow");
    FAtlasParameterDef Def; Def.Name = TEXT("prompt"); Def.DefaultValue = TEXT("Initial text");
    Workflow->Schema.Inputs.Add(Def);
    Def.Name = TEXT("result"); Def.Type = EAtlasValueType::Image; Workflow->Schema.Outputs.Add(Def);
    // Call the original Blueprint interface before Slate construction, as existing callers do.
    UFunction* SetWorkflow = Panel->FindFunction(TEXT("SetWorkflow"));
    if (!TestNotNull(TEXT("Original SetWorkflow entry point"), SetWorkflow)) return false;
    FStructOnScope Args(SetWorkflow);
    auto* WorkflowPin = FindFProperty<FObjectPropertyBase>(SetWorkflow, TEXT("CurrentWorkflow"));
    if (!TestNotNull(TEXT("Original workflow pin"), WorkflowPin)) return false;
    WorkflowPin->SetObjectPropertyValue_InContainer(Args.GetStructMemory(), Workflow.Get());
    Panel->ProcessEvent(SetWorkflow, Args.GetStructMemory());
    TSharedPtr<SWidget> SlatePanel = Panel->TakeWidget();
    auto* InputBox = Cast<UVerticalBox>(Panel->GetWidgetFromName(TEXT("VB_Inputs_Container")));
    auto* OutputBox = Cast<UVerticalBox>(Panel->GetWidgetFromName(TEXT("VB_Outputs_Container")));
    auto* Run = Cast<UButton>(Panel->GetWidgetFromName(TEXT("ButtonRunWorkflow")));
    if (!TestEqual(TEXT("One generated input"), InputBox->GetChildrenCount(), 1)) return false;
    TestEqual(TEXT("One generated output"), OutputBox->GetChildrenCount(), 1);
    FAtlasValue Collected; Panel->Inputs.GetValue(TEXT("prompt"), Collected);
    TestEqual(TEXT("Default collected after construction"), Collected.StringValue, FString(TEXT("Initial text")));
    TestTrue(TEXT("Valid input enables Run"), Run->GetIsEnabled());
    TStrongObjectPtr<UAtlasInputFieldWidget> OldField(Cast<UAtlasInputFieldWidget>(InputBox->GetChildAt(0)));
    auto* Text = Cast<UMultiLineEditableText>(OldField->GetWidgetFromName(TEXT("EditableText_string")));
    Text->SetText(FText::FromString(TEXT("Pending edit")));
    Panel->RefreshInputs(); Panel->Inputs.GetValue(TEXT("prompt"), Collected);
    TestEqual(TEXT("Collection includes uncommitted edit"), Collected.StringValue, FString(TEXT("Pending edit")));
    SlatePanel.Reset(); Panel->ReleaseSlateResources(true); SlatePanel = Panel->TakeWidget();
    Panel->Inputs.GetValue(TEXT("prompt"), Collected);
    TestEqual(TEXT("Reconstruction preserves edit"), Collected.StringValue, FString(TEXT("Pending edit")));
    Panel->SelectWorkflow(nullptr);
    TestEqual(TEXT("Switch clears inputs"), InputBox->GetChildrenCount(), 0);
    TestFalse(TEXT("Old input subscription detached"), OldField->OnInputUpdated.IsAlreadyBound(Panel.Get(), &UAtlasCurrentWorkflowWidget::RefreshInputs));
    TestFalse(TEXT("No workflow disables Run"), Run->GetIsEnabled());
    Panel->RunWorkflow(); // Invalid state must not create any job or access an API.
    TestTrue(TEXT("No stale collected values"), Panel->Inputs.Values.IsEmpty());
    Def.Name = TEXT("file"); Def.Type = EAtlasValueType::Image; Def.DefaultValue.Empty();
    Workflow->Schema.Inputs = {Def}; Panel->SelectWorkflow(Workflow.Get());
    TestFalse(TEXT("Missing required file disables Run"), Run->GetIsEnabled());
    TestFalse(TEXT("Validation explains missing file"), Run->GetToolTipText().IsEmpty());
    TStrongObjectPtr<UAtlasParameterRowWidget> Row(CreateWidget<UAtlasParameterRowWidget>(World, RowClass));
    UFunction* SetParam = Row->FindFunction(TEXT("SetParam"));
    if (!TestNotNull(TEXT("Original SetParam entry point"), SetParam)) return false;
    FStructOnScope RowArgs(SetParam);
    FindFProperty<FStrProperty>(SetParam, TEXT("ParamName"))->SetPropertyValue_InContainer(RowArgs.GetStructMemory(), TEXT("Result"));
    *FindFProperty<FStructProperty>(SetParam, TEXT("atlasValue"))->ContainerPtrToValuePtr<FAtlasValue>(RowArgs.GetStructMemory()) = FAtlasValue::MakeString(TEXT("Result value"));
    Row->ProcessEvent(SetParam, RowArgs.GetStructMemory());
    TSharedPtr<SWidget> SlateRow = Row->TakeWidget();
    auto* RowText = Cast<UEditableText>(Row->GetWidgetFromName(TEXT("ValueText00")));
    TestEqual(TEXT("SetParam before construction renders value"), RowText->GetText().ToString(), FString(TEXT("Result value")));
    TestTrue(TEXT("History result is read-only"), RowText->GetIsReadOnly());
    FAtlasValue ImageValue; ImageValue.Type = EAtlasValueType::Image;
    Row->ConfigureParameter(false, TEXT("image"), ImageValue, false, FAtlasJobHistoryRecord());
    TestTrue(TEXT("Schema placeholder hides import"), Row->GetWidgetFromName(TEXT("EUW_ImportImage"))->GetVisibility() == ESlateVisibility::Collapsed);
    Row->ConfigureParameter(false, TEXT("flag"), FAtlasValue::MakeBool(true), false, FAtlasJobHistoryRecord());
    TestTrue(TEXT("Boolean row rendered"), Cast<UCheckBox>(Row->GetWidgetFromName(TEXT("ValueBool1")))->IsChecked());
    TestNull(TEXT("Previous image brush cleared"), Cast<UImage>(Row->GetWidgetFromName(TEXT("ImagePReview")))->GetBrush().GetResourceObject());
    SlateRow.Reset(); Row->ReleaseSlateResources(true);
    SlatePanel.Reset(); Panel->ReleaseSlateResources(true);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasNativeUIDesigner, "Atlas.NativeUI.DesignerCompileLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAtlasNativeUIDesigner::RunTest(const FString&)
{
    FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    Registry.Get().ScanPathsSynchronous({TEXT("/AtlasWorkflow/Core/UI")}, true);
    TArray<FAssetData> Assets;
    Registry.Get().GetAssetsByPath(TEXT("/AtlasWorkflow/Core/UI"), Assets, true);
    int32 Previewed = 0;
    for (const auto& Asset : Assets)
    {
        auto* BP = Cast<UWidgetBlueprint>(Asset.GetAsset());
        if (!BP) continue;
        TArray<UK2Node*> Nodes;
        FBlueprintEditorUtils::GetAllNodesOfClass(BP, Nodes);
        for (auto* Node : Nodes)
            TestTrue(BP->GetName() + TEXT(" node supports undo: ") + Node->GetName(), Node->HasAnyFlags(RF_Transactional));
        const auto OriginalStatus = BP->Status;
        // These are the non-Actor repair passes run when Blueprint Editor opens a widget.
        FBlueprintEditorUtils::PurgeNullGraphs(BP);
        FKismetEditorUtilities::UpgradeCosmeticallyStaleBlueprint(BP);
        FBlueprintEditorUtils::ConformCallsToParentFunctions(BP);
        FBlueprintEditorUtils::ConformImplementedEvents(BP);
        FBlueprintEditorUtils::ConformImplementedInterfaces(BP);
        FBlueprintEditorUtils::UpdateOutOfDateCompositeNodes(BP);
        FBlueprintEditorUtils::UpdateTransactionalFlags(BP);
        TestTrue(BP->GetName() + TEXT(" opens without repair changing compile status"), BP->Status == OriginalStatus);
        if (BP->ParentClass != UAtlasCurrentWorkflowWidget::StaticClass()
            && BP->ParentClass != UAtlasParameterRowWidget::StaticClass()
            && BP->ParentClass != UAtlasInputFieldWidget::StaticClass()
            && BP->ParentClass != UAtlasFilePickerWidget::StaticClass()
            && BP->ParentClass != UAtlasHistoryFilterWidget::StaticClass()
            && BP->ParentClass != UAtlasFileCard::StaticClass()
            && BP->ParentClass != UAtlasFilePanel::StaticClass()) continue;
        // Use the Designer preview factory, then exercise incomplete bindings as seen during
        // Blueprint reinstancing. Normal CreateWidget/reconstruction did not cover this case.
        FWidgetBlueprintEditorUtils::FCreateWidgetFromBlueprintParams Params;
        Params.FlagsToApply = EWidgetDesignFlags::Designing;
        UWorld* World = GEditor->GetEditorWorldContext().World();
        auto* Preview = FWidgetBlueprintEditorUtils::CreateUserWidgetFromBlueprint(World, BP, Params);
        if (!TestNotNull(TEXT("Designer preview created"), Preview)) return false;
        TSharedPtr<SWidget> Slate = Preview->TakeWidget();
        // Reinstancing can clear some or all bindings before the old Slate preview is released.
        // Explicitly reproduce that state for all four bases, including partially initialized objects.
        for (TFieldIterator<FObjectPropertyBase> It(Preview->GetClass()); It; ++It)
            if (It->HasMetaData(TEXT("BindWidget")) || It->HasMetaData(TEXT("BindWidgetOptional"))) It->SetObjectPropertyValue_InContainer(Preview, nullptr);
        if (auto* RunProperty = FindFProperty<FObjectPropertyBase>(Preview->GetClass(), TEXT("ButtonRunWorkflow")))
            TestNull(TEXT("Exercises the null binding reported by the crash"), RunProperty->GetObjectPropertyValue_InContainer(Preview));
        // Dropping the Slate preview calls NativeDestruct even though NativeConstruct never ran.
        Slate.Reset();
        FWidgetBlueprintEditorUtils::DestroyUserWidget(Preview);
        FKismetEditorUtilities::CompileBlueprint(BP);
        TestTrue(BP->GetName() + TEXT(" compiles after preview teardown"), BP->IsUpToDate());
        Preview = FWidgetBlueprintEditorUtils::CreateUserWidgetFromBlueprint(World, BP, Params);
        Slate = Preview->TakeWidget();
        Slate.Reset();
        FWidgetBlueprintEditorUtils::DestroyUserWidget(Preview);
        ++Previewed;
    }
    TestEqual(TEXT("All seven native widgets previewed, compiled and reopened"), Previewed, 7);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasNativeUIFilters, "Atlas.NativeUI.HistoryFilters",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAtlasNativeUIFilters::RunTest(const FString&)
{
    auto* Settings = GetMutableDefault<UAtlasSDKSettings>();
    const FString PreviousOutput = Settings->OutputFolder.Path;
    Settings->OutputFolder.Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("FilterFixture") / FGuid::NewGuid().ToString(EGuidFormats::Digits));
    ON_SCOPE_EXIT { Settings->OutputFolder.Path = PreviousOutput; };
    auto* History = GEditor->GetEditorSubsystem<UAtlasEditorSubsystem>()->GetJobManager()->GetHistoryManager();
    const FString ApiA = FGuid::NewGuid().ToString();
    const FString ApiB = FGuid::NewGuid().ToString();
    auto Save = [this, History](const FString& Api, EAtlasJobStatus Status, FDateTime Date)
    {
        FAtlasJobHistoryRecord Record;
        Record.JobId = FGuid::NewGuid(); Record.ApiId = Api; Record.WorkflowName = TEXT("Duplicate name");
        Record.Status = Status; Record.StartedAt = Record.CompletedAt = Date;
        TestTrue(TEXT("Save isolated filter fixture"), History->SaveRecord(Record));
    };
    const FDateTime Today = FDateTime::Today();
    Save(ApiA, EAtlasJobStatus::Success, Today + FTimespan::FromMinutes(1));
    Save(ApiA, EAtlasJobStatus::Failed, Today + FTimespan::FromMinutes(2));
    Save(ApiB, EAtlasJobStatus::Failed, Today + FTimespan::FromMinutes(3));
    Save(ApiA, EAtlasJobStatus::Failed, Today - FTimespan::FromDays(8));
    UClass* Class = LoadClass<UUserWidget>(nullptr, TEXT("/AtlasWorkflow/Core/UI/Editor/JobHistory/EUW_JobLister.EUW_JobLister_C"));
    if (!TestNotNull(TEXT("Existing job lister loads"), Class)) return false;
    TStrongObjectPtr<UUserWidget> Lister(CreateWidget<UUserWidget>(GEditor->GetEditorWorldContext().World(), Class));
    TSharedPtr<SWidget> SlateLister = Lister->TakeWidget();
    auto* Filter = Cast<UAtlasHistoryFilterWidget>(Lister->GetWidgetFromName(TEXT("EUW_HistoryFilterBar")));
    if (!TestNotNull(TEXT("Lister contains migrated filter"), Filter)) return false;
    auto* Status = Cast<UComboBoxString>(Filter->GetWidgetFromName(TEXT("ComboFilter_Status")));
    auto* Workflow = Cast<UComboBoxString>(Filter->GetWidgetFromName(TEXT("ComboFilter_Type")));
    auto* Date = Cast<UComboBoxString>(Filter->GetWidgetFromName(TEXT("ComboFilter_Date")));
    Filter->RefreshWorkflowOptions();
    TestEqual(TEXT("All plus two distinct workflow IDs"), Workflow->GetOptionCount(), 3);
    TStrongObjectPtr<UAtlasNativeUITestObserver> Observer(NewObject<UAtlasNativeUITestObserver>());
    Filter->OnRefreshHistoryList.AddDynamic(Observer.Get(), &UAtlasNativeUITestObserver::QueryChanged);
    auto QueryFromLister = [&]() -> FAtlasHistoryQuery
    {
        return *FindFProperty<FStructProperty>(Lister->GetClass(), TEXT("lastQuery"))->ContainerPtrToValuePtr<FAtlasHistoryQuery>(Lister.Get());
    };
    auto GroupCount = [&](FName Name)
    {
        UObject* Group = Lister->GetWidgetFromName(Name);
        FArrayProperty* Records = FindFProperty<FArrayProperty>(Group->GetClass(), TEXT("currentRecords"));
        return FScriptArrayHelper(Records, Records->ContainerPtrToValuePtr<void>(Group)).Num();
    };
    Status->SetSelectedOption(TEXT("Failed"));
    TestEqual(TEXT("One filter notification"), Observer->Changes, 1);
    TestTrue(TEXT("Existing Blueprint listener receives status"), QueryFromLister().bFilterByStatus);
    TestEqual(TEXT("Rendered Today group filtered to two failures"), GroupCount(TEXT("EUW_HistoryGroup_Today")), 2);
    Workflow->SetSelectedOption(TEXT("Duplicate name [") + ApiA + TEXT("]"));
    Date->SetSelectedOption(TEXT("Today"));
    const auto Combined = QueryFromLister();
    TestEqual(TEXT("Uses API ID rather than label"), Combined.ApiId, ApiA);
    TestTrue(TEXT("Status and date retained together"), Combined.bFilterByStatus && Combined.bFilterByDate);
    TestEqual(TEXT("All three filters reach actual rendered group"), GroupCount(TEXT("EUW_HistoryGroup_Today")), 1);
    TestEqual(TEXT("Older group cleared"), GroupCount(TEXT("EUW_HistoryGroup_30days")), 0);
    FAtlasJobHistoryRecord Boundary; Boundary.ApiId = ApiA; Boundary.Status = EAtlasJobStatus::Failed; Boundary.CompletedAt = Today + FTimespan::FromDays(1);
    TestFalse(TEXT("Today excludes next midnight"), Combined.Matches(Boundary));
    Date->SetSelectedOption(TEXT("Last 7 Days"));
    Boundary.CompletedAt = Today - FTimespan::FromDays(6);
    TestTrue(TEXT("Seven calendar days include first midnight"), Filter->currentQuery.Matches(Boundary));
    Boundary.CompletedAt -= FTimespan(1);
    TestFalse(TEXT("Seven calendar days exclude preceding tick"), Filter->currentQuery.Matches(Boundary));
    Date->SetSelectedOption(TEXT("All"));
    TestFalse(TEXT("All dates removes date only"), QueryFromLister().bFilterByDate);
    TestTrue(TEXT("Status remains selected"), QueryFromLister().bFilterByStatus);
    TestEqual(TEXT("Older matching job restored"), GroupCount(TEXT("EUW_HistoryGroup_30days")), 1);
    const int32 BeforeRefresh = Observer->Changes;
    Filter->RefreshWorkflowOptions();
    TestEqual(TEXT("Refreshing options does not trigger another query"), Observer->Changes, BeforeRefresh);
    TestEqual(TEXT("Selected workflow preserved"), Filter->currentQuery.ApiId, ApiA);
    const int32 WorkflowOptionCount = Workflow->GetOptionCount();
    SlateLister.Reset(); Lister->ReleaseSlateResources(true); SlateLister = Lister->TakeWidget();
    TestEqual(TEXT("Reconstruction does not duplicate options"), Workflow->GetOptionCount(), WorkflowOptionCount);
    TestEqual(TEXT("Reconstruction retains status"), Status->GetSelectedOption(), FString(TEXT("Failed")));
    const int32 BeforeChange = Observer->Changes;
    Status->SetSelectedOption(TEXT("Cancelled"));
    TestEqual(TEXT("One event after reconstruction"), Observer->Changes, BeforeChange + 1);
    TestEqual(TEXT("No-match filter clears current rows"), GroupCount(TEXT("EUW_HistoryGroup_Today")), 0);
    Status->SetSelectedOption(TEXT("All")); Workflow->SetSelectedOption(TEXT("All"));
    TestFalse(TEXT("All statuses clears status"), QueryFromLister().bFilterByStatus);
    TestTrue(TEXT("All workflows clears ID"), QueryFromLister().ApiId.IsEmpty());
    TestEqual(TEXT("Unfiltered Today restored"), GroupCount(TEXT("EUW_HistoryGroup_Today")), 3);
    Filter->OnRefreshHistoryList.RemoveDynamic(Observer.Get(), &UAtlasNativeUITestObserver::QueryChanged);
    SlateLister.Reset(); Lister->ReleaseSlateResources(true);
    return true;
}
#endif

