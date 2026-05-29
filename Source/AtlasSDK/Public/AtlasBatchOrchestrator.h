// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Types/AtlasBatchTypes.h"
#include "AtlasBatchPersistence.h"
#include "AtlasBatchOrchestrator.generated.h"

class UAtlasWorkflowAsset;
class UAtlasJobManager;
class UAtlasJob;

// ==================== Delegates ====================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBatchProgress, UAtlasBatchOrchestrator*, Orchestrator, const FAtlasBatchProgress&, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBatchRowComplete, UAtlasBatchOrchestrator*, Orchestrator, int32, RowIndex, EAtlasBatchRowStatus, RowStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBatchComplete, UAtlasBatchOrchestrator*, Orchestrator, const FAtlasBatchProgress&, FinalProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBatchFailed, UAtlasBatchOrchestrator*, Orchestrator, const FString&, ErrorMessage);

/**
 * Orchestrates the execution of a batch of workflow runs.
 *
 * Manages a queue of rows, dispatches them as concurrent jobs up to a
 * configurable limit, handles transient failure retries with exponential
 * backoff, and supports mid-batch cancellation.
 *
 * Usage:
 *  1. Create via NewObject or through UAtlasJobManager
 *  2. Bind to OnBatchProgress / OnBatchRowComplete / OnBatchComplete
 *  3. Call RunBatch()
 *  4. Optionally call CancelBatch() to stop early
 */
UCLASS(BlueprintType)
class ATLASSDK_API UAtlasBatchOrchestrator : public UObject
{
	GENERATED_BODY()

public:
	UAtlasBatchOrchestrator();

	// ==================== Execution ====================

	/**
	 * Start executing a batch against a workflow asset.
	 * Validates all rows first; if any fail, returns false without starting.
	 * @param InWorkflowAsset The workflow to execute each row against
	 * @param InBatch The batch definition (rows, concurrency, retry settings)
	 * @param InJobManager The job manager to create jobs through
	 * @return True if batch started, false if validation failed or already running
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Batch")
	bool RunBatch(UAtlasWorkflowAsset* InWorkflowAsset, const FAtlasBatchDefinition& InBatch, UAtlasJobManager* InJobManager);

	/**
	 * Cancel the running batch.
	 * Pending rows are skipped; in-flight jobs are cancelled.
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Batch")
	void CancelBatch();

	// ==================== Queries ====================

	/** Get the current progress snapshot */
	UFUNCTION(BlueprintPure, Category = "Atlas|Batch")
	FAtlasBatchProgress GetProgress() const { return Progress; }

	/** Get the batch ID for this run */
	UFUNCTION(BlueprintPure, Category = "Atlas|Batch")
	FString GetBatchId() const { return BatchId; }

	/** Whether a batch is currently in progress */
	UFUNCTION(BlueprintPure, Category = "Atlas|Batch")
	bool IsRunning() const { return bIsRunning; }

	/** Get the batch definition (with updated row statuses) */
	UFUNCTION(BlueprintPure, Category = "Atlas|Batch")
	const FAtlasBatchDefinition& GetBatchDefinition() const { return Batch; }

	// ==================== Events ====================

	/** Fired after each row completes (success, fail, or cancel) with updated aggregate progress */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|Batch|Events")
	FOnBatchProgress OnBatchProgress;

	/** Fired when a single row reaches a terminal state */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|Batch|Events")
	FOnBatchRowComplete OnBatchRowComplete;

	/** Fired when the entire batch finishes (all rows terminal) */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|Batch|Events")
	FOnBatchComplete OnBatchComplete;

	/** Fired if the batch cannot start (validation failure, missing asset, etc.) */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|Batch|Events")
	FOnBatchFailed OnBatchFailed;

private:
	// ==================== Internal Methods ====================

	/** Dispatch the next pending row(s) up to the concurrency limit */
	void DispatchNextRows();

	/** Create and execute a job for a specific row index */
	void ExecuteRow(int32 RowIndex);

	/** Called when a row's job reaches a terminal state */
	UFUNCTION()
	void HandleJobStateChanged(UAtlasJob* Job, EAtlasJobState NewState);

	/** Schedule a retry for a row after backoff delay */
	void ScheduleRetry(int32 RowIndex);

	/** Called by timer to retry a specific row */
	void RetryRow(int32 RowIndex);

	/** Recalculate the Progress struct from current row statuses */
	void UpdateProgress();

	/** Check if the batch is fully complete and fire OnBatchComplete if so */
	void CheckBatchComplete();

	/** Write the initial manifest to disk */
	void WriteInitialManifest();

	/** Update a single row in the on-disk manifest */
	void UpdateManifestForRow(int32 RowIndex, const FGuid& JobId, EAtlasBatchRowStatus Status, const FString& ErrorMessage = TEXT(""), const FString& JobFolderPath = TEXT(""), const FString& JobJsonPath = TEXT(""));

	/** Finalize the manifest with completion timestamp */
	void FinalizeManifest();

	/** Calculate exponential backoff with jitter for a given attempt number */
	static float CalculateBackoff(int32 AttemptNumber);

	// ==================== State ====================

	/** Unique ID for this batch run */
	FString BatchId;

	/** Whether a batch is currently running */
	bool bIsRunning = false;

	/** Whether cancellation has been requested */
	bool bCancellationRequested = false;

	/** The batch definition (rows are mutated with status updates) */
	FAtlasBatchDefinition Batch;

	/** Aggregate progress counters */
	FAtlasBatchProgress Progress;

	/** Workflow asset for this run */
	UPROPERTY()
	TObjectPtr<UAtlasWorkflowAsset> WorkflowAsset;

	/** Job manager used to create and track jobs */
	UPROPERTY()
	TObjectPtr<UAtlasJobManager> JobManager;

	/** Number of rows currently in-flight */
	int32 ActiveRowCount = 0;

	/** Index of the next row to dequeue */
	int32 NextRowToDispatch = 0;

	/** Per-row retry attempt counts (RowIndex -> attempt number, starting at 0) */
	TMap<int32, int32> RetryAttempts;

	/** Map from JobId -> RowIndex for looking up which row a job belongs to */
	TMap<FGuid, int32> JobToRowMap;

	/** Timer handles for retry backoff (RowIndex -> handle) */
	TMap<int32, FTimerHandle> RetryTimerHandles;
};
