// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasJob.h"
#include "AtlasFileWork.h"
#include "Async/Async.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "AtlasArchiveSafety.h"
#include "UObject/StrongObjectPtr.h"
#include "AtlasWorkflowAsset.h"
#include "AtlasFileManager.h"
#include "AtlasHttpRequest.h"
#include "AtlasJsonObject.h"
#include "AtlasOutputManager.h"
#include "AtlasPlatformAuth.h"
#include "AtlasSDKSettings.h"
#include "Types/AtlasErrorTypes.h"
#include "AtlasScheduling.h"
#include "AtlasJobManager.h"
#include "AtlasHistoryManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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
	CreatedAt = FDateTime::UtcNow();  // UTC for newly written records
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

UAtlasWorkflowAsset* UAtlasJob::GetExecutionWorkflow() const
{
	return ExecutionWorkflow ? ExecutionWorkflow.Get() : WorkflowAsset.Get();
}

void UAtlasJob::SetFileManager(UAtlasFileManager* InFileManager)
{
	FileManager = InFileManager;
}

// ==================== Execution ====================

bool UAtlasJob::Execute()
{
	TStrongObjectPtr<UAtlasJob> KeepAlive(this);
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
	if (!IsValid(GetExecutionWorkflow()) || !GetExecutionWorkflow()->IsValid()
		|| GetExecutionWorkflow()->Schema.BaseUrl.IsEmpty() || GetExecutionWorkflow()->Schema.Version.IsEmpty())
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

	if (!FAtlasPlatformAuth::HasConfiguredApiKey())
	{
		FAtlasError AuthError;
		AuthError.Code = EAtlasErrorCode::Unauthorized;
		AuthError.Message = FAtlasPlatformAuth::GetConfigureApiKeyMessage();
		AuthError.Timestamp = FDateTime::UtcNow();
		FailWithError(AuthError);
		return false;
	}

	TArray<FString> ValidationErrors;
	if (!GetExecutionWorkflow()->GetSchema().ValidateInputs(Inputs, ValidationErrors))
	{
		FailWithError(FAtlasError(EAtlasErrorCode::ValidationError, FString::Join(ValidationErrors, TEXT("; "))));
		return false;
	}
	ExecutionWorkflow = NewObject<UAtlasWorkflowAsset>(this);
	ExecutionWorkflow->Schema = WorkflowAsset->Schema;
	ExecutionWorkflow->bIsValid = WorkflowAsset->bIsValid;
	FTCHARToUTF8 Key(*FAtlasPlatformAuth::GetResolvedApiKey());
	ExecutionCredentialScope = FMD5::HashBytes(reinterpret_cast<const uint8*>(Key.Get()), Key.Length());
	ExecutionRequestTimeout = UAtlasSDKSettings::Get()->RequestTimeoutSeconds;
	StartedMonotonic = FPlatformTime::Seconds();
	bExecutionStarted = true;
	StartedAt = FDateTime::UtcNow();  // UTC for newly written records

	// Read poll interval and max execution time from project settings
	if (const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get())
	{
		PollIntervalSeconds = Settings->StatusPollIntervalSeconds;
		MaxExecutionTimeSeconds = Settings->MaxExecutionTimeSeconds;
	}

	// The deadline includes queued preparation time. No UObject is accessed by the disk worker.
	StartExecutionTimeout();
	ArchiveInputFilesToRunFolder();
	return !IsFinished();
}

void UAtlasJob::ContinueExecution()
{
	if (!CheckExecutionCredentials()) return;
	if (bCancellationRequested || IsFinished()) return;
	// Transition to Running state
	SetState(EAtlasJobState::Running);
	if (IsFinished()) return;
	SetPhase(EAtlasJobPhase::Initializing);
	if (IsFinished()) return;
	SetProgress(0.0f);
	if (IsFinished()) return;

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] started execution for workflow '%s' (poll=%.1fs, timeout=%.0fs)"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *WorkflowName, PollIntervalSeconds, MaxExecutionTimeSeconds);



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

}

void UAtlasJob::Cancel()
{
	if (bFinishing || AtlasJobHelpers::IsTerminalState(State)) return;
	bCancellationRequested = true;
	FAtlasError CancelError;
	CancelError.Code = EAtlasErrorCode::Cancelled;
	CancelError.Message = State == EAtlasJobState::Pending
		? TEXT("Job was cancelled before execution started") : TEXT("Job was cancelled during execution");
	CancelError.Timestamp = FDateTime::UtcNow();
	FailWithError(CancelError);
}

// ==================== Pipeline: Upload Phase ====================

