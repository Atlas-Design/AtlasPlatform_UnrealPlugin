// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasJobManager.h"
#include "AtlasArchiveSafety.h"
#include "UObject/StrongObjectPtr.h"
#include "AtlasLibraryProfiler.h"
#include "AtlasWorkflowAsset.h"
#include "AtlasFileManager.h"
#include "AtlasHistoryManager.h"
#include "AtlasOutputManager.h"
#include "AtlasBatchOrchestrator.h"
#include "AtlasBatchPersistence.h"
#include "AtlasSDKSettings.h"
#include "AssetRegistry/AssetRegistryModule.h"

UAtlasJobManager::UAtlasJobManager()
{
}

// ==================== History Manager ====================

void UAtlasJobManager::CreateHistoryManagerIfNeeded()
{
	if (!HistoryManager)
	{
		HistoryManager = NewObject<UAtlasHistoryManager>(this);
		UE_LOG(LogTemp, Log, TEXT("AtlasJobManager: HistoryManager created"));
	}
}

UAtlasHistoryManager* UAtlasJobManager::GetHistoryManager()
{
	CreateHistoryManagerIfNeeded();
	return HistoryManager;
}

// ==================== Output Manager ====================

void UAtlasJobManager::CreateOutputManagerIfNeeded()
{
	if (!OutputManager)
	{
		OutputManager = NewObject<UAtlasOutputManager>(this);
		UE_LOG(LogTemp, Log, TEXT("AtlasJobManager: OutputManager created"));
	}
}

void UAtlasJobManager::AutoSaveJobOutputs(UAtlasJob* Job)
{
	if (!IsValid(Job) || Job->bOutputsArchiveAttempted)
	{
		return;
	}

	// Check if auto-save is enabled
	const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
	if (!Settings || !Settings->bAutoSaveOutputFiles)
	{
		return;
	}

	CreateOutputManagerIfNeeded();

	// Iterate through outputs and save file types
	for (auto& Pair : Job->Outputs.Values)
	{
		const FString& OutputName = Pair.Key;
		FAtlasValue& OutputValue = Pair.Value;

		// Skip non-file types or outputs without data
		if (OutputValue.FileData.Num() == 0)
		{
			continue;
		}

		// Determine file extension from type
		FString Extension;
		if (!OutputValue.FileName.IsEmpty())
		{
			Extension = FPaths::GetExtension(OutputValue.FileName, true);
		}

		if (Extension.IsEmpty())
		{
			switch (OutputValue.Type)
			{
			case EAtlasValueType::Image:
				Extension = TEXT(".png");
				break;
			case EAtlasValueType::Mesh:
				Extension = TEXT(".glb");
				break;
			case EAtlasValueType::Audio:
				Extension = TEXT(".mp3");
				break;
			case EAtlasValueType::File:
				Extension = TEXT(".bin");
				break;
			default:
				continue; // Skip non-file types
			}
		}

		// Create deterministic filename: Output_{OutputName}{Extension}
		const FString SafeOutputName = AtlasArchive::Component(OutputName);
		FString FileName = FString::Printf(TEXT("Output_%s%s"), *SafeOutputName, *Extension);
		
		// Save the file into the run's outputs/ folder.
		FAtlasSaveResult SaveResult = OutputManager->SaveToJobOutputFolder(Job, OutputValue.FileData, FileName);

		if (SaveResult.bSuccess)
		{
			// Update the output value with the saved file path
			OutputValue.FilePath = SaveResult.FilePath;
			OutputValue.FileName = FileName;

			UE_LOG(LogTemp, Log, TEXT("AtlasJobManager: Auto-saved output '%s' to %s"),
				*OutputName, *SaveResult.FilePath);
		}
		else
		{
			Job->OutputWarnings.Add(FString::Printf(TEXT("Save %s: %s"), *OutputName, *SaveResult.ErrorMessage));
			UE_LOG(LogTemp, Warning, TEXT("AtlasJobManager: Failed to auto-save output '%s': %s"),
				*OutputName, *SaveResult.ErrorMessage);
		}
	}
}

