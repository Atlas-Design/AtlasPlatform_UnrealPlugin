// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Types/AtlasJobTypes.h"
#include "Types/AtlasWorkflowTypes.h"
#include "Types/AtlasErrorTypes.h"
#include "Engine/TimerHandle.h"
#include "AtlasJob.generated.h"

class UAtlasWorkflowAsset;
class UAtlasFileManager;
class UAtlasHttpRequest;
class UAtlasJsonObject;

// ==================== Delegate Declarations ====================

/** Delegate fired when job progress changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAtlasJobProgress, UAtlasJob*, Job, float, Progress, EAtlasJobPhase, Phase);

/** Delegate fired when job completes successfully */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAtlasJobComplete, UAtlasJob*, Job, const FAtlasWorkflowOutputs&, Outputs);

/** Delegate fired when job fails or is cancelled */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAtlasJobFailed, UAtlasJob*, Job, const FAtlasError&, Error);

/** Delegate fired when job state changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAtlasJobStateChanged, UAtlasJob*, Job, EAtlasJobState, NewState);

/**
 * Represents a single workflow execution job.
 * 
 * UAtlasJob is a self-contained object that tracks the entire lifecycle
 * of a workflow execution, from input preparation through result retrieval.
 * 
 * Usage:
 * 1. Create via UAtlasJobManager::CreateJob() or CreateJobFromAsset()
 * 2. Bind to OnComplete/OnFailed/OnProgress delegates
 * 3. Call Execute() to start the job
 * 4. Monitor progress or wait for completion
 * 5. Access outputs via GetOutputs() after completion
 * 
 * Jobs support cancellation and provide detailed error information
 * if anything goes wrong during execution.
 */
UCLASS(BlueprintType)
class ATLASSDK_API UAtlasJob : public UObject
{
	GENERATED_BODY()

public:
	UAtlasJob();

	// ==================== Identification ====================

	/** Unique identifier for this job */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Job")
	FGuid JobId;

	/** API identifier of the workflow being executed */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Job")
	FString ApiId;

	/** Human-readable name of the workflow (from schema) */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Job")
	FString WorkflowName;

	/** Optional reference to the workflow asset (if created from one) */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Job")
	TWeakObjectPtr<UAtlasWorkflowAsset> WorkflowAsset;

	// ==================== State ====================

	/** Current state of the job */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Job")
	EAtlasJobState State;

	/** Current execution phase (when State == Running) */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Job")
	EAtlasJobPhase Phase;

	/** Overall progress from 0.0 to 1.0 */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Job")
	float Progress;

	// ==================== Inputs/Outputs ====================

	/** Input values provided for this job execution */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Job")
	FAtlasWorkflowInputs Inputs;

	/** Output values from successful execution (empty until completed) */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Job")
	FAtlasWorkflowOutputs Outputs;

	/** Error information if job failed */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Job")
	FAtlasError Error;

	// ==================== Timestamps ====================

	/** When the job was created */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Job")
	FDateTime CreatedAt;

	/** When Execute() was called (invalid if not started) */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Job")
	FDateTime StartedAt;

	/** When the job completed/failed/cancelled (invalid if not finished) */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Job")
	FDateTime CompletedAt;

	// ==================== Events ====================

	/** Fired when progress updates (includes phase changes) */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|Job|Events")
	FOnAtlasJobProgress OnProgress;

	/** Fired when job completes successfully with outputs */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|Job|Events")
	FOnAtlasJobComplete OnComplete;

	/** Fired when job fails or is cancelled */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|Job|Events")
	FOnAtlasJobFailed OnFailed;

	/** Fired when job state changes */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|Job|Events")
	FOnAtlasJobStateChanged OnStateChanged;

	// ==================== Methods ====================

