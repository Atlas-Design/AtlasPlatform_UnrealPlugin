// Copyright Atlas Platform. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "AtlasHistoryManager.h"
#include "AtlasSDKSettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "UObject/StrongObjectPtr.h"

struct FAtlasHistoryIndexTestAccess
{
	static void SetDirectory(UAtlasHistoryManager* Manager, const FString& Directory) { Manager->TestHistoryDirectory = Directory; }
	static bool WriteLegacy(UAtlasHistoryManager* Manager, const TArray<FAtlasJobHistoryRecord>& Records)
	{
		Manager->EnsureHistoryDirectoryExists();
		return Manager->SaveHistoryFile(Manager->GetHistoryFilePath(TEXT("test")), Records);
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasHistoryIndexTest, "Atlas.History.Index",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAtlasHistoryIndexTest::RunTest(const FString& Parameters)
{
	UAtlasSDKSettings* Settings = GetMutableDefault<UAtlasSDKSettings>();
	const FString OriginalOutput = Settings->OutputFolder.Path;
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/HistoryIndex") / FGuid::NewGuid().ToString(EGuidFormats::Digits));
	Settings->OutputFolder.Path = Root / TEXT("Output");
	ON_SCOPE_EXIT { Settings->OutputFolder.Path = OriginalOutput; };
	TStrongObjectPtr<UAtlasHistoryManager> First(NewObject<UAtlasHistoryManager>());
	TStrongObjectPtr<UAtlasHistoryManager> Second(NewObject<UAtlasHistoryManager>());
	for (auto* Manager : {First.Get(), Second.Get()}) FAtlasHistoryIndexTestAccess::SetDirectory(Manager, Root / TEXT("History"));
	FAtlasJobHistoryRecord Old;
	Old.JobId = FGuid::NewGuid(); Old.ApiId = TEXT("test"); Old.WorkflowName = TEXT("Old label");
	Old.StartedAt = Old.CompletedAt = FDateTime(2026, 1, 1);
	FAtlasJobHistoryRecord Newer = Old;
	Newer.JobId = FGuid::NewGuid(); Newer.CompletedAt += FTimespan::FromDays(1); Newer.WorkflowName = TEXT("Newest label");
	Newer.Status = EAtlasJobStatus::Failed;
	TestTrue(TEXT("Write legacy fixture"), FAtlasHistoryIndexTestAccess::WriteLegacy(First.Get(), {Old, Newer}));
	FAtlasHistoryQuery Query; Query.Limit = 1;
	auto Page = First->QueryHistory(Query);
	TestEqual(TEXT("First page size"), Page.Num(), 1);
	if (Page.Num() != 1) return false;
	TestEqual(TEXT("Newest first"), Page[0].JobId, Newer.JobId);
	TestEqual(TEXT("Count ignores page size"), First->GetHistoryCount(Query), 2);
	Query.Offset = 1;
	TestEqual(TEXT("Offset picks older record"), First->QueryHistory(Query)[0].JobId, Old.JobId);
	Query.Offset = 0; Query.bNewestFirst = false;
	TestEqual(TEXT("Ascending order"), First->QueryHistory(Query)[0].JobId, Old.JobId);
	Query.bFilterByStatus = true; Query.StatusFilter = EAtlasJobStatus::Failed;
	TestEqual(TEXT("Status filter"), First->GetHistoryCount(Query), 1);
	Query.bFilterByStatus = false; Query.bFilterByDate = true; Query.DateFrom = Newer.CompletedAt; Query.DateTo = Newer.CompletedAt;
	TestEqual(TEXT("Inclusive date filter"), First->GetHistoryCount(Query), 1);
	Query.bFilterByDate = false; Query.ApiId = TEXT("missing");
	TestEqual(TEXT("Workflow filter"), First->GetHistoryCount(Query), 0);
	Query.ApiId.Empty();
	TestEqual(TEXT("Second manager initial count"), Second->GetHistoryCount(Query), 2);
	Newer.WorkflowName = TEXT("Archive wins");
	TestTrue(TEXT("Write per-job override"), First->SaveRecordToJobJson(Newer));
	TestEqual(TEXT("Second manager observes archive override"), Second->GetWorkflowInfoWithHistory()[0].DisplayName, Newer.WorkflowName);
	TestEqual(TEXT("Duplicate GUID is not duplicated"), Second->GetHistoryCount(Query), 2);
	FAtlasJobHistoryRecord Added = Old; Added.JobId = FGuid::NewGuid(); Added.CompletedAt += FTimespan::FromDays(3);
	TestTrue(TEXT("Save new record"), First->SaveRecord(Added));
	TestEqual(TEXT("Other managers see insert"), Second->GetHistoryCount(Query), 3);
	FAtlasJobHistoryRecord Updated = Added; Updated.Status = EAtlasJobStatus::Cancelled;
	TestTrue(TEXT("Update record"), First->SaveRecord(Updated));
	FAtlasJobHistoryRecord Found;
	TestTrue(TEXT("Updated record found"), Second->FindRecord(Updated.JobId, Found));
	TestTrue(TEXT("Update visible"), Found.Status == EAtlasJobStatus::Cancelled);
	// A different output root must not retain the previous root's per-job override.
	Settings->OutputFolder.Path = Root / TEXT("OtherOutput");
	TestEqual(TEXT("Root change retains only legacy records"), Second->GetHistoryCount(Query), 2);
	TestEqual(TEXT("Root change discards previous archive name"), Second->GetHistoryForWorkflow(TEXT("test"))[0].WorkflowName, FString(TEXT("Newest label")));
	// External refresh reloads even an already-cached legacy file.
	TestTrue(TEXT("External edit"), FFileHelper::SaveStringToFile(TEXT("[]"), *First->GetHistoryFilePath(TEXT("test"))));
	First->RefreshHistory();
	TestEqual(TEXT("Explicit refresh reaches other managers"), Second->GetHistoryCount(Query), 0);
	TestEqual(TEXT("Empty workflow retained with fallback label"), Second->GetWorkflowInfoWithHistory()[0].DisplayName, FString(TEXT("test")));
	TestTrue(TEXT("Write again"), First->SaveRecord(Old));
	FAtlasJobHistoryRecord Saved;
	First->FindRecord(Old.JobId, Saved);
	TestTrue(TEXT("Delete archived record"), First->DeleteRecord(Old.JobId));
	TestEqual(TEXT("Deletion suppresses archive on refresh"), Second->GetHistoryCount(Query), 0);
	TestTrue(TEXT("Deletion preserves generated archive"), FPaths::FileExists(Saved.JobJsonPath));
	First->ClearHistory();
	TestEqual(TEXT("Clear retains no visible records"), Second->GetAllHistory().Num(), 0);
	// Invalid JSON cannot poison the snapshot or crash a query.
	TestTrue(TEXT("Corrupt isolated legacy file"), FFileHelper::SaveStringToFile(TEXT("{"), *First->GetHistoryFilePath(TEXT("bad"))));
	AddExpectedError(TEXT("Failed to parse history file"), EAutomationExpectedErrorFlags::Contains, 1);
	First->RefreshHistory();
	TestEqual(TEXT("Corrupt file skipped"), First->GetHistoryCount(Query), 0);
	return true;
}
#endif
