// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasJob.h"
#include "AtlasWorkflowAsset.h"
#include "AtlasFileManager.h"
#include "AtlasHttpRequest.h"
#include "AtlasJsonObject.h"
#include "Async/Async.h"
#include "TimerManager.h"
#include "Engine/World.h"

UAtlasJob::UAtlasJob()
	: State(EAtlasJobState::Pending)
	, Phase(EAtlasJobPhase::Initializing)
	, Progress(0.0f)
	, bAutoDownloadOutputs(true)
	, bExecutionStarted(false)
	, bCancellationRequested(false)
	, UploadsCompleted(0)
	, UploadFailures(0)
	, DownloadsCompleted(0)
	, DownloadFailures(0)
{
	JobId = FGuid::NewGuid();
	CreatedAt = FDateTime::Now();  // Use local time for display consistency
}

// ==================== Initialization ====================

void UAtlasJob::Initialize(const FString& InApiId, const FString& InWorkflowName, const FAtlasWorkflowInputs& InInputs)
{
	ApiId = InApiId;
	WorkflowName = InWorkflowName;
	Inputs = InInputs;
}

void UAtlasJob::SetWorkflowAsset(UAtlasWorkflowAsset* InAsset)
{
	WorkflowAsset = InAsset;
}

void UAtlasJob::SetFileManager(UAtlasFileManager* InFileManager)
{
	FileManager = InFileManager;
}

// ==================== Execution ====================

bool UAtlasJob::Execute()
{
	// Can only execute from Pending state
	if (State != EAtlasJobState::Pending)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasJob::Execute() called on job not in Pending state (current: %s)"),
			*AtlasJobHelpers::StateToString(State));
		return false;
	}

	// Prevent multiple executions
	if (bExecutionStarted)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasJob::Execute() called but execution already started"));
		return false;
	}

	// Need a workflow asset to get URLs
	if (!WorkflowAsset.IsValid())
	{
		FAtlasError NoAssetError;
		NoAssetError.Code = EAtlasErrorCode::ValidationError;
		NoAssetError.Message = TEXT("Cannot execute job without a valid WorkflowAsset");
		NoAssetError.Timestamp = FDateTime::UtcNow();
		FailWithError(NoAssetError);
		return false;
	}

	// Need a file manager
	if (!FileManager)
	{
		FAtlasError NoManagerError;
		NoManagerError.Code = EAtlasErrorCode::ValidationError;
		NoManagerError.Message = TEXT("Cannot execute job without a FileManager");
		NoManagerError.Timestamp = FDateTime::UtcNow();
		FailWithError(NoManagerError);
		return false;
	}

	bExecutionStarted = true;
	StartedAt = FDateTime::Now();  // Use local time for display consistency

	// Transition to Running state
	SetState(EAtlasJobState::Running);
	SetPhase(EAtlasJobPhase::Initializing);
	SetProgress(0.0f);

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] started execution for workflow '%s'"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *WorkflowName);

	// Start the pipeline: scan inputs for files
	ScanInputsForUploads();

	// If there are files to upload, start upload phase
	// Otherwise, skip directly to execute phase
	if (FilesToUpload.Num() > 0)
	{
		StartUploadPhase();
	}
	else
	{
		StartExecutePhase();
	}

	return true;
}

void UAtlasJob::Cancel()
{
	// Already in terminal state?
	if (AtlasJobHelpers::IsTerminalState(State))
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasJob::Cancel() called on already-finished job (state: %s)"),
			*AtlasJobHelpers::StateToString(State));
		return;
	}

	bCancellationRequested = true;

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] cancellation requested"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens));

	// If still pending, transition directly to cancelled
	if (State == EAtlasJobState::Pending)
	{
		CompletedAt = FDateTime::Now();  // Use local time for display consistency
		
		Error.Code = EAtlasErrorCode::Cancelled;
		Error.Message = TEXT("Job was cancelled before execution started");
		Error.Timestamp = CompletedAt;

		SetState(EAtlasJobState::Cancelled);
		OnFailed.Broadcast(this, Error);
		return;
	}

	// Cancel any pending execute request
	if (ExecuteRequest)
	{
		ExecuteRequest->CancelRequest();
	}

	// Cancel any pending status request
	if (StatusRequest)
	{
		StatusRequest->CancelRequest();
	}

	// Mark as cancelled
	FAtlasError CancelError;
	CancelError.Code = EAtlasErrorCode::Cancelled;
	CancelError.Message = TEXT("Job was cancelled during execution");
	CancelError.Timestamp = FDateTime::UtcNow();
	FailWithError(CancelError);
}

// ==================== Pipeline: Upload Phase ====================

