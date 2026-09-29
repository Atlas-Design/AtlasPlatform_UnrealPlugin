// Copyright Atlas Platform. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "AtlasJob.h"
#include "AtlasJobManager.h"
#include "AtlasFileManager.h"
#include "AtlasBatchOrchestrator.h"
#include "AtlasHttpRequest.h"
#include "AtlasLifetimeTestObserver.h"
#include "AtlasUploadTestObserver.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

struct FAtlasCancellationTestAccess
{
	static void SeedBatch(UAtlasBatchOrchestrator* Batch, UAtlasJobManager* Manager)
	{
		Batch->JobManager = Manager;
		Batch->BatchId = FGuid::NewGuid().ToString();
		Batch->bIsRunning = true;
		Batch->Batch.Rows.SetNum(3);
		Batch->Batch.MaxConcurrentJobs = 2;
		Batch->NextRowToDispatch = 2;
		Batch->Progress.TotalRows = 3;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			auto& Row = Batch->Batch.Rows[Index];
			Row.RowIndex = Index;
			Row.Status = EAtlasBatchRowStatus::Pending;
			if (Index == 2) continue;
			UAtlasJob* Job = Manager->CreateJob(TEXT("cancellation-fixture"), TEXT("Cancellation"), FAtlasWorkflowInputs());
			Job->State = EAtlasJobState::Running;
			Row.JobId = Job->JobId;
			Row.Status = EAtlasBatchRowStatus::Running;
			Batch->JobToRowMap.Add(Job->JobId, Index);
			++Batch->ActiveRowCount;
			Job->OnStateChanged.AddDynamic(Batch, &UAtlasBatchOrchestrator::HandleJobStateChanged);
		}
		Batch->UpdateProgress();
		Batch->WriteInitialManifest();
	}
	static bool Drained(UAtlasBatchOrchestrator* Batch)
	{
		return !Batch->bIsRunning && Batch->JobToRowMap.IsEmpty() && Batch->RetryTimerHandles.IsEmpty()
			&& Batch->ActiveRowCount == 0 && Batch->Progress.CancelledCount == 3;
	}
	static int32 Retained(UAtlasJobManager* Manager) { return Manager->ActiveOrchestrators.Num(); }
	static FGuid SeedTransfer(UAtlasFileManager* Manager, UAtlasJob* Owner, bool bUpload, TArray<TStrongObjectPtr<UAtlasHttpRequest>>& Requests)
	{
		const FGuid Id = FGuid::NewGuid();
		UAtlasHttpRequest* Request = UAtlasHttpRequest::CreateRequest();
		Request->AddToRoot();
		Requests.Emplace(Request); // No network dispatch: exercise ownership and cleanup only.
		if (bUpload)
		{
			FAtlasPendingUpload Pending;
			Pending.OperationId = Id;
			Pending.Request = Request;
			Manager->PendingUploads.Add(Pending);
			if (Owner) Owner->UploadOperations.Add(Id, TEXT("input"));
		}
		else
		{
			FAtlasPendingDownload Pending;
			Pending.OperationId = Id;
			Pending.Request = Request;
			Manager->PendingDownloads.Add(Pending);
			if (Owner) Owner->DownloadOperations.Add(Id, TEXT("output"));
		}
		return Id;
	}
	static void Fail(UAtlasJob* Job) { Job->FailWithError(FAtlasError(EAtlasErrorCode::UploadFailed, TEXT("Controlled failure"))); }
	static bool OwnershipCleared(UAtlasJob* Job) { return Job->UploadOperations.IsEmpty() && Job->DownloadOperations.IsEmpty(); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasCancelAllBatches, "Atlas.Cancellation.AllBatches", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAtlasCancelAllBatches::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UAtlasJobManager> Manager(NewObject<UAtlasJobManager>());
	TStrongObjectPtr<UAtlasBatchOrchestrator> First(Manager->CreateBatchOrchestrator());
	TStrongObjectPtr<UAtlasBatchOrchestrator> Second(Manager->CreateBatchOrchestrator());
	TStrongObjectPtr<UAtlasLifetimeTestObserver> Observer(NewObject<UAtlasLifetimeTestObserver>());
	FAtlasCancellationTestAccess::SeedBatch(First.Get(), Manager.Get());
	FAtlasCancellationTestAccess::SeedBatch(Second.Get(), Manager.Get());
	Observer->BatchCallback = [this, &Manager](UAtlasBatchOrchestrator*)
	{
		Manager->CancelAllJobs(); // Reentrant cancellation must be harmless.
		TestNull(TEXT("Creation blocked while Cancel All broadcasts"), Manager->CreateJob(TEXT("blocked"), TEXT("blocked"), FAtlasWorkflowInputs()));
	};
	First->OnBatchComplete.AddDynamic(Observer.Get(), &UAtlasLifetimeTestObserver::BatchReceived);
	Second->OnBatchComplete.AddDynamic(Observer.Get(), &UAtlasLifetimeTestObserver::BatchReceived);
	Manager->CancelAllJobs();
	TestTrue(TEXT("First batch active and queued rows cancelled"), FAtlasCancellationTestAccess::Drained(First.Get()));
	TestTrue(TEXT("Second batch active and queued rows cancelled"), FAtlasCancellationTestAccess::Drained(Second.Get()));
	TestEqual(TEXT("One completion per batch"), Observer->BatchCount, 2);
	TestEqual(TEXT("No abandoned active jobs"), Manager->GetActiveJobs().Num(), 0);
	TestEqual(TEXT("Completed batches released"), FAtlasCancellationTestAccess::Retained(Manager.Get()), 0);
	UAtlasJob* Later = Manager->CreateJob(TEXT("later"), TEXT("later"), FAtlasWorkflowInputs());
	TestNotNull(TEXT("Creation allowed after cancellation returns"), Later);
	if (Later) Later->Cancel();
	Observer->BatchCallback = nullptr;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasCancelOwnedTransfers, "Atlas.Cancellation.OwnedTransfers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAtlasCancelOwnedTransfers::RunTest(const FString& Parameters)
{
	for (bool bFail : {false, true})
	{
		TStrongObjectPtr<UAtlasFileManager> Files(NewObject<UAtlasFileManager>());
		TStrongObjectPtr<UAtlasJob> Job(NewObject<UAtlasJob>());
		TStrongObjectPtr<UAtlasUploadTestObserver> Observer(NewObject<UAtlasUploadTestObserver>());
		Job->State = EAtlasJobState::Running;
		Job->SetFileManager(Files.Get());
		Job->OnFailed.AddDynamic(Observer.Get(), &UAtlasUploadTestObserver::Failed);
		TArray<TStrongObjectPtr<UAtlasHttpRequest>> Requests;
		TArray<FGuid> OwnedUploads, OwnedDownloads;
		for (int32 Index = 0; Index < 2; ++Index)
		{
			OwnedUploads.Add(FAtlasCancellationTestAccess::SeedTransfer(Files.Get(), Job.Get(), true, Requests));
			OwnedDownloads.Add(FAtlasCancellationTestAccess::SeedTransfer(Files.Get(), Job.Get(), false, Requests));
		}
		const FGuid OtherUpload = FAtlasCancellationTestAccess::SeedTransfer(Files.Get(), nullptr, true, Requests);
		const FGuid OtherDownload = FAtlasCancellationTestAccess::SeedTransfer(Files.Get(), nullptr, false, Requests);
		if (bFail) FAtlasCancellationTestAccess::Fail(Job.Get()); else Job->Cancel();
		Job->Cancel();
		TestEqual(TEXT("Terminal event once"), Observer->FailureCount, 1);
		TestTrue(TEXT("Correct terminal state"), Job->State == (bFail ? EAtlasJobState::Failed : EAtlasJobState::Cancelled));
		TestTrue(TEXT("Job releases operation ownership"), FAtlasCancellationTestAccess::OwnershipCleared(Job.Get()));
		for (const FGuid& Id : OwnedUploads) TestFalse(TEXT("Owned upload stopped"), Files->IsUploadInProgress(Id));
		for (const FGuid& Id : OwnedDownloads) TestFalse(TEXT("Owned download stopped"), Files->IsDownloadInProgress(Id));
		for (int32 Index = 0; Index < 4; ++Index) TestFalse(TEXT("Owned request unrooted"), Requests[Index]->IsRooted());
		TestTrue(TEXT("Unrelated upload preserved"), Files->IsUploadInProgress(OtherUpload));
		TestTrue(TEXT("Unrelated download preserved"), Files->IsDownloadInProgress(OtherDownload));
		Files->CancelAllUploads();
		Files->CancelAllDownloads();
	}
	return true;
}
#endif