// ==================== Job Creation ====================

UAtlasJob* UAtlasJobManager::CreateJob(const FString& ApiId, const FString& WorkflowName, const FAtlasWorkflowInputs& Inputs)
{
	return CreateJobInternal(ApiId, WorkflowName, Inputs, nullptr);
}

UAtlasJob* UAtlasJobManager::CreateJobInternal(const FString& ApiId, const FString& WorkflowName, const FAtlasWorkflowInputs& Inputs, UAtlasWorkflowAsset* Asset)
{
	if (bCancellingAllJobs) return nullptr;
	// Create new job object owned by this manager
	UAtlasJob* NewJob = NewObject<UAtlasJob>(this);
	NewJob->Initialize(ApiId, WorkflowName, Inputs);
	NewJob->SetWorkflowAsset(Asset);
	TStrongObjectPtr<UAtlasJob> KeepAlive(NewJob);
	
	// Set the FileManager for uploads/downloads
	if (FileManager)
	{
		NewJob->SetFileManager(FileManager);
	}

	// Bind to job events for auto-management
	BindJobEvents(NewJob);

	// Add to active jobs (GC protection via UPROPERTY)
	ActiveJobs.Add(NewJob);

	UE_LOG(LogTemp, Log, TEXT("AtlasJobManager: Created job %s for workflow '%s' (%s). Active jobs: %d"),
		*NewJob->JobId.ToString(EGuidFormats::DigitsWithHyphens),
		*WorkflowName,
		*ApiId,
		ActiveJobs.Num());

	// Broadcast job created event
	UE_LOG(LogTemp, Log, TEXT("AtlasJobManager: Broadcasting OnJobCreated (bound listeners: %d)"), OnJobCreated.IsBound() ? 1 : 0);
	OnJobCreated.Broadcast(NewJob);

	return NewJob;
}

UAtlasJob* UAtlasJobManager::CreateJobFromAsset(UAtlasWorkflowAsset* WorkflowAsset, const FAtlasWorkflowInputs& Inputs)
{
	if (WorkflowAsset && !WorkflowAsset->CatalogSource.IsNull()) WorkflowAsset = WorkflowAsset->CatalogSource.LoadSynchronous();
	if (!IsValid(WorkflowAsset))
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasJobManager::CreateJobFromAsset called with invalid asset"));
		return nullptr;
	}

	if (!WorkflowAsset->IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasJobManager::CreateJobFromAsset called with unparsed/invalid workflow asset"));
		return nullptr;
	}

	UAtlasJob* NewJob = CreateJobInternal(WorkflowAsset->GetApiId(), WorkflowAsset->GetWorkflowName(), Inputs, WorkflowAsset);

	if (NewJob)
	{
		NewJob->SetWorkflowAsset(WorkflowAsset);
	}

	return NewJob;
}

// ==================== Job Queries ====================

TArray<UAtlasJob*> UAtlasJobManager::GetActiveJobs() const
{
	TArray<UAtlasJob*> Result;
	Result.Reserve(ActiveJobs.Num());
	
	for (const TObjectPtr<UAtlasJob>& Job : ActiveJobs)
	{
		if (IsValid(Job))
		{
			Result.Add(Job);
		}
	}
	
	return Result;
}

TArray<UAtlasJob*> UAtlasJobManager::GetJobsForWorkflow(const FString& ApiId) const
{
	TArray<UAtlasJob*> Result;
	
	for (const TObjectPtr<UAtlasJob>& Job : ActiveJobs)
	{
		if (IsValid(Job) && Job->ApiId == ApiId)
		{
			Result.Add(Job);
		}
	}
	
	return Result;
}

UAtlasJob* UAtlasJobManager::FindJob(const FGuid& JobId) const
{
	for (const TObjectPtr<UAtlasJob>& Job : ActiveJobs)
	{
		if (IsValid(Job) && Job->JobId == JobId)
		{
			return Job;
		}
	}
	
	return nullptr;
}