void UAtlasJob::ScanInputsForUploads()
{
	FilesToUpload.Empty();

	for (const auto& Pair : Inputs.Values)
	{
		const FString& InputName = Pair.Key;
		const FAtlasValue& Value = Pair.Value;

		// Check if this input is a file type that needs uploading
		if (Value.Type == EAtlasValueType::Image || 
			Value.Type == EAtlasValueType::Mesh || 
			Value.Type == EAtlasValueType::File)
		{
			// If it already has a FileId, no need to upload
			if (!Value.FileId.IsEmpty())
			{
				UploadedFileIds.Add(InputName, Value.FileId);
				UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] input '%s' already has FileId: %s"),
					*JobId.ToString(EGuidFormats::DigitsWithHyphens), *InputName, *Value.FileId);
			}
			// If it has a file path, we need to upload
			else if (!Value.FilePath.IsEmpty())
			{
				FilesToUpload.Add(InputName, Value.FilePath);
				UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] input '%s' needs upload from: %s"),
					*JobId.ToString(EGuidFormats::DigitsWithHyphens), *InputName, *Value.FilePath);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AtlasJob[%s] file input '%s' has no FilePath or FileId"),
					*JobId.ToString(EGuidFormats::DigitsWithHyphens), *InputName);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] found %d files to upload, %d already have FileIds"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), FilesToUpload.Num(), UploadedFileIds.Num());
}

void UAtlasJob::StartUploadPhase()
{
	if (bCancellationRequested) return;

	SetPhase(EAtlasJobPhase::Uploading);
	UploadsCompleted = 0;
	UploadFailures = 0;
	UploadOperations.Empty();

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] starting upload phase with %d files"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), FilesToUpload.Num());

	// Bind to FileManager events
	FileManager->OnUploadComplete.AddDynamic(this, &UAtlasJob::HandleUploadComplete);
	FileManager->OnUploadFailed.AddDynamic(this, &UAtlasJob::HandleUploadFailed);

	// Start all uploads in parallel
	for (const auto& Pair : FilesToUpload)
	{
		const FString& InputName = Pair.Key;
		const FString& FilePath = Pair.Value;

		// Use the FileManager to upload
		FGuid OperationId = FileManager->UploadFile(WorkflowAsset.Get(), FilePath);

		if (OperationId.IsValid())
		{
			UploadOperations.Add(OperationId, InputName);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("AtlasJob[%s] failed to start upload for '%s' from '%s'"),
				*JobId.ToString(EGuidFormats::DigitsWithHyphens), *InputName, *FilePath);
			
			UploadsCompleted++;
			UploadFailures++;
		}
	}

	UpdateOverallProgress();
	CheckUploadPhaseComplete();
}

void UAtlasJob::OnFileUploadComplete(const FGuid& OperationId, const FString& FileId, const FString& InputName)
{
	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] upload complete for '%s': FileId=%s"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *InputName, *FileId);

	UploadedFileIds.Add(InputName, FileId);
	UploadOperations.Remove(OperationId);
	UploadsCompleted++;

	UpdateOverallProgress();
	CheckUploadPhaseComplete();
}

void UAtlasJob::OnFileUploadFailed(const FGuid& OperationId, const FString& ErrorMessage, int32 StatusCode, const FString& InputName)
{
	UE_LOG(LogTemp, Error, TEXT("AtlasJob[%s] upload failed for '%s': %s (status %d)"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *InputName, *ErrorMessage, StatusCode);

	UploadOperations.Remove(OperationId);
	UploadsCompleted++;
	UploadFailures++;

	// Fail immediately on first upload error
	FAtlasError UploadError;
	UploadError.Code = EAtlasErrorCode::UploadFailed;
	UploadError.Message = FString::Printf(TEXT("Failed to upload '%s': %s"), *InputName, *ErrorMessage);
	UploadError.HttpStatusCode = StatusCode;
	UploadError.Timestamp = FDateTime::UtcNow();
	FailWithError(UploadError);
}

void UAtlasJob::CheckUploadPhaseComplete()
{
	if (bCancellationRequested || AtlasJobHelpers::IsTerminalState(State))
	{
		return;
	}

	if (UploadsCompleted >= FilesToUpload.Num())
	{
		// Unbind from FileManager events
		if (FileManager)
		{
			FileManager->OnUploadComplete.RemoveDynamic(this, &UAtlasJob::HandleUploadComplete);
			FileManager->OnUploadFailed.RemoveDynamic(this, &UAtlasJob::HandleUploadFailed);
		}

		if (UploadFailures > 0)
		{
			// Already failed via OnFileUploadFailed
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] all uploads complete, starting execute phase"),
			*JobId.ToString(EGuidFormats::DigitsWithHyphens));

		StartExecutePhase();
	}
}

