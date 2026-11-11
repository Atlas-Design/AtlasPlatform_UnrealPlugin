// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasHistoryManager.h"
#include "AtlasWorkflowAsset.h"
#include "JsonObjectConverter.h"
#include "AtlasArchiveSafety.h"
#include "AtlasLibraryProfiler.h"
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
#if WITH_EDITOR
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"
#endif

namespace
{
	// All managers in this process observe writes, including standalone job.json updates.
	uint64 GAtlasHistoryRevision = 0;
}

UAtlasHistoryManager::UAtlasHistoryManager()
{
}

void UAtlasHistoryManager::BeginDestroy()
{
#if WITH_EDITOR
	ClearDirectoryWatches();
#endif
	Super::BeginDestroy();
}

#if WITH_EDITOR
void UAtlasHistoryManager::ClearDirectoryWatches()
{
	if (FDirectoryWatcherModule* Module = FModuleManager::GetModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher")))
	{
		if (IDirectoryWatcher* Watcher = Module->Get())
			for (const auto& Pair : DirectoryWatchHandles)
				Watcher->UnregisterDirectoryChangedCallback_Handle(Pair.Key, Pair.Value);
	}
	DirectoryWatchHandles.Empty();
}

void UAtlasHistoryManager::UpdateDirectoryWatches(const FString& OutputRoot)
{
	// Re-arm after a rebuild: an external tool may have removed/recreated a watched root.
	ClearDirectoryWatches();
	IDirectoryWatcher* Watcher = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher")).Get();
	if (!Watcher) return;
	for (const FString& Directory : {FPaths::ConvertRelativePathToFull(GetHistoryDirectory()), OutputRoot})
	{
		if (DirectoryWatchHandles.Contains(Directory) || !FPaths::DirectoryExists(Directory)) continue;
		FDelegateHandle Handle;
		const TWeakObjectPtr<UAtlasHistoryManager> WeakThis(this);
		if (Watcher->RegisterDirectoryChangedCallback_Handle(Directory,
			IDirectoryWatcher::FDirectoryChanged::CreateLambda([WeakThis](const TArray<FFileChangeData>& Changes)
			{
				for (const FFileChangeData& Change : Changes)
				{
					const FString Extension = FPaths::GetExtension(Change.Filename);
					if (Change.Action == FFileChangeData::FCA_RescanRequired || Extension.IsEmpty() || Extension.Equals(TEXT("json"), ESearchCase::IgnoreCase))
					{
						if (WeakThis.IsValid()) WeakThis->RefreshHistory();
						break;
					}
				}
			}), Handle, IDirectoryWatcher::IncludeDirectoryChanges))
			DirectoryWatchHandles.Add(Directory, Handle);
	}
}
#endif

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

	// Per-job metadata is the sole commit point. Legacy arrays remain read-only migration inputs.
	if (!SaveRecordToJobJson(RecordToSave)) return false;
	HistoryCache.Remove(RecordToSave.ApiId);
	return true;
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

	if (!AtlasArchive::IsContained(UAtlasSDKSettings::Get()->GetOutputFolderPath(), RecordToSave.JobJsonPath)
		|| !AtlasArchive::IsContained(UAtlasSDKSettings::Get()->GetOutputFolderPath(), RecordToSave.InputsFolderPath)
		|| !AtlasArchive::IsContained(UAtlasSDKSettings::Get()->GetOutputFolderPath(), RecordToSave.OutputsFolderPath))
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

	if (!AtlasArchive::WriteText(RecordToSave.JobJsonPath, JsonString))
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasHistoryManager: Failed to save job.json: %s"), *RecordToSave.JobJsonPath);
		return false;
	}

	++GAtlasHistoryRevision;
	return true;
}