void UAtlasJob::ArchiveInputFilesToRunFolder()
{
	UAtlasOutputManager* OutputManager = NewObject<UAtlasOutputManager>(this);
	const FAtlasJobFolderInfo Folder = OutputManager->GetJobFolderInfo(this);
	struct FInputCopy { FString Name, Source, Destination; TArray<uint8> Bytes; };
	TArray<FInputCopy> Copies;
	for (const auto& Pair : Inputs.Values)
	{
		const FAtlasValue& Value = Pair.Value;
		if (!Value.IsFileType() || !Value.FileId.IsEmpty()) continue;
		FString Extension = FPaths::GetExtension(Value.FilePath.IsEmpty() ? Value.FileName : Value.FilePath).ToLower();
		bool bSafeExtension = !Extension.IsEmpty() && Extension.Len() <= 12;
		for (TCHAR Ch : Extension) bSafeExtension &= FChar::IsAlnum(Ch);
		if (!bSafeExtension) Extension = TEXT("bin");
		const FString Destination = Folder.InputsDiskPath / (TEXT("Input_") + AtlasArchive::Component(Pair.Key) + TEXT(".") + Extension);
		if (!AtlasArchive::IsContained(UAtlasSDKSettings::Get()->GetOutputFolderPath(), Destination))
		{
			FailWithError(FAtlasError(EAtlasErrorCode::FileError, TEXT("Unsafe input archive destination")));
			return;
		}
		Copies.Add({Pair.Key, Value.FilePath, Destination, Value.FileData});
	}
	if (Copies.IsEmpty()) { ContinueExecution(); return; }
	const TWeakObjectPtr<UAtlasJob> WeakThis(this);
	AtlasFileWork::Enqueue([WeakThis, Copies = MoveTemp(Copies)]() mutable
	{
		bool bSuccess = true;
		for (FInputCopy& Copy : Copies)
		{
			if (!Copy.Source.IsEmpty() && FPaths::FileExists(Copy.Source))
			{
				Copy.Bytes.Empty();
				if (!FFileHelper::LoadFileToArray(Copy.Bytes, *Copy.Source)) { bSuccess = false; break; }
			}
			if (Copy.Bytes.IsEmpty() || !AtlasArchive::WriteBytes(Copy.Destination, Copy.Bytes.GetData(), Copy.Bytes.Num())) { bSuccess = false; break; }
			Copy.Bytes.Empty();
		}
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bSuccess, Copies = MoveTemp(Copies)]()
		{
			TStrongObjectPtr<UAtlasJob> Job(WeakThis.Get());
			if (!Job.IsValid() || Job->bCancellationRequested || Job->IsFinished()) return;
			if (!bSuccess) { Job->FailWithError(FAtlasError(EAtlasErrorCode::FileError, TEXT("Could not archive input files"))); return; }
			for (const FInputCopy& Copy : Copies)
			{
				if (FAtlasValue* Value = Job->Inputs.Values.Find(Copy.Name))
				{
					Value->FilePath = Copy.Destination;
					Value->FileName = FPaths::GetCleanFilename(Copy.Destination);
					Value->FileData.Empty();
				}
			}
			Job->ContinueExecution();
		});
	});
}

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
			Value.Type == EAtlasValueType::Audio ||
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
	if (bCancellationRequested || AtlasJobHelpers::IsTerminalState(State)) return;

	SetPhase(EAtlasJobPhase::Uploading);
	// Phase listeners can cancel the job synchronously.
	if (bCancellationRequested || AtlasJobHelpers::IsTerminalState(State)) return;
	UploadsCompleted = 0;
	UploadFailures = 0;
	UploadOperations.Empty();

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] starting upload phase with %d files"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), FilesToUpload.Num());

	// Bind to FileManager events
	FileManager->OnUploadComplete.AddDynamic(this, &UAtlasJob::HandleUploadComplete);
	FileManager->OnUploadFailed.AddDynamic(this, &UAtlasJob::HandleUploadFailed);

	{
		TGuardValue<bool> StartingUploads(bStartingUploads, true);
		// Start uploads in parallel, but stop dispatching after a synchronous failure/cancellation.
		for (const auto& Pair : FilesToUpload)
		{
			if (bCancellationRequested || AtlasJobHelpers::IsTerminalState(State)) return;
			const FString InputName = Pair.Key;
			const FString FilePath = Pair.Value;
			const FGuid OperationId = FGuid::NewGuid();
			// Register BEFORE the call: request creation/ProcessRequest can fail synchronously.
			UploadOperations.Add(OperationId, InputName);
			const FGuid StartedOperation = FileManager->UploadFileWithOperationId(GetExecutionWorkflow(), FilePath, OperationId);
			if (!StartedOperation.IsValid() && !AtlasJobHelpers::IsTerminalState(State))
			{
				OnFileUploadFailed(OperationId,
					TEXT("Upload could not start. Check the workflow and that the input file exists, is readable, and is not empty."),
					0, InputName);
				return;
			}
		}
	}

	if (bCancellationRequested || AtlasJobHelpers::IsTerminalState(State)) return;
	UpdateOverallProgress();
	CheckUploadPhaseComplete();
}

void UAtlasJob::OnFileUploadComplete(const FGuid& OperationId, const FString& FileId, FString InputName)
{
	if (bCancellationRequested || AtlasJobHelpers::IsTerminalState(State)) return;
	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] upload complete for '%s': FileId=%s"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *InputName, *FileId);

	UploadedFileIds.Add(InputName, FileId);
	UploadOperations.Remove(OperationId);
	UploadsCompleted++;

	UpdateOverallProgress();
	CheckUploadPhaseComplete();
}

