#include "AtlasHistoryFilterWidget.h"
#include "AtlasUIHelpers.h"
#include "AtlasEditorSubsystem.h"
#include "AtlasJobManager.h"
#include "AtlasHistoryManager.h"

namespace
{
const EAtlasJobStatus Statuses[] = {EAtlasJobStatus::Success, EAtlasJobStatus::Failed, EAtlasJobStatus::Cancelled, EAtlasJobStatus::Running};
}
void UAtlasHistoryFilterWidget::NativeConstruct()
{
    Super::NativeConstruct();
    bReady = true;
    ComboFilter_Status->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::StatusChanged);
    ComboFilter_Type->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::WorkflowChanged);
    ComboFilter_Date->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::DateChanged);
    ComboFilter_Type->OnOpening.AddUniqueDynamic(this, &ThisClass::RefreshWorkflowOptions);
    InitializeFilters();
}
void UAtlasHistoryFilterWidget::NativeDestruct()
{
    bReady = false;
    // Designer previews/reinstancing can have incomplete native control bindings.
    if (IsValid(ComboFilter_Status)) ComboFilter_Status->OnSelectionChanged.RemoveDynamic(this, &ThisClass::StatusChanged);
    if (IsValid(ComboFilter_Type))
    {
        ComboFilter_Type->OnSelectionChanged.RemoveDynamic(this, &ThisClass::WorkflowChanged);
        ComboFilter_Type->OnOpening.RemoveDynamic(this, &ThisClass::RefreshWorkflowOptions);
    }
    if (IsValid(ComboFilter_Date)) ComboFilter_Date->OnSelectionChanged.RemoveDynamic(this, &ThisClass::DateChanged);
    Super::NativeDestruct();
}
void UAtlasHistoryFilterWidget::InitializeFilters()
{
    if (!bReady) return;
    {
        TGuardValue<bool> Guard(bUpdating, true);
        ComboFilter_Status->ClearOptions();
        for (const TCHAR* Label : {TEXT("All"), TEXT("Success"), TEXT("Failed"), TEXT("Cancelled"), TEXT("Running")}) ComboFilter_Status->AddOption(Label);
        int32 StatusIndex = 0;
        if (currentQuery.bFilterByStatus)
            for (int32 I = 0; I < UE_ARRAY_COUNT(Statuses); ++I)
                if (Statuses[I] == currentQuery.StatusFilter) StatusIndex = I + 1;
        ComboFilter_Status->SetSelectedIndex(StatusIndex);
        ComboFilter_Date->ClearOptions();
        for (const TCHAR* Label : {TEXT("All"), TEXT("Today"), TEXT("Yesterday"), TEXT("Last 7 Days"), TEXT("Last 30 Days"), TEXT("Older than 30 Days")}) ComboFilter_Date->AddOption(Label);
        ComboFilter_Date->SetSelectedIndex(DateSelection);
        RefreshWorkflowOptions();
    }
    BroadcastQuery();
}
void UAtlasHistoryFilterWidget::RefreshWorkflowOptions()
{
    if (!bReady) return;
    auto* Subsystem = UAtlasUIHelpers::GetEditorSubsystem(this);
    auto* Jobs = Subsystem ? Subsystem->GetJobManager() : nullptr;
    auto* History = Jobs ? Jobs->GetHistoryManager() : nullptr;
    if (History) SetWorkflowOptions(History->GetWorkflowInfoWithHistory());
    else SetWorkflowOptions({});
}
void UAtlasHistoryFilterWidget::SetWorkflowOptions(const TArray<FAtlasWorkflowInfo>& Workflows)
{
    if (!bReady) return;
    TGuardValue<bool> Guard(bUpdating, true);
    TArray<FAtlasWorkflowInfo> Sorted = Workflows;
    Sorted.Sort([](const FAtlasWorkflowInfo& A, const FAtlasWorkflowInfo& B)
    {
        const int32 Comparison = A.DisplayName.Compare(B.DisplayName, ESearchCase::IgnoreCase);
        return Comparison == 0 ? A.ApiId < B.ApiId : Comparison < 0;
    });
    TMap<FString,int32> NameCounts;
    for (const auto& Info : Sorted) ++NameCounts.FindOrAdd((Info.DisplayName.IsEmpty() ? Info.ApiId : Info.DisplayName).ToLower());
    ComboFilter_Type->ClearOptions(); WorkflowIds.Reset();
    ComboFilter_Type->AddOption(TEXT("All")); WorkflowIds.Add(FString());
    TSet<FString> Labels = {TEXT("All")};
    for (const auto& Info : Sorted)
    {
        if (Info.ApiId.IsEmpty() || WorkflowIds.Contains(Info.ApiId)) continue;
        FString Label = Info.DisplayName.IsEmpty() ? Info.ApiId : Info.DisplayName;
        if (NameCounts.FindRef(Label.ToLower()) > 1 || Label.Equals(TEXT("All"), ESearchCase::IgnoreCase)) Label += TEXT(" [") + Info.ApiId + TEXT("]");
        while (Labels.Contains(Label)) Label += TEXT(" [") + Info.ApiId + TEXT("]");
        Labels.Add(Label); WorkflowIds.Add(Info.ApiId); ComboFilter_Type->AddOption(Label);
    }
    int32 Selected = WorkflowIds.IndexOfByKey(currentQuery.ApiId);
    // If the selected workflow was removed, retain an explicit selection rather than silently broadening the query.
    if (Selected == INDEX_NONE)
    {
        Selected = WorkflowIds.Add(currentQuery.ApiId);
        ComboFilter_Type->AddOption(TEXT("Unavailable [") + currentQuery.ApiId + TEXT("]"));
    }
    ComboFilter_Type->SetSelectedIndex(Selected);
}
void UAtlasHistoryFilterWidget::StatusChanged(FString, ESelectInfo::Type)
{
    if (!bReady || bUpdating) return;
    const int32 Index = ComboFilter_Status->GetSelectedIndex();
    currentQuery.bFilterByStatus = Index > 0 && Index <= UE_ARRAY_COUNT(Statuses);
    if (currentQuery.bFilterByStatus) currentQuery.StatusFilter = Statuses[Index - 1];
    BroadcastQuery();
}
void UAtlasHistoryFilterWidget::WorkflowChanged(FString, ESelectInfo::Type)
{
    if (!bReady || bUpdating) return;
    const int32 Index = ComboFilter_Type->GetSelectedIndex();
    if (!WorkflowIds.IsValidIndex(Index)) return;
    currentQuery.ApiId = WorkflowIds[Index];
    BroadcastQuery();
}
void UAtlasHistoryFilterWidget::DateChanged(FString, ESelectInfo::Type)
{
    if (!bReady || bUpdating) return;
    DateSelection = FMath::Max(0, ComboFilter_Date->GetSelectedIndex());
    BroadcastQuery();
}
void UAtlasHistoryFilterWidget::UpdateDateRange()
{
    currentQuery.bFilterByDate = DateSelection != 0;
    const FDateTime Today = FDateTime::Today();
    const FTimespan Tick(1);
    currentQuery.DateFrom = FDateTime::MinValue();
    currentQuery.DateTo = FDateTime::MaxValue();
    // Query endpoints are inclusive. Exclude the following midnight by one tick.
    if (DateSelection >= 1 && DateSelection <= 4) currentQuery.DateTo = Today + FTimespan::FromDays(1) - Tick;
    switch (DateSelection)
    {
    case 1: currentQuery.DateFrom = Today; break;
    case 2: currentQuery.DateFrom = Today - FTimespan::FromDays(1); currentQuery.DateTo = Today - Tick; break;
    case 3: currentQuery.DateFrom = Today - FTimespan::FromDays(6); break;
    case 4: currentQuery.DateFrom = Today - FTimespan::FromDays(29); break;
    case 5: currentQuery.DateTo = Today - FTimespan::FromDays(29) - Tick; break;
    default: currentQuery.bFilterByDate = false; break;
    }
}
void UAtlasHistoryFilterWidget::BroadcastQuery()
{
    if (!bReady || bUpdating) return;
    UpdateDateRange(); currentQuery.Offset = 0;
    OnRefreshHistoryList.Broadcast(currentQuery);
    OnFilterPresentationChanged();
}
