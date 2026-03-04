// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasHistoryManager.h"
#include "AtlasJob.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

UAtlasHistoryManager::UAtlasHistoryManager()
{
}

// ==================== Save Operations ====================

bool UAtlasHistoryManager::SaveJobToHistory(UAtlasJob* Job)
{
	if (!IsValid(Job))
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasHistoryManager: Cannot save null job to history"));
		return false;
	}

	FAtlasJobHistoryRecord Record = JobToRecord(Job);
	return SaveRecord(Record);
}

bool UAtlasHistoryManager::SaveJobToHistoryWithRecord(UAtlasJob* Job, FAtlasJobHistoryRecord& OutRecord)
{
	if (!IsValid(Job))
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasHistoryManager: Cannot save null job to history"));
		return false;
	}

	OutRecord = JobToRecord(Job);
	return SaveRecord(OutRecord);
}

bool UAtlasHistoryManager::SaveRecord(const FAtlasJobHistoryRecord& Record)
{
	if (!Record.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasHistoryManager: Cannot save invalid record"));
		return false;
	}

	EnsureHistoryDirectoryExists();

	FString FilePath = GetHistoryFilePath(Record.ApiId);

	// Load existing records
	TArray<FAtlasJobHistoryRecord> Records = LoadHistoryFile(FilePath);

	// Check if record already exists (update) or is new (insert)
	int32 ExistingIndex = Records.IndexOfByPredicate([&](const FAtlasJobHistoryRecord& R) {
		return R.JobId == Record.JobId;
	});

	if (ExistingIndex != INDEX_NONE)
	{
		Records[ExistingIndex] = Record;
	}
	else
	{
		Records.Insert(Record, 0); // Insert at beginning (newest first)
	}

	// Update cache
	HistoryCache.Add(Record.ApiId, Records);

	// Save back to file
	bool bSaved = SaveHistoryFile(FilePath, Records);

	if (bSaved)
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasHistoryManager: Saved job %s to history (%s)"),
			*Record.JobId.ToString(EGuidFormats::DigitsWithHyphens), *Record.WorkflowName);
	}

	return bSaved;
}

// ==================== Query Operations ====================

TArray<FAtlasJobHistoryRecord> UAtlasHistoryManager::GetHistoryForWorkflow(const FString& ApiId)
{
	FAtlasHistoryQuery Query;
	Query.ApiId = ApiId;
	Query.Limit = 0; // No limit
	return QueryHistory(Query);
}

TArray<FAtlasJobHistoryRecord> UAtlasHistoryManager::QueryHistory(const FAtlasHistoryQuery& Query)
{
	TArray<FAtlasJobHistoryRecord> Results;

	// Determine which files to search
	TArray<FString> ApiIds;
	if (Query.ApiId.IsEmpty())
	{
		// Search all workflows
		ApiIds = GetWorkflowsWithHistory();
	}
	else
	{
		ApiIds.Add(Query.ApiId);
	}

	// Collect all matching records
	TArray<FAtlasJobHistoryRecord> AllRecords;
	for (const FString& ApiId : ApiIds)
	{
		// Check cache first
		if (TArray<FAtlasJobHistoryRecord>* CachedRecords = HistoryCache.Find(ApiId))
		{
			AllRecords.Append(*CachedRecords);
		}
		else
		{
			FString FilePath = GetHistoryFilePath(ApiId);
			TArray<FAtlasJobHistoryRecord> LoadedRecords = LoadHistoryFile(FilePath);
			HistoryCache.Add(ApiId, LoadedRecords);
			AllRecords.Append(LoadedRecords);
		}
	}

	// Filter by query
	for (const FAtlasJobHistoryRecord& Record : AllRecords)
	{
		if (Query.Matches(Record))
		{
			Results.Add(Record);
		}
	}

	// Sort by completion date
	Results.Sort([&Query](const FAtlasJobHistoryRecord& A, const FAtlasJobHistoryRecord& B) {
		if (Query.bNewestFirst)
		{
			return A.CompletedAt > B.CompletedAt;
		}
		else
		{
			return A.CompletedAt < B.CompletedAt;
		}
	});

	// Apply pagination
	if (Query.Offset > 0)
	{
		if (Query.Offset >= Results.Num())
		{
			Results.Empty();
		}
		else
		{
			Results.RemoveAt(0, Query.Offset);
		}
	}

	if (Query.Limit > 0 && Results.Num() > Query.Limit)
	{
		Results.SetNum(Query.Limit);
	}

	return Results;
}