void UAtlasJob::OnFileUploadFailed(const FGuid& OperationId, const FString& ErrorMessage, int32 StatusCode, FString InputName)
{
	// InputName owns its characters: callers may pass a value from UploadOperations,
	// whose storage is destroyed by Remove below.
	if (bCancellationRequested || AtlasJobHelpers::IsTerminalState(State)) return;
	UE_LOG(LogTemp, Error, TEXT("AtlasJob[%s] upload failed for '%s': %s (status %d)"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *InputName, TEXT("[details omitted]"), StatusCode);

	UploadOperations.Remove(OperationId);
	UploadsCompleted++;
	UploadFailures++;

	// Fail immediately on first upload error
	FAtlasError UploadError = FAtlasError::FromHttpStatus(StatusCode, FileManager ? FileManager->GetTransferResponse(OperationId) : FString());
	const FString BackendMessage = UploadError.RawResponse.IsEmpty() ? FString() : UploadError.Message;
	if (StatusCode == 401)
	{
		UploadError.Code = EAtlasErrorCode::Unauthorized;
		UploadError.Message = FString::Printf(
			TEXT("Failed to upload '%s': %s"),
			*InputName,
			*FAtlasPlatformAuth::GetConfigureApiKeyMessage());
	}
	else
	{
		UploadError.Code = StatusCode > 0 ? FAtlasError::FromHttpStatus(StatusCode, FString()).Code : EAtlasErrorCode::UploadFailed;
		UploadError.Message = FString::Printf(
			TEXT("Failed to upload '%s': %s"),
			*InputName,
			*FAtlasPlatformAuth::GetHttpFailureMessage(StatusCode, ErrorMessage));
	}
	if (!BackendMessage.IsEmpty()) UploadError.Message = FString::Printf(TEXT("Failed to upload '%s': %s"), *InputName, *BackendMessage);
	UploadError.HttpStatusCode = StatusCode;
	UploadError.Timestamp = FDateTime::UtcNow();
	FailWithError(UploadError);
}

void UAtlasJob::CheckUploadPhaseComplete()
{
	if (bStartingUploads || Phase != EAtlasJobPhase::Uploading
		|| bCancellationRequested || AtlasJobHelpers::IsTerminalState(State))
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
	if (!CheckExecutionCredentials()) return;
	if (bCancellationRequested) return;

	SetPhase(EAtlasJobPhase::Executing);
	UpdateOverallProgress();
	if (bCancellationRequested || IsFinished()) return;

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
	FString ExecuteUrl = GetExecutionWorkflow()->GetExecuteUrl();

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] POSTing to: %s"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), TEXT("[destination omitted]"));

	// Create and configure the request
	ExecuteRequest = UAtlasHttpRequest::CreateRequest();
	ExecuteRequest->AddToRoot(); // Prevent GC
	ExecuteRequest->SetURL(ExecuteUrl);
	ExecuteRequest->SetVerb(EAtlasHttpVerb::POST);
	ExecuteRequest->SetContentType(EAtlasHttpContentType::JSON);
	ExecuteRequest->SetRequestBodyJson(Payload);

	FAtlasPlatformAuth::ApplyPlatformRequestSettings(ExecuteRequest);
	ExecuteRequest->SetTimeout(ExecutionRequestTimeout);

	// Bind callbacks
	ExecuteRequest->OnRequestComplete.AddDynamic(this, &UAtlasJob::OnExecuteRequestComplete);
	ExecuteRequest->OnRequestFailed.AddDynamic(this, &UAtlasJob::OnExecuteRequestFailed);

	// Mark before dispatch, which may fail synchronously after the server accepted work.
	bExecuteRequestSent = true;
	// Execute
#if WITH_DEV_AUTOMATION_TESTS
	if (ExecuteRequestDispatcherForTests)
	{
		ExecuteRequestDispatcherForTests(ExecuteRequest);
		return;
	}
#endif
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
		case EAtlasValueType::Audio:
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
			{
				TSharedPtr<FJsonValue> Json;
				if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Value.JsonValue), Json) || !Json.IsValid()
					|| (Json->Type != EJson::Object && Json->Type != EJson::Array)) return nullptr;
				Payload->GetJsonObject()->SetField(InputName, Json);
			}
			break;

		default:
			UE_LOG(LogTemp, Warning, TEXT("AtlasJob: Unknown value type for input '%s'"), *InputName);
			break;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] built payload: %s"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), TEXT("[payload omitted]"));

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
		const FString ResponseBody = Request->GetResponseContent();
		FAtlasError ExecError = FAtlasError::FromHttpStatus(StatusCode, ResponseBody);
		ExecError.HttpStatusCode = StatusCode;
		ExecError.Timestamp = FDateTime::UtcNow();

		if (StatusCode == 401 || StatusCode == 403)
		{
			ExecError.Message = FAtlasPlatformAuth::GetHttpFailureMessage(StatusCode, ExecError.Message);
		}
		else if (ResponseJson)
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

		if (ExecError.Code == EAtlasErrorCode::None || ExecError.Code == EAtlasErrorCode::Unknown)
		{
			ExecError.Code = EAtlasErrorCode::ExecutionFailed;
		}

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
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), TEXT("[details omitted]"), StatusCode);

	FAtlasError ExecError = FAtlasError::FromHttpStatus(StatusCode, Request ? Request->GetResponseContent() : FString());
	ExecError.HttpStatusCode = StatusCode;
	ExecError.Timestamp = FDateTime::UtcNow();

	if (StatusCode == 401 || StatusCode == 403)
	{
		ExecError.Message = FAtlasPlatformAuth::GetHttpFailureMessage(StatusCode, ErrorMessage);
	}
	else
	{
		if (StatusCode == 0) ExecError.Code = EAtlasErrorCode::NetworkError;
		if (ExecError.RawResponse.IsEmpty()) ExecError.Message = ErrorMessage;
	}

	CleanupExecuteRequest();
	FailWithError(ExecError);
}

