#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "Types/AtlasSchemaTypes.h"
#include "AtlasNativeInputWidgets.generated.h"

class UAtlasFilePanel;
class UEditableTextBox;
class UButton;
class UTextBlock;
class UWidgetSwitcher;
class USinglePropertyView;
class UTexture2D;
class UStaticMesh;
class UCheckBox;
class USpinBox;
class UMultiLineEditableText;
class UBorder;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAtlasPickerSelection, const FString&, filepath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAtlasInputUpdated);

/** Behavior for the designer-owned file picker. Existing Blueprint wrappers remain compatible. */
UCLASS(Abstract, Blueprintable)
class ATLASWORKFLOWUI_API UAtlasFilePickerWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|File", meta=(ExposeOnSpawn=true)) bool isTexture = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|File") TObjectPtr<UTexture2D> texture;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|File") TObjectPtr<UStaticMesh> mesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|File") FString FilePickerPath;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|File") int32 currentType = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|File") FString title;
	UPROPERTY(BlueprintAssignable, Category="Atlas|File") FAtlasPickerSelection OnFileSelected;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|Appearance") FText EmptyPathText = NSLOCTEXT("Atlas", "NoSelectedFile", "Select a file");
	UFUNCTION(BlueprintCallable, Category="Atlas|File") void ConfigurePicker(EAtlasValueType InType, const TArray<FString>& InAllowedExtensions);
	UFUNCTION(BlueprintCallable, Category="Atlas|File") void SelectFile(const FString& Path);
	UFUNCTION(BlueprintCallable, Category="Atlas|File") void SelectAssetId(const FString& InAssetId);
	UFUNCTION(BlueprintPure, Category="Atlas|File") FString GetSelectedAssetId() const;
	UFUNCTION(BlueprintCallable, Category="Atlas|File") void SetPickerMode(int32 Mode);
	UFUNCTION(BlueprintCallable, Category="Atlas|File") FString ResolveImagePath();
	UFUNCTION(BlueprintCallable, Category="Atlas|File") FString ResolveMeshPath();
	UFUNCTION(BlueprintImplementableEvent, Category="Atlas|Appearance") void OnPickerPresentationChanged();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> AssetIdInput;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> EditorUtilityButton_FromFile;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> EditorUtilityButton_FromProject;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> EditorUtilityButton_BrowseFile;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidgetSwitcher> WidgetSwitcher_Type;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> TextBlock_91;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<USinglePropertyView> SinglePropertyView_Texture;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<USinglePropertyView> SinglePropertyView_Mesh;
private:
	UPROPERTY(Transient) EAtlasValueType FileType = EAtlasValueType::None;
	UPROPERTY(Transient) TArray<FString> AllowedExtensions;
	TWeakObjectPtr<UObject> CachedAsset;
	FString CachedExport;
	FString SelectedAssetId;
	bool bUpdatingId = false;
	bool CanUseProject() const;
	UFUNCTION() void AssetIdChanged(const FText& InText);
	bool bReady = false;
	FDelegateHandle AssetModifiedHandle;
	void AssetModified(UObject* Object);
	void RefreshPicker();
	FString ResolveAssetPath(bool bImage);
	UFUNCTION() void Browse();
	UFUNCTION() void UseFile();
	UFUNCTION() void UseProject();
};

/** Scalar input behavior; Blueprint retains layout, colors and presentation hooks. */
UCLASS(Abstract, Blueprintable)
class ATLASWORKFLOWUI_API UAtlasInputFieldWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()
public:
	UAtlasInputFieldWidget();
    UPROPERTY(BlueprintReadOnly,Category="Atlas|Input") bool bUseServerDefault=false;
    UPROPERTY(BlueprintReadWrite,Category="Atlas|Input") FString ServiceOrigin;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UAtlasFilePanel> FileSelector;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UCheckBox> UseServerDefault;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> OptionalControls;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ValidationMessage;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> ExpandTextButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> BooleanValueLabel;
    bool ValidateSelection(FString& Error);
    bool PrepareValue(FAtlasValue& Value,FString& Error);
    void InvalidateSelection();
    int64 DefinitionRevision=0;

	// Names retained for existing CreateWidget pins and callers.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|Input", meta=(ExposeOnSpawn=true)) FAtlasParameterDef currentDefParameter;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|Input", meta=(ExposeOnSpawn=true)) FAtlasValue defaultValue;
	UPROPERTY(BlueprintAssignable, Category="Atlas|Input") FAtlasInputUpdated OnInputUpdated;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|Appearance") TMap<EAtlasValueType, FLinearColor> TypeColors;
	UFUNCTION(BlueprintCallable, Category="Atlas|Input") void ConfigureInput(const FAtlasParameterDef& Definition);
	UFUNCTION(BlueprintCallable, Category="Atlas|Input") void ApplyDefaultValue();
	UFUNCTION(BlueprintCallable, Category="Atlas|Input") void CollectValue(FString& OutName, FAtlasValue& OutValue);
	UFUNCTION(BlueprintImplementableEvent, Category="Atlas|Appearance") void OnInputPresentationChanged();
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidgetSwitcher> WidgetSwitcher_101;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> InputName;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UBorder> DisplayIcon;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> CheckBox_Bool;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<USpinBox> EditorUtilitySpinBox_260;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UMultiLineEditableText> EditableText_string;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UAtlasFilePickerWidget> EUW_FilePicker_Image;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UAtlasFilePickerWidget> EUW_FilePicker_Mesh;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> InputType_Bool;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> InputType_Number;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> InputType_String;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> InputType_Image;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> InputType_Mesh;
private:
	bool bReady = false;
	bool bUpdating = false;
	bool bAppliedInitialValue = false;
	void RefreshInput();
	UFUNCTION() void BoolChanged(bool bChecked);
	UFUNCTION() void NumberCommitted(float Number, ETextCommit::Type Method);
	UFUNCTION() void TextCommitted(const FText& Text, ETextCommit::Type Method);
	UFUNCTION() void FileChanged(const FString& Path);
    UFUNCTION() void DefaultChanged(bool Checked);
    UFUNCTION() void SelectionChanged();
    UFUNCTION() void ExpandText();
    void UpdatePresence();
	void NotifyChanged();
};