	/**
	 * Start executing this job.
	 * The job must be in Pending state to execute.
	 * Execution is asynchronous - use events to track progress/completion.
	 * @return True if execution started, false if job is not in Pending state
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Job")
	bool Execute();

	/**
	 * Cancel this job.
	 * If the job is running, it will be stopped as soon as possible.
	 * If pending, it will transition directly to Cancelled state.
	 * No effect if already completed/failed/cancelled.
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Job")
	void Cancel();

	// ==================== Getters (BlueprintPure) ====================

	/**
	 * Get the workflow name for this job.
	 * @return Workflow name
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job")
	FString GetWorkflowName() const { return WorkflowName; }

	/**
	 * Get the API ID for this job.
	 * @return API identifier
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job")
	FString GetApiId() const { return ApiId; }

	/**
	 * Get the unique job ID.
	 * @return Job GUID
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job")
	FGuid GetJobId() const { return JobId; }

	/**
	 * Get the inputs for this job.
	 * @return Reference to inputs
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job")
	const FAtlasWorkflowInputs& GetInputs() const { return Inputs; }

	/**
	 * Get when the job was created.
	 * @return Creation timestamp
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job")
	FDateTime GetCreatedAt() const { return CreatedAt; }

	/**
	 * Get when the job started executing.
	 * @return Start timestamp (invalid if not started)
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job")
	FDateTime GetStartedAt() const { return StartedAt; }

	/**
	 * Get the current state of this job.
	 * @return Current job state
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job")
	EAtlasJobState GetState() const { return State; }

	/**
	 * Get the current execution phase.
	 * Only meaningful when State == Running.
	 * @return Current phase
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job")
	EAtlasJobPhase GetPhase() const { return Phase; }

	/**
	 * Get the current progress (0.0 to 1.0).
	 * @return Progress value
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job")
	float GetProgress() const { return Progress; }

	/**
	 * Check if the job has finished (completed, failed, or cancelled).
	 * @return True if in a terminal state
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job")
	bool IsFinished() const { return AtlasJobHelpers::IsTerminalState(State); }

	/**
	 * Check if the job completed successfully.
	 * @return True if State == Completed
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job")
	bool IsSuccessful() const { return State == EAtlasJobState::Completed; }

	/**
	 * Check if the job is currently running.
	 * @return True if State == Running
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job")
	bool IsRunning() const { return State == EAtlasJobState::Running; }

	/**
	 * Get the outputs from a successful job.
	 * @return Reference to outputs (empty if not completed)
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job")
	const FAtlasWorkflowOutputs& GetOutputs() const { return Outputs; }

	/**
	 * Get the error from a failed job.
	 * @return Reference to error (IsError() == false if no error)
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job")
	const FAtlasError& GetError() const { return Error; }

	/**
	 * Get the duration of the job execution.
	 * @return Duration in seconds (0 if not started or not finished)
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job")
	float GetDurationSeconds() const;

	/**
	 * Get a human-readable status string for this job.
	 * @return Status string (e.g., "Running - Uploading Files (45%)")
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job")
	FString GetStatusString() const;

	// ==================== Internal Methods (used by JobManager/Pipeline) ====================

	/** Initialize the job with workflow info and inputs */
	void Initialize(const FString& InApiId, const FString& InWorkflowName, const FAtlasWorkflowInputs& InInputs);

	/** Set the workflow asset reference */
	void SetWorkflowAsset(UAtlasWorkflowAsset* InAsset);

	/** Set the file manager to use for uploads/downloads */
	void SetFileManager(UAtlasFileManager* InFileManager);

	/** Whether to auto-download file outputs after execution (default: true) */
	UPROPERTY(BlueprintReadWrite, Category = "Atlas|Job")
	bool bAutoDownloadOutputs = true;

	// ==================== User Data (for tracking context in callbacks) ====================

	/**
	 * User-defined integer value for tracking context across async callbacks.
	 * Useful for storing enum values, indices, or other identifiers.
	 * This value is preserved throughout the job lifecycle and accessible in callbacks.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Atlas|Job|UserData")
	int32 UserIndex = 0;

	/**
	 * User-defined name/tag for tracking context across async callbacks.
	 * Useful for storing string identifiers or categories.
	 * This value is preserved throughout the job lifecycle and accessible in callbacks.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Atlas|Job|UserData")
	FName UserTag = NAME_None;

	/**
	 * User-defined object reference for tracking context across async callbacks.
	 * Useful for storing references to data assets, actors, or other UObjects.
	 * This value is preserved throughout the job lifecycle and accessible in callbacks.
	 * Note: This is a weak reference - ensure the object stays alive during job execution.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Atlas|Job|UserData")
	TObjectPtr<UObject> UserObject = nullptr;

protected:
	/** Update state and fire OnStateChanged event */
	void SetState(EAtlasJobState NewState);

	/** Update phase and fire OnProgress event */
	void SetPhase(EAtlasJobPhase NewPhase);

	/** Update progress and fire OnProgress event */
	void SetProgress(float NewProgress);

	/** Mark job as completed successfully */
	void CompleteWithOutputs(const FAtlasWorkflowOutputs& InOutputs);

	/** Mark job as failed with error */
	void FailWithError(const FAtlasError& InError);

	// ==================== Pipeline Internal Methods ====================

	/** Scan inputs for files that need uploading */
	void ScanInputsForUploads();

	/** Start uploading all pending files */
	void StartUploadPhase();

	/** Called when a file upload completes */
	void OnFileUploadComplete(const FGuid& OperationId, const FString& FileId, const FString& InputName);

	/** Called when a file upload fails */
	void OnFileUploadFailed(const FGuid& OperationId, const FString& ErrorMessage, int32 StatusCode, const FString& InputName);