// ==================== Pipeline: Execute Phase ====================

void UAtlasJob::StartExecutePhase()
{
	if (bCancellationRequested) return;

	SetPhase(EAtlasJobPhase::Executing);
	UpdateOverallProgress();

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] starting execute phase"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens));

	// Build the payload
	UAtlasJsonObject* Payload = BuildExecutePayload();
	if (!Payload)
	{
		FAtlasError PayloadError;
		PayloadError.Code = EAtlasErrorCode::ValidationError;
		PayloadError.Message = TEXT("Failed to build execute payload");
		PayloadError.Timestamp = FDateTime::UtcNow();
		FailWithError(PayloadError);
		return;
	}

	// Get the execute URL
	FString ExecuteUrl = WorkflowAsset->GetExecuteUrl();

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] POSTing to: %s"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *ExecuteUrl);

	// Create and configure the request
	ExecuteRequest = UAtlasHttpRequest::CreateRequest();
	ExecuteRequest->AddToRoot(); // Prevent GC
	ExecuteRequest->SetURL(ExecuteUrl);
	ExecuteRequest->SetVerb(EAtlasHttpVerb::POST);
	ExecuteRequest->SetContentType(EAtlasHttpContentType::JSON);
	ExecuteRequest->SetRequestBodyJson(Payload);

	// Bind callbacks
	ExecuteRequest->OnRequestComplete.AddDynamic(this, &UAtlasJob::OnExecuteRequestComplete);
	ExecuteRequest->OnRequestFailed.AddDynamic(this, &UAtlasJob::OnExecuteRequestFailed);

	// Execute
	ExecuteRequest->ProcessRequest();
}

UAtlasJsonObject* UAtlasJob::BuildExecutePayload()
{
	UAtlasJsonObject* Payload = NewObject<UAtlasJsonObject>();

	for (const auto& Pair : Inputs.Values)
	{
		const FString& InputName = Pair.Key;
		const FAtlasValue& Value = Pair.Value;

		switch (Value.Type)
		{
		case EAtlasValueType::String:
			Payload->SetStringField(InputName, Value.StringValue);
			break;

		case EAtlasValueType::Number:
			Payload->SetNumberField(InputName, Value.NumberValue);
			break;

		case EAtlasValueType::Integer:
			Payload->SetIntegerField(InputName, Value.IntValue);
			break;

		case EAtlasValueType::Boolean:
			Payload->SetBoolField(InputName, Value.BoolValue);
			break;

		case EAtlasValueType::Image:
		case EAtlasValueType::Mesh:
		case EAtlasValueType::File:
			// Use the uploaded FileId
			if (FString* FileIdPtr = UploadedFileIds.Find(InputName))
			{
				Payload->SetStringField(InputName, *FileIdPtr);
			}
			else if (!Value.FileId.IsEmpty())
			{
				Payload->SetStringField(InputName, Value.FileId);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AtlasJob: No FileId for file input '%s'"), *InputName);
			}
			break;

		case EAtlasValueType::FileId:
			Payload->SetStringField(InputName, Value.FileId);
			break;

		case EAtlasValueType::Json:
			// Parse and embed the JSON
			{
				UAtlasJsonObject* NestedJson = UAtlasJsonObject::FromJsonString(Value.JsonValue);
				if (NestedJson)
				{
					Payload->SetObjectField(InputName, NestedJson);
				}
				else
				{
					// If parsing fails, send as string
					Payload->SetStringField(InputName, Value.JsonValue);
				}
			}
			break;

		default:
			UE_LOG(LogTemp, Warning, TEXT("AtlasJob: Unknown value type for input '%s'"), *InputName);
			break;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] built payload: %s"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *Payload->EncodeJson());

	return Payload;
}

void UAtlasJob::OnExecuteRequestComplete(UAtlasHttpRequest* Request, UAtlasJsonObject* ResponseJson, int32 StatusCode)
{
	if (bCancellationRequested || AtlasJobHelpers::IsTerminalState(State))
	{
		CleanupExecuteRequest();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] execute response: status=%d"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), StatusCode);

	// Check for success (2xx)
	if (StatusCode >= 200 && StatusCode < 300)
	{
		// Async execute returns execution_id - need to poll for status
		if (ResponseJson)
		{
			ExecutionId = ResponseJson->GetStringField(TEXT("execution_id"));
		}

		CleanupExecuteRequest();

		if (ExecutionId.IsEmpty())
		{
			FAtlasError ExecError;
			ExecError.Code = EAtlasErrorCode::ExecutionFailed;
			ExecError.Message = TEXT("Execute response missing execution_id");
			ExecError.Timestamp = FDateTime::UtcNow();
			FailWithError(ExecError);
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] got execution_id: %s, starting polling"),
			*JobId.ToString(EGuidFormats::DigitsWithHyphens), *ExecutionId);

		// Start polling for status
		StartPollingPhase(ExecutionId);
	}
	else
	{
		FAtlasError ExecError;
		ExecError.Code = EAtlasErrorCode::ExecutionFailed;
		ExecError.HttpStatusCode = StatusCode;
		
		if (ResponseJson)
		{
			ExecError.Message = ResponseJson->GetStringField(TEXT("error"));
			if (ExecError.Message.IsEmpty())
			{
				ExecError.Message = ResponseJson->GetStringField(TEXT("message"));
			}
		}
		if (ExecError.Message.IsEmpty())
		{
			ExecError.Message = FString::Printf(TEXT("Execute request failed with status %d"), StatusCode);
		}
		ExecError.Timestamp = FDateTime::UtcNow();

		CleanupExecuteRequest();
		FailWithError(ExecError);
	}
}

