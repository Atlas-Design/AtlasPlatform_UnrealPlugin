#include "AtlasNativeWorkflowWidgets.h"
#include "AtlasNativeInputWidgets.h"
#include "AtlasUIHelpers.h"
#include "AtlasEditorSubsystem.h"
#include "AtlasWorkflowAsset.h"
#include "AtlasJob.h"
#include "AtlasOutputManager.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/EditableText.h"
#include "Components/CheckBox.h"
#include "Components/SpinBox.h"
#include "Components/Image.h"
#include "Components/ExpandableArea.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SinglePropertyView.h"
#include "Editor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundWave.h"
#include "ScopedTransaction.h"

namespace
{
UAtlasOutputManager* OutputManager(UObject* Context)
{
    auto* Subsystem = UAtlasUIHelpers::GetEditorSubsystem(Context);
    return Subsystem ? Subsystem->GetOutputManager() : nullptr;
}
}

UAtlasParameterRowWidget::UAtlasParameterRowWidget()
{
    TypeColors = {{EAtlasValueType::String,{.084376f,.296138f,.760525f}},
        {EAtlasValueType::Number,{.791298f,.351533f,.045186f}},
        {EAtlasValueType::Boolean,{.799103f,.072272f,.045186f}},
        {EAtlasValueType::Image,{.791298f,.708376f,.174647f}},
        {EAtlasValueType::Mesh,{.423268f,.219526f,1.f}},
        {EAtlasValueType::Audio,{.949020f,.058824f,.537255f}}};
}
void UAtlasParameterRowWidget::NativeConstruct()
{
    Super::NativeConstruct();
    bReady = true;
    EUW_ImportImage->OnClicked.AddUniqueDynamic(this, &ThisClass::ImportToProject);
    EUW_ImportMeshProject->OnClicked.AddUniqueDynamic(this, &ThisClass::ImportToProject);
    EUW_ImportAudio->OnClicked.AddUniqueDynamic(this, &ThisClass::ImportToProject);
    EUW_ImportMeshScene->OnClicked.AddUniqueDynamic(this, &ThisClass::ImportToScene);
    ValueText00->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::TextCommitted);
    ValueNumber2->OnValueCommitted.AddUniqueDynamic(this, &ThisClass::NumberCommitted);
    ValueBool1->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::BoolChanged);
    RefreshParameter();
}
void UAtlasParameterRowWidget::NativeDestruct()
{
    // Designer previews and reinstanced objects can be torn down without bound controls.
    bReady = false;
    if (IsValid(ImagePReview)) UAtlasUIHelpers::LoadPreviewIntoImage(ImagePReview, FString(), nullptr);
    if (IsValid(EUW_ImportImage)) EUW_ImportImage->OnClicked.RemoveDynamic(this, &ThisClass::ImportToProject);
    if (IsValid(EUW_ImportMeshProject)) EUW_ImportMeshProject->OnClicked.RemoveDynamic(this, &ThisClass::ImportToProject);
    if (IsValid(EUW_ImportAudio)) EUW_ImportAudio->OnClicked.RemoveDynamic(this, &ThisClass::ImportToProject);
    if (IsValid(EUW_ImportMeshScene)) EUW_ImportMeshScene->OnClicked.RemoveDynamic(this, &ThisClass::ImportToScene);
    if (IsValid(ValueText00)) ValueText00->OnTextCommitted.RemoveDynamic(this, &ThisClass::TextCommitted);
    if (IsValid(ValueNumber2)) ValueNumber2->OnValueCommitted.RemoveDynamic(this, &ThisClass::NumberCommitted);
    if (IsValid(ValueBool1)) ValueBool1->OnCheckStateChanged.RemoveDynamic(this, &ThisClass::BoolChanged);
    Super::NativeDestruct();
}
void UAtlasParameterRowWidget::ConfigureParameter(bool InEditable, const FString& InName, const FAtlasValue& InValue, bool InOutput, const FAtlasJobHistoryRecord& InRecord)
{
    bEditable = InEditable; ParameterName = InName; Value = InValue; bOutput = InOutput; Record = InRecord;
    RefreshParameter();
}
void UAtlasParameterRowWidget::RefreshParameter()
{
    if (!bReady) return;
    TGuardValue<bool> Guard(bRendering, true);
    UAtlasUIHelpers::LoadPreviewIntoImage(ImagePReview, FString(), nullptr);
    image = nullptr; mesh = nullptr; audio = nullptr;
    ImageProperty->SetVisibility(ESlateVisibility::Collapsed);
    MeshProperty->SetVisibility(ESlateVisibility::Collapsed);
    AudioProperty->SetVisibility(ESlateVisibility::Collapsed);
    Text_ParamName->SetText(FText::FromString(ParameterName));
    const EAtlasValueType ColorType = Value.Type == EAtlasValueType::Integer ? EAtlasValueType::Number : Value.Type;
    const FLinearColor* Color = TypeColors.Find(ColorType);
    StatusDot->SetColorAndOpacity(Color ? *Color : FLinearColor::White);
    int32 Index = 0;
    switch (Value.Type)
    {
    case EAtlasValueType::Boolean: Index = 1; break;
    case EAtlasValueType::Number: case EAtlasValueType::Integer: Index = 2; break;
    case EAtlasValueType::Image: Index = 3; break;
    case EAtlasValueType::Mesh: Index = 4; break;
    case EAtlasValueType::Audio: Index = 5; break;
    default: break;
    }
    WidgetSwitcher_ParamType->SetActiveWidgetIndex(Index);
    FString Text = Value.StringValue;
    if (Value.Type == EAtlasValueType::Json) Text = Value.JsonValue;
    else if (Value.Type == EAtlasValueType::FileId) Text = Value.FileId;
    else if (Value.IsFileType()) Text = Value.FileName;
    ValueText00->SetText(FText::FromString(Text));
    ValueBool1->SetIsChecked(Value.BoolValue);
    ValueNumber2->ClearMinValue(); ValueNumber2->ClearMaxValue();
    ValueNumber2->ClearMinSliderValue(); ValueNumber2->ClearMaxSliderValue();
    ValueNumber2->SetValue(Value.Type == EAtlasValueType::Integer ? Value.IntValue : Value.NumberValue);
    for (UEditableText* Label : {ValueImage3.Get(), ValueMesh4.Get(), ValueAudio5.Get()})
        Label->SetText(FText::FromString(Value.FileName));
    const bool bImportable = bOutput && Record.Outputs.Values.Contains(ParameterName);
    for (UButton* Button : {EUW_ImportImage.Get(), EUW_ImportMeshProject.Get(), EUW_ImportMeshScene.Get(), EUW_ImportAudio.Get()})
        Button->SetVisibility(bImportable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (Value.Type == EAtlasValueType::Image || Value.Type == EAtlasValueType::Mesh || Value.Type == EAtlasValueType::Audio)
    {
        if (auto* Manager = OutputManager(this))
        {
            if (bImportable)
            {
                const auto Imported = Manager->FindImportedJobOutputFromHistory(Record, ParameterName);
                if (Imported.bSuccess)
                {
                    if (Index == 3) PresentImage(Imported.ImportedAsset);
                    else if (Index == 4) PresentMesh(Imported.ImportedAsset);
                    else PresentAudio(Imported.ImportedAsset);
                }
            }
            if (Index == 3 && !Value.FilePath.IsEmpty())
                UAtlasUIHelpers::LoadPreviewIntoImage(ImagePReview, Value.FilePath, Manager);
        }
    }
    RefreshEditability();
    OnParameterPresentationChanged();
}
void UAtlasParameterRowWidget::RefreshEditability()
{
    if (!bReady) return;
    ValueText00->SetIsReadOnly(!bEditable);
    ValueNumber2->SetIsEnabled(bEditable); ValueBool1->SetIsEnabled(bEditable);
    for (UEditableText* Label : {ValueImage3.Get(), ValueMesh4.Get(), ValueAudio5.Get()}) Label->SetIsReadOnly(true);
    for (USinglePropertyView* Property : {ImageProperty.Get(), MeshProperty.Get(), AudioProperty.Get()}) Property->SetIsEnabled(false);
}
void UAtlasParameterRowWidget::PresentProperty(UObject* InObject, USinglePropertyView* InPropertyWidget, FName InPropertyName)
{
    if (!InPropertyWidget) return;
    InPropertyWidget->SetPropertyName(InPropertyName);
    InPropertyWidget->SetObject(this);
    InPropertyWidget->SetVisibility(InObject ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    InPropertyWidget->SetIsEnabled(false);
}
void UAtlasParameterRowWidget::PresentImage(UObject* InObject)
{
    image = Cast<UTexture2D>(InObject); PresentProperty(image, ImageProperty, GET_MEMBER_NAME_CHECKED(ThisClass, image));
}
void UAtlasParameterRowWidget::PresentMesh(UObject* InObject)
{
    mesh = Cast<UStaticMesh>(InObject); PresentProperty(mesh, MeshProperty, GET_MEMBER_NAME_CHECKED(ThisClass, mesh));
}
void UAtlasParameterRowWidget::PresentAudio(UObject* InObject)
{
    audio = Cast<USoundWave>(InObject); PresentProperty(audio, AudioProperty, GET_MEMBER_NAME_CHECKED(ThisClass, audio));
}
UObject* UAtlasParameterRowWidget::ImportAsset()
{
    if (!bOutput || !Record.Outputs.Values.Contains(ParameterName)) return nullptr;
    auto* Manager = OutputManager(this);
    if (!Manager) return nullptr;
    const FAtlasImportResult Result = Manager->ImportJobOutputFromHistory(Record, ParameterName);
    if (!Result.bSuccess || !Result.ImportedAsset)
    {
        UAtlasUIHelpers::ShowNotification(Result.ErrorMessage.IsEmpty() ? TEXT("Could not import this output.") : Result.ErrorMessage, false);
        return nullptr;
    }
    if (Value.Type == EAtlasValueType::Image) PresentImage(Result.ImportedAsset);
    else if (Value.Type == EAtlasValueType::Mesh) PresentMesh(Result.ImportedAsset);
    else if (Value.Type == EAtlasValueType::Audio) PresentAudio(Result.ImportedAsset);
    return Result.ImportedAsset;
}
void UAtlasParameterRowWidget::ImportToProject() { ImportAsset(); }
void UAtlasParameterRowWidget::ImportToScene()
{
    auto* ImportedMesh = Cast<UStaticMesh>(ImportAsset());
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!ImportedMesh || !World || !World->GetCurrentLevel()) return;
    const FScopedTransaction Transaction(NSLOCTEXT("Atlas", "PlaceOutputMesh", "Place Atlas output mesh"));
    // AddActor participates in the editor transaction; preserve the old graph's origin placement.
    auto* Actor = Cast<AStaticMeshActor>(GEditor->AddActor(World->GetCurrentLevel(), AStaticMeshActor::StaticClass(), FTransform::Identity, true, RF_Transactional));
    if (Actor)
    {
        Actor->GetStaticMeshComponent()->SetStaticMesh(ImportedMesh);
        Actor->SetActorLabel(ImportedMesh->GetName());
        Actor->MarkPackageDirty();
    }
}
void UAtlasParameterRowWidget::TextCommitted(const FText& InText, ETextCommit::Type)
{
    if (bRendering || !bEditable) return;
    if (Value.Type == EAtlasValueType::String) Value.StringValue = InText.ToString();
    else if (Value.Type == EAtlasValueType::Json) Value.JsonValue = InText.ToString();
    else if (Value.Type == EAtlasValueType::FileId) Value.FileId = InText.ToString();
}
void UAtlasParameterRowWidget::NumberCommitted(float InNumber, ETextCommit::Type)
{
    if (bRendering || !bEditable) return;
    if (Value.Type == EAtlasValueType::Integer) Value.IntValue = FMath::RoundToInt(InNumber);
    else if (Value.Type == EAtlasValueType::Number) Value.NumberValue = InNumber;
}
void UAtlasParameterRowWidget::BoolChanged(bool InChecked)
{
    if (!bRendering && bEditable && Value.Type == EAtlasValueType::Boolean) Value.BoolValue = InChecked;
}

void UAtlasCurrentWorkflowWidget::NativeConstruct()
{
    Super::NativeConstruct(); bReady = true;
    ButtonRunWorkflow->OnClicked.AddUniqueDynamic(this, &ThisClass::RunWorkflow);
    for (UAtlasInputFieldWidget* Field : Fields) Field->OnInputUpdated.AddUniqueDynamic(this, &ThisClass::RefreshInputs);
    RefreshWorkflow();
}
void UAtlasCurrentWorkflowWidget::NativeDestruct()
{
    // Designer previews and reinstanced objects can be torn down without bound controls.
    bReady = false;
    if (IsValid(ButtonRunWorkflow)) ButtonRunWorkflow->OnClicked.RemoveDynamic(this, &ThisClass::RunWorkflow);
    for (UAtlasInputFieldWidget* Field : Fields) if (IsValid(Field)) Field->OnInputUpdated.RemoveDynamic(this, &ThisClass::RefreshInputs);
    Super::NativeDestruct();
}
void UAtlasCurrentWorkflowWidget::SelectWorkflow(UAtlasWorkflowAsset* InWorkflow)
{
    CurrentWorkflow = InWorkflow; bNeedsRebuild = true;
    RefreshWorkflow();
}
void UAtlasCurrentWorkflowWidget::RefreshWorkflow()
{
    if (!bReady) return;
    WorkflowNameText->SetText(FText::FromString(CurrentWorkflow ? CurrentWorkflow->GetWorkflowName() : TEXT("Select a workflow")));
    RunWorkflowNameText->SetText(FText::FromString(CurrentWorkflow ? CurrentWorkflow->GetWorkflowName() : FString()));
    VersionText->SetText(FText::FromString(CurrentWorkflow ? CurrentWorkflow->Schema.Version : FString()));
    BaseUrlText->SetText(FText::FromString(CurrentWorkflow ? CurrentWorkflow->Schema.BaseUrl : FString()));
    if (bNeedsRebuild)
    {
        ClearFields();
        if (CurrentWorkflow)
        {
            RebuildInputs(CurrentWorkflow->Schema.Inputs);
            RebuildOutputs(CurrentWorkflow->Schema.Outputs);
        }
        bNeedsRebuild = false;
    }
    for (FName Name : {FName(TEXT("ExpandableArea-Inputs")), FName(TEXT("ExpandableArea-Outputs"))})
        if (auto* Area = Cast<UExpandableArea>(GetWidgetFromName(Name))) Area->SetIsExpanded(CurrentWorkflow != nullptr);
    RefreshInputs();
    OnWorkflowPresentationChanged();
}
void UAtlasCurrentWorkflowWidget::ClearInputFields()
{
    for (UAtlasInputFieldWidget* Field : Fields) Field->OnInputUpdated.RemoveDynamic(this, &ThisClass::RefreshInputs);
    Fields.Reset();
    if (VB_Inputs_Container) VB_Inputs_Container->ClearChildren();
    Inputs.Values.Reset(); bInputsValid = false;
}
void UAtlasCurrentWorkflowWidget::ClearFields()
{
    ClearInputFields();
    if (VB_Outputs_Container) VB_Outputs_Container->ClearChildren();
    if (ButtonRunWorkflow) ButtonRunWorkflow->SetIsEnabled(false);
}
void UAtlasCurrentWorkflowWidget::RebuildInputs(const TArray<FAtlasParameterDef>& Definitions)
{
    if (!bReady) { bNeedsRebuild = true; return; }
    {
        TGuardValue<bool> Guard(bBuilding, true);
        ClearInputFields();
        if (InputWidgetClass)
            for (const auto& Def : Definitions)
            {
                auto* Field = CreateWidget<UAtlasInputFieldWidget>(this, InputWidgetClass);
                if (!Field) continue;
                Field->currentDefParameter = Def;
                Field->defaultValue = Def.GetDefaultValueAsAtlasValue();
                VB_Inputs_Container->AddChildToVerticalBox(Field);
                Field->TakeWidget(); // Initialize controls even before the parent enters a visible panel.
                Fields.Add(Field);
                Field->OnInputUpdated.AddUniqueDynamic(this, &ThisClass::RefreshInputs);
            }
    }
    RefreshInputs();
}
void UAtlasCurrentWorkflowWidget::RebuildOutputs(const TArray<FAtlasParameterDef>& Definitions)
{
    if (!bReady) { bNeedsRebuild = true; return; }
    VB_Outputs_Container->ClearChildren();
    if (!OutputRowClass) return;
    for (const auto& Def : Definitions)
    {
        auto* Row = CreateWidget<UAtlasParameterRowWidget>(this, OutputRowClass);
        if (!Row) continue;
        FAtlasValue Placeholder; Placeholder.Type = Def.Type;
        Row->ConfigureParameter(false, Def.GetDisplayName(), Placeholder, false, FAtlasJobHistoryRecord());
        VB_Outputs_Container->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 3.f));
    }
}
void UAtlasCurrentWorkflowWidget::RefreshInputs()
{
    if (!bReady || bBuilding || bCollecting) return;
    TGuardValue<bool> Guard(bCollecting, true);
    Inputs.Values.Reset();
    for (UAtlasInputFieldWidget* Field : Fields)
    {
        FString Name; FAtlasValue Collected;
        Field->CollectValue(Name, Collected); Inputs.SetValue(Name, Collected);
    }
    TArray<FString> Errors;
    bInputsValid = CurrentWorkflow && CurrentWorkflow->ValidateInputs(Inputs, Errors);
    ButtonRunWorkflow->SetIsEnabled(bInputsValid);
    ButtonRunWorkflow->SetToolTipText(FText::FromString(CurrentWorkflow ? FString::Join(Errors, TEXT("\n")) : TEXT("Select a workflow")));
}
void UAtlasCurrentWorkflowWidget::RunWorkflow()
{
    RefreshInputs(); // Include edits that have not yet emitted a commit event.
    if (!bReady || !bInputsValid || !CurrentWorkflow) return;
    auto* Subsystem = UAtlasUIHelpers::GetEditorSubsystem(this);
    UAtlasJob* Job = Subsystem ? Subsystem->CreateJob(CurrentWorkflow, Inputs) : nullptr;
    if (Job) Job->Execute();
    else UAtlasUIHelpers::ShowNotification(TEXT("Could not create the workflow job."), false);
}


