// Copyright Atlas Platform. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "AtlasLifetimeTestObserver.h"
#include "AtlasJobManager.h"
#include "AtlasJob.h"
#include "AtlasFileManager.h"
#include "AtlasWorkflowAsset.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"

struct FAtlasLifetimeTestAccess
{
	static UAtlasJob* PrepareAction(UAtlasExecuteWorkflowAction* Action, bool bOwned = true)
	{
		Action->Manager = NewObject<UAtlasJobManager>(bOwned ? static_cast<UObject*>(Action) : GetTransientPackage());
		Action->FileMgr = NewObject<UAtlasFileManager>(Action->Manager);
		Action->CurrentJob = NewObject<UAtlasJob>(Action->Manager);
		Action->bHasActivated = true;
		Action->CurrentJob->State = EAtlasJobState::Running;
		Action->CurrentJob->OnStateChanged.AddDynamic(Action, &UAtlasExecuteWorkflowAction::OnJobStateChanged);
		return Action->CurrentJob;
	}
	static bool IsCleared(UAtlasExecuteWorkflowAction* Action)
	{
		return !Action->CurrentJob && !Action->Manager && !Action->FileMgr && !Action->Workflow
			&& !Action->WorldContext && !Action->StoredUserObject && Action->InputValues.Values.IsEmpty();
	}
	static int32 Retained(UAtlasJobManager* Manager) { return Manager->ActiveOrchestrators.Num(); }
	static void BeginQueued(UAtlasBatchOrchestrator* Orch, UAtlasJobManager* Manager)
	{
		// Isolate ownership from job dispatch/retry bugs tracked separately.
		Orch->JobManager = Manager;
		Orch->BatchId = FGuid::NewGuid().ToString();
		Orch->Batch.Rows.SetNum(1);
		Orch->Batch.Rows[0].Status = EAtlasBatchRowStatus::Pending;
		Orch->bIsRunning = true;
		Orch->Progress.TotalRows = 1;
		Orch->Progress.PendingCount = 1;
		Orch->WriteInitialManifest();
	}
	static void Complete(UAtlasBatchOrchestrator* Orch)
	{
		Orch->Progress.PendingCount = 0;
		Orch->Progress.SucceededCount = 1;
		Orch->CheckBatchComplete();
	}
};

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	struct FActionContext
	{
		TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
		TStrongObjectPtr<UWorld> World{NewObject<UWorld>()};
		FActionContext() { World->SetGameInstance(Instance.Get()); }
		UAtlasExecuteWorkflowAction* Action(UAtlasWorkflowAsset* Workflow = nullptr)
		{
			FAtlasWorkflowInputs Inputs;
			FAtlasValue Large;
			Large.Type = EAtlasValueType::File;
			Large.FileData.SetNumZeroed(1024 * 1024);
			Inputs.Values.Add(TEXT("large-input"), MoveTemp(Large));
			auto* Result = UAtlasExecuteWorkflowAction::ExecuteWorkflow(World.Get(), Workflow, Inputs, 0, NAME_None, nullptr, nullptr, nullptr);
			Result->ClearFlags(RF_StrongRefOnFrame); // Verify real ownership, not temporary Blueprint-frame protection.
			return Result;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasActionTerminalGC, "Atlas.Lifetime.AsyncTerminalGC", Flags)
bool FAtlasActionTerminalGC::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("AtlasExecuteWorkflowAction: Execution failed"), EAutomationExpectedErrorFlags::Contains, 0);
	FActionContext Context;
	TStrongObjectPtr<UAtlasLifetimeTestObserver> Observer(NewObject<UAtlasLifetimeTestObserver>());
	for (int32 Cycle = 0; Cycle < 6; ++Cycle)
	{
		const EAtlasJobState State = Cycle % 3 == 0 ? EAtlasJobState::Completed
			: (Cycle % 3 == 1 ? EAtlasJobState::Failed : EAtlasJobState::Cancelled);
		TWeakObjectPtr<UAtlasExecuteWorkflowAction> Action = Context.Action();
		TWeakObjectPtr<UAtlasJob> Job = FAtlasLifetimeTestAccess::PrepareAction(Action.Get());
		Observer->ResultCount = 0;
		Action->OnSuccess.AddDynamic(Observer.Get(), &UAtlasLifetimeTestObserver::ResultReceived);
		Action->OnFailure.AddDynamic(Observer.Get(), &UAtlasLifetimeTestObserver::ResultReceived);
		Action->OnComplete.AddDynamic(Observer.Get(), &UAtlasLifetimeTestObserver::ResultReceived);
		Observer->ResultCallback = [this, Action, Job](const FAtlasExecutionResult& Result)
		{
			CollectGarbage(RF_NoFlags);
			TestTrue(TEXT("Action survives result callback GC"), Action.IsValid());
			TestTrue(TEXT("Result job survives result callback GC"), Job.IsValid() && Result.Job == Job.Get());
			Action->Activate(); // Must not reactivate while completing.
		};
		CollectGarbage(RF_NoFlags);
		TestTrue(TEXT("Game instance retains active action"), Action.IsValid());
		TestTrue(TEXT("Action retains its job"), Job.IsValid());
		if (!Action.IsValid() || !Job.IsValid()) return false;
		Job->Error = FAtlasError(EAtlasErrorCode::UploadFailed, TEXT("controlled failure"));
		Job->State = State;
		Job->OnStateChanged.Broadcast(Job.Get(), State);
		TestEqual(TEXT("Outcome plus completion delivered once"), Observer->ResultCount, 2);
		TestTrue(TEXT("Action clears retained payloads"), FAtlasLifetimeTestAccess::IsCleared(Action.Get()));
		Job->OnStateChanged.Broadcast(Job.Get(), State);
		TestEqual(TEXT("Late state ignored"), Observer->ResultCount, 2);
		CollectGarbage(RF_NoFlags);
		TestFalse(TEXT("Completed action collected despite live game instance"), Action.IsValid());
		TestFalse(TEXT("Owned job graph collected"), Job.IsValid());
	}
	Observer->ResultCallback = nullptr;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasActionEarlyFailureGC, "Atlas.Lifetime.AsyncEarlyFailureGC", Flags)
