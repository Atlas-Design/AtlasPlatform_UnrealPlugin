#include "AtlasEditorSubsystem.h"
#include "AtlasJobManager.h"
#include "AtlasHistoryManager.h"
#include "AtlasTempStorageManager.h"
#include "AtlasSDKSettings.h"
#include "AtlasJob.h"
#include "AtlasWorkflowAsset.h"
#include "AtlasFileManager.h"
#include "AtlasOutputManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

void UAtlasEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[AtlasEditorSubsystem] Initialize"));

	// Reconcile any jobs that were still running when the editor last shut down
	UAtlasHistoryManager* HistoryMgr = GetJobManager()->GetHistoryManager();
	if (HistoryMgr)
	{
		HistoryMgr->ReconcileStaleJobs();
	}

	// Clean up temp directories on editor start if enabled
	const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
	if (Settings && Settings->bCleanTempOnEditorStart)
	{
		int32 Cleaned = UAtlasTempStorageManager::CleanupTempDirectory();
		if (Cleaned > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[AtlasEditorSubsystem] Cleaned %d temp file(s) on startup"), Cleaned);
		}
	}

	// Bind to job completion to check temp storage after each job
	GetJobManager()->OnJobSavedToHistory.AddDynamic(this, &UAtlasEditorSubsystem::OnJobSavedToHistory);
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

void UAtlasEditorSubsystem::OnJobSavedToHistory(const FAtlasJobHistoryRecord& Record)
{
	if (UAtlasTempStorageManager::CheckAndWarnTempStorage())
	{
		FString SizeStr = UAtlasTempStorageManager::GetTempDirectorySizeFormatted();
		const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
		int32 ThresholdMB = Settings ? Settings->TempStorageWarningMB : 500;

		FString Message = FString::Printf(
			TEXT("Atlas temp storage is using %s (threshold: %d MB). Consider cleaning up via Project Settings > Atlas SDK."),
			*SizeStr, ThresholdMB);

		FNotificationInfo Info(FText::FromString(Message));
		Info.ExpireDuration = 8.0f;
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}