void UAtlasJob::CleanupExecuteRequest()
{
	if (ExecuteRequest)
	{
		ExecuteRequest->OnRequestComplete.RemoveDynamic(this, &UAtlasJob::OnExecuteRequestComplete);
		ExecuteRequest->OnRequestFailed.RemoveDynamic(this, &UAtlasJob::OnExecuteRequestFailed);
		ExecuteRequest->CancelRequest();
		ExecuteRequest->RemoveFromRoot();
		ExecuteRequest = nullptr;
	}
}

// ==================== Pipeline: Polling Phase ====================

void UAtlasJob::BeginDestroy()
{
	AtlasScheduling::Cancel(ExecutionTimeoutHandle);
	CleanupStatusRequest();
	CleanupExecuteRequest();
	Super::BeginDestroy();
}

void UAtlasJob::StartExecutionTimeout()
{
	AtlasScheduling::Cancel(ExecutionTimeoutHandle);
	if (FMath::IsFinite(MaxExecutionTimeSeconds) && MaxExecutionTimeSeconds > 0.0f)
	{
		ExecutionTimeoutHandle = AtlasScheduling::Schedule(this, MaxExecutionTimeSeconds, [](UAtlasJob* Job)
		{
			Job->ExecutionTimeoutHandle.Reset();
			Job->OnExecutionTimeout();
		});
	}
}

void UAtlasJob::StartPollingPhase(const FString& InExecutionId)
{
	if (bCancellationRequested || IsFinished()) return;
	ExecutionId = InExecutionId;
	ConsecutivePollFailures = 0;
	// Persist the accepted execution as soon as its identity is known, not only on completion.
	if (UAtlasJobManager* Manager = GetTypedOuter<UAtlasJobManager>())
	{
		Manager->GetHistoryManager()->SaveJobToHistory(this);
	}
	SchedulePoll(PollIntervalSeconds);
}

void UAtlasJob::SchedulePoll(float DelaySeconds)
{
	AtlasScheduling::Cancel(StatusPollTimerHandle);
	if (bCancellationRequested || IsFinished()) return;
	const float SafeDelay = FMath::IsFinite(DelaySeconds) ? FMath::Max(0.1f, DelaySeconds) : 5.0f;
	StatusPollTimerHandle = AtlasScheduling::Schedule(this, SafeDelay, [](UAtlasJob* Job)
	{
		Job->StatusPollTimerHandle.Reset();
		Job->PollStatus();
	});
}

void UAtlasJob::HandlePollError(const FAtlasError& PollError)
{
	const int32 StatusCode = PollError.HttpStatusCode;
	const bool bTransient = StatusCode == 0 || StatusCode == 408 || StatusCode == 429
		|| (StatusCode >= 500 && StatusCode <= 599);
	if (bTransient && ++ConsecutivePollFailures <= 5 && !ExecutionId.IsEmpty())
	{
		const float BaseDelay = FMath::IsFinite(PollIntervalSeconds) ? FMath::Max(0.1f, PollIntervalSeconds) : 5.0f;
		const float Delay = FMath::Min(30.0f, BaseDelay * FMath::Pow(2.0f, ConsecutivePollFailures - 1));
		UE_LOG(LogTemp, Warning, TEXT("AtlasJob: Status check failed; retrying the same execution in %.1fs (%d/5)"), Delay, ConsecutivePollFailures);
		SchedulePoll(Delay);
		return;
	}
	FailWithError(PollError);
}