	/** Check if all uploads are complete */
	void CheckUploadPhaseComplete();

	/** Start the execute HTTP request */
	void StartExecutePhase();

	/** Build the JSON payload for execution */
	UAtlasJsonObject* BuildExecutePayload();

	/** Handle execute response */
	UFUNCTION()
	void OnExecuteRequestComplete(UAtlasHttpRequest* Request, UAtlasJsonObject* ResponseJson, int32 StatusCode);

	UFUNCTION()
	void OnExecuteRequestFailed(UAtlasHttpRequest* Request, const FString& ErrorMessage, int32 StatusCode);

	/** Cleanup execute request resources */
	void CleanupExecuteRequest();

	/** Start polling for execution status */
	void StartPollingPhase(const FString& InExecutionId);

	/** Poll execution status once */
	void PollStatus();

	/** Handle status poll response */
	UFUNCTION()
	void OnStatusPollComplete(UAtlasHttpRequest* Request, UAtlasJsonObject* ResponseJson, int32 StatusCode);

	UFUNCTION()
	void OnStatusPollFailed(UAtlasHttpRequest* Request, const FString& ErrorMessage, int32 StatusCode);

	/** Cleanup status request resources */
	void CleanupStatusRequest();

	/** Parse outputs from status response */
	void ParseStatusResponse(UAtlasJsonObject* ResponseJson);

	/** Start downloading file outputs */
	void StartDownloadPhase();

	/** Called when a file download completes */
	void OnFileDownloadComplete(const FGuid& OperationId, const FString& FileId, const TArray<uint8>& FileData, const FString& OutputName);

	/** Called when a file download fails */
	void OnFileDownloadFailed(const FGuid& OperationId, const FString& ErrorMessage, int32 StatusCode, const FString& OutputName);

	/** Check if all downloads are complete */
	void CheckDownloadPhaseComplete();

	/** Finalize job completion */
	void FinalizeCompletion();

	/** Update overall progress based on current phase and sub-progress */
	void UpdateOverallProgress();

	// ==================== FileManager Event Handlers ====================

	/** Handle FileManager upload complete event */
	UFUNCTION()
	void HandleUploadComplete(const FGuid& OperationId, const FString& FileId, const FString& ContentHash);

	/** Handle FileManager upload failed event */
	UFUNCTION()
	void HandleUploadFailed(const FGuid& OperationId, const FString& ErrorMessage, int32 HttpStatusCode);

	/** Handle FileManager download complete event */
	UFUNCTION()
	void HandleDownloadComplete(const FGuid& OperationId, const FString& FileId, const TArray<uint8>& FileData);

	/** Handle FileManager download failed event */
	UFUNCTION()
	void HandleDownloadFailed(const FGuid& OperationId, const FString& ErrorMessage, int32 HttpStatusCode);

private:
	/** Flag to prevent multiple Execute() calls */
	bool bExecutionStarted;

	/** Flag to track cancellation request */
	bool bCancellationRequested;

	/** File manager for uploads/downloads */
	UPROPERTY()
	TObjectPtr<UAtlasFileManager> FileManager;

	/** HTTP request for execute call */
	UPROPERTY()
	TObjectPtr<UAtlasHttpRequest> ExecuteRequest;

	/** HTTP request for status polling */
	UPROPERTY()
	TObjectPtr<UAtlasHttpRequest> StatusRequest;

	/** Execution ID returned from async execute */
	FString ExecutionId;

	/** Timer handle for status polling */
	FTimerHandle StatusPollTimerHandle;

	/** Polling interval in seconds */
	float PollIntervalSeconds = 2.0f;

	// ==================== Upload Tracking ====================

	/** Files that need uploading: InputName -> FilePath */
	TMap<FString, FString> FilesToUpload;

	/** Upload operation IDs: OperationId -> InputName */
	TMap<FGuid, FString> UploadOperations;

	/** Uploaded file IDs: InputName -> FileId */
	TMap<FString, FString> UploadedFileIds;

	/** Number of uploads completed (success or fail) */
	int32 UploadsCompleted;

	/** Number of upload failures */
	int32 UploadFailures;

	// ==================== Download Tracking ====================

	/** Files that need downloading: OutputName -> FileId */
	TMap<FString, FString> FilesToDownload;

	/** Download operation IDs: OperationId -> OutputName */
	TMap<FGuid, FString> DownloadOperations;

	/** Downloaded file data: OutputName -> FileData */
	TMap<FString, TArray<uint8>> DownloadedFiles;

	/** Number of downloads completed (success or fail) */
	int32 DownloadsCompleted;

	/** Number of download failures */
	int32 DownloadFailures;

	// Grant access to internal methods for the job manager and pipeline
	friend class UAtlasJobManager;
	friend class FAtlasJobPipeline;
};
