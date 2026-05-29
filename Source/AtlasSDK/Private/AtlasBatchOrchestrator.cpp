// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasBatchOrchestrator.h"
#include "AtlasBatchValidator.h"
#include "AtlasBatchPersistence.h"
#include "AtlasJobManager.h"
#include "AtlasOutputManager.h"
#include "AtlasWorkflowAsset.h"
#include "AtlasJob.h"
#include "TimerManager.h"
#include "Engine/World.h"

UAtlasBatchOrchestrator::UAtlasBatchOrchestrator()
{
}

// ==================== Execution ====================

bool UAtlasBatchOrchestrator::RunBatch(
	UAtlasWorkflowAsset* InWorkflowAsset,
	const FAtlasBatchDefinition& InBatch,
	UAtlasJobManager* InJobManager)
{
	if (bIsRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasBatchOrchestrator: Cannot start — a batch is already running"));
		OnBatchFailed.Broadcast(this, TEXT("A batch is already running"));
		return false;
	}

	if (!IsValid(InWorkflowAsset) || !InWorkflowAsset->IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasBatchOrchestrator: Invalid workflow asset"));
		OnBatchFailed.Broadcast(this, TEXT("Workflow asset is invalid or not loaded"));
		return false;
	}

	if (!IsValid(InJobManager))
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasBatchOrchestrator: Invalid job manager"));
		OnBatchFailed.Broadcast(this, TEXT("Job manager is required"));
		return false;
	}

	// Validate all rows before starting
	FAtlasBatchValidationResult ValidationResult = UAtlasBatchValidator::ValidateBatchAgainstAsset(InWorkflowAsset, InBatch);
	if (!ValidationResult.bAllValid)
	{
		FString ErrorSummary = FString::Printf(
			TEXT("Batch validation failed: %d of %d rows have errors"),
			ValidationResult.InvalidRowCount, InBatch.Rows.Num());
		for (const FString& BatchError : ValidationResult.BatchErrors)
		{
			ErrorSummary += TEXT("\n  ") + BatchError;
		}

		UE_LOG(LogTemp, Warning, TEXT("AtlasBatchOrchestrator: %s"), *ErrorSummary);
		OnBatchFailed.Broadcast(this, ErrorSummary);
		return false;
	}

	// Initialize state
	WorkflowAsset = InWorkflowAsset;
	JobManager = InJobManager;
	Batch = InBatch;
	BatchId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	bIsRunning = true;
	bCancellationRequested = false;
	ActiveRowCount = 0;
	NextRowToDispatch = 0;
	RetryAttempts.Empty();
	JobToRowMap.Empty();
	RetryTimerHandles.Empty();

	// Initialize all rows to Pending
	for (int32 i = 0; i < Batch.Rows.Num(); ++i)
	{
		Batch.Rows[i].RowIndex = i;
		Batch.Rows[i].Status = EAtlasBatchRowStatus::Pending;
		Batch.Rows[i].ErrorMessage.Empty();
		Batch.Rows[i].JobId = FGuid();
	}

	// Initialize progress
	Progress = FAtlasBatchProgress();
	Progress.BatchId = BatchId;
	Progress.TotalRows = Batch.Rows.Num();
	Progress.PendingCount = Batch.Rows.Num();

	UE_LOG(LogTemp, Log, TEXT("AtlasBatchOrchestrator: Starting batch '%s' (%s) with %d rows, max concurrent=%d, max retries=%d"),
		*Batch.BatchName, *BatchId, Batch.Rows.Num(), Batch.MaxConcurrentJobs, Batch.MaxRetriesPerRow);

	WriteInitialManifest();

	// Start dispatching
	DispatchNextRows();
	return true;
}