void UAtlasJob::PollStatus()
{
	if (!CheckExecutionCredentials()) return;
	AtlasScheduling::Cancel(StatusPollTimerHandle);
	if (StatusRequest) return; // Never overlap status requests.
	if (bCancellationRequested || AtlasJobHelpers::IsTerminalState(State))
	{
		return;
	}

	if (!IsValid(GetExecutionWorkflow()))
	{
		FAtlasError PollError;
		PollError.Code = EAtlasErrorCode::ValidationError;
		PollError.Message = TEXT("WorkflowAsset became invalid during polling");
		PollError.Timestamp = FDateTime::UtcNow();
		FailWithError(PollError);
		return;
	}

	FString StatusUrl = GetExecutionWorkflow()->GetStatusUrl(ExecutionId);

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] polling status: %s"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), TEXT("[details omitted]"));

	// Create status request
	StatusRequest = UAtlasHttpRequest::CreateRequest();
	StatusRequest->AddToRoot();
	StatusRequest->SetURL(StatusUrl);
	StatusRequest->SetVerb(EAtlasHttpVerb::GET);

	FAtlasPlatformAuth::ApplyPlatformRequestSettings(StatusRequest);
	StatusRequest->SetTimeout(ExecutionRequestTimeout);

	// Bind callbacks
	StatusRequest->OnRequestComplete.AddDynamic(this, &UAtlasJob::OnStatusPollComplete);
	StatusRequest->OnRequestFailed.AddDynamic(this, &UAtlasJob::OnStatusPollFailed);

#if WITH_DEV_AUTOMATION_TESTS
	if (StatusRequestDispatcherForTests)
	{
		StatusRequestDispatcherForTests(StatusRequest);
		return;
	}
#endif
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
		const FString ResponseBody = Request->GetResponseContent();
		FAtlasError ExecError = FAtlasError::FromHttpStatus(StatusCode, ResponseBody);
		ExecError.HttpStatusCode = StatusCode;
		ExecError.Timestamp = FDateTime::UtcNow();

		if (StatusCode == 401 || StatusCode == 403)
		{
			ExecError.Message = FAtlasPlatformAuth::GetHttpFailureMessage(StatusCode, ExecError.Message);
		}
		else if (ExecError.Message.IsEmpty())
		{
			ExecError.Message = FString::Printf(TEXT("Status poll failed with status %d"), StatusCode);
		}

		if (ExecError.Code == EAtlasErrorCode::None || ExecError.Code == EAtlasErrorCode::Unknown)
		{
			ExecError.Code = EAtlasErrorCode::ExecutionFailed;
		}

		HandlePollError(ExecError);
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

	ConsecutivePollFailures = 0;
	FString Status = ResponseJson->GetStringField(TEXT("status"));
	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] status: %s"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *Status);

	if (Status == TEXT("completed"))
	{
		// Parse outputs from result
		if (!ParseStatusResponse(ResponseJson))
		{
			FailWithError(FAtlasError(EAtlasErrorCode::ExecutionFailed, TEXT("Completed response is missing required outputs or contains invalid output types")));
			return;
		}

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
		SchedulePoll(PollIntervalSeconds);
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

	UE_LOG(LogTemp, Warning, TEXT("AtlasJob[%s] status poll failed: %s (status %d)"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), TEXT("[details omitted]"), StatusCode);

	CleanupStatusRequest();

	FAtlasError ExecError = FAtlasError::FromHttpStatus(StatusCode, Request ? Request->GetResponseContent() : FString());
	ExecError.HttpStatusCode = StatusCode;
	ExecError.Timestamp = FDateTime::UtcNow();

	if (StatusCode == 401 || StatusCode == 403)
	{
		ExecError.Message = FAtlasPlatformAuth::GetHttpFailureMessage(StatusCode, ErrorMessage);
	}
	else
	{
		if (StatusCode == 0) ExecError.Code = EAtlasErrorCode::NetworkError;
		if (ExecError.RawResponse.IsEmpty()) ExecError.Message = ErrorMessage;
	}

	HandlePollError(ExecError);
}

void UAtlasJob::CleanupStatusRequest()
{
	if (StatusRequest)
	{
		StatusRequest->OnRequestComplete.RemoveDynamic(this, &UAtlasJob::OnStatusPollComplete);
		StatusRequest->OnRequestFailed.RemoveDynamic(this, &UAtlasJob::OnStatusPollFailed);
		StatusRequest->CancelRequest();
		StatusRequest->RemoveFromRoot();
		StatusRequest = nullptr;
	}

	AtlasScheduling::Cancel(StatusPollTimerHandle);
}