void UAtlasJob::OnExecuteRequestFailed(UAtlasHttpRequest* Request, const FString& ErrorMessage, int32 StatusCode)
{
	if (bCancellationRequested || AtlasJobHelpers::IsTerminalState(State))
	{
		CleanupExecuteRequest();
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("AtlasJob[%s] execute request failed: %s (status %d)"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *ErrorMessage, StatusCode);

	FAtlasError ExecError;
	ExecError.Code = EAtlasErrorCode::NetworkError;
	ExecError.Message = ErrorMessage;
	ExecError.HttpStatusCode = StatusCode;
	ExecError.Timestamp = FDateTime::UtcNow();

	CleanupExecuteRequest();
	FailWithError(ExecError);
}

void UAtlasJob::CleanupExecuteRequest()
{
	if (ExecuteRequest)
	{
		ExecuteRequest->OnRequestComplete.RemoveDynamic(this, &UAtlasJob::OnExecuteRequestComplete);
		ExecuteRequest->OnRequestFailed.RemoveDynamic(this, &UAtlasJob::OnExecuteRequestFailed);
		ExecuteRequest->RemoveFromRoot();
		ExecuteRequest = nullptr;
	}
}

// ==================== Pipeline: Polling Phase ====================

void UAtlasJob::StartPollingPhase(const FString& InExecutionId)
{
	if (bCancellationRequested) return;

	ExecutionId = InExecutionId;

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] starting status polling for execution %s"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *ExecutionId);

	// Schedule first poll after delay
	if (GWorld)
	{
		GWorld->GetTimerManager().SetTimer(
			StatusPollTimerHandle,
			FTimerDelegate::CreateUObject(this, &UAtlasJob::PollStatus),
			PollIntervalSeconds,
			false // Not looping - we'll reschedule after each poll
		);
	}
	else
	{
		// Fallback: poll immediately if no world
		PollStatus();
	}
}

void UAtlasJob::PollStatus()
{
	if (bCancellationRequested || AtlasJobHelpers::IsTerminalState(State))
	{
		return;
	}

	if (!WorkflowAsset.IsValid())
	{
		FAtlasError PollError;
		PollError.Code = EAtlasErrorCode::ValidationError;
		PollError.Message = TEXT("WorkflowAsset became invalid during polling");
		PollError.Timestamp = FDateTime::UtcNow();
		FailWithError(PollError);
		return;
	}

	FString StatusUrl = WorkflowAsset->GetStatusUrl(ExecutionId);

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] polling status: %s"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *StatusUrl);

	// Create status request
	StatusRequest = UAtlasHttpRequest::CreateRequest();
	StatusRequest->AddToRoot();
	StatusRequest->SetURL(StatusUrl);
	StatusRequest->SetVerb(EAtlasHttpVerb::GET);

	// Bind callbacks
	StatusRequest->OnRequestComplete.AddDynamic(this, &UAtlasJob::OnStatusPollComplete);
	StatusRequest->OnRequestFailed.AddDynamic(this, &UAtlasJob::OnStatusPollFailed);

	StatusRequest->ProcessRequest();
}

