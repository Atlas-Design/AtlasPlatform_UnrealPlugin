// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AtlasJob.h"
#include "Types/AtlasHistoryTypes.h"
#include "AtlasJobManager.generated.h"

class UAtlasWorkflowAsset;
class UAtlasFileManager;
class UAtlasHistoryManager;
class UAtlasOutputManager;

/** Delegate fired when a new job is created and added to active jobs */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAtlasJobCreated, UAtlasJob*, Job);

/** Delegate fired when a job is removed from active jobs (completed, failed, cancelled, or manually removed) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAtlasJobRemoved, UAtlasJob*, Job);

/** Delegate fired when a job is saved to history (after reaching terminal state) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAtlasJobSavedToHistory, const FAtlasJobHistoryRecord&, Record);

/**
 * Central manager for Atlas workflow jobs.
 * 
 * The Job Manager is responsible for:
 * - Creating and tracking active jobs (preventing garbage collection)
 * - Providing queries for active jobs by workflow or ID
 * - Handling job cancellation
 * - Moving completed jobs out of the active list
 * 
 * In editor context, this is typically owned by UAtlasEditorSubsystem.
 * The manager holds strong references to jobs via UPROPERTY, ensuring
 * they are not garbage collected while active.
 */
UCLASS(BlueprintType)
class ATLASSDK_API UAtlasJobManager : public UObject
{
	GENERATED_BODY()

public:
	UAtlasJobManager();

	// ==================== Job Creation ====================

