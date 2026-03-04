// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AtlasRuntimeSubsystem.generated.h"

class UAtlasJobManager;
class UAtlasJob;
class UAtlasWorkflowAsset;
class UAtlasFileManager;
class UAtlasOutputManager;
struct FAtlasWorkflowInputs;

/**
 * Runtime Subsystem for Atlas SDK.
 * 
 * Provides access to the Job Manager, File Manager, and Output Manager
 * for executing Atlas workflows at runtime (in packaged builds).
 * 
 * This subsystem is automatically created when the game starts and
 * persists across level transitions (owned by GameInstance).
 * 
 * Usage in Blueprint:
 *   Get Game Instance -> Get Subsystem (AtlasRuntimeSubsystem) -> Get Job Manager
 * 
 * Usage in C++:
 *   UAtlasRuntimeSubsystem* Subsystem = UGameplayStatics::GetGameInstance(this)->GetSubsystem<UAtlasRuntimeSubsystem>();
 *   UAtlasJobManager* JobManager = Subsystem->GetJobManager();
 */
UCLASS()
class ATLASSDK_API UAtlasRuntimeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// ==================== Job Manager Access ====================

	/**
	 * Get the Job Manager instance.
	 * The job manager is created lazily on first access.
	 * @return The job manager (never null)
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Runtime")
	UAtlasJobManager* GetJobManager();

	// ==================== File Manager Access ====================

	/**
	 * Get the File Manager instance.
	 * The file manager is created lazily on first access.
	 * Used for uploading/downloading files for workflow execution.
	 * @return The file manager (never null)
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Runtime")
	UAtlasFileManager* GetFileManager();

	// ==================== Output Manager Access ====================

	/**
	 * Get the Output Manager instance.
	 * The output manager is created lazily on first access.
	 * Used for saving downloaded files to disk and creating textures from bytes.
	 * Note: Asset import functions are only available in editor builds.
	 * @return The output manager (never null)
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Runtime")
	UAtlasOutputManager* GetOutputManager();

	// ==================== Convenience Methods ====================

	/**
	 * Create a job from a workflow asset.
	 * Shorthand for GetJobManager()->CreateJobFromAsset()
	 * @param WorkflowAsset The workflow to execute
	 * @param Inputs The input values for the workflow
	 * @return The created job (ready to Execute())
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Runtime")
	UAtlasJob* CreateJob(UAtlasWorkflowAsset* WorkflowAsset, const FAtlasWorkflowInputs& Inputs);

	/**
	 * Get all active jobs.
	 * Shorthand for GetJobManager()->GetActiveJobs()
	 * @return Array of currently active jobs
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Runtime")
	TArray<UAtlasJob*> GetActiveJobs() const;

	/**
	 * Cancel all active jobs.
	 * Shorthand for GetJobManager()->CancelAllJobs()
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Runtime")
	void CancelAllJobs();

	/**
	 * Check if there are any running jobs.
	 * @return True if at least one job is currently running
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Runtime")
	bool HasRunningJobs() const;

	// ==================== Static Helper ====================

	/**
	 * Get the Atlas Runtime Subsystem from a world context.
	 * Convenience function for Blueprint and C++ access.
	 * @param WorldContextObject Any object with world context (Actor, Component, etc.)
	 * @return The runtime subsystem, or nullptr if not available
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Runtime", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Atlas Runtime Subsystem"))
	static UAtlasRuntimeSubsystem* Get(const UObject* WorldContextObject);

private:
	/** Job Manager instance - created lazily */
	UPROPERTY()
	TObjectPtr<UAtlasJobManager> JobManager;

	/** File Manager instance - created lazily */
	UPROPERTY()
	TObjectPtr<UAtlasFileManager> FileManager;

	/** Output Manager instance - created lazily */
	UPROPERTY()
	TObjectPtr<UAtlasOutputManager> OutputManager;

	void CreateJobManagerIfNeeded();
	void CreateFileManagerIfNeeded();
	void CreateOutputManagerIfNeeded();
};