void UAtlasJob::OnStatusPollComplete(UAtlasHttpRequest* Request, UAtlasJsonObject* ResponseJson, int32 StatusCode)
{
	if (bCancellationRequested || AtlasJobHelpers::IsTerminalState(State))
	{
		CleanupStatusRequest();
		return;
	}

	CleanupStatusRequest();

	if (StatusCode < 200 || StatusCode >= 300)
	{
		FAtlasError ExecError;
		ExecError.Code = EAtlasErrorCode::ExecutionFailed;
		ExecError.HttpStatusCode = StatusCode;
		ExecError.Message = FString::Printf(TEXT("Status poll failed with status %d"), StatusCode);
		ExecError.Timestamp = FDateTime::UtcNow();
		FailWithError(ExecError);
		return;
	}

	if (!ResponseJson)
	{
		FAtlasError ExecError;
		ExecError.Code = EAtlasErrorCode::ExecutionFailed;
		ExecError.Message = TEXT("Status response has no JSON body");
		ExecError.Timestamp = FDateTime::UtcNow();
		FailWithError(ExecError);
		return;
	}

	FString Status = ResponseJson->GetStringField(TEXT("status"));
	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] status: %s"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *Status);

	if (Status == TEXT("completed"))
	{
		// Parse outputs from result
		ParseStatusResponse(ResponseJson);

		// Check if we need to download outputs
		if (bAutoDownloadOutputs && FilesToDownload.Num() > 0)
		{
			StartDownloadPhase();
		}
		else
		{
			FinalizeCompletion();
		}
	}
	else if (Status == TEXT("failed"))
	{
		// Extract error information
		FAtlasError ExecError;
		ExecError.Code = EAtlasErrorCode::ExecutionFailed;
		ExecError.Timestamp = FDateTime::UtcNow();

		UAtlasJsonObject* ErrorObj = ResponseJson->GetObjectField(TEXT("error"));
		if (ErrorObj)
		{
			ExecError.Message = ErrorObj->GetStringField(TEXT("error"));
			FString NodeName = ErrorObj->GetStringField(TEXT("node_name"));
			FString NodeType = ErrorObj->GetStringField(TEXT("node_type"));
			FString NodeId = ErrorObj->GetStringField(TEXT("node_id"));

			if (!NodeName.IsEmpty() || !NodeType.IsEmpty())
			{
				ExecError.Message += FString::Printf(TEXT(" (node: %s)"), 
					NodeName.IsEmpty() ? *NodeType : *NodeName);
			}
			if (!NodeId.IsEmpty())
			{
				ExecError.Message += FString::Printf(TEXT(" [id: %s...]"), *NodeId.Left(8));
			}
		}
		else
		{
			ExecError.Message = TEXT("Workflow execution failed");
		}

		FailWithError(ExecError);
	}
	else if (Status == TEXT("pending") || Status == TEXT("running"))
	{
		// Schedule next poll
		if (GWorld)
		{
			GWorld->GetTimerManager().SetTimer(
				StatusPollTimerHandle,
				FTimerDelegate::CreateUObject(this, &UAtlasJob::PollStatus),
				PollIntervalSeconds,
				false
			);
		}
		else
		{
			// Fallback: use AsyncTask to schedule next poll
			FTimerDelegate PollDelegate = FTimerDelegate::CreateUObject(this, &UAtlasJob::PollStatus);
			AsyncTask(ENamedThreads::GameThread, [this]()
			{
				FPlatformProcess::Sleep(PollIntervalSeconds);
				if (!bCancellationRequested && !AtlasJobHelpers::IsTerminalState(State))
				{
					PollStatus();
				}
			});
		}
	}
	else
	{
		FAtlasError ExecError;
		ExecError.Code = EAtlasErrorCode::ExecutionFailed;
		ExecError.Message = FString::Printf(TEXT("Unknown execution status: %s"), *Status);
		ExecError.Timestamp = FDateTime::UtcNow();
		FailWithError(ExecError);
	}
}