bool FAtlasActionEarlyFailureGC::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("AtlasExecuteWorkflowAction: Execution failed"), EAutomationExpectedErrorFlags::Contains, 1);
	FActionContext Context;
	TWeakObjectPtr<UAtlasExecuteWorkflowAction> Action = Context.Action();
	TStrongObjectPtr<UAtlasLifetimeTestObserver> Observer(NewObject<UAtlasLifetimeTestObserver>());
	Action->OnFailure.AddDynamic(Observer.Get(), &UAtlasLifetimeTestObserver::ResultReceived);
	Action->OnComplete.AddDynamic(Observer.Get(), &UAtlasLifetimeTestObserver::ResultReceived);
	Action->Activate(); // Actual validation failure, before creating any managers/job.
	Action->Activate();
	TestEqual(TEXT("Validation failure only completes once"), Observer->ResultCount, 2);
	TestTrue(TEXT("Preflight data released"), FAtlasLifetimeTestAccess::IsCleared(Action.Get()));
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("Early-failed action unregistered from game instance"), Action.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasActionContextRelease, "Atlas.Lifetime.ContextReleaseAndLateCallback", Flags)
bool FAtlasActionContextRelease::RunTest(const FString& Parameters)
{
	FActionContext Context;
	TWeakObjectPtr<UAtlasExecuteWorkflowAction> Action = Context.Action();
	TStrongObjectPtr<UAtlasJob> Job(FAtlasLifetimeTestAccess::PrepareAction(Action.Get(), false));
	TStrongObjectPtr<UAtlasLifetimeTestObserver> Observer(NewObject<UAtlasLifetimeTestObserver>());
	Action->OnComplete.AddDynamic(Observer.Get(), &UAtlasLifetimeTestObserver::ResultReceived);
	Context.World->SetGameInstance(nullptr);
	Context.World.Reset();
	Context.Instance.Reset();
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("Action collected with its game context"), Action.IsValid());
	Job->OnStateChanged.Broadcast(Job.Get(), EAtlasJobState::Completed);
	TestEqual(TEXT("Dead action receives no late callback"), Observer->ResultCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasBatchPendingGC, "Atlas.Lifetime.BatchPendingGC", Flags)
