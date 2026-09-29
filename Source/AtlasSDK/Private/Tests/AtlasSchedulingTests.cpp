// Copyright Atlas Platform. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "AtlasJob.h"
#include "AtlasJobManager.h"
#include "AtlasHistoryManager.h"
#include "AtlasBatchOrchestrator.h"
#include "AtlasBatchValidator.h"
#include "AtlasWorkflowAsset.h"
#include "AtlasHttpRequest.h"
#include "AtlasJsonObject.h"
#include "AtlasScheduling.h"
#include "Misc/AutomationTest.h"
#include "UObject/GarbageCollection.h"

struct FAtlasSchedulingTestAccess
{
	static void Start(UAtlasJob* Job, TFunction<void(UAtlasHttpRequest*)> Execute, TFunction<void(UAtlasHttpRequest*)> Poll)
	{
		Job->ExecuteRequestDispatcherForTests = MoveTemp(Execute);
		Job->StatusRequestDispatcherForTests = MoveTemp(Poll);
		Job->State = EAtlasJobState::Running;
		Job->StartExecutePhase();
	}
	static void Poll(UAtlasJob* Job) { Job->PollStatus(); }
	static bool PollQueued(UAtlasJob* Job) { return Job->StatusPollTimerHandle.IsValid(); }
	static void Fail(UAtlasJob* Job) { Job->FailWithError(FAtlasError(EAtlasErrorCode::NetworkError, TEXT("Fixture network failure"))); }
	static void Complete(UAtlasJob* Job) { Job->CompleteWithOutputs(FAtlasWorkflowOutputs()); }
	static void SetDispatcher(UAtlasBatchOrchestrator* Batch, TFunction<void(UAtlasJob*)> Dispatcher) { Batch->JobDispatcherForTests = MoveTemp(Dispatcher); }
	static void Ready(UAtlasBatchOrchestrator* Batch, int32 Index) { Batch->RetryRow(Index); }
	static int32 Active(UAtlasBatchOrchestrator* Batch) { return Batch->ActiveRowCount; }
	static int32 ReadyCount(UAtlasBatchOrchestrator* Batch) { return Batch->ReadyRetryRows.Num(); }
	static int32 WaitingCount(UAtlasBatchOrchestrator* Batch) { return Batch->RetryTimerHandles.Num(); }
};

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	void Configure(UAtlasWorkflowAsset* Asset)
	{
		Asset->Schema.ApiId = TEXT("scheduling-fixture");
		Asset->Schema.BaseUrl = TEXT("https://scheduling-test.invalid");
		Asset->Schema.Version = TEXT("0.2");
		Asset->bIsValid = true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasPollRecovery, "Atlas.Scheduling.PollRecovery", Flags)