void UAtlasJob::OnStatusPollFailed(UAtlasHttpRequest* Request, const FString& ErrorMessage, int32 StatusCode)
{
	if (bCancellationRequested || AtlasJobHelpers::IsTerminalState(State))
	{
		CleanupStatusRequest();
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("AtlasJob[%s] status poll failed: %s (status %d)"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *ErrorMessage, StatusCode);

	CleanupStatusRequest();

	FAtlasError ExecError;
	ExecError.Code = EAtlasErrorCode::NetworkError;
	ExecError.Message = ErrorMessage;
	ExecError.HttpStatusCode = StatusCode;
	ExecError.Timestamp = FDateTime::UtcNow();
	FailWithError(ExecError);
}

void UAtlasJob::CleanupStatusRequest()
{
	if (StatusRequest)
	{
		StatusRequest->OnRequestComplete.RemoveDynamic(this, &UAtlasJob::OnStatusPollComplete);
		StatusRequest->OnRequestFailed.RemoveDynamic(this, &UAtlasJob::OnStatusPollFailed);
		StatusRequest->RemoveFromRoot();
		StatusRequest = nullptr;
	}

	// Cancel any pending poll timer
	if (GWorld)
	{
		GWorld->GetTimerManager().ClearTimer(StatusPollTimerHandle);
	}
}

void UAtlasJob::ParseStatusResponse(UAtlasJsonObject* ResponseJson)
{
	if (!ResponseJson)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasJob[%s] status response has no JSON body"),
			*JobId.ToString(EGuidFormats::DigitsWithHyphens));
		return;
	}

	FilesToDownload.Empty();

	// Get the output schema from the workflow
	if (!WorkflowAsset.IsValid())
	{
		return;
	}

	// Status response has outputs nested in result.outputs
	UAtlasJsonObject* ResultObj = ResponseJson->GetObjectField(TEXT("result"));
	if (!ResultObj)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasJob[%s] status response missing 'result' field"),
			*JobId.ToString(EGuidFormats::DigitsWithHyphens));
		return;
	}

	UAtlasJsonObject* OutputsObj = ResultObj->GetObjectField(TEXT("outputs"));
	if (!OutputsObj)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasJob[%s] status response missing 'result.outputs' field"),
			*JobId.ToString(EGuidFormats::DigitsWithHyphens));
		return;
	}

	const FAtlasWorkflowSchema& Schema = WorkflowAsset->GetSchema();

	// Parse each output according to schema
	for (const FAtlasParameterDef& OutputDef : Schema.Outputs)
	{
		const FString& OutputName = OutputDef.Name;
		FAtlasValue OutputValue;
		OutputValue.Type = OutputDef.Type;

		switch (OutputDef.Type)
		{
		case EAtlasValueType::String:
			OutputValue.StringValue = OutputsObj->GetStringField(OutputName);
			break;

		case EAtlasValueType::Number:
			OutputValue.NumberValue = OutputsObj->GetNumberField(OutputName);
			break;

		case EAtlasValueType::Integer:
			OutputValue.IntValue = OutputsObj->GetIntegerField(OutputName);
			break;

		case EAtlasValueType::Boolean:
			OutputValue.BoolValue = OutputsObj->GetBoolField(OutputName);
			break;

		case EAtlasValueType::Image:
		case EAtlasValueType::Mesh:
		case EAtlasValueType::File:
		case EAtlasValueType::FileId:
			// Server returns FileId for file outputs
			OutputValue.FileId = OutputsObj->GetStringField(OutputName);
			if (!OutputValue.FileId.IsEmpty())
			{
				FilesToDownload.Add(OutputName, OutputValue.FileId);
			}
			break;

		case EAtlasValueType::Json:
			{
				UAtlasJsonObject* NestedJson = OutputsObj->GetObjectField(OutputName);
				if (NestedJson)
				{
					OutputValue.JsonValue = NestedJson->EncodeJson();
				}
			}
			break;

		default:
			break;
		}

		Outputs.Values.Add(OutputName, OutputValue);
	}

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] parsed %d outputs, %d files to download"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), Outputs.Values.Num(), FilesToDownload.Num());
}

// ==================== Pipeline: Download Phase ====================

void UAtlasJob::StartDownloadPhase()
{
	if (bCancellationRequested) return;

	SetPhase(EAtlasJobPhase::Downloading);
	DownloadsCompleted = 0;
	DownloadFailures = 0;
	DownloadOperations.Empty();
	DownloadedFiles.Empty();

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] starting download phase with %d files"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), FilesToDownload.Num());

	// Bind to FileManager download events
	FileManager->OnDownloadComplete.AddDynamic(this, &UAtlasJob::HandleDownloadComplete);
	FileManager->OnDownloadFailed.AddDynamic(this, &UAtlasJob::HandleDownloadFailed);

	// Start all downloads in parallel
	for (const auto& Pair : FilesToDownload)
	{
		const FString& OutputName = Pair.Key;
		const FString& FileId = Pair.Value;

		FGuid OperationId = FileManager->DownloadFile(WorkflowAsset.Get(), FileId);

		if (OperationId.IsValid())
		{
			DownloadOperations.Add(OperationId, OutputName);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("AtlasJob[%s] failed to start download for '%s' (FileId=%s)"),
				*JobId.ToString(EGuidFormats::DigitsWithHyphens), *OutputName, *FileId);
			
			DownloadsCompleted++;
			DownloadFailures++;
		}
	}

	UpdateOverallProgress();
	CheckDownloadPhaseComplete();
}