bool UAtlasJobManager::HasRunningJobs() const
{
	for (const TObjectPtr<UAtlasJob>& Job : ActiveJobs)
	{
		if (IsValid(Job) && Job->IsRunning())
		{
			return true;
		}
	}
	
	return false;
}

// ==================== Job Management ====================

void UAtlasJobManager::CancelJob(UAtlasJob* Job)
{
	if (!IsValid(Job))
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("AtlasJobManager: Cancelling job %s"),
		*Job->JobId.ToString(EGuidFormats::DigitsWithHyphens));

	// Cancel the job (this will trigger state change -> auto removal)
	Job->Cancel();
}

void UAtlasJobManager::CancelAllJobs()
{
	if (bCancellingAllJobs) return;
	TStrongObjectPtr<UAtlasJobManager> KeepAlive(this);
	TGuardValue<bool> Cancelling(bCancellingAllJobs, true);

	// Hold both snapshots across callbacks, including callbacks that release manager retention.
	TArray<TStrongObjectPtr<UAtlasJob>> Jobs;
	for (UAtlasJob* Job : ActiveJobs)
	{
		if (IsValid(Job)) Jobs.Emplace(Job);
	}
	TArray<TStrongObjectPtr<UAtlasBatchOrchestrator>> Batches;
	for (UAtlasBatchOrchestrator* Batch : ActiveOrchestrators)
	{
		if (IsValid(Batch)) Batches.Emplace(Batch);
	}
	// The manager guard blocks dispatch in EVERY batch before the first cancellation event.
	for (const auto& Batch : Batches) Batch->CancelBatch();
	for (const auto& Job : Jobs) Job->Cancel();
	// Terminal-state handlers remove jobs; never discard newly created work blindly.
	ActiveJobs.RemoveAll([](const TObjectPtr<UAtlasJob>& Job) { return !IsValid(Job); });
}

bool UAtlasJobManager::RemoveJob(UAtlasJob* Job)
{
	if (!IsValid(Job))
	{
		return false;
	}

	// Unbind events before removal
	UnbindJobEvents(Job);

	int32 RemovedCount = ActiveJobs.Remove(Job);
	
	if (RemovedCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasJobManager: Removed job %s. Active jobs: %d"),
			*Job->JobId.ToString(EGuidFormats::DigitsWithHyphens),
			ActiveJobs.Num());

		// Broadcast job removed event
		OnJobRemoved.Broadcast(Job);
	}
	
	return RemovedCount > 0;
}

// ==================== Batch Execution ====================

TArray<UAtlasJob*> UAtlasJobManager::GetJobsByBatchId(const FString& InBatchId) const
{
	TArray<UAtlasJob*> Result;
	for (const TObjectPtr<UAtlasJob>& Job : ActiveJobs)
	{
		if (IsValid(Job) && Job->BatchId == InBatchId)
		{
			Result.Add(Job);
		}
	}
	return Result;
}

UAtlasBatchOrchestrator* UAtlasJobManager::CreateBatchOrchestrator()
{
	UAtlasBatchOrchestrator* Orchestrator = NewObject<UAtlasBatchOrchestrator>(this);
	RetainBatchOrchestrator(Orchestrator);
	UE_LOG(LogTemp, Log, TEXT("AtlasJobManager: Created batch orchestrator (active: %d)"), ActiveOrchestrators.Num());
	return Orchestrator;
}

void UAtlasJobManager::RetainBatchOrchestrator(UAtlasBatchOrchestrator* Orchestrator)
{
	// A not-yet-started orchestrator is intentionally retained. !IsRunning() is not
	// a completion test: it also describes every object returned by CreateBatchOrchestrator.
	ActiveOrchestrators.RemoveAll([](const TObjectPtr<UAtlasBatchOrchestrator>& Orch)
	{
		return !IsValid(Orch);
	});
	ActiveOrchestrators.AddUnique(Orchestrator);
	Orchestrator->OnBatchComplete.AddUniqueDynamic(this, &UAtlasJobManager::OnBatchOrchestratorComplete);
}