TArray<FAtlasJobHistoryRecord> UAtlasHistoryManager::GetAllHistory()
{
	FAtlasHistoryQuery Query;
	Query.Limit = 0; // No limit
	return QueryHistory(Query);
}

int32 UAtlasHistoryManager::GetHistoryCount(const FAtlasHistoryQuery& Query)
{
	// Use a modified query with no pagination
	FAtlasHistoryQuery CountQuery = Query;
	CountQuery.Offset = 0;
	CountQuery.Limit = 0;

	TArray<FAtlasJobHistoryRecord> Results = QueryHistory(CountQuery);
	return Results.Num();
}

bool UAtlasHistoryManager::FindRecord(const FGuid& JobId, FAtlasJobHistoryRecord& OutRecord)
{
	TArray<FAtlasJobHistoryRecord> AllRecords = GetAllHistory();

	for (const FAtlasJobHistoryRecord& Record : AllRecords)
	{
		if (Record.JobId == JobId)
		{
			OutRecord = Record;
			return true;
		}
	}

	return false;
}

// ==================== Management Operations ====================

void UAtlasHistoryManager::ClearHistory(const FString& ApiId)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (ApiId.IsEmpty())
	{
		// Clear all history
		FString HistoryDir = GetHistoryDirectory();
		TArray<FString> Files;
		PlatformFile.FindFiles(Files, *HistoryDir, TEXT("json"));

		for (const FString& File : Files)
		{
			PlatformFile.DeleteFile(*File);
		}

		HistoryCache.Empty();
		UE_LOG(LogTemp, Log, TEXT("AtlasHistoryManager: Cleared all history (%d files)"), Files.Num());
	}
	else
	{
		// Clear specific workflow
		FString FilePath = GetHistoryFilePath(ApiId);
		if (PlatformFile.DeleteFile(*FilePath))
		{
			HistoryCache.Remove(ApiId);
			UE_LOG(LogTemp, Log, TEXT("AtlasHistoryManager: Cleared history for %s"), *ApiId);
		}
	}
}

bool UAtlasHistoryManager::DeleteRecord(const FGuid& JobId)
{
	// Find which file contains this record
	TArray<FString> ApiIds = GetWorkflowsWithHistory();

	for (const FString& ApiId : ApiIds)
	{
		TArray<FAtlasJobHistoryRecord>& Records = HistoryCache.FindOrAdd(ApiId);
		if (Records.Num() == 0)
		{
			Records = LoadHistoryFile(GetHistoryFilePath(ApiId));
		}

		int32 Index = Records.IndexOfByPredicate([&](const FAtlasJobHistoryRecord& R) {
			return R.JobId == JobId;
		});

		if (Index != INDEX_NONE)
		{
			Records.RemoveAt(Index);
			SaveHistoryFile(GetHistoryFilePath(ApiId), Records);
			UE_LOG(LogTemp, Log, TEXT("AtlasHistoryManager: Deleted record %s"),
				*JobId.ToString(EGuidFormats::DigitsWithHyphens));
			return true;
		}
	}

	return false;
}

TArray<FString> UAtlasHistoryManager::GetWorkflowsWithHistory()
{
	TArray<FString> Result;

	FString HistoryDir = GetHistoryDirectory();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	TArray<FString> Files;
	PlatformFile.FindFiles(Files, *HistoryDir, TEXT("json"));

	for (const FString& FilePath : Files)
	{
		FString FileName = FPaths::GetBaseFilename(FilePath);
		Result.Add(FileName);
	}

	return Result;
}

TArray<FAtlasWorkflowInfo> UAtlasHistoryManager::GetWorkflowInfoWithHistory()
{
	TArray<FAtlasWorkflowInfo> Result;

	TArray<FString> ApiIds = GetWorkflowsWithHistory();

	for (const FString& ApiId : ApiIds)
	{
		// Load the first record from this workflow's history to get the name
		TArray<FAtlasJobHistoryRecord> Records = GetHistoryForWorkflow(ApiId);
		
		FString DisplayName = ApiId; // Fallback to API ID if no records
		if (Records.Num() > 0)
		{
			DisplayName = Records[0].WorkflowName;
		}

		Result.Add(FAtlasWorkflowInfo(ApiId, DisplayName));
	}

	return Result;
}

// ==================== Utility ====================

FString UAtlasHistoryManager::GetHistoryDirectory() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Atlas"), TEXT("History"));
}

FString UAtlasHistoryManager::GetHistoryFilePath(const FString& ApiId) const
{
	return FPaths::Combine(GetHistoryDirectory(), ApiId + TEXT(".json"));
}