	/**
	 * Create a new job for a workflow by API ID.
	 * The job is added to ActiveJobs and protected from GC.
	 * @param ApiId The workflow API identifier
	 * @param WorkflowName Human-readable name (for display)
	 * @param Inputs The inputs for this job execution
	 * @return The created job (never null)
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|JobManager")
	UAtlasJob* CreateJob(const FString& ApiId, const FString& WorkflowName, const FAtlasWorkflowInputs& Inputs);

	/**
	 * Create a new job from a workflow asset.
	 * Convenience method that extracts ApiId and name from the asset.
	 * @param WorkflowAsset The workflow asset to execute
	 * @param Inputs The inputs for this job execution
	 * @return The created job (never null), or nullptr if asset is invalid
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|JobManager")
	UAtlasJob* CreateJobFromAsset(UAtlasWorkflowAsset* WorkflowAsset, const FAtlasWorkflowInputs& Inputs);

	// ==================== Job Queries ====================

	/**
	 * Get all currently active jobs.
	 * @return Array of all active jobs
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|JobManager")
	TArray<UAtlasJob*> GetActiveJobs() const;

	/**
	 * Get all active jobs for a specific workflow.
	 * @param ApiId The workflow API identifier to filter by
	 * @return Array of jobs matching the ApiId
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|JobManager")
	TArray<UAtlasJob*> GetJobsForWorkflow(const FString& ApiId) const;

	/**
	 * Find a job by its unique ID.
	 * @param JobId The GUID of the job to find
	 * @return The job if found, nullptr otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|JobManager")
	UAtlasJob* FindJob(const FGuid& JobId) const;

	/**
	 * Get the count of currently active jobs.
	 * @return Number of active jobs
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|JobManager")
	int32 GetActiveJobCount() const { return ActiveJobs.Num(); }

	/**
	 * Check if there are any running jobs.
	 * @return True if at least one job is in Running state
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|JobManager")
	bool HasRunningJobs() const;

	// ==================== Job Management ====================

	/**
	 * Cancel a specific job.
	 * The job will be cancelled and removed from active jobs.
	 * @param Job The job to cancel
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|JobManager")
	void CancelJob(UAtlasJob* Job);

	/**
	 * Cancel all active jobs.
	 * All jobs will be cancelled and the active list cleared.
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|JobManager")
	void CancelAllJobs();

	/**
	 * Remove a job from the active list.
	 * Does NOT cancel the job - use CancelJob() for that.
	 * Typically called internally when a job completes.
	 * @param Job The job to remove
	 * @return True if the job was found and removed
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|JobManager")
	bool RemoveJob(UAtlasJob* Job);

	// ==================== Workflow Asset Queries ====================

	/**
	 * Get all workflow assets in the project.
	 * Queries the Asset Registry for all UAtlasWorkflowAsset instances.
	 * @return Array of all workflow assets
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|JobManager")
	TArray<UAtlasWorkflowAsset*> GetAllWorkflowAssets() const;

	// ==================== Configuration ====================

	/**
	 * Set the FileManager used for file uploads/downloads.
	 * Jobs created after this will use the specified FileManager.
	 * @param InFileManager The FileManager to use
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|JobManager")
	void SetFileManager(UAtlasFileManager* InFileManager);

	/**
	 * Get the current FileManager.
	 * @return The FileManager, or nullptr if not set
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|JobManager")
	UAtlasFileManager* GetFileManager() const { return FileManager; }

	// ==================== History Queries ====================

	/**
	 * Get all history for a specific workflow.
	 * @param ApiId The workflow API ID
	 * @return Array of history records, newest first
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|JobManager|History")
	TArray<FAtlasJobHistoryRecord> GetHistoryForWorkflow(const FString& ApiId);

	/**
	 * Query history with flexible filters.
	 * @param Query The query parameters (filters, pagination)
	 * @return Array of matching history records
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|JobManager|History")
	TArray<FAtlasJobHistoryRecord> QueryHistory(const FAtlasHistoryQuery& Query);

	/**
	 * Get all history across all workflows.
	 * @return Array of all history records, newest first
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|JobManager|History")
	TArray<FAtlasJobHistoryRecord> GetAllHistory();

	/**
	 * Get the count of records matching a query.
	 * Useful for pagination without loading all data.
	 * @param Query The query parameters (Offset/Limit ignored)
	 * @return Number of matching records
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|JobManager|History")
	int32 GetHistoryCount(const FAtlasHistoryQuery& Query);

	/**
	 * Clear history for a specific workflow or all history.
	 * @param ApiId The workflow API ID (empty = clear ALL history)
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|JobManager|History")
	void ClearHistory(const FString& ApiId = TEXT(""));

	/**
	 * Re-run a job from history with the same inputs.
	 * Creates a new job with the same inputs as the historical record.
	 * @param Record The history record to re-run
	 * @return The new job, or nullptr if inputs couldn't be restored
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|JobManager|History")
	UAtlasJob* RerunFromHistory(const FAtlasJobHistoryRecord& Record);

	/**
	 * Get the HistoryManager for direct access.
	 * @return The HistoryManager instance
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|JobManager|History")
	UAtlasHistoryManager* GetHistoryManager();

	// ==================== Events ====================

	/**
	 * Event fired when a new job is created and added to active jobs.
	 * Use this to update UI when jobs are started.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|JobManager|Events")
	FOnAtlasJobCreated OnJobCreated;

	/**
	 * Event fired when a job is removed from active jobs.
	 * This happens when a job completes, fails, is cancelled, or is manually removed.
	 * Use this to update UI when jobs finish.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|JobManager|Events")
	FOnAtlasJobRemoved OnJobRemoved;

	/**
	 * Event fired when a job is saved to history.
	 * Use this to update the history viewer when new records are available.
	 * The record contains all job details including inputs, outputs, and duration.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|JobManager|Events")
	FOnAtlasJobSavedToHistory OnJobSavedToHistory;

protected:
	/** Array of active jobs - UPROPERTY ensures GC protection */
	UPROPERTY()
	TArray<TObjectPtr<UAtlasJob>> ActiveJobs;

	/** Handle job state changes to auto-remove completed jobs */
	UFUNCTION()
	void OnJobStateChanged(UAtlasJob* Job, EAtlasJobState NewState);

private:
	/** Internal helper to bind to job events */
	void BindJobEvents(UAtlasJob* Job);

	/** Internal helper to unbind from job events */
	void UnbindJobEvents(UAtlasJob* Job);

	/** File manager for uploads/downloads */
	UPROPERTY()
	TObjectPtr<UAtlasFileManager> FileManager;

	/** History manager for persistent job records */
	UPROPERTY()
	TObjectPtr<UAtlasHistoryManager> HistoryManager;

	/** Output manager for saving files to disk */
	UPROPERTY()
	TObjectPtr<UAtlasOutputManager> OutputManager;

	/** Create history manager if needed */
	void CreateHistoryManagerIfNeeded();

	/** Create output manager if needed */
	void CreateOutputManagerIfNeeded();

	/** 
	 * Auto-save output files from a completed job.
	 * Saves file outputs to disk and updates the job's output FilePath values.
	 * Uses JobId to create unique filenames.
	 */
	void AutoSaveJobOutputs(UAtlasJob* Job);
};
