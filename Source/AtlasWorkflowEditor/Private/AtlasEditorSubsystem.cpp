#include "AtlasEditorSubsystem.h"
#include "AtlasJobManager.h"
#include "AtlasJob.h"
#include "AtlasWorkflowAsset.h"
#include "AtlasFileManager.h"
#include "AtlasOutputManager.h"

void UAtlasEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[AtlasEditorSubsystem] Initialize"));
	// JobManager, FileManager, OutputManager are created lazily on first access
}

void UAtlasEditorSubsystem::Deinitialize()
{
	// Cancel any active jobs before shutdown
	if (JobManager)
	{
		JobManager->CancelAllJobs();
		JobManager = nullptr;
	}

	// Cancel any pending file operations before shutdown
	if (FileManager)
	{
		FileManager->CancelAllUploads();
		FileManager->CancelAllDownloads();
		FileManager = nullptr;
	}

	// Clean up output manager
	OutputManager = nullptr;

	Super::Deinitialize();
}

// ==================== Job Manager ====================

UAtlasJobManager* UAtlasEditorSubsystem::GetJobManager()
{
	CreateJobManagerIfNeeded();
	return JobManager;
}

void UAtlasEditorSubsystem::CreateJobManagerIfNeeded()
{
	if (!JobManager)
	{
		JobManager = NewObject<UAtlasJobManager>(this);
		
		// Wire up the FileManager (create if needed)
		CreateFileManagerIfNeeded();
		JobManager->SetFileManager(FileManager);
		
		UE_LOG(LogTemp, Log, TEXT("[AtlasEditorSubsystem] JobManager created"));
	}
}

// ==================== File Manager ====================

UAtlasFileManager* UAtlasEditorSubsystem::GetFileManager()
{
	CreateFileManagerIfNeeded();
	return FileManager;
}

void UAtlasEditorSubsystem::CreateFileManagerIfNeeded()
{
	if (!FileManager)
	{
		FileManager = NewObject<UAtlasFileManager>(this);
		UE_LOG(LogTemp, Log, TEXT("[AtlasEditorSubsystem] FileManager created"));
	}
}

// ==================== Output Manager ====================

UAtlasOutputManager* UAtlasEditorSubsystem::GetOutputManager()
{
	CreateOutputManagerIfNeeded();
	return OutputManager;
}

void UAtlasEditorSubsystem::CreateOutputManagerIfNeeded()
{
	if (!OutputManager)
	{
		OutputManager = NewObject<UAtlasOutputManager>(this);
		UE_LOG(LogTemp, Log, TEXT("[AtlasEditorSubsystem] OutputManager created"));
	}
}

UAtlasJob* UAtlasEditorSubsystem::CreateJob(UAtlasWorkflowAsset* WorkflowAsset, const FAtlasWorkflowInputs& Inputs)
{
	return GetJobManager()->CreateJobFromAsset(WorkflowAsset, Inputs);
}

TArray<UAtlasJob*> UAtlasEditorSubsystem::GetActiveJobs() const
{
	if (JobManager)
	{
		return JobManager->GetActiveJobs();
	}
	return TArray<UAtlasJob*>();
}

void UAtlasEditorSubsystem::CancelAllJobs()
{
	if (JobManager)
	{
		JobManager->CancelAllJobs();
	}
}

TArray<UAtlasWorkflowAsset*> UAtlasEditorSubsystem::GetAllWorkflowAssets()
{
	return GetJobManager()->GetAllWorkflowAssets();
}