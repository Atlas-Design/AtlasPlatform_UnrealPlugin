// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasHistoryManager.h"
#include "AtlasJob.h"
#include "AtlasOutputManager.h"
#include "AtlasSDKSettings.h"
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
	PopulateRunArchivePaths(Record);
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
	PopulateRunArchivePaths(OutRecord);
	return SaveRecord(OutRecord);
}

bool UAtlasHistoryManager::SaveRecord(const FAtlasJobHistoryRecord& Record)
{
	FAtlasJobHistoryRecord RecordToSave = Record;
	PopulateRunArchivePaths(RecordToSave);

	if (!RecordToSave.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasHistoryManager: Cannot save invalid record"));
		return false;
	}

	EnsureHistoryDirectoryExists();

	FString FilePath = GetHistoryFilePath(RecordToSave.ApiId);

	// Load existing records
	TArray<FAtlasJobHistoryRecord> Records = LoadHistoryFile(FilePath);

	// Check if record already exists (update) or is new (insert)
	int32 ExistingIndex = Records.IndexOfByPredicate([&](const FAtlasJobHistoryRecord& R) {
		return R.JobId == RecordToSave.JobId;
	});

	if (ExistingIndex != INDEX_NONE)
	{
		Records[ExistingIndex] = RecordToSave;
	}
	else
	{
		Records.Insert(RecordToSave, 0); // Insert at beginning (newest first)
	}

	// Update cache
	HistoryCache.Add(RecordToSave.ApiId, Records);

	// During the transition to per-run archives, keep writing the legacy
	// per-workflow history array so existing Blueprint/UI paths keep working.
	bool bSaved = SaveHistoryFile(FilePath, Records);
	const bool bSavedJobJson = SaveRecordToJobJson(RecordToSave);
	bSaved = bSaved && bSavedJobJson;

	if (bSaved)
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasHistoryManager: Saved job %s to history (%s)"),
			*RecordToSave.JobId.ToString(EGuidFormats::DigitsWithHyphens), *RecordToSave.WorkflowName);
	}

	return bSaved;
}

bool UAtlasHistoryManager::SaveRecordToJobJson(const FAtlasJobHistoryRecord& Record)
{
	FAtlasJobHistoryRecord RecordToSave = Record;
	PopulateRunArchivePaths(RecordToSave);

	if (!RecordToSave.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasHistoryManager: Cannot save invalid record to job.json"));
		return false;
	}

	if (RecordToSave.JobJsonPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasHistoryManager: Cannot save job.json without a metadata path"));
		return false;
	}

	const FString Directory = FPaths::GetPath(RecordToSave.JobJsonPath);
	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasHistoryManager: Failed to create job archive directory: %s"), *Directory);
		return false;
	}
	if (!RecordToSave.InputsFolderPath.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*RecordToSave.InputsFolderPath, true);
	}
	if (!RecordToSave.OutputsFolderPath.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*RecordToSave.OutputsFolderPath, true);
	}

	TSharedPtr<FJsonObject> RecordJson = RecordToJson(RecordToSave);
	if (!RecordJson.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasHistoryManager: Failed to build job.json for %s"),
			*RecordToSave.JobId.ToString(EGuidFormats::DigitsWithHyphens));
		return false;
	}

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	if (!FJsonSerializer::Serialize(RecordJson.ToSharedRef(), Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasHistoryManager: Failed to serialize job.json"));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(JsonString, *RecordToSave.JobJsonPath))
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasHistoryManager: Failed to save job.json: %s"), *RecordToSave.JobJsonPath);
		return false;
	}

	return true;
}

