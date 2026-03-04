// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasRuntimeSubsystem.h"
#include "AtlasJobManager.h"
#include "AtlasJob.h"
#include "AtlasWorkflowAsset.h"
#include "AtlasFileManager.h"
#include "AtlasOutputManager.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

void UAtlasRuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[AtlasRuntimeSubsystem] Initialize - Runtime workflow support ready"));
}

void UAtlasRuntimeSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("[AtlasRuntimeSubsystem] Deinitialize - Cleaning up"));

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

bool UAtlasRuntimeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Always create the subsystem - works in both editor and runtime
	return true;
}

// ==================== Job Manager ====================

UAtlasJobManager* UAtlasRuntimeSubsystem::GetJobManager()
{
	CreateJobManagerIfNeeded();
	return JobManager;
}

void UAtlasRuntimeSubsystem::CreateJobManagerIfNeeded()
{
	if (!JobManager)
	{
		JobManager = NewObject<UAtlasJobManager>(this);
		
		// Wire up the FileManager (create if needed)
		CreateFileManagerIfNeeded();
		JobManager->SetFileManager(FileManager);
		
		UE_LOG(LogTemp, Log, TEXT("[AtlasRuntimeSubsystem] JobManager created"));
	}
}

// ==================== File Manager ====================

UAtlasFileManager* UAtlasRuntimeSubsystem::GetFileManager()
{
	CreateFileManagerIfNeeded();
	return FileManager;
}

void UAtlasRuntimeSubsystem::CreateFileManagerIfNeeded()
{
	if (!FileManager)
	{
		FileManager = NewObject<UAtlasFileManager>(this);
		UE_LOG(LogTemp, Log, TEXT("[AtlasRuntimeSubsystem] FileManager created"));
	}
}

// ==================== Output Manager ====================

UAtlasOutputManager* UAtlasRuntimeSubsystem::GetOutputManager()
{
	CreateOutputManagerIfNeeded();
	return OutputManager;
}

void UAtlasRuntimeSubsystem::CreateOutputManagerIfNeeded()
{
	if (!OutputManager)
	{
		OutputManager = NewObject<UAtlasOutputManager>(this);
		UE_LOG(LogTemp, Log, TEXT("[AtlasRuntimeSubsystem] OutputManager created"));
	}
}

// ==================== Convenience Methods ====================

UAtlasJob* UAtlasRuntimeSubsystem::CreateJob(UAtlasWorkflowAsset* WorkflowAsset, const FAtlasWorkflowInputs& Inputs)
{
	return GetJobManager()->CreateJobFromAsset(WorkflowAsset, Inputs);
}

TArray<UAtlasJob*> UAtlasRuntimeSubsystem::GetActiveJobs() const
{
	if (JobManager)
	{
		return JobManager->GetActiveJobs();
	}
	return TArray<UAtlasJob*>();
}

void UAtlasRuntimeSubsystem::CancelAllJobs()
{
	if (JobManager)
	{
		JobManager->CancelAllJobs();
	}
}

bool UAtlasRuntimeSubsystem::HasRunningJobs() const
{
	if (JobManager)
	{
		return JobManager->HasRunningJobs();
	}
	return false;
}

// ==================== Static Helper ====================

UAtlasRuntimeSubsystem* UAtlasRuntimeSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<UAtlasRuntimeSubsystem>();
}