bool UAtlasJob::ParseStatusResponse(UAtlasJsonObject* ResponseJson)
{
	if (!ResponseJson || !IsValid(GetExecutionWorkflow())) return false;
	const TSharedPtr<FJsonObject>* Result = nullptr;
	const TSharedPtr<FJsonObject>* JsonOutputs = nullptr;
	if (!ResponseJson->GetJsonObject()->TryGetObjectField(TEXT("result"), Result) || !Result || !Result->IsValid()
		|| !(*Result)->TryGetObjectField(TEXT("outputs"), JsonOutputs) || !JsonOutputs || !JsonOutputs->IsValid()) return false;
	FAtlasWorkflowOutputs Parsed;
	TMap<FString, FString> Downloads;
	for (const FAtlasParameterDef& Def : GetExecutionWorkflow()->GetSchema().Outputs)
	{
		const TSharedPtr<FJsonValue>* Field = (*JsonOutputs)->Values.Find(Def.Name);
		if (!Field || !Field->IsValid()) return false;
		const auto& Json = *Field;
		FAtlasValue Value;
		Value.Type = Def.Type;
		switch (Def.Type)
		{
		case EAtlasValueType::String:
			if (!Json->TryGetString(Value.StringValue)) return false;
			break;
		case EAtlasValueType::Number:
		case EAtlasValueType::Integer:
		{
			double Number;
			if (!Json->TryGetNumber(Number) || !FMath::IsFinite(Number)) return false;
			if (Def.Type == EAtlasValueType::Integer)
			{
				if (Number < MIN_int32 || Number > MAX_int32 || FMath::FloorToDouble(Number) != Number) return false;
				Value.IntValue = static_cast<int32>(Number);
			}
			else { Value.NumberValue = Number; if (!FMath::IsFinite(Value.NumberValue)) return false; }
			break;
		}
		case EAtlasValueType::Boolean:
			if (!Json->TryGetBool(Value.BoolValue)) return false;
			break;
		case EAtlasValueType::Json:
			if (Json->Type != EJson::Object && Json->Type != EJson::Array) return false;
			if (!FJsonSerializer::Serialize(Json, TEXT(""), TJsonWriterFactory<>::Create(&Value.JsonValue))) return false;
			break;
		case EAtlasValueType::Image:
		case EAtlasValueType::Mesh:
		case EAtlasValueType::Audio:
		case EAtlasValueType::File:
		case EAtlasValueType::FileId:
			if (!Json->TryGetString(Value.FileId) || Value.FileId.IsEmpty()) return false;
			{
				FString Extension = Def.Format.ToLower();
				Extension.RemoveFromStart(TEXT("."));
				bool bSafe = !Extension.IsEmpty() && Extension.Len() <= 12;
				for (TCHAR Ch : Extension) bSafe &= FChar::IsAlnum(Ch);
				Value.FileName = TEXT("Output_") + AtlasArchive::Component(Def.Name) + TEXT(".") + (bSafe ? Extension : TEXT("bin"));
			}
			Downloads.Add(Def.Name, Value.FileId);
			break;
		default: return false;
		}
		Parsed.Values.Add(Def.Name, MoveTemp(Value));
	}
	Outputs = MoveTemp(Parsed);
	FilesToDownload = MoveTemp(Downloads);
	return true;
}

// ==================== Pipeline: Download Phase ====================

void UAtlasJob::StartDownloadPhase()
{
	if (!CheckExecutionCredentials()) return;
	if (bCancellationRequested) return;

	SetPhase(EAtlasJobPhase::Downloading);
	if (bCancellationRequested || IsFinished()) return;
	DownloadsCompleted = 0;
	DownloadFailures = 0;
	DownloadOperations.Empty();


	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] starting download phase with %d files"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), FilesToDownload.Num());

	// Bind to FileManager download events
	FileManager->OnDownloadComplete.AddDynamic(this, &UAtlasJob::HandleDownloadComplete);
	FileManager->OnDownloadFailed.AddDynamic(this, &UAtlasJob::HandleDownloadFailed);

	{
		TGuardValue<bool> StartingDownloads(bStartingDownloads, true);
		for (const auto& Pair : FilesToDownload)
		{
			if (bCancellationRequested || IsFinished()) return;
			const FString OutputName = Pair.Key;
			const FGuid OperationId = FGuid::NewGuid();
			// Record ownership before any synchronous failure/completion can call back.
			DownloadOperations.Add(OperationId, OutputName);
			const FGuid Started = FileManager->DownloadFileWithOperationId(GetExecutionWorkflow(), Pair.Value, OperationId);
			if (!Started.IsValid() && DownloadOperations.Contains(OperationId))
			{
				OnFileDownloadFailed(OperationId, TEXT("Download could not start"), 0, OutputName);
			}
		}
	}

	UpdateOverallProgress();
	CheckDownloadPhaseComplete();
}

void UAtlasJob::OnFileDownloadComplete(const FGuid& OperationId, const FString& FileId, const TArray<uint8>& FileData, const FString& OutputName)
{
	if (bFinishing || bCancellationRequested || IsFinished()) return;
	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] download complete for '%s': %d bytes"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *OutputName, FileData.Num());

	// Store the downloaded data


	// Update the output value with the file data
	if (FAtlasValue* OutputValue = Outputs.Values.Find(OutputName))
	{
		OutputValue->FileData = FileData;
		FString Extension = FileManager->GetDownloadExtension(OperationId);
		// Recognizable bytes take precedence over MIME/schema labels.
		if (FileData.Num() >= 12)
		{
			if (FileData[0] == 0x89 && FileData[1] == 'P' && FileData[2] == 'N' && FileData[3] == 'G') Extension = TEXT("png");
			else if (FileData[0] == 0xFF && FileData[1] == 0xD8 && FileData[2] == 0xFF) Extension = TEXT("jpg");
			else if (FMemory::Memcmp(FileData.GetData(), "RIFF", 4) == 0 && FMemory::Memcmp(FileData.GetData() + 8, "WAVE", 4) == 0) Extension = TEXT("wav");
			else if (FMemory::Memcmp(FileData.GetData(), "OggS", 4) == 0) Extension = TEXT("ogg");
			else if (FMemory::Memcmp(FileData.GetData(), "glTF", 4) == 0) Extension = TEXT("glb");
		}
		if (!Extension.IsEmpty()) OutputValue->FileName = TEXT("Output_") + AtlasArchive::Component(OutputName) + TEXT(".") + Extension;
	}

	DownloadOperations.Remove(OperationId);
	DownloadsCompleted++;

	UpdateOverallProgress();
	CheckDownloadPhaseComplete();
}

