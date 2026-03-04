// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasAsyncActions.h"
#include "AtlasWorkflowAsset.h"
#include "AtlasJob.h"
#include "AtlasJobManager.h"
#include "AtlasFileManager.h"

UAtlasExecuteWorkflowAction* UAtlasExecuteWorkflowAction::ExecuteWorkflow(
	UObject* WorldContextObject,
	UAtlasWorkflowAsset* WorkflowAsset,
	const FAtlasWorkflowInputs& Inputs,
	int32 UserIndex,
	FName UserTag,
	UObject* UserObject,
	UAtlasJobManager* JobManager,
	UAtlasFileManager* FileManager)
{
	UAtlasExecuteWorkflowAction* Action = NewObject<UAtlasExecuteWorkflowAction>();
	Action->WorldContext = WorldContextObject;
	Action->Workflow = WorkflowAsset;
	Action->InputValues = Inputs;
	Action->StoredUserIndex = UserIndex;
	Action->StoredUserTag = UserTag;
	Action->StoredUserObject = UserObject;
	Action->Manager = JobManager;
	Action->FileMgr = FileManager;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UAtlasExecuteWorkflowAction::Activate()
{
	// Validate workflow asset
	if (!IsValid(Workflow))
	{
		CompleteWithError(FAtlasError(EAtlasErrorCode::ValidationError, TEXT("Workflow asset is null or invalid")));
		return;
	}

	if (!Workflow->IsValid())
	{
		CompleteWithError(FAtlasError(EAtlasErrorCode::ValidationError, 
			FString::Printf(TEXT("Workflow asset is not valid: %s"), *Workflow->ParseError)));
		return;
	}

	// Validate inputs
	TArray<FString> ValidationErrors;
	if (!Workflow->ValidateInputs(InputValues, ValidationErrors))
	{
		FString CombinedErrors = FString::Join(ValidationErrors, TEXT(", "));
		CompleteWithError(FAtlasError::ValidationFailed(
			FString::Printf(TEXT("Input validation failed: %s"), *CombinedErrors)));
		return;
	}

	// Create managers if not provided
	if (!Manager)
	{
		Manager = NewObject<UAtlasJobManager>(this);
		bOwnManagers = true;
	}

	if (!FileMgr)
	{
		FileMgr = NewObject<UAtlasFileManager>(this);
		bOwnManagers = true;
	}

	// Ensure manager has file manager
	Manager->SetFileManager(FileMgr);

	// Create the job
	CurrentJob = Manager->CreateJobFromAsset(Workflow, InputValues);

	if (!CurrentJob)
	{
		CompleteWithError(FAtlasError(EAtlasErrorCode::Unknown, TEXT("Failed to create job")));
		return;
	}

	// Apply user data to the job for tracking in callbacks
	CurrentJob->UserIndex = StoredUserIndex;
	CurrentJob->UserTag = StoredUserTag;
	CurrentJob->UserObject = StoredUserObject;

	// Bind to job state changes
	CurrentJob->OnStateChanged.AddDynamic(this, &UAtlasExecuteWorkflowAction::OnJobStateChanged);

	// Start execution
	UE_LOG(LogTemp, Log, TEXT("AtlasExecuteWorkflowAction: Starting execution of workflow '%s'"),
		*Workflow->GetWorkflowName());

	CurrentJob->Execute();
}

void UAtlasExecuteWorkflowAction::OnJobStateChanged(UAtlasJob* ChangedJob, EAtlasJobState NewState)
{
	if (bHasCompleted || ChangedJob != CurrentJob)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("AtlasExecuteWorkflowAction: Job state changed to %s"),
		*AtlasJobHelpers::StateToString(NewState));

	switch (NewState)
	{
	case EAtlasJobState::Completed:
		CompleteWithSuccess();
		break;

	case EAtlasJobState::Failed:
	case EAtlasJobState::Cancelled:
		CompleteWithError(CurrentJob->Error);
		break;

	default:
		// Still in progress
		break;
	}
}

void UAtlasExecuteWorkflowAction::CompleteWithSuccess()
{
	if (bHasCompleted)
	{
		return;
	}

	bHasCompleted = true;

	FAtlasExecutionResult Result;
	Result.bSuccess = true;
	Result.Job = CurrentJob;
	Result.Outputs = CurrentJob->Outputs;

	UE_LOG(LogTemp, Log, TEXT("AtlasExecuteWorkflowAction: Execution completed successfully"));

	// Broadcast delegates
	OnSuccess.Broadcast(Result);
	OnComplete.Broadcast(Result);

	Cleanup();
}

void UAtlasExecuteWorkflowAction::CompleteWithError(const FAtlasError& InError)
{
	if (bHasCompleted)
	{
		return;
	}

	bHasCompleted = true;

	FAtlasExecutionResult Result;
	Result.bSuccess = false;
	Result.Job = CurrentJob;
	Result.Error = InError;

	UE_LOG(LogTemp, Warning, TEXT("AtlasExecuteWorkflowAction: Execution failed: %s"), *InError.GetSummary());

	// Broadcast delegates
	OnFailure.Broadcast(Result);
	OnComplete.Broadcast(Result);

	Cleanup();
}

void UAtlasExecuteWorkflowAction::Cleanup()
{
	// Unbind from job
	if (CurrentJob)
	{
		CurrentJob->OnStateChanged.RemoveDynamic(this, &UAtlasExecuteWorkflowAction::OnJobStateChanged);
	}

	// Note: Don't destroy managers - let GC handle them
	// The job is stored in history by the manager
}