bool UAtlasHistoryManager::LoadRecordFromJobJson(const FString& JobJsonPath, FAtlasJobHistoryRecord& OutRecord)
{
	OutRecord = FAtlasJobHistoryRecord();

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *JobJsonPath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasHistoryManager: Failed to parse job.json: %s"), *JobJsonPath);
		return false;
	}

	if (!JsonToRecord(JsonObject, OutRecord))
	{
		return false;
	}

	if (OutRecord.JobJsonPath.IsEmpty())
	{
		OutRecord.JobJsonPath = JobJsonPath;
	}
	PopulateRunArchivePaths(OutRecord);
	return true;
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
	TMap<FGuid, FAtlasJobHistoryRecord> RecordsByJobId;

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
	for (const FString& ApiId : ApiIds)
	{
		// Check cache first
		if (TArray<FAtlasJobHistoryRecord>* CachedRecords = HistoryCache.Find(ApiId))
		{
			for (const FAtlasJobHistoryRecord& Record : *CachedRecords)
			{
				if (Record.IsValid())
				{
					RecordsByJobId.FindOrAdd(Record.JobId) = Record;
				}
			}
		}
		else
		{
			FString FilePath = GetHistoryFilePath(ApiId);
			TArray<FAtlasJobHistoryRecord> LoadedRecords = LoadHistoryFile(FilePath);
			HistoryCache.Add(ApiId, LoadedRecords);
			for (const FAtlasJobHistoryRecord& Record : LoadedRecords)
			{
				if (Record.IsValid())
				{
					RecordsByJobId.FindOrAdd(Record.JobId) = Record;
				}
			}
		}
	}

	// Per-run job.json is the new source of truth. It intentionally overwrites
	// legacy records for the same JobId during this transition period.
	TArray<FAtlasJobHistoryRecord> JobJsonRecords = LoadJobJsonRecords(Query.ApiId);
	for (const FAtlasJobHistoryRecord& Record : JobJsonRecords)
	{
		if (Record.IsValid())
		{
			RecordsByJobId.Add(Record.JobId, Record);
		}
	}

	// Filter by query
	for (const auto& Pair : RecordsByJobId)
	{
		const FAtlasJobHistoryRecord& Record = Pair.Value;
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
	TSet<FString> UniqueApiIds;

	FString HistoryDir = GetHistoryDirectory();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	TArray<FString> Files;
	PlatformFile.FindFiles(Files, *HistoryDir, TEXT("json"));

	for (const FString& FilePath : Files)
	{
		FString FileName = FPaths::GetBaseFilename(FilePath);
		UniqueApiIds.Add(FileName);
	}

	for (const FAtlasJobHistoryRecord& Record : LoadJobJsonRecords())
	{
		if (!Record.ApiId.IsEmpty())
		{
			UniqueApiIds.Add(Record.ApiId);
		}
	}

	return UniqueApiIds.Array();
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

// ==================== Batch History Queries ====================

TArray<FString> UAtlasHistoryManager::GetBatchIds()
{
	TSet<FString> UniqueIds;
	TArray<FAtlasJobHistoryRecord> AllRecords = GetAllHistory();

	for (const FAtlasJobHistoryRecord& Record : AllRecords)
	{
		if (Record.IsBatchJob())
		{
			UniqueIds.Add(Record.BatchId);
		}
	}

	return UniqueIds.Array();
}

TArray<FAtlasJobHistoryRecord> UAtlasHistoryManager::GetHistoryForBatch(const FString& BatchId)
{
	TArray<FAtlasJobHistoryRecord> AllRecords = GetAllHistory();
	TArray<FAtlasJobHistoryRecord> BatchRecords;

	for (const FAtlasJobHistoryRecord& Record : AllRecords)
	{
		if (Record.BatchId == BatchId)
		{
			BatchRecords.Add(Record);
		}
	}

	BatchRecords.Sort([](const FAtlasJobHistoryRecord& A, const FAtlasJobHistoryRecord& B)
	{
		return A.BatchIndex < B.BatchIndex;
	});

	return BatchRecords;
}

TArray<FAtlasBatchSummary> UAtlasHistoryManager::QueryBatchSummaries()
{
	TMap<FString, FAtlasBatchSummary> SummaryMap;
	TArray<FAtlasJobHistoryRecord> AllRecords = GetAllHistory();

	for (const FAtlasJobHistoryRecord& Record : AllRecords)
	{
		if (!Record.IsBatchJob())
		{
			continue;
		}

		FAtlasBatchSummary& Summary = SummaryMap.FindOrAdd(Record.BatchId);

		if (Summary.BatchId.IsEmpty())
		{
			Summary.BatchId = Record.BatchId;
			Summary.WorkflowApiId = Record.ApiId;
			Summary.WorkflowName = Record.WorkflowName;
			Summary.StartedAt = Record.StartedAt;
			Summary.CompletedAt = Record.CompletedAt;
		}

		Summary.TotalRows++;

		if (Record.StartedAt < Summary.StartedAt)
		{
			Summary.StartedAt = Record.StartedAt;
		}
		if (Record.CompletedAt > Summary.CompletedAt)
		{
			Summary.CompletedAt = Record.CompletedAt;
		}

		switch (Record.Status)
		{
		case EAtlasJobStatus::Success:   Summary.SucceededCount++; break;
		case EAtlasJobStatus::Failed:    Summary.FailedCount++;    break;
		case EAtlasJobStatus::Cancelled: Summary.CancelledCount++; break;
		case EAtlasJobStatus::Running:   Summary.RunningCount++;   break;
		}
	}

	TArray<FAtlasBatchSummary> Results;
	SummaryMap.GenerateValueArray(Results);

	Results.Sort([](const FAtlasBatchSummary& A, const FAtlasBatchSummary& B)
	{
		return A.StartedAt > B.StartedAt;
	});

	return Results;
}

// ==================== Reconciliation ====================

int32 UAtlasHistoryManager::ReconcileStaleJobs(int32 ThresholdHours)
{
	if (ThresholdHours <= 0)
	{
		if (const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get())
		{
			ThresholdHours = Settings->StaleJobThresholdHours;
		}
		else
		{
			ThresholdHours = 48;
		}
	}

	const FDateTime CutoffTime = FDateTime::Now() - FTimespan::FromHours(ThresholdHours);
	int32 ReconciledCount = 0;
	TSet<FGuid> ReconciledJobIds;

	TArray<FString> ApiIds = GetWorkflowsWithHistory();

	for (const FString& ApiId : ApiIds)
	{
		FString FilePath = GetHistoryFilePath(ApiId);
		TArray<FAtlasJobHistoryRecord> Records = LoadHistoryFile(FilePath);
		bool bModified = false;

		for (FAtlasJobHistoryRecord& Record : Records)
		{
			if (Record.Status == EAtlasJobStatus::Running && Record.StartedAt < CutoffTime)
			{
				Record.Status = EAtlasJobStatus::Failed;
				Record.ErrorMessage = FString::Printf(
					TEXT("Job was still running after %d hours — likely interrupted by editor crash or shutdown"),
					ThresholdHours);
				Record.CompletedAt = FDateTime::Now();
				Record.DurationSeconds = (Record.CompletedAt - Record.StartedAt).GetTotalSeconds();
				bModified = true;
				ReconciledCount++;
				ReconciledJobIds.Add(Record.JobId);

				UE_LOG(LogTemp, Warning, TEXT("AtlasHistoryManager: Reconciled stale job %s (%s) — started %s"),
					*Record.JobId.ToString(EGuidFormats::DigitsWithHyphens),
					*Record.WorkflowName,
					*Record.StartedAt.ToString());
			}
		}

		if (bModified)
		{
			HistoryCache.Add(ApiId, Records);
			SaveHistoryFile(FilePath, Records);
			for (const FAtlasJobHistoryRecord& Record : Records)
			{
				if (Record.Status != EAtlasJobStatus::Running && !Record.JobJsonPath.IsEmpty())
				{
					SaveRecordToJobJson(Record);
				}
			}
		}
	}

	TArray<FAtlasJobHistoryRecord> JobJsonRecords = LoadJobJsonRecords();
	for (FAtlasJobHistoryRecord& Record : JobJsonRecords)
	{
		if (ReconciledJobIds.Contains(Record.JobId))
		{
			continue;
		}

		if (Record.Status == EAtlasJobStatus::Running && Record.StartedAt < CutoffTime)
		{
			Record.Status = EAtlasJobStatus::Failed;
			Record.ErrorMessage = FString::Printf(
				TEXT("Job was still running after %d hours — likely interrupted by editor crash or shutdown"),
				ThresholdHours);
			Record.CompletedAt = FDateTime::Now();
			Record.DurationSeconds = (Record.CompletedAt - Record.StartedAt).GetTotalSeconds();
			SaveRecord(Record);
			ReconciledCount++;

			UE_LOG(LogTemp, Warning, TEXT("AtlasHistoryManager: Reconciled stale job.json record %s (%s) — started %s"),
				*Record.JobId.ToString(EGuidFormats::DigitsWithHyphens),
				*Record.WorkflowName,
				*Record.StartedAt.ToString());
		}
	}

	if (ReconciledCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasHistoryManager: Reconciled %d stale running job(s)"), ReconciledCount);
	}

	return ReconciledCount;
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

	// Copy batch metadata
	Record.BatchId = Job->BatchId;
	Record.BatchIndex = Job->BatchIndex;

	// Copy retry lineage
	Record.RetryOfJobId = Job->RetryOfJobId;

	PopulateRunArchivePaths(Record);

	return Record;
}

void UAtlasHistoryManager::PopulateRunArchivePaths(FAtlasJobHistoryRecord& Record) const
{
	if (!Record.IsValid())
	{
		return;
	}

	const bool bHasAllArchivePaths =
		!Record.JobFolderPath.IsEmpty() &&
		!Record.InputsFolderPath.IsEmpty() &&
		!Record.OutputsFolderPath.IsEmpty() &&
		!Record.JobJsonPath.IsEmpty();
	if (bHasAllArchivePaths)
	{
		return;
	}

	UAtlasOutputManager* OutputManager = NewObject<UAtlasOutputManager>(const_cast<UAtlasHistoryManager*>(this));
	if (!OutputManager)
	{
		return;
	}

	const FAtlasJobFolderInfo FolderInfo = OutputManager->GetJobFolderInfoFromHistory(Record);
	if (!FolderInfo.IsValid())
	{
		return;
	}

	if (Record.JobFolderPath.IsEmpty())
	{
		Record.JobFolderPath = FolderInfo.DiskPath;
	}
	if (Record.InputsFolderPath.IsEmpty())
	{
		Record.InputsFolderPath = FolderInfo.InputsDiskPath;
	}
	if (Record.OutputsFolderPath.IsEmpty())
	{
		Record.OutputsFolderPath = FolderInfo.OutputsDiskPath;
	}
	if (Record.JobJsonPath.IsEmpty())
	{
		Record.JobJsonPath = FolderInfo.JobJsonPath;
	}
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

TArray<FAtlasJobHistoryRecord> UAtlasHistoryManager::LoadJobJsonRecords(const FString& ApiIdFilter)
{
	TArray<FAtlasJobHistoryRecord> Records;

	UAtlasOutputManager* OutputManager = NewObject<UAtlasOutputManager>(this);
	if (!OutputManager)
	{
		return Records;
	}

	const FString OutputRoot = OutputManager->GetOutputFolder();
	if (OutputRoot.IsEmpty() || !FPaths::DirectoryExists(OutputRoot))
	{
		return Records;
	}

	TArray<FString> JobJsonFiles;
	IFileManager::Get().FindFilesRecursive(
		JobJsonFiles,
		*OutputRoot,
		TEXT("job.json"),
		true,
		false,
		false
	);

	for (const FString& JobJsonPath : JobJsonFiles)
	{
		FAtlasJobHistoryRecord Record;
		if (LoadRecordFromJobJson(JobJsonPath, Record))
		{
			if (ApiIdFilter.IsEmpty() || Record.ApiId == ApiIdFilter)
			{
				Records.Add(Record);
			}
		}
	}

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

	// Serialize run archive paths
	Json->SetStringField(TEXT("jobFolderPath"), Record.JobFolderPath);
	Json->SetStringField(TEXT("inputsFolderPath"), Record.InputsFolderPath);
	Json->SetStringField(TEXT("outputsFolderPath"), Record.OutputsFolderPath);
	Json->SetStringField(TEXT("jobJsonPath"), Record.JobJsonPath);

	// Serialize batch metadata
	if (Record.IsBatchJob())
	{
		Json->SetStringField(TEXT("batchId"), Record.BatchId);
		Json->SetNumberField(TEXT("batchIndex"), Record.BatchIndex);
	}

	// Serialize retry lineage
	if (Record.IsRetry())
	{
		Json->SetStringField(TEXT("retryOfJobId"), Record.RetryOfJobId.ToString(EGuidFormats::DigitsWithHyphens));
	}

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
	JsonObj->TryGetStringField(TEXT("jobFolderPath"), OutRecord.JobFolderPath);
	JsonObj->TryGetStringField(TEXT("inputsFolderPath"), OutRecord.InputsFolderPath);
	JsonObj->TryGetStringField(TEXT("outputsFolderPath"), OutRecord.OutputsFolderPath);
	JsonObj->TryGetStringField(TEXT("jobJsonPath"), OutRecord.JobJsonPath);

	// Deserialize batch metadata
	JsonObj->TryGetStringField(TEXT("batchId"), OutRecord.BatchId);
	{
		double TempBatchIndex;
		if (JsonObj->TryGetNumberField(TEXT("batchIndex"), TempBatchIndex))
		{
			OutRecord.BatchIndex = FMath::RoundToInt(TempBatchIndex);
		}
	}

	// Deserialize retry lineage
	{
		FString RetryOfStr;
		if (JsonObj->TryGetStringField(TEXT("retryOfJobId"), RetryOfStr))
		{
			FGuid::Parse(RetryOfStr, OutRecord.RetryOfJobId);
		}
	}

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
		case EAtlasValueType::Audio:
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
			else if (TypeStr.Contains(TEXT("Audio"))) Value.Type = EAtlasValueType::Audio;
			else if (TypeStr.Contains(TEXT("FileId"))) Value.Type = EAtlasValueType::FileId;
			else if (TypeStr.Contains(TEXT("File"))) Value.Type = EAtlasValueType::File;
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
		case EAtlasValueType::Audio:
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