void UAtlasJob::OnFileDownloadComplete(const FGuid& OperationId, const FString& FileId, const TArray<uint8>& FileData, const FString& OutputName)
{
	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] download complete for '%s': %d bytes"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *OutputName, FileData.Num());

	// Store the downloaded data
	DownloadedFiles.Add(OutputName, FileData);

	// Update the output value with the file data
	if (FAtlasValue* OutputValue = Outputs.Values.Find(OutputName))
	{
		OutputValue->FileData = FileData;
	}

	DownloadOperations.Remove(OperationId);
	DownloadsCompleted++;

	UpdateOverallProgress();
	CheckDownloadPhaseComplete();
}

void UAtlasJob::OnFileDownloadFailed(const FGuid& OperationId, const FString& ErrorMessage, int32 StatusCode, const FString& OutputName)
{
	UE_LOG(LogTemp, Warning, TEXT("AtlasJob[%s] download failed for '%s': %s (status %d)"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *OutputName, *ErrorMessage, StatusCode);

	// Don't fail the whole job for download failures - outputs still have FileIds
	DownloadOperations.Remove(OperationId);
	DownloadsCompleted++;
	DownloadFailures++;

	UpdateOverallProgress();
	CheckDownloadPhaseComplete();
}

void UAtlasJob::CheckDownloadPhaseComplete()
{
	if (bCancellationRequested || AtlasJobHelpers::IsTerminalState(State))
	{
		return;
	}

	if (DownloadsCompleted >= FilesToDownload.Num())
	{
		// Unbind from FileManager events
		if (FileManager)
		{
			FileManager->OnDownloadComplete.RemoveDynamic(this, &UAtlasJob::HandleDownloadComplete);
			FileManager->OnDownloadFailed.RemoveDynamic(this, &UAtlasJob::HandleDownloadFailed);
		}

		if (DownloadFailures > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("AtlasJob[%s] %d/%d downloads failed, completing anyway"),
				*JobId.ToString(EGuidFormats::DigitsWithHyphens), DownloadFailures, FilesToDownload.Num());
		}

		FinalizeCompletion();
	}
}

// ==================== Pipeline: Completion ====================

void UAtlasJob::FinalizeCompletion()
{
	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] finalizing completion"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens));

	CompleteWithOutputs(Outputs);
}

void UAtlasJob::UpdateOverallProgress()
{
	// Progress weights for each phase:
	// Initializing: 0% - 5%
	// Uploading: 5% - 40%
	// Executing: 40% - 70%
	// Downloading: 70% - 100%

	float NewProgress = 0.0f;

	switch (Phase)
	{
	case EAtlasJobPhase::Initializing:
		NewProgress = 0.02f;
		break;

	case EAtlasJobPhase::Uploading:
		if (FilesToUpload.Num() > 0)
		{
			float UploadProgress = static_cast<float>(UploadsCompleted) / FilesToUpload.Num();
			NewProgress = 0.05f + (UploadProgress * 0.35f);
		}
		else
		{
			NewProgress = 0.40f;
		}
		break;

	case EAtlasJobPhase::Executing:
		NewProgress = 0.50f; // Midpoint of execute phase
		break;

	case EAtlasJobPhase::Downloading:
		if (FilesToDownload.Num() > 0)
		{
			float DownloadProgress = static_cast<float>(DownloadsCompleted) / FilesToDownload.Num();
			NewProgress = 0.70f + (DownloadProgress * 0.30f);
		}
		else
		{
			NewProgress = 1.0f;
		}
		break;

	case EAtlasJobPhase::Done:
		NewProgress = 1.0f;
		break;
	}

	SetProgress(NewProgress);
}

// ==================== FileManager Event Handlers ====================
// These are UFUNCTIONs that handle the broadcast delegates

void UAtlasJob::HandleUploadComplete(const FGuid& OperationId, const FString& FileId, const FString& ContentHash)
{
	// Check if this operation belongs to this job
	if (FString* InputNamePtr = UploadOperations.Find(OperationId))
	{
		OnFileUploadComplete(OperationId, FileId, *InputNamePtr);
	}
}

void UAtlasJob::HandleUploadFailed(const FGuid& OperationId, const FString& ErrorMessage, int32 HttpStatusCode)
{
	// Check if this operation belongs to this job
	if (FString* InputNamePtr = UploadOperations.Find(OperationId))
	{
		OnFileUploadFailed(OperationId, ErrorMessage, HttpStatusCode, *InputNamePtr);
	}
}

void UAtlasJob::HandleDownloadComplete(const FGuid& OperationId, const FString& FileId, const TArray<uint8>& FileData)
{
	// Check if this operation belongs to this job
	if (FString* OutputNamePtr = DownloadOperations.Find(OperationId))
	{
		OnFileDownloadComplete(OperationId, FileId, FileData, *OutputNamePtr);
	}
}