bool FAtlasBatchPendingGC::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UAtlasJobManager> Manager(NewObject<UAtlasJobManager>());
	TWeakObjectPtr<UAtlasBatchOrchestrator> First = Manager->CreateBatchOrchestrator();
	TWeakObjectPtr<UAtlasBatchOrchestrator> Second = Manager->CreateBatchOrchestrator();
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("First pending batch survives another creation"), First.IsValid());
	TestTrue(TEXT("Newest pending batch survives GC"), Second.IsValid());
	TestEqual(TEXT("Both pending objects retained"), FAtlasLifetimeTestAccess::Retained(Manager.Get()), 2);
	Manager.Reset();
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("Pending batch released with manager"), First.IsValid());
	TestFalse(TEXT("Other pending batch released with manager"), Second.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasBatchTerminalGC, "Atlas.Lifetime.BatchTerminalGC", Flags)
bool FAtlasBatchTerminalGC::RunTest(const FString& Parameters)
{
	for (bool bCancel : {false, true})
	{
		TStrongObjectPtr<UAtlasJobManager> Manager(NewObject<UAtlasJobManager>());
		TStrongObjectPtr<UAtlasLifetimeTestObserver> Observer(NewObject<UAtlasLifetimeTestObserver>());
		TWeakObjectPtr<UAtlasBatchOrchestrator> Batch = Manager->CreateBatchOrchestrator();
		FAtlasLifetimeTestAccess::BeginQueued(Batch.Get(), Manager.Get());
		CollectGarbage(RF_NoFlags);
		TestTrue(TEXT("Running batch remains alive"), Batch.IsValid());
		if (!Batch.IsValid()) return false;
		Observer->BatchCallback = [this](UAtlasBatchOrchestrator* Orchestrator)
		{
			CollectGarbage(RF_NoFlags);
			TestTrue(TEXT("Batch alive throughout completion delivery"), IsValid(Orchestrator));
		};
		Batch->OnBatchComplete.AddDynamic(Observer.Get(), &UAtlasLifetimeTestObserver::BatchReceived);
		if (bCancel) Batch->CancelBatch();
		else FAtlasLifetimeTestAccess::Complete(Batch.Get());
		TestEqual(TEXT("One batch completion"), Observer->BatchCount, 1);
		TestEqual(TEXT("Terminal batch released immediately"), FAtlasLifetimeTestAccess::Retained(Manager.Get()), 0);
		CollectGarbage(RF_NoFlags);
		TestFalse(TEXT("Terminal batch collectible"), Batch.IsValid());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasBatchReuseGC, "Atlas.Lifetime.BatchReuse", Flags)
bool FAtlasBatchReuseGC::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UAtlasJobManager> Manager(NewObject<UAtlasJobManager>());
	TStrongObjectPtr<UAtlasBatchOrchestrator> Batch(Manager->CreateBatchOrchestrator());
	TStrongObjectPtr<UAtlasWorkflowAsset> Workflow(NewObject<UAtlasWorkflowAsset>());
	Workflow->Schema.ApiId = TEXT("lifetime-test");
	Workflow->bIsValid = true;
	FAtlasBatchDefinition Definition;
	Definition.Rows.SetNum(1);
	Definition.MaxRetriesPerRow = 0;
	// No file manager: jobs fail local validation before any authentication or network dispatch.
	for (int32 Run = 0; Run < 2; ++Run)
	{
		TestTrue(TEXT("Batch can start/restart"), Batch->RunBatch(Workflow.Get(), Definition, Manager.Get()));
		TestFalse(TEXT("Locally failed row completes batch"), Batch->IsRunning());
		TestEqual(TEXT("Completed run releases retention"), FAtlasLifetimeTestAccess::Retained(Manager.Get()), 0);
	}
	return true;
}
#endif
