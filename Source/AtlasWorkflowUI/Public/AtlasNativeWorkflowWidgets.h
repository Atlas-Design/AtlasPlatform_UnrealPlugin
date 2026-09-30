#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "Types/AtlasSchemaTypes.h"
#include "Types/AtlasHistoryTypes.h"
#include "AtlasNativeWorkflowWidgets.generated.h"

class UAtlasWorkflowAsset;
class UAtlasInputFieldWidget;
class UAtlasOutputManager;
class UButton;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;
class UEditableText;
class UCheckBox;
class USpinBox;
class UImage;
class USinglePropertyView;
class UTexture2D;
class UStaticMesh;
class USoundWave;

/** Native behavior for the existing Designer-owned parameter row. */
UCLASS(Abstract, Blueprintable)
class ATLASWORKFLOWUI_API UAtlasParameterRowWidget : public UEditorUtilityWidget
{
    GENERATED_BODY()
public:
    UAtlasParameterRowWidget();
    UPROPERTY(BlueprintReadOnly, Category="Atlas|Parameter") FAtlasValue Value;
    UPROPERTY(BlueprintReadOnly, Category="Atlas|Parameter") FString ParameterName;
    UPROPERTY(BlueprintReadOnly, Category="Atlas|Parameter") bool bEditable = false;
    UPROPERTY(BlueprintReadOnly, Category="Atlas|Parameter") bool bOutput = false;
    UPROPERTY(BlueprintReadOnly, Category="Atlas|Parameter") FAtlasJobHistoryRecord Record;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|Appearance") TMap<EAtlasValueType, FLinearColor> TypeColors;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|Parameter") TObjectPtr<UTexture2D> image;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|Parameter") TObjectPtr<UStaticMesh> mesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|Parameter") TObjectPtr<USoundWave> audio;
    UFUNCTION(BlueprintCallable, Category="Atlas|Parameter") void ConfigureParameter(bool InEditable, const FString& InName, const FAtlasValue& InValue, bool InOutput, const FAtlasJobHistoryRecord& InRecord);
    UFUNCTION(BlueprintCallable, Category="Atlas|Parameter") void RefreshEditability();
    UFUNCTION(BlueprintCallable, Category="Atlas|Parameter") void PresentImage(UObject* InObject);
    UFUNCTION(BlueprintCallable, Category="Atlas|Parameter") void PresentMesh(UObject* InObject);
    UFUNCTION(BlueprintCallable, Category="Atlas|Parameter") void PresentAudio(UObject* InObject);
    UFUNCTION(BlueprintCallable, Category="Atlas|Parameter") void PresentProperty(UObject* InObject, USinglePropertyView* InPropertyWidget, FName InPropertyName);
    UFUNCTION(BlueprintImplementableEvent, Category="Atlas|Appearance") void OnParameterPresentationChanged();
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_ParamName;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> StatusDot;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> ImagePReview;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidgetSwitcher> WidgetSwitcher_ParamType;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableText> ValueText00;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> ValueBool1;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<USpinBox> ValueNumber2;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableText> ValueImage3;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableText> ValueMesh4;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableText> ValueAudio5;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<USinglePropertyView> ImageProperty;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<USinglePropertyView> MeshProperty;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<USinglePropertyView> AudioProperty;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> EUW_ImportImage;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> EUW_ImportMeshProject;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> EUW_ImportMeshScene;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> EUW_ImportAudio;

private:
    bool bReady = false;
    bool bRendering = false;
    void RefreshParameter();
    UObject* ImportAsset();
    UFUNCTION() void ImportToProject();
    UFUNCTION() void ImportToScene();
    UFUNCTION() void TextCommitted(const FText& InText, ETextCommit::Type Method);
    UFUNCTION() void NumberCommitted(float InNumber, ETextCommit::Type Method);
    UFUNCTION() void BoolChanged(bool InChecked);
};

/** Workflow coordination; child classes and all layout remain editable in Blueprint. */
UCLASS(Abstract, Blueprintable)
class ATLASWORKFLOWUI_API UAtlasCurrentWorkflowWidget : public UEditorUtilityWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, Category="Atlas|Workflow") TObjectPtr<UAtlasWorkflowAsset> CurrentWorkflow;
    UPROPERTY(BlueprintReadOnly, Category="Atlas|Workflow") FAtlasWorkflowInputs Inputs;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atlas|Widgets") TSubclassOf<UAtlasInputFieldWidget> InputWidgetClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atlas|Widgets") TSubclassOf<UAtlasParameterRowWidget> OutputRowClass;
    UFUNCTION(BlueprintCallable, Category="Atlas|Workflow") void SelectWorkflow(UAtlasWorkflowAsset* InWorkflow);
    UFUNCTION(BlueprintCallable, Category="Atlas|Workflow") void RebuildInputs(const TArray<FAtlasParameterDef>& Definitions);
    UFUNCTION(BlueprintCallable, Category="Atlas|Workflow") void RebuildOutputs(const TArray<FAtlasParameterDef>& Definitions);
    UFUNCTION(BlueprintCallable, Category="Atlas|Workflow") void ClearFields();
    UFUNCTION(BlueprintCallable, Category="Atlas|Workflow") void RefreshInputs();
    UFUNCTION(BlueprintCallable, Category="Atlas|Workflow") void RunWorkflow();
    UFUNCTION(BlueprintImplementableEvent, Category="Atlas|Appearance") void OnWorkflowPresentationChanged();
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> WorkflowNameText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> VersionText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> BaseUrlText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> RunWorkflowNameText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> VB_Inputs_Container;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> VB_Outputs_Container;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ButtonRunWorkflow;

private:
    UPROPERTY(Transient) TArray<TObjectPtr<UAtlasInputFieldWidget>> Fields;
    bool bReady = false;
    bool bBuilding = false;
    bool bCollecting = false;
    bool bNeedsRebuild = true;
    bool bInputsValid = false;
    void RefreshWorkflow();
    void ClearInputFields();
};
