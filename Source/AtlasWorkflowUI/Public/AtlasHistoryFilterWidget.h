#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "Types/AtlasHistoryTypes.h"
#include "Components/ComboBoxString.h"
#include "AtlasHistoryFilterWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAtlasHistoryFilterChanged, FAtlasHistoryQuery, query);

/** Behavior for the existing Designer-owned history filter bar. */
UCLASS(Abstract, Blueprintable)
class ATLASWORKFLOWUI_API UAtlasHistoryFilterWidget : public UEditorUtilityWidget
{
    GENERATED_BODY()
public:
    // Names/signature retained for the existing JobLister Blueprint.
    UPROPERTY(BlueprintReadOnly, Category="Atlas|History") FAtlasHistoryQuery currentQuery;
    UPROPERTY(BlueprintAssignable, Category="Atlas|History") FAtlasHistoryFilterChanged OnRefreshHistoryList;
    UFUNCTION(BlueprintCallable, Category="Atlas|History") void InitializeFilters();
    UFUNCTION(BlueprintCallable, Category="Atlas|History") void BroadcastQuery();
    UFUNCTION(BlueprintCallable, Category="Atlas|History") void RefreshWorkflowOptions();
    UFUNCTION(BlueprintImplementableEvent, Category="Atlas|Appearance") void OnFilterPresentationChanged();
    // Separates option presentation from history discovery; retains selection by API ID.
    void SetWorkflowOptions(const TArray<FAtlasWorkflowInfo>& Workflows);
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> ComboFilter_Status;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> ComboFilter_Type;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> ComboFilter_Date;
private:
    bool bReady = false;
    bool bUpdating = false;
    int32 DateSelection = 0;
    TArray<FString> WorkflowIds;
    UFUNCTION() void StatusChanged(FString Selected, ESelectInfo::Type SelectionType);
    UFUNCTION() void WorkflowChanged(FString Selected, ESelectInfo::Type SelectionType);
    UFUNCTION() void DateChanged(FString Selected, ESelectInfo::Type SelectionType);
    void UpdateDateRange();
};