bool UAtlasHistoryManager::LoadRecordFromJobJson(const FString& JobJsonPath, FAtlasJobHistoryRecord& OutRecord)
{
	FAtlasLibraryProfileScope Profile(TEXT("History.ReadJobJson"));
	OutRecord = FAtlasJobHistoryRecord();
	if (!AtlasArchive::IsContained(UAtlasSDKSettings::Get()->GetOutputFolderPath(), JobJsonPath)) return false;

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

	// The file being read defines its archive location; stored absolute paths cannot redirect it.
	OutRecord.JobJsonPath = JobJsonPath;
	OutRecord.JobFolderPath = FPaths::GetPath(JobJsonPath);
	OutRecord.InputsFolderPath = OutRecord.JobFolderPath / TEXT("inputs");
	OutRecord.OutputsFolderPath = OutRecord.JobFolderPath / TEXT("outputs");
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
	FAtlasLibraryProfileScope Profile(TEXT("History.Query"));
	EnsureHistoryIndex();
	TArray<FAtlasJobHistoryRecord> Results;
	const int32 Offset = FMath::Max(0, Query.Offset);
	TArray<const FAtlasJobHistoryRecord*> Visible;
	TMap<FString, int32> VisibleCounts;
	const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
	for (const auto& Record : IndexedRecords)
	{
		if (Settings->bLimitVisibleHistory && Settings->MaxHistoryRecordsPerWorkflow > 0
			&& Record.Status != EAtlasJobStatus::Running
			&& ++VisibleCounts.FindOrAdd(Record.ApiId) > Settings->MaxHistoryRecordsPerWorkflow) continue;
		if (Query.Matches(Record)) Visible.Add(&Record);
	}
	for (int32 Position = Offset; Position < Visible.Num(); ++Position)
	{
		const int32 Index = Query.bNewestFirst ? Position : Visible.Num() - 1 - Position;
		Results.Add(*Visible[Index]);
		if (Query.Limit > 0 && Results.Num() >= Query.Limit) break;
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
	EnsureHistoryIndex();
	const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
	TMap<FString, int32> VisibleCounts;
	int32 Count = 0;
	for (const auto& Record : IndexedRecords)
	{
		if (Settings->bLimitVisibleHistory && Settings->MaxHistoryRecordsPerWorkflow > 0
			&& Record.Status != EAtlasJobStatus::Running && ++VisibleCounts.FindOrAdd(Record.ApiId) > Settings->MaxHistoryRecordsPerWorkflow) continue;
		if (Query.Matches(Record)) ++Count;
	}
	return Count;
}

bool UAtlasHistoryManager::FindRecord(const FGuid& JobId, FAtlasJobHistoryRecord& OutRecord)
{
	EnsureHistoryIndex();

	for (const FAtlasJobHistoryRecord& Record : IndexedRecords)
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
	EnsureHistoryIndex();
	const TArray<FAtlasJobHistoryRecord> Records = IndexedRecords;
	for (const auto& Record : Records)
	{
		if (!ApiId.IsEmpty() && Record.ApiId != ApiId) continue;
		if (!DeleteRecord(Record.JobId)) UE_LOG(LogTemp, Error, TEXT("Atlas: Failed to hide history record %s"), *Record.JobId.ToString());
	}
}

bool UAtlasHistoryManager::DeleteRecord(const FGuid& JobId)
{
	if (!JobId.IsValid()) return false;
	// A durable tombstone wins over both stores, including stale writes by an in-flight job.
	const FString Path = GetHistoryDirectory() / TEXT("Deleted") / (JobId.ToString(EGuidFormats::Digits) + TEXT(".json"));
	if (!AtlasArchive::WriteText(Path, TEXT("{\"deleted\":true}"))) return false;
	++GAtlasHistoryRevision;
	HistoryCache.Empty();
	return true;
}

TArray<FString> UAtlasHistoryManager::GetWorkflowsWithHistory()
{
	FAtlasLibraryProfileScope Profile(TEXT("History.WorkflowIds"));
	EnsureHistoryIndex();
	return IndexedWorkflowIds;
}

TArray<FAtlasWorkflowInfo> UAtlasHistoryManager::GetWorkflowInfoWithHistory()
{
	FAtlasLibraryProfileScope Profile(TEXT("History.WorkflowInfo"));
	EnsureHistoryIndex();
	return IndexedWorkflowInfo;
}

void UAtlasHistoryManager::RefreshHistory()
{
	++GAtlasHistoryRevision;
}

void UAtlasHistoryManager::EnsureHistoryIndex()
{
	const FString OutputRoot = UAtlasSDKSettings::Get()->GetOutputFolderPath();
	if (IndexedRevision == GAtlasHistoryRevision && IndexedOutputRoot == OutputRoot) return;
	FAtlasLibraryProfileScope Profile(TEXT("History.BuildIndex"));
	AtlasArchive::FScopedPathChecks PathChecks;
#if WITH_EDITOR
	UpdateDirectoryWatches(OutputRoot);
#endif
	TMap<FGuid, FAtlasJobHistoryRecord> RecordsById;
	TSet<FString> WorkflowIds;
	HistoryCache.Empty();
	TArray<FString> LegacyFiles;
	IFileManager::Get().FindFiles(LegacyFiles, *(GetHistoryDirectory() / TEXT("*.json")), true, false);
	for (const FString& File : LegacyFiles)
	{
		const FString ApiId = FPaths::GetBaseFilename(File);
		WorkflowIds.Add(ApiId); // Preserve even an empty legacy workflow file in the dropdown.
		TArray<FAtlasJobHistoryRecord> Records = LoadHistoryFile(GetHistoryDirectory() / File);
		for (const FAtlasJobHistoryRecord& Record : Records)
		{
			if (Record.IsValid()) RecordsById.Add(Record.JobId, Record);
		}
		HistoryCache.Add(ApiId, MoveTemp(Records));
	}
	// Preserve the existing per-job-over-legacy precedence for duplicate GUIDs.
	for (const FAtlasJobHistoryRecord& Record : LoadJobJsonRecords())
	{
		if (!Record.IsValid()) continue;
		WorkflowIds.Add(Record.ApiId);
		RecordsById.Add(Record.JobId, Record);
	}
	TArray<FString> DeletedIds;
	IFileManager::Get().FindFiles(DeletedIds, *(GetHistoryDirectory() / TEXT("Deleted/*.json")), true, false);
	for (const FString& File : DeletedIds)
	{
		FGuid Id;
		if (FGuid::Parse(FPaths::GetBaseFilename(File), Id)) RecordsById.Remove(Id);
	}
	RecordsById.GenerateValueArray(IndexedRecords);
	IndexedRecords.Sort([](const FAtlasJobHistoryRecord& A, const FAtlasJobHistoryRecord& B)
	{
		return A.CompletedAt > B.CompletedAt;
	});
	IndexedWorkflowIds = WorkflowIds.Array();
	TMap<FString, FString> LatestNames;
	for (const FAtlasJobHistoryRecord& Record : IndexedRecords)
	{
		if (!LatestNames.Contains(Record.ApiId)) LatestNames.Add(Record.ApiId, Record.WorkflowName);
	}
	IndexedWorkflowInfo.Reset(IndexedWorkflowIds.Num());
	for (const FString& ApiId : IndexedWorkflowIds)
	{
		const FString* Name = LatestNames.Find(ApiId);
		IndexedWorkflowInfo.Emplace(ApiId, Name ? *Name : ApiId);
	}
	IndexedOutputRoot = OutputRoot;
	IndexedRevision = GAtlasHistoryRevision;
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
	FAtlasLibraryProfileScope Profile(TEXT("History.Reconcile"));
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

	EnsureHistoryIndex();
	const TArray<FAtlasJobHistoryRecord> Records = IndexedRecords;
	int32 ReconciledCount = 0;
	for (FAtlasJobHistoryRecord Record : Records)
	{
		const FDateTime Now = Record.TimeBasis == TEXT("UTC") ? FDateTime::UtcNow() : FDateTime::Now();
		if (Record.Status != EAtlasJobStatus::Running || Record.StartedAt >= Now - FTimespan::FromHours(ThresholdHours)) continue;
		Record.Status = EAtlasJobStatus::Failed;
		Record.ErrorMessage = FString::Printf(TEXT("Job was still running after %d hours; likely interrupted by editor shutdown"), ThresholdHours);
		Record.CompletedAt = Now;
		Record.DurationSeconds = FMath::Max(0.0, (Now - Record.StartedAt).GetTotalSeconds());
		if (SaveRecord(Record)) ++ReconciledCount;
	}

	return ReconciledCount;
}

// ==================== Utility ====================

FString UAtlasHistoryManager::GetHistoryDirectory() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (!TestHistoryDirectory.IsEmpty()) return TestHistoryDirectory;
#endif
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Atlas"), TEXT("History"));
}