void UAtlasBatchOrchestrator::CancelBatch()
{
	if (!bIsRunning)
	{
		return;
	}

	bCancellationRequested = true;

	UE_LOG(LogTemp, Log, TEXT("AtlasBatchOrchestrator: Cancelling batch %s"), *BatchId);

	// Clear any pending retry timers
	if (GWorld)
	{
		for (auto& Pair : RetryTimerHandles)
		{
			GWorld->GetTimerManager().ClearTimer(Pair.Value);
		}
	}
	RetryTimerHandles.Empty();

	// Cancel all in-flight jobs
	for (const auto& Pair : JobToRowMap)
	{
		const FGuid& JobId = Pair.Key;
		if (UAtlasJob* Job = JobManager->FindJob(JobId))
		{
			if (!Job->IsFinished())
			{
				Job->Cancel();
			}
		}
	}

	// Mark all remaining Pending rows as Cancelled
	for (FAtlasBatchRow& Row : Batch.Rows)
	{
		if (Row.Status == EAtlasBatchRowStatus::Pending)
		{
			Row.Status = EAtlasBatchRowStatus::Cancelled;
			Row.ErrorMessage = TEXT("Batch was cancelled before this row started");
			UpdateManifestForRow(Row.RowIndex, Row.JobId, EAtlasBatchRowStatus::Cancelled, Row.ErrorMessage);
		}
	}

	UpdateProgress();
	CheckBatchComplete();
}

// ==================== Internal Methods ====================

void UAtlasBatchOrchestrator::DispatchNextRows()
{
	if (bCancellationRequested)
	{
		return;
	}

	while (ActiveRowCount < Batch.MaxConcurrentJobs && NextRowToDispatch < Batch.Rows.Num())
	{
		if (bCancellationRequested)
		{
			break;
		}

		int32 RowIndex = NextRowToDispatch++;
		FAtlasBatchRow& Row = Batch.Rows[RowIndex];

		if (Row.Status != EAtlasBatchRowStatus::Pending)
		{
			continue;
		}

		ExecuteRow(RowIndex);
	}
}

void UAtlasBatchOrchestrator::ExecuteRow(int32 RowIndex)
{
	FAtlasBatchRow& Row = Batch.Rows[RowIndex];
	const FGuid RetryOfJobId = RetryAttempts.Contains(RowIndex) ? Row.JobId : FGuid();

	// Build inputs from row values
	FAtlasWorkflowInputs RowInputs;
	for (const auto& Pair : Row.Values)
	{
		RowInputs.SetValue(Pair.Key, Pair.Value);
	}

	// Create job through the manager
	UAtlasJob* Job = JobManager->CreateJobFromAsset(WorkflowAsset, RowInputs);
	if (!Job)
	{
		Row.Status = EAtlasBatchRowStatus::Failed;
		Row.ErrorMessage = TEXT("Failed to create job");
		UpdateProgress();
		OnBatchRowComplete.Broadcast(this, RowIndex, EAtlasBatchRowStatus::Failed);
		CheckBatchComplete();
		return;
	}

	// Tag the job with batch metadata
	Job->BatchId = BatchId;
	Job->BatchIndex = RowIndex;
	Job->RetryOfJobId = RetryOfJobId;

	// Track
	Row.Status = EAtlasBatchRowStatus::Running;
	Row.ErrorMessage.Empty();
	Row.JobId = Job->JobId;
	JobToRowMap.Add(Job->JobId, RowIndex);
	ActiveRowCount++;

	// Bind to state changes
	Job->OnStateChanged.AddDynamic(this, &UAtlasBatchOrchestrator::HandleJobStateChanged);

	UpdateProgress();

	UE_LOG(LogTemp, Log, TEXT("AtlasBatchOrchestrator: Row %d started (Job %s)"),
		RowIndex, *Job->JobId.ToString(EGuidFormats::DigitsWithHyphens));

	// Execute the job
	if (Job->Execute())
	{
		UAtlasOutputManager* OutputManager = NewObject<UAtlasOutputManager>(this);
		const FAtlasJobFolderInfo FolderInfo = OutputManager ? OutputManager->GetJobFolderInfo(Job) : FAtlasJobFolderInfo();
		const bool bJobAlreadyFinished = AtlasJobHelpers::IsTerminalState(Job->State);
		UpdateManifestForRow(
			RowIndex,
			Job->JobId,
			bJobAlreadyFinished ? Row.Status : EAtlasBatchRowStatus::Running,
			bJobAlreadyFinished ? Row.ErrorMessage : TEXT(""),
			FolderInfo.DiskPath,
			FolderInfo.JobJsonPath
		);
	}
}