bool FAtlasPollRecovery::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UAtlasWorkflowAsset> Asset(NewObject<UAtlasWorkflowAsset>());
	Configure(Asset.Get());
	for (bool bExhaustRetries : {false, true})
	{
		TStrongObjectPtr<UAtlasJobManager> Manager(NewObject<UAtlasJobManager>());
		TStrongObjectPtr<UAtlasJob> Job(Manager->CreateJobFromAsset(Asset.Get(), FAtlasWorkflowInputs()));
		Job->SetWorkflowAsset(Asset.Get());
		Job->bAutoDownloadOutputs = false;
		int32 ExecuteCount = 0;
		TStrongObjectPtr<UAtlasHttpRequest> PollRequest;
		FAtlasSchedulingTestAccess::Start(Job.Get(), [&ExecuteCount](UAtlasHttpRequest* Request)
		{
			++ExecuteCount;
			TStrongObjectPtr<UAtlasJsonObject> Reply(UAtlasJsonObject::FromJsonString(TEXT("{\"execution_id\":\"same-execution\"}")));
			Request->OnRequestComplete.Broadcast(Request, Reply.Get(), 200);
		}, [&PollRequest](UAtlasHttpRequest* Request) { PollRequest.Reset(Request); });
		FAtlasJobHistoryRecord Record;
		TestTrue(TEXT("Accepted execution saved before completion"), Manager->GetHistoryManager()->FindRecord(Job->JobId, Record));
		FAtlasJobHistoryRecord Reloaded;
		TestTrue(TEXT("Accepted execution archive reloads"), Manager->GetHistoryManager()->LoadRecordFromJobJson(Record.JobJsonPath, Reloaded));
		TestEqual(TEXT("Server ID persisted"), Reloaded.ExecutionId, FString(TEXT("same-execution")));
		TestTrue(TEXT("Accepted record remains Running"), Reloaded.Status == EAtlasJobStatus::Running);
		const int32 Failures = bExhaustRetries ? 6 : 2;
		for (int32 Index = 0; Index < Failures; ++Index)
		{
			FAtlasSchedulingTestAccess::Poll(Job.Get());
			TestTrue(TEXT("GET keeps accepted execution identity"), PollRequest->GetURL().Contains(TEXT("same-execution")));
			if (Index == 1) PollRequest->OnRequestComplete.Broadcast(PollRequest.Get(), nullptr, 503);
			else PollRequest->OnRequestFailed.Broadcast(PollRequest.Get(), TEXT("Temporary fixture outage"), 0);
			if (Index < 5)
			{
				TestTrue(TEXT("Transient GET failure keeps job running"), Job->State == EAtlasJobState::Running);
				TestTrue(TEXT("Retry queued without blocking"), FAtlasSchedulingTestAccess::PollQueued(Job.Get()));
			}
		}
		if (bExhaustRetries)
		{
			TestTrue(TEXT("Polling retry budget is bounded"), Job->State == EAtlasJobState::Failed);
			TestFalse(TEXT("No resubmission after polling exhaustion"), Job->CanRetryAutomatically());
		}
		else
		{
			FAtlasSchedulingTestAccess::Poll(Job.Get());
			TStrongObjectPtr<UAtlasJsonObject> Reply(UAtlasJsonObject::FromJsonString(TEXT("{\"status\":\"completed\",\"result\":{\"outputs\":{}}}")));
			PollRequest->OnRequestComplete.Broadcast(PollRequest.Get(), Reply.Get(), 200);
			TestTrue(TEXT("Recovery retrieves original result"), Job->State == EAtlasJobState::Completed);
		}
		TestEqual(TEXT("Exactly one execute POST"), ExecuteCount, 1);
		TestFalse(TEXT("Terminal job has no queued poll"), FAtlasSchedulingTestAccess::PollQueued(Job.Get()));
	}
	// A lost execute response must not qualify for automatic whole-row retry either.
	TStrongObjectPtr<UAtlasJob> Uncertain(NewObject<UAtlasJob>());
	Uncertain->SetWorkflowAsset(Asset.Get());
	FAtlasSchedulingTestAccess::Start(Uncertain.Get(), [](UAtlasHttpRequest*) {}, [](UAtlasHttpRequest*) {});
	FAtlasSchedulingTestAccess::Fail(Uncertain.Get());
	TestFalse(TEXT("Uncertain submission cannot be replayed automatically"), Uncertain->CanRetryAutomatically());
	TestTrue(TEXT("Uncertain outcome explained"), Uncertain->Error.Message.Contains(TEXT("uncertain")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasRetryCapacity, "Atlas.Scheduling.RetryCapacity", Flags)
bool FAtlasRetryCapacity::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UAtlasWorkflowAsset> Asset(NewObject<UAtlasWorkflowAsset>());
	Configure(Asset.Get());
	TStrongObjectPtr<UAtlasJobManager> Manager(NewObject<UAtlasJobManager>());
	TStrongObjectPtr<UAtlasBatchOrchestrator> Batch(Manager->CreateBatchOrchestrator());
	TArray<TStrongObjectPtr<UAtlasJob>> Attempts;
	FAtlasSchedulingTestAccess::SetDispatcher(Batch.Get(), [&Attempts](UAtlasJob* Job)
	{
		Job->State = EAtlasJobState::Running;
		Attempts.Emplace(Job); // Hold at the dispatch boundary; no HTTP or credentials required.
	});
	FAtlasBatchDefinition Definition;
	Definition.MaxConcurrentJobs = 1;
	Definition.MaxRetriesPerRow = 1;
	Definition.Rows.SetNum(2);
	TestTrue(TEXT("Batch starts"), Batch->RunBatch(Asset.Get(), Definition, Manager.Get()));
	if (Attempts.Num() != 1) return false;
	FAtlasSchedulingTestAccess::Fail(Attempts[0].Get());
	TestEqual(TEXT("Vacant slot used by second row"), Attempts.Num(), 2);
	TestEqual(TEXT("Failed row waits for nonblocking backoff"), FAtlasSchedulingTestAccess::WaitingCount(Batch.Get()), 1);
	FAtlasSchedulingTestAccess::Ready(Batch.Get(), 0);
	TestEqual(TEXT("Ready retry does not exceed capacity"), Attempts.Num(), 2);
	TestEqual(TEXT("Retry remains queued"), FAtlasSchedulingTestAccess::ReadyCount(Batch.Get()), 1);
	if (Attempts.Num() == 2) FAtlasSchedulingTestAccess::Complete(Attempts[1].Get());
	TestEqual(TEXT("Retry starts when a slot is freed"), Attempts.Num(), 3);
	TestEqual(TEXT("Active attempts stay within limit"), FAtlasSchedulingTestAccess::Active(Batch.Get()), 1);
	Batch->CancelBatch();
	TestEqual(TEXT("Cancellation clears retry queue"), FAtlasSchedulingTestAccess::ReadyCount(Batch.Get()), 0);
	Definition.MaxConcurrentJobs = 0;
	TestFalse(TEXT("Zero concurrency rejected"), UAtlasBatchValidator::ValidateBatch(Asset->Schema, Definition).bAllValid);
	Definition.MaxConcurrentJobs = 1;
	Definition.MaxRetriesPerRow = -1;
	TestFalse(TEXT("Negative retry budget rejected"), UAtlasBatchValidator::ValidateBatch(Asset->Schema, Definition).bAllValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasMonotonicScheduler, "Atlas.Scheduling.MonotonicDeadlines", Flags)
bool FAtlasMonotonicScheduler::RunTest(const FString& Parameters)
{
	FTSTicker Ticker;
	TStrongObjectPtr<UObject> Owner(NewObject<UAtlasJob>());
	int32 Fired = 0;
	auto Immediate = AtlasScheduling::Schedule(Owner.Get(), 0.0, [&Fired](UObject*) { ++Fired; }, Ticker);
	Ticker.Tick(0.0f);
	TestEqual(TEXT("Worldless callback fires without waiting"), Fired, 1);
	auto Later = AtlasScheduling::Schedule(Owner.Get(), 3600.0, [&Fired](UObject*) { ++Fired; }, Ticker);
	Ticker.Tick(1000000.0f);
	TestEqual(TEXT("Deadline uses real time, not accumulated tick delta"), Fired, 1);
	AtlasScheduling::Cancel(Later);
	auto Cancelled = AtlasScheduling::Schedule(Owner.Get(), 0.0, [&Fired](UObject*) { ++Fired; }, Ticker);
	AtlasScheduling::Cancel(Cancelled);
	Ticker.Tick(0.0f);
	TestEqual(TEXT("Cancelled callback suppressed"), Fired, 1);
	auto Expired = AtlasScheduling::Schedule(Owner.Get(), 0.0, [&Fired](UObject*) { ++Fired; }, Ticker);
	Owner.Reset();
	CollectGarbage(RF_NoFlags);
	Ticker.Tick(0.0f);
	TestEqual(TEXT("Destroyed owner is never called"), Fired, 1);
	return true;
}
#endif
