#include "AtlasNativeInputWidgets.h"
#include "AtlasUIHelpers.h"
#include "AtlasSDKSettings.h"
#include "Utilities/AtlasAssetExporter.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/SinglePropertyView.h"
#include "Components/CheckBox.h"
#include "Components/SpinBox.h"
#include "Components/MultiLineEditableText.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Exporters/GLTFExporter.h"
#include "Options/GLTFExportOptions.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"

void UAtlasFilePickerWidget::NativeConstruct()
{
    Super::NativeConstruct();
    bReady = true;
    EditorUtilityButton_FromFile->OnClicked.AddUniqueDynamic(this, &ThisClass::UseFile);
    EditorUtilityButton_FromProject->OnClicked.AddUniqueDynamic(this, &ThisClass::UseProject);
    EditorUtilityButton_BrowseFile->OnClicked.AddUniqueDynamic(this, &ThisClass::Browse);
    if (!AssetModifiedHandle.IsValid()) AssetModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddUObject(this, &ThisClass::AssetModified);
    if (FileType == EAtlasValueType::None) FileType = isTexture ? EAtlasValueType::Image : EAtlasValueType::Mesh;
    SinglePropertyView_Texture->SetPropertyName(GET_MEMBER_NAME_CHECKED(ThisClass, texture));
    SinglePropertyView_Texture->SetObject(this);
    SinglePropertyView_Mesh->SetPropertyName(GET_MEMBER_NAME_CHECKED(ThisClass, mesh));
    SinglePropertyView_Mesh->SetObject(this);
    RefreshPicker();
}

void UAtlasFilePickerWidget::NativeDestruct()
{
    // Designer previews and reinstanced objects can be torn down without bound controls.
    bReady = false;
    if (IsValid(EditorUtilityButton_FromFile)) EditorUtilityButton_FromFile->OnClicked.RemoveDynamic(this, &ThisClass::UseFile);
    if (IsValid(EditorUtilityButton_FromProject)) EditorUtilityButton_FromProject->OnClicked.RemoveDynamic(this, &ThisClass::UseProject);
    if (IsValid(EditorUtilityButton_BrowseFile)) EditorUtilityButton_BrowseFile->OnClicked.RemoveDynamic(this, &ThisClass::Browse);
    FCoreUObjectDelegates::OnObjectModified.Remove(AssetModifiedHandle);
    AssetModifiedHandle.Reset();
    if (IsValid(SinglePropertyView_Texture)) SinglePropertyView_Texture->SetObject(nullptr);
    if (IsValid(SinglePropertyView_Mesh)) SinglePropertyView_Mesh->SetObject(nullptr);
    Super::NativeDestruct();
}

void UAtlasFilePickerWidget::ConfigurePicker(EAtlasValueType InType, const TArray<FString>& InAllowedExtensions)
{
    FileType = InType;
    isTexture = InType == EAtlasValueType::Image;
    AllowedExtensions = InAllowedExtensions;
    if (InType != EAtlasValueType::Image && InType != EAtlasValueType::Mesh) currentType = 0;
    RefreshPicker();
}