void UAtlasBatchOrchestrator::HandleJobStateChanged(UAtlasJob* Job, EAtlasJobState NewState)
{
	if (!AtlasJobHelpers::IsTerminalState(NewState))
	{
		return;
	}

	// Find which row this job belongs to
	int32* RowIndexPtr = JobToRowMap.Find(Job->JobId);
	if (!RowIndexPtr)
	{
		return;
	}

	int32 RowIndex = *RowIndexPtr;
	FAtlasBatchRow& Row = Batch.Rows[RowIndex];

	// Unbind
	Job->OnStateChanged.RemoveDynamic(this, &UAtlasBatchOrchestrator::HandleJobStateChanged);
	JobToRowMap.Remove(Job->JobId);
	ActiveRowCount--;

	if (NewState == EAtlasJobState::Completed)
	{
		Row.Status = EAtlasBatchRowStatus::Succeeded;
		Row.ErrorMessage.Empty();

		UE_LOG(LogTemp, Log, TEXT("AtlasBatchOrchestrator: Row %d succeeded"), RowIndex);
	}
	else if (NewState == EAtlasJobState::Cancelled)
	{
		Row.Status = EAtlasBatchRowStatus::Cancelled;
		Row.ErrorMessage = Job->Error.GetSummary();

		UE_LOG(LogTemp, Log, TEXT("AtlasBatchOrchestrator: Row %d cancelled"), RowIndex);
	}
	else // Failed
	{
		// Check if we should retry (transient failure + retries remaining)
		int32& Attempts = RetryAttempts.FindOrAdd(RowIndex, 0);
		bool bIsTransient = Job->Error.IsRetryable();

		if (bIsTransient && Attempts < Batch.MaxRetriesPerRow && !bCancellationRequested)
		{
			Attempts++;
			UE_LOG(LogTemp, Log, TEXT("AtlasBatchOrchestrator: Row %d failed (transient), scheduling retry %d/%d"),
				RowIndex, Attempts, Batch.MaxRetriesPerRow);

			Row.Status = EAtlasBatchRowStatus::Pending;
			UpdateManifestForRow(RowIndex, Row.JobId, EAtlasBatchRowStatus::Pending, TEXT("Retrying..."));
			ScheduleRetry(RowIndex);
			UpdateProgress();
			DispatchNextRows();
			return;
		}

		Row.Status = EAtlasBatchRowStatus::Failed;
		Row.ErrorMessage = Job->Error.GetSummary();

		UE_LOG(LogTemp, Log, TEXT("AtlasBatchOrchestrator: Row %d failed permanently: %s"),
			RowIndex, *Row.ErrorMessage);
	}

	UpdateManifestForRow(RowIndex, Row.JobId, Row.Status, Row.ErrorMessage);

	UpdateProgress();
	OnBatchRowComplete.Broadcast(this, RowIndex, Row.Status);
	OnBatchProgress.Broadcast(this, Progress);

	// Try to dispatch more rows
	DispatchNextRows();

	// Check if the entire batch is done
	CheckBatchComplete();
}

void UAtlasBatchOrchestrator::ScheduleRetry(int32 RowIndex)
{
	int32 AttemptNumber = RetryAttempts.FindOrAdd(RowIndex, 0);
	float BackoffSeconds = CalculateBackoff(AttemptNumber);

	UE_LOG(LogTemp, Log, TEXT("AtlasBatchOrchestrator: Row %d retry in %.1f seconds"), RowIndex, BackoffSeconds);

	if (GWorld)
	{
		FTimerHandle Handle;
		GWorld->GetTimerManager().SetTimer(
			Handle,
			FTimerDelegate::CreateUObject(this, &UAtlasBatchOrchestrator::RetryRow, RowIndex),
			BackoffSeconds,
			false
		);
		RetryTimerHandles.Add(RowIndex, Handle);
	}
	else
	{
		// Fallback: retry immediately
		RetryRow(RowIndex);
	}
}

void UAtlasBatchOrchestrator::RetryRow(int32 RowIndex)
{
	RetryTimerHandles.Remove(RowIndex);

	if (bCancellationRequested)
	{
		Batch.Rows[RowIndex].Status = EAtlasBatchRowStatus::Cancelled;
		UpdateProgress();
		CheckBatchComplete();
		return;
	}

	ExecuteRow(RowIndex);
}