void UAtlasHistoryManager::EnsureHistoryDirectoryExists()
{
	FString HistoryDir = GetHistoryDirectory();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (!PlatformFile.DirectoryExists(*HistoryDir))
	{
		PlatformFile.CreateDirectoryTree(*HistoryDir);
	}
}

// ==================== Internal Methods ====================

FAtlasJobHistoryRecord UAtlasHistoryManager::JobToRecord(UAtlasJob* Job)
{
	FAtlasJobHistoryRecord Record;

	Record.JobId = Job->JobId;
	Record.ApiId = Job->ApiId;
	Record.WorkflowName = Job->WorkflowName;
	Record.StartedAt = Job->StartedAt;
	Record.CompletedAt = Job->CompletedAt;
	Record.DurationSeconds = Job->GetDurationSeconds();

	// Map job state to history status
	switch (Job->State)
	{
	case EAtlasJobState::Completed:
		Record.Status = EAtlasJobStatus::Success;
		break;
	case EAtlasJobState::Cancelled:
		Record.Status = EAtlasJobStatus::Cancelled;
		break;
	default:
		Record.Status = EAtlasJobStatus::Failed;
		break;
	}

	// Copy inputs (without raw file bytes - store paths/FileIds instead)
	Record.Inputs = Job->Inputs;
	for (auto& Pair : Record.Inputs.Values)
	{
		// Clear file bytes from inputs (store path/FileId only)
		Pair.Value.FileData.Empty();
	}

	// Copy outputs (without raw file bytes - store FileIds instead)
	Record.Outputs = Job->Outputs;
	for (auto& Pair : Record.Outputs.Values)
	{
		const FString& OutputName = Pair.Key;
		FAtlasValue& OutputValue = Pair.Value;

		// Clear file bytes from outputs (store FileId only)
		OutputValue.FileData.Empty();

		// Capture saved file paths in OutputFilePaths map
		if (!OutputValue.FilePath.IsEmpty())
		{
			Record.OutputFilePaths.Add(OutputName, OutputValue.FilePath);
		}
	}

	// Copy error message
	if (Job->Error.IsError())
	{
		Record.ErrorMessage = Job->Error.GetSummary();
	}

	return Record;
}

TArray<FAtlasJobHistoryRecord> UAtlasHistoryManager::LoadHistoryFile(const FString& FilePath)
{
	TArray<FAtlasJobHistoryRecord> Records;

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		// File doesn't exist yet, return empty
		return Records;
	}

	TSharedPtr<FJsonValue> JsonValue;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonValue) || !JsonValue.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasHistoryManager: Failed to parse history file: %s"), *FilePath);
		return Records;
	}

	if (JsonValue->Type != EJson::Array)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasHistoryManager: History file is not an array: %s"), *FilePath);
		return Records;
	}

	TArray<TSharedPtr<FJsonValue>> JsonArray = JsonValue->AsArray();
	for (const TSharedPtr<FJsonValue>& ItemValue : JsonArray)
	{
		if (ItemValue.IsValid() && ItemValue->Type == EJson::Object)
		{
			FAtlasJobHistoryRecord Record;
			if (JsonToRecord(ItemValue->AsObject(), Record))
			{
				Records.Add(Record);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AtlasHistoryManager: Loaded %d records from %s"), Records.Num(), *FilePath);
	return Records;
}

bool UAtlasHistoryManager::SaveHistoryFile(const FString& FilePath, const TArray<FAtlasJobHistoryRecord>& Records)
{
	TArray<TSharedPtr<FJsonValue>> JsonArray;

	for (const FAtlasJobHistoryRecord& Record : Records)
	{
		TSharedPtr<FJsonObject> RecordJson = RecordToJson(Record);
		if (RecordJson.IsValid())
		{
			JsonArray.Add(MakeShared<FJsonValueObject>(RecordJson));
		}
	}

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	
	if (!FJsonSerializer::Serialize(JsonArray, Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasHistoryManager: Failed to serialize history to JSON"));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(JsonString, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasHistoryManager: Failed to save history file: %s"), *FilePath);
		return false;
	}

	return true;
}

TSharedPtr<FJsonObject> UAtlasHistoryManager::RecordToJson(const FAtlasJobHistoryRecord& Record)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	Json->SetStringField(TEXT("jobId"), Record.JobId.ToString(EGuidFormats::DigitsWithHyphens));
	Json->SetStringField(TEXT("apiId"), Record.ApiId);
	Json->SetStringField(TEXT("workflowName"), Record.WorkflowName);
	Json->SetStringField(TEXT("startedAt"), Record.StartedAt.ToIso8601());
	Json->SetStringField(TEXT("completedAt"), Record.CompletedAt.ToIso8601());
	Json->SetNumberField(TEXT("durationSeconds"), Record.DurationSeconds);
	Json->SetStringField(TEXT("status"), AtlasHistoryHelpers::StatusToString(Record.Status));
	Json->SetStringField(TEXT("errorMessage"), Record.ErrorMessage);

	// Serialize inputs
	Json->SetObjectField(TEXT("inputs"), InputsToJson(Record.Inputs));

	// Serialize outputs
	Json->SetObjectField(TEXT("outputs"), OutputsToJson(Record.Outputs));

	// Serialize output file paths
	TSharedPtr<FJsonObject> PathsJson = MakeShared<FJsonObject>();
	for (const auto& Pair : Record.OutputFilePaths)
	{
		PathsJson->SetStringField(Pair.Key, Pair.Value);
	}
	Json->SetObjectField(TEXT("outputFilePaths"), PathsJson);

	return Json;
}