void UAtlasJob::OnFileDownloadFailed(const FGuid& OperationId, const FString& ErrorMessage, int32 StatusCode, const FString& OutputName)
{
	if (bFinishing || bCancellationRequested || IsFinished()) return;
	UE_LOG(LogTemp, Warning, TEXT("AtlasJob[%s] download failed for '%s': %s (status %d)"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), *OutputName, TEXT("[details omitted]"), StatusCode);

	OutputWarnings.Add(FString::Printf(TEXT("Download %s: %s"), *OutputName, *ErrorMessage));
	// Don't fail the whole job for download failures - outputs still have FileIds
	DownloadOperations.Remove(OperationId);
	DownloadsCompleted++;
	DownloadFailures++;

	UpdateOverallProgress();
	CheckDownloadPhaseComplete();
}

void UAtlasJob::CheckDownloadPhaseComplete()
{
	if (bStartingDownloads || bFinishing || bCancellationRequested || AtlasJobHelpers::IsTerminalState(State))
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
	if (bSavingOutputs || bCancellationRequested || IsFinished()) return;
	bOutputsArchiveAttempted = true;
	if (!UAtlasSDKSettings::Get()->bAutoSaveOutputFiles) { CompleteWithOutputs(Outputs); return; }
	struct FOutputWrite { FString Name, Path; TArray<uint8> Bytes; bool bSaved = false; };
	TArray<FOutputWrite> Writes;
	UAtlasOutputManager* OutputManager = NewObject<UAtlasOutputManager>(this);
	const FAtlasJobFolderInfo Folder = OutputManager->GetJobFolderInfo(this);
	const FString Root = UAtlasSDKSettings::Get()->GetOutputFolderPath();
	for (auto& Pair : Outputs.Values)
	{
		if ((!Pair.Value.IsFileType() && !Pair.Value.IsFileId()) || Pair.Value.FileData.IsEmpty()) continue;
		FString Extension = FPaths::GetExtension(Pair.Value.FileName).ToLower();
		if (Extension.IsEmpty() || Extension.Len() > 12 || !Extension.GetCharArray().FilterByPredicate([](TCHAR Ch) { return Ch && !FChar::IsAlnum(Ch); }).IsEmpty()) Extension = TEXT("bin");
		const FString Path = Folder.OutputsDiskPath / (TEXT("Output_") + AtlasArchive::Component(Pair.Key) + TEXT(".") + Extension);
		if (!Folder.IsValid() || !AtlasArchive::IsContained(Root, Path)) { OutputWarnings.Add(TEXT("Unsafe output archive path")); continue; }
		Writes.Add({ Pair.Key, Path, MoveTemp(Pair.Value.FileData), false });
	}
	if (Writes.IsEmpty()) { CompleteWithOutputs(Outputs); return; }
	bSavingOutputs = true;
	TWeakObjectPtr<UAtlasJob> WeakJob(this);
	AtlasFileWork::Enqueue([WeakJob, Writes = MoveTemp(Writes)]() mutable
	{
		for (auto& Write : Writes) Write.bSaved = AtlasArchive::WriteBytes(Write.Path, Write.Bytes.GetData(), Write.Bytes.Num());
		AsyncTask(ENamedThreads::GameThread, [WeakJob, Writes = MoveTemp(Writes)]() mutable
		{
			TStrongObjectPtr<UAtlasJob> Job(WeakJob.Get());
			if (!Job.IsValid()) return;
			Job->bSavingOutputs = false;
			for (auto& Write : Writes)
			{
				if (auto* Value = Job->Outputs.Values.Find(Write.Name))
				{
					Value->FileData = MoveTemp(Write.Bytes);
					if (Write.bSaved) { Value->FilePath = Write.Path; Value->FileName = FPaths::GetCleanFilename(Write.Path); }
					else Job->OutputWarnings.Add(FString::Printf(TEXT("Could not archive output '%s'; bytes remain available"), *Write.Name));
				}
			}
			if (!Job->bCancellationRequested && !Job->IsFinished()) Job->CompleteWithOutputs(Job->Outputs);
		});
	});
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

// ==================== Execution Timeout ====================

void UAtlasJob::OnExecutionTimeout()
{
	if (bCancellationRequested || AtlasJobHelpers::IsTerminalState(State))
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("AtlasJob[%s] timed out after %.0f seconds"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), MaxExecutionTimeSeconds);

	FAtlasError TimeoutError;
	TimeoutError.Code = EAtlasErrorCode::Timeout;
	TimeoutError.Message = FString::Printf(
		TEXT("Job exceeded maximum execution time of %.0f seconds"), MaxExecutionTimeSeconds);
	TimeoutError.Timestamp = FDateTime::UtcNow();
	FailWithError(TimeoutError);
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
	if (bFinishing || AtlasJobHelpers::IsTerminalState(State))
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasJob::CompleteWithOutputs() called on already-finished job"));
		return;
	}
	TStrongObjectPtr<UAtlasJob> KeepAlive(this);
	TGuardValue<bool> Finishing(bFinishing, true);


	// Clear execution timeout timer
	AtlasScheduling::Cancel(ExecutionTimeoutHandle);

	Outputs = InOutputs;
	FinishedMonotonic = FPlatformTime::Seconds();
	CompletedAt = FDateTime::UtcNow();  // UTC for newly written records
	
	SetPhase(EAtlasJobPhase::Done);
	SetProgress(1.0f);
	SetState(EAtlasJobState::Completed);

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] completed successfully with %d outputs"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), Outputs.Values.Num());

	OnComplete.Broadcast(this, Outputs);
}