void UAtlasJob::HandleDownloadFailed(const FGuid& OperationId, const FString& ErrorMessage, int32 HttpStatusCode)
{
	// Check if this operation belongs to this job
	if (FString* OutputNamePtr = DownloadOperations.Find(OperationId))
	{
		OnFileDownloadFailed(OperationId, ErrorMessage, HttpStatusCode, *OutputNamePtr);
	}
}

// ==================== State Management ====================

void UAtlasJob::SetState(EAtlasJobState NewState)
{
	if (State != NewState)
	{
		EAtlasJobState OldState = State;
		State = NewState;

		UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] state: %s -> %s"),
			*JobId.ToString(EGuidFormats::DigitsWithHyphens),
			*AtlasJobHelpers::StateToString(OldState),
			*AtlasJobHelpers::StateToString(NewState));

		OnStateChanged.Broadcast(this, NewState);
	}
}

void UAtlasJob::SetPhase(EAtlasJobPhase NewPhase)
{
	if (Phase != NewPhase)
	{
		Phase = NewPhase;

		UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] phase: %s"),
			*JobId.ToString(EGuidFormats::DigitsWithHyphens),
			*AtlasJobHelpers::PhaseToString(NewPhase));

		OnProgress.Broadcast(this, Progress, Phase);
	}
}

void UAtlasJob::SetProgress(float NewProgress)
{
	Progress = FMath::Clamp(NewProgress, 0.0f, 1.0f);
	OnProgress.Broadcast(this, Progress, Phase);
}

// ==================== Completion ====================

void UAtlasJob::CompleteWithOutputs(const FAtlasWorkflowOutputs& InOutputs)
{
	if (AtlasJobHelpers::IsTerminalState(State))
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasJob::CompleteWithOutputs() called on already-finished job"));
		return;
	}

	Outputs = InOutputs;
	CompletedAt = FDateTime::Now();  // Use local time for display consistency
	
	SetPhase(EAtlasJobPhase::Done);
	SetProgress(1.0f);
	SetState(EAtlasJobState::Completed);

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] completed successfully with %d outputs"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), Outputs.Values.Num());

	OnComplete.Broadcast(this, Outputs);
}

void UAtlasJob::FailWithError(const FAtlasError& InError)
{
	if (AtlasJobHelpers::IsTerminalState(State))
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasJob::FailWithError() called on already-finished job"));
		return;
	}

	Error = InError;
	CompletedAt = FDateTime::Now();  // Use local time for display consistency

	// Cleanup any pending resources
	CleanupExecuteRequest();
	CleanupStatusRequest();
	
	if (FileManager)
	{
		FileManager->OnUploadComplete.RemoveDynamic(this, &UAtlasJob::HandleUploadComplete);
		FileManager->OnUploadFailed.RemoveDynamic(this, &UAtlasJob::HandleUploadFailed);
		FileManager->OnDownloadComplete.RemoveDynamic(this, &UAtlasJob::HandleDownloadComplete);
		FileManager->OnDownloadFailed.RemoveDynamic(this, &UAtlasJob::HandleDownloadFailed);
	}

	// Determine final state based on whether this was a cancellation
	EAtlasJobState FinalState = bCancellationRequested ? EAtlasJobState::Cancelled : EAtlasJobState::Failed;

	SetPhase(EAtlasJobPhase::Done);
	SetState(FinalState);

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] failed: %s"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *Error.GetSummary());

	OnFailed.Broadcast(this, Error);
}

// ==================== Getters ====================

float UAtlasJob::GetDurationSeconds() const
{
	if (!StartedAt.GetTicks())
	{
		return 0.0f; // Not started
	}

	FDateTime EndTime = CompletedAt.GetTicks() ? CompletedAt : FDateTime::Now();
	return (EndTime - StartedAt).GetTotalSeconds();
}

FString UAtlasJob::GetStatusString() const
{
	switch (State)
	{
	case EAtlasJobState::Pending:
		return TEXT("Pending");

	case EAtlasJobState::Running:
		{
			FString PhaseStr = AtlasJobHelpers::PhaseToString(Phase);
			int32 ProgressPercent = FMath::RoundToInt(Progress * 100.0f);
			return FString::Printf(TEXT("Running - %s (%d%%)"), *PhaseStr, ProgressPercent);
		}

	case EAtlasJobState::Completed:
		return TEXT("Completed");

	case EAtlasJobState::Failed:
		return FString::Printf(TEXT("Failed: %s"), *Error.GetSummary());

	case EAtlasJobState::Cancelled:
		return TEXT("Cancelled");

	default:
		return TEXT("Unknown");
	}
}