void UAtlasBatchOrchestrator::UpdateProgress()
{
	Progress.PendingCount = 0;
	Progress.RunningCount = 0;
	Progress.SucceededCount = 0;
	Progress.FailedCount = 0;
	Progress.CancelledCount = 0;

	for (const FAtlasBatchRow& Row : Batch.Rows)
	{
		switch (Row.Status)
		{
		case EAtlasBatchRowStatus::Pending:   Progress.PendingCount++;   break;
		case EAtlasBatchRowStatus::Running:   Progress.RunningCount++;   break;
		case EAtlasBatchRowStatus::Succeeded: Progress.SucceededCount++; break;
		case EAtlasBatchRowStatus::Failed:    Progress.FailedCount++;    break;
		case EAtlasBatchRowStatus::Cancelled: Progress.CancelledCount++; break;
		}
	}
}

void UAtlasBatchOrchestrator::CheckBatchComplete()
{
	if (!bIsRunning)
	{
		return;
	}

	// Also account for rows waiting on retry timers
	if (Progress.IsComplete() && RetryTimerHandles.Num() == 0)
	{
		bIsRunning = false;

		FinalizeManifest();

		UE_LOG(LogTemp, Log, TEXT("AtlasBatchOrchestrator: Batch %s complete — %d succeeded, %d failed, %d cancelled"),
			*BatchId, Progress.SucceededCount, Progress.FailedCount, Progress.CancelledCount);

		OnBatchComplete.Broadcast(this, Progress);
	}
}

// ==================== Manifest Persistence ====================

void UAtlasBatchOrchestrator::WriteInitialManifest()
{
	FAtlasBatchManifest Manifest;
	Manifest.BatchId = BatchId;
	Manifest.BatchName = Batch.BatchName;
	Manifest.StartedAt = FDateTime::Now();
	Manifest.TotalRows = Batch.Rows.Num();

	if (IsValid(WorkflowAsset))
	{
		Manifest.WorkflowApiId = WorkflowAsset->GetApiId();
		Manifest.WorkflowName = WorkflowAsset->GetWorkflowName();
	}

	Manifest.Rows.Reserve(Batch.Rows.Num());
	for (const FAtlasBatchRow& Row : Batch.Rows)
	{
		FAtlasBatchManifestRow ManifestRow;
		ManifestRow.RowIndex = Row.RowIndex;
		ManifestRow.Status = EAtlasBatchRowStatus::Pending;
		Manifest.Rows.Add(ManifestRow);
	}

	if (!UAtlasBatchPersistence::SaveManifest(Manifest))
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasBatchOrchestrator: Failed to write initial manifest for batch %s"), *BatchId);
	}
}

void UAtlasBatchOrchestrator::UpdateManifestForRow(int32 RowIndex, const FGuid& JobId, EAtlasBatchRowStatus Status, const FString& ErrorMessage, const FString& JobFolderPath, const FString& JobJsonPath)
{
	if (!UAtlasBatchPersistence::UpdateManifestRow(BatchId, RowIndex, JobId, Status, ErrorMessage, JobFolderPath, JobJsonPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasBatchOrchestrator: Failed to update manifest row %d for batch %s"), RowIndex, *BatchId);
	}
}

void UAtlasBatchOrchestrator::FinalizeManifest()
{
	FAtlasBatchManifest Manifest;
	if (UAtlasBatchPersistence::LoadManifest(BatchId, Manifest))
	{
		Manifest.CompletedAt = FDateTime::Now();
		UAtlasBatchPersistence::SaveManifest(Manifest);
	}
}

float UAtlasBatchOrchestrator::CalculateBackoff(int32 AttemptNumber)
{
	// Base: 2 seconds, exponential with jitter, capped at 30 seconds
	const float BaseSeconds = 2.0f;
	const float MaxSeconds = 30.0f;
	float Delay = BaseSeconds * FMath::Pow(2.0f, static_cast<float>(AttemptNumber - 1));
	Delay = FMath::Min(Delay, MaxSeconds);

	// Add ±25% jitter
	float Jitter = Delay * 0.25f;
	Delay += FMath::FRandRange(-Jitter, Jitter);

	return FMath::Max(Delay, 1.0f);
}