void UAtlasJob::FailWithError(const FAtlasError& InError)
{
	if (bFinishing || AtlasJobHelpers::IsTerminalState(State))
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasJob::FailWithError() called on already-finished job"));
		return;
	}
	TStrongObjectPtr<UAtlasJob> KeepAlive(this);
	TGuardValue<bool> Finishing(bFinishing, true);


	Error = InError;
	if (bExecuteRequestSent && Error.IsRetryable())
	{
		Error.Message += ExecutionId.IsEmpty()
			? TEXT(" Submission outcome is uncertain; automatic resubmission is disabled to avoid duplicate execution.")
			: TEXT(" Remote execution may still be running; automatic resubmission is disabled.");
	}
	FinishedMonotonic = FPlatformTime::Seconds();
	CompletedAt = FDateTime::UtcNow();  // UTC for newly written records

	// Cleanup any pending resources
	CleanupExecuteRequest();
	CleanupStatusRequest();

	// Clear execution timeout timer
	AtlasScheduling::Cancel(ExecutionTimeoutHandle);
	
	if (FileManager)
	{
		FileManager->OnUploadComplete.RemoveDynamic(this, &UAtlasJob::HandleUploadComplete);
		FileManager->OnUploadFailed.RemoveDynamic(this, &UAtlasJob::HandleUploadFailed);
		FileManager->OnDownloadComplete.RemoveDynamic(this, &UAtlasJob::HandleDownloadComplete);
		FileManager->OnDownloadFailed.RemoveDynamic(this, &UAtlasJob::HandleDownloadFailed);
	}

	// Detach this job before cancellation events are broadcast by the shared file manager.
	TArray<FGuid> UploadIds, DownloadIds;
	UploadOperations.GetKeys(UploadIds);
	DownloadOperations.GetKeys(DownloadIds);
	UploadOperations.Empty();
	DownloadOperations.Empty();
	if (FileManager)
	{
		for (const FGuid& Id : UploadIds) FileManager->CancelUpload(Id);
		for (const FGuid& Id : DownloadIds) FileManager->CancelDownload(Id);
	}

	// Determine final state based on whether this was a cancellation
	EAtlasJobState FinalState = bCancellationRequested ? EAtlasJobState::Cancelled : EAtlasJobState::Failed;

	SetPhase(EAtlasJobPhase::Done);
	SetState(FinalState);

	UE_LOG(LogTemp, Log, TEXT("AtlasJob[%s] failed: %s"),
		*JobId.ToString(EGuidFormats::DigitsWithHyphens), TEXT("[details available on job]"));

	OnFailed.Broadcast(this, Error);
}

// ==================== Getters ====================

float UAtlasJob::GetDurationSeconds() const
{
	if (StartedMonotonic >= 0.0) return FMath::Max(0.0, (FinishedMonotonic >= 0.0 ? FinishedMonotonic : FPlatformTime::Seconds()) - StartedMonotonic);
	if (!StartedAt.GetTicks())
	{
		return 0.0f; // Not started
	}

	FDateTime EndTime = CompletedAt.GetTicks() ? CompletedAt : FDateTime::UtcNow();
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

bool UAtlasJob::CheckExecutionCredentials()
{
	if (ExecutionCredentialScope.IsEmpty()) return true; // Manually constructed/test jobs have no execution snapshot.
	FTCHARToUTF8 Key(*FAtlasPlatformAuth::GetResolvedApiKey());
	if (ExecutionCredentialScope == FMD5::HashBytes(reinterpret_cast<const uint8*>(Key.Get()), Key.Length())) return true;
	FailWithError(FAtlasError(EAtlasErrorCode::Unauthorized, TEXT("Workspace credentials changed during this run; start a new job in the selected workspace")));
	return false;
}