bool UAtlasHistoryManager::JsonToRecord(const TSharedPtr<FJsonObject>& JsonObj, FAtlasJobHistoryRecord& OutRecord)
{
	if (!JsonObj.IsValid())
	{
		return false;
	}

	FString JobIdStr;
	if (JsonObj->TryGetStringField(TEXT("jobId"), JobIdStr))
	{
		FGuid::Parse(JobIdStr, OutRecord.JobId);
	}

	JsonObj->TryGetStringField(TEXT("apiId"), OutRecord.ApiId);
	JsonObj->TryGetStringField(TEXT("workflowName"), OutRecord.WorkflowName);

	FString StartedAtStr, CompletedAtStr;
	if (JsonObj->TryGetStringField(TEXT("startedAt"), StartedAtStr))
	{
		FDateTime::ParseIso8601(*StartedAtStr, OutRecord.StartedAt);
	}
	if (JsonObj->TryGetStringField(TEXT("completedAt"), CompletedAtStr))
	{
		FDateTime::ParseIso8601(*CompletedAtStr, OutRecord.CompletedAt);
	}

	JsonObj->TryGetNumberField(TEXT("durationSeconds"), OutRecord.DurationSeconds);

	FString StatusStr;
	if (JsonObj->TryGetStringField(TEXT("status"), StatusStr))
	{
		OutRecord.Status = AtlasHistoryHelpers::StringToStatus(StatusStr);
	}

	JsonObj->TryGetStringField(TEXT("errorMessage"), OutRecord.ErrorMessage);

	// Deserialize inputs
	const TSharedPtr<FJsonObject>* InputsJson;
	if (JsonObj->TryGetObjectField(TEXT("inputs"), InputsJson))
	{
		JsonToInputs(*InputsJson, OutRecord.Inputs);
	}

	// Deserialize outputs
	const TSharedPtr<FJsonObject>* OutputsJson;
	if (JsonObj->TryGetObjectField(TEXT("outputs"), OutputsJson))
	{
		JsonToOutputs(*OutputsJson, OutRecord.Outputs);
	}

	// Deserialize output file paths
	const TSharedPtr<FJsonObject>* PathsJson;
	if (JsonObj->TryGetObjectField(TEXT("outputFilePaths"), PathsJson))
	{
		for (const auto& Pair : (*PathsJson)->Values)
		{
			FString PathValue;
			if (Pair.Value->TryGetString(PathValue))
			{
				OutRecord.OutputFilePaths.Add(Pair.Key, PathValue);
			}
		}
	}

	return OutRecord.IsValid();
}

TSharedPtr<FJsonObject> UAtlasHistoryManager::InputsToJson(const FAtlasWorkflowInputs& Inputs)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	for (const auto& Pair : Inputs.Values)
	{
		const FString& Name = Pair.Key;
		const FAtlasValue& Value = Pair.Value;

		TSharedPtr<FJsonObject> ValueJson = MakeShared<FJsonObject>();
		ValueJson->SetStringField(TEXT("type"), UEnum::GetValueAsString(Value.Type));

		switch (Value.Type)
		{
		case EAtlasValueType::String:
			ValueJson->SetStringField(TEXT("value"), Value.StringValue);
			break;
		case EAtlasValueType::Number:
			ValueJson->SetNumberField(TEXT("value"), Value.NumberValue);
			break;
		case EAtlasValueType::Integer:
			ValueJson->SetNumberField(TEXT("value"), Value.IntValue);
			break;
		case EAtlasValueType::Boolean:
			ValueJson->SetBoolField(TEXT("value"), Value.BoolValue);
			break;
		case EAtlasValueType::Image:
		case EAtlasValueType::Mesh:
		case EAtlasValueType::File:
			ValueJson->SetStringField(TEXT("filePath"), Value.FilePath);
			ValueJson->SetStringField(TEXT("fileId"), Value.FileId);
			ValueJson->SetStringField(TEXT("fileName"), Value.FileName);
			break;
		case EAtlasValueType::FileId:
			ValueJson->SetStringField(TEXT("fileId"), Value.FileId);
			break;
		case EAtlasValueType::Json:
			ValueJson->SetStringField(TEXT("value"), Value.JsonValue);
			break;
		default:
			break;
		}

		Json->SetObjectField(Name, ValueJson);
	}

	return Json;
}