void UAtlasFilePickerWidget::RefreshPicker()
{
    if (!bReady) return;
    const bool bProject = FileType == EAtlasValueType::Image || FileType == EAtlasValueType::Mesh;
    EditorUtilityButton_FromProject->SetVisibility(bProject ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    SinglePropertyView_Texture->SetVisibility(isTexture ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    SinglePropertyView_Mesh->SetVisibility(!isTexture && bProject ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    WidgetSwitcher_Type->SetActiveWidgetIndex(currentType == 1 && bProject ? 1 : 0);
    TextBlock_91->SetText(FilePickerPath.IsEmpty() ? EmptyPathText : FText::FromString(FPaths::GetCleanFilename(FilePickerPath)));
    TextBlock_91->SetToolTipText(FText::FromString(FilePickerPath));
    OnPickerPresentationChanged();
}

void UAtlasFilePickerWidget::SelectFile(const FString& Path)
{
    if (!Path.IsEmpty())
    {
        FString Error;
        const EAtlasValueType EffectiveType = FileType == EAtlasValueType::None ? (isTexture ? EAtlasValueType::Image : EAtlasValueType::Mesh) : FileType;
        if (!UAtlasUIHelpers::ValidateFilePath(Path, EffectiveType, AllowedExtensions, Error))
        { UAtlasUIHelpers::ShowNotification(Error, false); return; }
    }
    FilePickerPath = Path;
    currentType = 0;
    RefreshPicker();
    OnFileSelected.Broadcast(FilePickerPath);
}

void UAtlasFilePickerWidget::Browse()
{
    FString Path;
    if (UAtlasUIHelpers::OpenFileDialogForType(FileType, AllowedExtensions, Path)) SelectFile(Path);
}
void UAtlasFilePickerWidget::UseFile() { SetPickerMode(0); }
void UAtlasFilePickerWidget::UseProject() { SetPickerMode(1); }
void UAtlasFilePickerWidget::SetPickerMode(int32 Mode)
{
    const bool bProject = FileType == EAtlasValueType::Image || FileType == EAtlasValueType::Mesh;
    currentType = Mode == 1 && bProject ? 1 : 0;
    RefreshPicker();
    OnFileSelected.Broadcast(currentType == 0 ? FilePickerPath : ResolveAssetPath(isTexture));
}

void UAtlasFilePickerWidget::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    const FName PropertyName = Event.GetPropertyName();
    const bool bSelectionChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, texture)
        || PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, mesh);
    // Details widgets notify the edited UObject. Retain state before the base can rebuild the view.
    if (bSelectionChanged) { CachedAsset.Reset(); CachedExport.Reset(); }
    Super::PostEditChangeProperty(Event);
    if (bReady && bSelectionChanged && currentType == 1)
        OnFileSelected.Broadcast(ResolveAssetPath(isTexture));
}

void UAtlasFilePickerWidget::AssetModified(UObject* Object)
{
    // Material/texture edits can affect a mesh export too. Invalidate on persistent asset edits,
    // not transient widget changes, so repeated validation can reuse an unchanged export.
    if (Object && Object->GetOutermost() != GetTransientPackage()
        && !Object->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
    { CachedAsset.Reset(); CachedExport.Reset(); }
}

FString UAtlasFilePickerWidget::ResolveAssetPath(bool bImage)
{
    if (currentType == 0) return FilePickerPath;
    UObject* Asset = bImage ? static_cast<UObject*>(texture.Get()) : static_cast<UObject*>(mesh.Get());
    if (!IsValid(Asset)) return FString();
    if (CachedAsset.Get() == Asset && FPaths::FileExists(CachedExport)) return CachedExport;
    CachedAsset.Reset(); CachedExport.Reset();
    if (bImage)
    {
        // Export current texture source art, not an unrelated original import path.
        const FAtlasExportResult Result = UAtlasAssetExporter::ExportTextureToPNG(texture);
        if (Result.bSuccess) CachedExport = Result.FilePath;
        else UAtlasUIHelpers::ShowNotification(Result.ErrorMessage, false);
    }
    else
    {
        // Preserve the existing Blueprint picker's self-contained GLB export contract.
        const FString Folder = UAtlasSDKSettings::Get()->GetTempExportFolderPath();
        const FString Path = Folder / (TEXT("Mesh_") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".glb"));
        TStrongObjectPtr<UGLTFExportOptions> Options(NewObject<UGLTFExportOptions>());
        if (IFileManager::Get().MakeDirectory(*Folder, true) && UGLTFExporter::ExportToGLTF(mesh, Path, Options.Get()) && FPaths::FileExists(Path))
            CachedExport = Path;
        else UAtlasUIHelpers::ShowNotification(TEXT("Could not export the selected mesh to GLB"), false);
    }
    if (!CachedExport.IsEmpty()) CachedAsset = Asset;
    return CachedExport;
}
FString UAtlasFilePickerWidget::ResolveImagePath() { return ResolveAssetPath(true); }
FString UAtlasFilePickerWidget::ResolveMeshPath() { return ResolveAssetPath(false); }