void UAtlasJobManager::OnBatchOrchestratorComplete(UAtlasBatchOrchestrator* Orchestrator, const FAtlasBatchProgress& Progress)
{
	// Another listener may already have restarted this object.
	if (IsValid(Orchestrator) && !Orchestrator->IsRunning())
	{
		ActiveOrchestrators.Remove(Orchestrator);
	}
}

// ==================== Configuration ====================

void UAtlasJobManager::SetFileManager(UAtlasFileManager* InFileManager)
{
	FileManager = InFileManager;
	UE_LOG(LogTemp, Log, TEXT("AtlasJobManager: FileManager set to %s"), 
		InFileManager ? *InFileManager->GetName() : TEXT("nullptr"));
}

// ==================== Workflow Asset Queries ====================

TArray<UAtlasWorkflowAsset*> UAtlasJobManager::GetAllWorkflowAssets() const
{
	FAtlasLibraryProfileScope Profile(TEXT("Workflow.DiscoverManager"));
	TArray<UAtlasWorkflowAsset*> Result;

	// Query the Asset Registry for all UAtlasWorkflowAsset instances
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByClass(UAtlasWorkflowAsset::StaticClass()->GetClassPathName(), AssetDataList);

	Result.Reserve(AssetDataList.Num());

	for (const FAssetData& AssetData : AssetDataList)
	{
		UAtlasWorkflowAsset* Asset = Cast<UAtlasWorkflowAsset>(AssetData.GetAsset());
		if (IsValid(Asset))
		{
			Result.Add(Asset);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AtlasJobManager: Found %d workflow assets"), Result.Num());

	return Result;
}

// ==================== History Queries ====================

TArray<FAtlasJobHistoryRecord> UAtlasJobManager::GetHistoryForWorkflow(const FString& ApiId)
{
	CreateHistoryManagerIfNeeded();
	return HistoryManager->GetHistoryForWorkflow(ApiId);
}

TArray<FAtlasJobHistoryRecord> UAtlasJobManager::QueryHistory(const FAtlasHistoryQuery& Query)
{
	CreateHistoryManagerIfNeeded();
	return HistoryManager->QueryHistory(Query);
}

TArray<FAtlasJobHistoryRecord> UAtlasJobManager::GetAllHistory()
{
	CreateHistoryManagerIfNeeded();
	return HistoryManager->GetAllHistory();
}

int32 UAtlasJobManager::GetHistoryCount(const FAtlasHistoryQuery& Query)
{
	CreateHistoryManagerIfNeeded();
	return HistoryManager->GetHistoryCount(Query);
}

void UAtlasJobManager::ClearHistory(const FString& ApiId)
{
	CreateHistoryManagerIfNeeded();
	HistoryManager->ClearHistory(ApiId);
}

UAtlasJob* UAtlasJobManager::RerunFromHistory(const FAtlasJobHistoryRecord& Record)
{
	if (!Record.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasJobManager: Cannot rerun from invalid history record"));
		return nullptr;
	}

	FAtlasWorkflowInputs RestoredInputs = Record.Inputs;
	for (auto& Pair : RestoredInputs.Values)
		if (!Pair.Value.FilePath.IsEmpty() && !AtlasArchive::IsContained(UAtlasSDKSettings::Get()->GetOutputFolderPath(), Pair.Value.FilePath)) Pair.Value.FilePath.Empty();
	// Try to find the workflow asset for this ApiId
	UAtlasWorkflowAsset* FoundAsset = UAtlasWorkflowAsset::FindWorkflowByApiId(Record.ApiId);

	if (FoundAsset)
	{
		// Create job from asset with restored inputs
		UAtlasJob* NewJob = CreateJobFromAsset(FoundAsset, RestoredInputs);
		UE_LOG(LogTemp, Log, TEXT("AtlasJobManager: Rerunning job from history (original: %s, new: %s)"),
			*Record.JobId.ToString(EGuidFormats::DigitsWithHyphens),
			NewJob ? *NewJob->JobId.ToString(EGuidFormats::DigitsWithHyphens) : TEXT("nullptr"));
		return NewJob;
	}
	else
	{
		// Create job without asset (will need manual execution setup)
		UAtlasJob* NewJob = CreateJob(Record.ApiId, Record.WorkflowName, RestoredInputs);
		UE_LOG(LogTemp, Warning, TEXT("AtlasJobManager: Rerunning job from history without asset (ApiId: %s)"),
			*Record.ApiId);
		return NewJob;
	}
}

UAtlasJob* UAtlasJobManager::RetryJob(const FAtlasJobHistoryRecord& Record, bool bPreserveBatch)
{
	if (!Record.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasJobManager: Cannot retry from invalid history record"));
		return nullptr;
	}

	// Create the new job via the existing rerun path
	UAtlasJob* NewJob = RerunFromHistory(Record);
	if (!NewJob)
	{
		return nullptr;
	}

	// Set retry lineage
	NewJob->RetryOfJobId = Record.JobId;

	// Optionally preserve batch metadata
	if (bPreserveBatch && Record.IsBatchJob())
	{
		NewJob->BatchId = Record.BatchId;
		NewJob->BatchIndex = Record.BatchIndex;

		// Update the manifest so it points to the new job for this row
		UAtlasBatchPersistence::UpdateManifestRow(
			Record.BatchId,
			Record.BatchIndex,
			NewJob->JobId,
			EAtlasBatchRowStatus::Pending
		);
	}

	UE_LOG(LogTemp, Log, TEXT("AtlasJobManager: Retry job created (original: %s, new: %s, preserveBatch: %s)"),
		*Record.JobId.ToString(EGuidFormats::DigitsWithHyphens),
		*NewJob->JobId.ToString(EGuidFormats::DigitsWithHyphens),
		bPreserveBatch ? TEXT("true") : TEXT("false"));

	return NewJob;
}

// ==================== Event Handling ====================

void UAtlasJobManager::OnJobStateChanged(UAtlasJob* Job, EAtlasJobState NewState)
{
	if (!IsValid(Job))
	{
		return;
	}

	// Save a Running record so crash recovery (ReconcileStaleJobs) can detect it
	if (NewState == EAtlasJobState::Running)
	{
		CreateHistoryManagerIfNeeded();
		HistoryManager->SaveJobToHistory(Job);
	}

	// Auto-remove completed jobs from active list and save to history
	if (AtlasJobHelpers::IsTerminalState(NewState))
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasJobManager: Job %s reached terminal state (%s), removing from active list"),
			*Job->JobId.ToString(EGuidFormats::DigitsWithHyphens),
			*AtlasJobHelpers::StateToString(NewState));

		// Auto-save output files (if enabled) - must happen BEFORE saving to history
		// so that file paths are captured in the history record
		AutoSaveJobOutputs(Job);

		// Save to history (for completed, failed, and cancelled jobs)
		CreateHistoryManagerIfNeeded();
		FAtlasJobHistoryRecord Record;
		if (HistoryManager->SaveJobToHistoryWithRecord(Job, Record))
		{
			// Broadcast history saved event
			OnJobSavedToHistory.Broadcast(Record);
		}

		// Remove from active list
		ActiveJobs.Remove(Job);
		UnbindJobEvents(Job);

		// Broadcast job removed event
		OnJobRemoved.Broadcast(Job);
	}
}

void UAtlasJobManager::BindJobEvents(UAtlasJob* Job)
{
	if (!IsValid(Job))
	{
		return;
	}

	Job->OnStateChanged.AddDynamic(this, &UAtlasJobManager::OnJobStateChanged);
}

void UAtlasJobManager::UnbindJobEvents(UAtlasJob* Job)
{
	if (!IsValid(Job))
	{
		return;
	}

	Job->OnStateChanged.RemoveDynamic(this, &UAtlasJobManager::OnJobStateChanged);
}