TSharedPtr<FJsonObject> UAtlasHistoryManager::OutputsToJson(const FAtlasWorkflowOutputs& Outputs)
{
	// Same format as inputs
	FAtlasWorkflowInputs TempInputs;
	TempInputs.Values = Outputs.Values;
	return InputsToJson(TempInputs);
}

bool UAtlasHistoryManager::JsonToInputs(const TSharedPtr<FJsonObject>& JsonObj, FAtlasWorkflowInputs& OutInputs)
{
	if (!JsonObj.IsValid())
	{
		return false;
	}

	OutInputs.Values.Empty();

	for (const auto& Pair : JsonObj->Values)
	{
		const FString& Name = Pair.Key;
		const TSharedPtr<FJsonObject>* ValueJson;

		if (!Pair.Value->TryGetObject(ValueJson) || !ValueJson->IsValid())
		{
			continue;
		}

		FAtlasValue Value;

		FString TypeStr;
		if ((*ValueJson)->TryGetStringField(TEXT("type"), TypeStr))
		{
			// Parse enum from string like "EAtlasValueType::String"
			if (TypeStr.Contains(TEXT("String"))) Value.Type = EAtlasValueType::String;
			else if (TypeStr.Contains(TEXT("Number"))) Value.Type = EAtlasValueType::Number;
			else if (TypeStr.Contains(TEXT("Integer"))) Value.Type = EAtlasValueType::Integer;
			else if (TypeStr.Contains(TEXT("Boolean"))) Value.Type = EAtlasValueType::Boolean;
			else if (TypeStr.Contains(TEXT("Image"))) Value.Type = EAtlasValueType::Image;
			else if (TypeStr.Contains(TEXT("Mesh"))) Value.Type = EAtlasValueType::Mesh;
			else if (TypeStr.Contains(TEXT("File"))) Value.Type = EAtlasValueType::File;
			else if (TypeStr.Contains(TEXT("FileId"))) Value.Type = EAtlasValueType::FileId;
			else if (TypeStr.Contains(TEXT("Json"))) Value.Type = EAtlasValueType::Json;
		}

		switch (Value.Type)
		{
		case EAtlasValueType::String:
			(*ValueJson)->TryGetStringField(TEXT("value"), Value.StringValue);
			break;
		case EAtlasValueType::Number:
			(*ValueJson)->TryGetNumberField(TEXT("value"), Value.NumberValue);
			break;
		case EAtlasValueType::Integer:
			{
				double TempNum;
				if ((*ValueJson)->TryGetNumberField(TEXT("value"), TempNum))
				{
					Value.IntValue = FMath::RoundToInt(TempNum);
				}
			}
			break;
		case EAtlasValueType::Boolean:
			(*ValueJson)->TryGetBoolField(TEXT("value"), Value.BoolValue);
			break;
		case EAtlasValueType::Image:
		case EAtlasValueType::Mesh:
		case EAtlasValueType::File:
			(*ValueJson)->TryGetStringField(TEXT("filePath"), Value.FilePath);
			(*ValueJson)->TryGetStringField(TEXT("fileId"), Value.FileId);
			(*ValueJson)->TryGetStringField(TEXT("fileName"), Value.FileName);
			break;
		case EAtlasValueType::FileId:
			(*ValueJson)->TryGetStringField(TEXT("fileId"), Value.FileId);
			break;
		case EAtlasValueType::Json:
			(*ValueJson)->TryGetStringField(TEXT("value"), Value.JsonValue);
			break;
		default:
			break;
		}

		OutInputs.Values.Add(Name, Value);
	}

	return true;
}

bool UAtlasHistoryManager::JsonToOutputs(const TSharedPtr<FJsonObject>& JsonObj, FAtlasWorkflowOutputs& OutOutputs)
{
	FAtlasWorkflowInputs TempInputs;
	bool bResult = JsonToInputs(JsonObj, TempInputs);
	OutOutputs.Values = TempInputs.Values;
	return bResult;
}