UAtlasInputFieldWidget::UAtlasInputFieldWidget()
{
    TypeColors.Add(EAtlasValueType::String, FLinearColor(.003922f, .231373f, 1.f));
    TypeColors.Add(EAtlasValueType::Number, FLinearColor(1.f, .345098f, .003922f));
    TypeColors.Add(EAtlasValueType::Integer, TypeColors[EAtlasValueType::Number]);
    TypeColors.Add(EAtlasValueType::Boolean, FLinearColor(1.f, .039216f, .113725f));
    TypeColors.Add(EAtlasValueType::Image, FLinearColor(1.f, .670588f, .003922f));
    TypeColors.Add(EAtlasValueType::Mesh, FLinearColor(.521569f, .101961f, .886275f));
    TypeColors.Add(EAtlasValueType::Audio, FLinearColor(.949020f, .058824f, .537255f));
}
void UAtlasInputFieldWidget::NativeConstruct()
{
    Super::NativeConstruct();
    bReady = true;
    CheckBox_Bool->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::BoolChanged);
    EditorUtilitySpinBox_260->OnValueCommitted.AddUniqueDynamic(this, &ThisClass::NumberCommitted);
    EditableText_string->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::TextCommitted);
    EUW_FilePicker_Image->OnFileSelected.AddUniqueDynamic(this, &ThisClass::FileChanged);
    EUW_FilePicker_Mesh->OnFileSelected.AddUniqueDynamic(this, &ThisClass::FileChanged);
    RefreshInput();
    if (!bAppliedInitialValue) { ApplyDefaultValue(); bAppliedInitialValue = true; }
}
void UAtlasInputFieldWidget::NativeDestruct()
{
    // Designer previews and reinstanced objects can be torn down without bound controls.
    bReady = false;
    if (IsValid(CheckBox_Bool)) CheckBox_Bool->OnCheckStateChanged.RemoveDynamic(this, &ThisClass::BoolChanged);
    if (IsValid(EditorUtilitySpinBox_260)) EditorUtilitySpinBox_260->OnValueCommitted.RemoveDynamic(this, &ThisClass::NumberCommitted);
    if (IsValid(EditableText_string)) EditableText_string->OnTextCommitted.RemoveDynamic(this, &ThisClass::TextCommitted);
    if (IsValid(EUW_FilePicker_Image)) EUW_FilePicker_Image->OnFileSelected.RemoveDynamic(this, &ThisClass::FileChanged);
    if (IsValid(EUW_FilePicker_Mesh)) EUW_FilePicker_Mesh->OnFileSelected.RemoveDynamic(this, &ThisClass::FileChanged);
    Super::NativeDestruct();
}
void UAtlasInputFieldWidget::ConfigureInput(const FAtlasParameterDef& Definition)
{
    currentDefParameter = Definition;
    RefreshInput();
}
void UAtlasInputFieldWidget::RefreshInput()
{
    if (!bReady) return;
    TGuardValue<bool> Guard(bUpdating, true);
    const EAtlasValueType Type = currentDefParameter.Type;
    UWidget* Active = InputType_String;
    switch (Type)
    {
    case EAtlasValueType::Boolean: Active = InputType_Bool; break;
    case EAtlasValueType::Integer:
    case EAtlasValueType::Number: Active = InputType_Number; break;
    case EAtlasValueType::Mesh: Active = InputType_Mesh; break;
    case EAtlasValueType::Image:
    case EAtlasValueType::Audio:
    case EAtlasValueType::File: Active = InputType_Image; break;
    default: break;
    }
    WidgetSwitcher_101->SetActiveWidget(Active);
    InputName->SetText(FText::FromString(currentDefParameter.GetDisplayName()));
    SetToolTipText(FText::FromString(currentDefParameter.Description));
    if (const FLinearColor* Color = TypeColors.Find(Type)) DisplayIcon->SetBrushColor(*Color);
    EUW_FilePicker_Image->ConfigurePicker(Type == EAtlasValueType::Image ? EAtlasValueType::Image : Type, currentDefParameter.AllowedExtensions);
    EUW_FilePicker_Mesh->ConfigurePicker(EAtlasValueType::Mesh, currentDefParameter.AllowedExtensions);
    // Clear previous constraints first: this widget can be configured for another parameter.
    EditorUtilitySpinBox_260->ClearMinValue(); EditorUtilitySpinBox_260->ClearMaxValue();
    EditorUtilitySpinBox_260->ClearMinSliderValue(); EditorUtilitySpinBox_260->ClearMaxSliderValue();
    const bool bLegacyRange = currentDefParameter.bHasRange && !currentDefParameter.bHasMinimum && !currentDefParameter.bHasMaximum;
    if (currentDefParameter.bHasMinimum || bLegacyRange) EditorUtilitySpinBox_260->SetMinValue(currentDefParameter.MinValue);
    if (currentDefParameter.bHasMaximum || bLegacyRange) EditorUtilitySpinBox_260->SetMaxValue(currentDefParameter.MaxValue);
    OnInputPresentationChanged();
}
void UAtlasInputFieldWidget::ApplyDefaultValue()
{
    if (!bReady) return;
    TGuardValue<bool> Guard(bUpdating, true);
    const FAtlasValue Value = defaultValue.Type == EAtlasValueType::None ? currentDefParameter.GetDefaultValueAsAtlasValue() : defaultValue;
    switch (Value.Type)
    {
    case EAtlasValueType::String: EditableText_string->SetText(FText::FromString(Value.StringValue)); break;
    case EAtlasValueType::Json: EditableText_string->SetText(FText::FromString(Value.JsonValue)); break;
    case EAtlasValueType::FileId: EditableText_string->SetText(FText::FromString(Value.FileId)); break;
    case EAtlasValueType::Number: EditorUtilitySpinBox_260->SetValue(Value.NumberValue); break;
    case EAtlasValueType::Integer: EditorUtilitySpinBox_260->SetValue(Value.IntValue); break;
    case EAtlasValueType::Boolean: CheckBox_Bool->SetIsChecked(Value.BoolValue); break;
    default: break;
    }
}
void UAtlasInputFieldWidget::CollectValue(FString& OutName, FAtlasValue& OutValue)
{
    OutName = currentDefParameter.Name; OutValue = FAtlasValue();
    if (!bReady) return;
    switch (currentDefParameter.Type)
    {
    case EAtlasValueType::String: OutValue = FAtlasValue::MakeString(EditableText_string->GetText().ToString()); break;
    case EAtlasValueType::Json: OutValue = FAtlasValue::MakeJson(EditableText_string->GetText().ToString()); break;
    case EAtlasValueType::FileId: OutValue = FAtlasValue::MakeFileId(EditableText_string->GetText().ToString()); break;
    case EAtlasValueType::Boolean: OutValue = FAtlasValue::MakeBool(CheckBox_Bool->IsChecked()); break;
    case EAtlasValueType::Number: OutValue = FAtlasValue::MakeNumber(EditorUtilitySpinBox_260->GetValue()); break;
    case EAtlasValueType::Integer:
    {
        const double Number = EditorUtilitySpinBox_260->GetValue();
        if (FMath::IsFinite(Number) && Number >= MIN_int32 && Number <= MAX_int32 && FMath::FloorToDouble(Number) == Number)
            OutValue = FAtlasValue::MakeInteger(static_cast<int32>(Number));
        break;
    }
    case EAtlasValueType::Image:
    case EAtlasValueType::Audio:
    case EAtlasValueType::File:
    case EAtlasValueType::Mesh:
    {
        const FString Path = currentDefParameter.Type == EAtlasValueType::Mesh ? EUW_FilePicker_Mesh->ResolveMeshPath() : EUW_FilePicker_Image->ResolveImagePath();
        if (!Path.IsEmpty()) OutValue = UAtlasUIHelpers::MakeFileValue(Path, currentDefParameter.Type);
        break;
    }
    default: break;
    }
}
void UAtlasInputFieldWidget::NotifyChanged() { if (bReady && !bUpdating) OnInputUpdated.Broadcast(); }
void UAtlasInputFieldWidget::BoolChanged(bool) { NotifyChanged(); }
void UAtlasInputFieldWidget::NumberCommitted(float, ETextCommit::Type) { NotifyChanged(); }
void UAtlasInputFieldWidget::TextCommitted(const FText&, ETextCommit::Type) { NotifyChanged(); }
void UAtlasInputFieldWidget::FileChanged(const FString&) { NotifyChanged(); }