FString UAtlasHistoryManager::GetHistoryFilePath(const FString& ApiId) const
{
	return FPaths::Combine(GetHistoryDirectory(), (ApiId == FPaths::GetCleanFilename(ApiId) && !ApiId.Contains(TEXT(":")) && !ApiId.Contains(TEXT("..")) ? ApiId : AtlasArchive::Component(ApiId)) + TEXT(".json"));
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

	Record.TimeBasis = TEXT("UTC");
	Record.OutputWarnings = Job->OutputWarnings;
	if (UAtlasWorkflowAsset* Workflow = Job->GetExecutionWorkflow())
		FJsonObjectConverter::UStructToJsonObjectString(Workflow->GetSchema(), Record.WorkflowSchemaJson);
	Record.JobId = Job->JobId;
	Record.ApiId = Job->ApiId;
	Record.WorkflowName = Job->WorkflowName;
	Record.StartedAt = Job->StartedAt;
	Record.CompletedAt = Job->CompletedAt;
	Record.DurationSeconds = Job->GetDurationSeconds();

	// Map job state to history status
	switch (Job->State)
	{
	case EAtlasJobState::Running:
		Record.Status = EAtlasJobStatus::Running;
		break;
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

	for (const auto& Pair : Job->Inputs.Values) Record.Inputs.Values.Add(Pair.Key, Pair.Value.WithoutFileData());
    if (const auto* Workflow=Job->GetExecutionWorkflow()) for (const auto& Def : Workflow->GetSchema().Inputs) {
        if (Def.bHasRequiredMetadata && !Def.bRequired && !Job->Inputs.Values.Contains(Def.Name)) {
            FAtlasValue Omitted; Omitted.Type=Def.Type; Omitted.OriginalType=Def.OriginalType; Omitted.FileFormat=Def.Format; Omitted.bIsArray=Def.bIsArray; Omitted.bOmitted=true;
            Record.Inputs.Values.Add(Def.Name,Omitted);
        }
    }
	for (const auto& Pair : Job->Outputs.Values)
	{
		Record.Outputs.Values.Add(Pair.Key, Pair.Value.WithoutFileData());
		if (!Pair.Value.FilePath.IsEmpty()) Record.OutputFilePaths.Add(Pair.Key, Pair.Value.FilePath);
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
	Record.ExecutionId = Job->ExecutionId;

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
	FAtlasLibraryProfileScope Profile(TEXT("History.ReadLegacy"));
	TArray<FAtlasJobHistoryRecord> Records;
	if (!AtlasArchive::IsContained(GetHistoryDirectory(), FilePath)) return Records;

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
	FAtlasLibraryProfileScope Profile(TEXT("History.ScanArchives"));
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
	TArray<FString> Directories{ OutputRoot };
	for (int32 Index = 0; Index < Directories.Num(); ++Index)
	{
		const FString Directory = Directories[Index];
		IPlatformFile::GetPlatformPhysical().IterateDirectory(*Directory, [&](const TCHAR* Path, bool bDirectory)
		{
			if (!AtlasArchive::IsContained(OutputRoot, Path)) return true;
			if (bDirectory) Directories.Add(Path);
			else if (FPaths::GetCleanFilename(Path) == TEXT("job.json")) JobJsonFiles.Add(Path);
			return true;
		});
	}


	FAtlasLibraryProfiler::Add(TEXT("History.ArchiveFilesFound"), 0, JobJsonFiles.Num());
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
	++GAtlasHistoryRevision;
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

	if (!AtlasArchive::WriteText(FilePath, JsonString))
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasHistoryManager: Failed to save history file: %s"), *FilePath);
		return false;
	}

	return true;
}

TSharedPtr<FJsonObject> UAtlasHistoryManager::RecordToJson(const FAtlasJobHistoryRecord& Record)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("archiveVersion"), 2);
	Json->SetStringField(TEXT("timeBasis"), Record.TimeBasis.IsEmpty() ? TEXT("legacy-unknown") : Record.TimeBasis);
	Json->SetStringField(TEXT("workflowSchema"), Record.WorkflowSchemaJson);
	TArray<TSharedPtr<FJsonValue>> Warnings;
	for (const FString& Warning : Record.OutputWarnings) Warnings.Add(MakeShared<FJsonValueString>(Warning));
	Json->SetArrayField(TEXT("outputWarnings"), Warnings);

	Json->SetStringField(TEXT("jobId"), Record.JobId.ToString(EGuidFormats::DigitsWithHyphens));
	Json->SetStringField(TEXT("apiId"), Record.ApiId);
	Json->SetStringField(TEXT("executionId"), Record.ExecutionId);
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
	FAtlasLibraryProfileScope Profile(TEXT("History.ParseRecord"));
	if (!JsonObj.IsValid())
	{
		return false;
	}

	OutRecord = FAtlasJobHistoryRecord();
	JsonObj->TryGetStringField(TEXT("timeBasis"), OutRecord.TimeBasis);
	JsonObj->TryGetStringField(TEXT("workflowSchema"), OutRecord.WorkflowSchemaJson);
	JsonObj->TryGetStringArrayField(TEXT("outputWarnings"), OutRecord.OutputWarnings);
	FString JobIdStr;
	if (JsonObj->TryGetStringField(TEXT("jobId"), JobIdStr))
	{
		FGuid::Parse(JobIdStr, OutRecord.JobId);
	}

	JsonObj->TryGetStringField(TEXT("apiId"), OutRecord.ApiId);
	OutRecord.ExecutionId.Empty();
	JsonObj->TryGetStringField(TEXT("executionId"), OutRecord.ExecutionId);
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
		if (!JsonToInputs(*InputsJson, OutRecord.Inputs)) return false;
	}

	// Deserialize outputs
	const TSharedPtr<FJsonObject>* OutputsJson;
	if (JsonObj->TryGetObjectField(TEXT("outputs"), OutputsJson))
	{
		if (!JsonToOutputs(*OutputsJson, OutRecord.Outputs)) return false;
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

	// Persisted paths are untrusted. Never restore a local path outside the configured archive.
	const FString Root = UAtlasSDKSettings::Get()->GetOutputFolderPath();
	for (FString* Path : { &OutRecord.JobFolderPath, &OutRecord.InputsFolderPath, &OutRecord.OutputsFolderPath, &OutRecord.JobJsonPath })
		if (!Path->IsEmpty() && !AtlasArchive::IsContained(Root, *Path)) Path->Empty();
	for (auto* Values : { &OutRecord.Inputs.Values, &OutRecord.Outputs.Values })
		for (auto& Pair : *Values)
			Pair.Value.ForEachFile([&](auto& Item, int32) { if (!Item.FilePath.IsEmpty() && !AtlasArchive::IsContained(Root, Item.FilePath)) Item.FilePath.Empty(); });
	for (auto It = OutRecord.OutputFilePaths.CreateIterator(); It; ++It)
		if (!AtlasArchive::IsContained(Root, It.Value())) It.RemoveCurrent();
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
		ValueJson->SetBoolField(TEXT("omitted"),Value.bOmitted);
		ValueJson->SetStringField(TEXT("originalType"), Value.OriginalType);
		ValueJson->SetStringField(TEXT("fileFormat"), Value.FileFormat);
        ValueJson->SetStringField(TEXT("SourceKind"), Value.SourceKind);
        ValueJson->SetStringField(TEXT("SourceAssetPath"), Value.SourceAssetPath);
        ValueJson->SetStringField(TEXT("SourceOrigin"), Value.SourceOrigin);
        ValueJson->SetStringField(TEXT("OriginalFileName"), Value.OriginalFileName);


        if (Value.bIsArray)
        {
            ValueJson->SetBoolField(TEXT("isArray"),true);
            TArray<TSharedPtr<FJsonValue>> Items;
            for (const auto& Item : Value.FileItems)
            {
                auto JsonItem=MakeShared<FJsonObject>();
                JsonItem->SetStringField(TEXT("fileId"),Item.FileId); JsonItem->SetStringField(TEXT("filePath"),Item.FilePath);
                JsonItem->SetStringField(TEXT("fileName"),Item.FileName); JsonItem->SetStringField(TEXT("warning"),Item.Warning);
                JsonItem->SetStringField(TEXT("SourceKind"), Item.SourceKind);
                JsonItem->SetStringField(TEXT("SourceAssetPath"), Item.SourceAssetPath);
                JsonItem->SetStringField(TEXT("SourceOrigin"), Item.SourceOrigin);
                JsonItem->SetStringField(TEXT("OriginalFileName"), Item.OriginalFileName);

                Items.Add(MakeShared<FJsonValueObject>(JsonItem));
            }
            ValueJson->SetArrayField(TEXT("items"),Items); Json->SetObjectField(Name,ValueJson); continue;
        }

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
	for (const auto& Pair : Outputs.Values) TempInputs.Values.Add(Pair.Key, Pair.Value.WithoutFileData());
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

		(*ValueJson)->TryGetBoolField(TEXT("omitted"),Value.bOmitted);
		(*ValueJson)->TryGetStringField(TEXT("originalType"), Value.OriginalType);
		(*ValueJson)->TryGetStringField(TEXT("fileFormat"), Value.FileFormat);
        (*ValueJson)->TryGetStringField(TEXT("SourceKind"), Value.SourceKind);
        (*ValueJson)->TryGetStringField(TEXT("SourceAssetPath"), Value.SourceAssetPath);
        (*ValueJson)->TryGetStringField(TEXT("SourceOrigin"), Value.SourceOrigin);
        (*ValueJson)->TryGetStringField(TEXT("OriginalFileName"), Value.OriginalFileName);

        (*ValueJson)->TryGetBoolField(TEXT("isArray"),Value.bIsArray);
        if (Value.bIsArray)
        {
            const TArray<TSharedPtr<FJsonValue>>* Items=nullptr;
            if (!Value.IsFileType() || !(*ValueJson)->TryGetArrayField(TEXT("items"),Items)) return false;
            for (const auto& JsonItem : *Items)
            {
                const TSharedPtr<FJsonObject>* Object=nullptr;
                if (!JsonItem->TryGetObject(Object)) return false;
                FAtlasFileReference Item;
                (*Object)->TryGetStringField(TEXT("fileId"),Item.FileId); (*Object)->TryGetStringField(TEXT("filePath"),Item.FilePath);
                (*Object)->TryGetStringField(TEXT("fileName"),Item.FileName); (*Object)->TryGetStringField(TEXT("warning"),Item.Warning);
                (*Object)->TryGetStringField(TEXT("SourceKind"), Item.SourceKind);
                (*Object)->TryGetStringField(TEXT("SourceAssetPath"), Item.SourceAssetPath);
                (*Object)->TryGetStringField(TEXT("SourceOrigin"), Item.SourceOrigin);
                (*Object)->TryGetStringField(TEXT("OriginalFileName"), Item.OriginalFileName);

                Value.FileItems.Add(Item);
            }
            OutInputs.Values.Add(Name,MoveTemp(Value)); continue;
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
