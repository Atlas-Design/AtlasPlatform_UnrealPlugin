#pragma once
#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "Components/ContentWidget.h"
#include "AtlasFileSelection.h"
#include "Types/AtlasHistoryTypes.h"
#include "AtlasFileWidgets.generated.h"
class UAtlasOutputManager; class UAtlasFilePanel; class UButton; class UTextBlock; class UImage; class UVerticalBox; class UTexture2D;
class SAtlasFileDropTarget;
UENUM(BlueprintType)
enum class EAtlasFilePresentation : uint8 { Editable, RecordedInput, Output };

/** Native Slate event bridge; the child content remains a UMG Designer hierarchy. */
UCLASS()
class ATLASWORKFLOWUI_API UAtlasFileDropSurface : public UContentWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Atlas") bool bReorderHandle=false;
    TWeakObjectPtr<UAtlasFilePanel> OwnerPanel;
    FGuid TargetId;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void OnSlotAdded(UPanelSlot* Slot) override;
    virtual void OnSlotRemoved(UPanelSlot* Slot) override;
private:
    TSharedPtr<SAtlasFileDropTarget> Surface;
};

UCLASS(Abstract,Blueprintable)
class ATLASWORKFLOWUI_API UAtlasFileCard : public UEditorUtilityWidget
{
    GENERATED_BODY()
public:
    void Configure(UAtlasFilePanel* Panel,const FAtlasFileChoice& InChoice,int32 Index);
    void Refresh();
    void FocusHandle();
    UPROPERTY(BlueprintReadOnly,Category="Atlas") FAtlasFileChoice Choice;
    UPROPERTY(BlueprintReadOnly,Category="Atlas") int32 ItemIndex=INDEX_NONE;
protected:
    virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool Enabled) const override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UAtlasFileDropSurface> DropTarget;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UAtlasFileDropSurface> ReorderHandle;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Filename;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Metadata;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> SourceStatus;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> TypeIcon;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> Thumbnail;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> PreviewButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> PrimaryButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> PrimaryLabel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> RemoveButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> RemoveLabel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> MoreButton;
private:
    TWeakObjectPtr<UAtlasFilePanel> Owner;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> PreviewTexture;
    int64 PreviewRevision=0;
    bool bPreviewRequested=false;
    void RequestPreview();
    bool bReady=false;
    UFUNCTION() void Primary();
    UFUNCTION() void Remove();
    UFUNCTION() void More();
    UFUNCTION() void Preview();
};

UCLASS(Abstract,Blueprintable)
class ATLASWORKFLOWUI_API UAtlasFilePanel : public UEditorUtilityWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Atlas|Appearance") TSubclassOf<UAtlasFileCard> CardClass;
    UPROPERTY(BlueprintReadOnly,Category="Atlas") TObjectPtr<UAtlasFileSelection> Selection;
    UPROPERTY(BlueprintReadOnly,Category="Atlas") EAtlasFilePresentation Mode=EAtlasFilePresentation::Editable;
    UPROPERTY(BlueprintAssignable,Category="Atlas") FAtlasSelectionChanged OnSelectionChanged;
    UPROPERTY(Transient) TObjectPtr<UAtlasOutputManager> OutputManager;
    UPROPERTY() FAtlasJobHistoryRecord Record;
    UPROPERTY() FString ParameterName;
    void Configure(const FAtlasParameterDef& Def,const FAtlasValue& Value,EAtlasFilePresentation InMode,const FAtlasJobHistoryRecord& InRecord=FAtlasJobHistoryRecord(),const FString& Origin=FString());
    void SetMutationEnabled(bool Enabled);
    void Invalidate();
    bool IsEditable() const;
    bool Drop(const TArray<FString>& Paths,const TArray<UObject*>& Assets,const FGuid& Target,bool Commit,FString& Hint);
    bool Reorder(const FGuid& Id,const FGuid& Target,bool Commit,FString& Hint);
    void Move(const FGuid& Id,int32 Delta);
    void OpenSource(const FGuid& Id=FGuid());
    void OpenMenu(const FGuid& Id);
    void Preview(const FGuid& Id);
    void Import(const FGuid& Id,bool AddToLevel=false);
    bool CanImport(const FAtlasFileChoice& Choice) const;
    UObject* FindImported(int32 Index) const;
    bool HasImported(int32 Index) const;
    FString LocalPath(const FAtlasFileChoice& Choice,int32 Index) const;
    bool IsImporting(const FGuid& Id) const { return Importing.Contains(Id); }
    FString ImportError(const FGuid& Id) const { return ImportErrors.FindRef(Id); }
    void ShowError(const FString& Error);
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> Cards;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> Header;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> CollapseButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> HeaderLabel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ImportAllButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UAtlasFileDropSurface> AppendTarget;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> EmptyHint;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> BrowseButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> BrowseLabel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> OtherSourcesButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> InlineError;
private:
    bool bReady=false,bCollapsed=false,bOmitted=false,bConfigured=false;
    TSet<FGuid> Importing;
    TMap<FGuid,FString> ImportErrors;
    FGuid FocusAfterRebuild;
    void RebuildCards();
    UFUNCTION() void Changed();
    UFUNCTION() void Browse();
    UFUNCTION() void OtherSources();
    UFUNCTION() void ToggleCollapsed();
    UFUNCTION() void ImportAll();
};
