#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "AtlasEditorSubsystem.generated.h"

class UAtlasJobManager;
class UAtlasJob;
class UAtlasWorkflowAsset;
class UAtlasFileManager;
class UAtlasOutputManager;
struct FAtlasWorkflowInputs;

/**
 * Editor Subsystem for Atlas SDK.
 * 
 * Provides access to the Job Manager, File Manager, and Output Manager
 * for executing Atlas workflows in the editor.
 * 
 * This subsystem is automatically created when the editor starts.
 */
UCLASS()
class ATLASWORKFLOWEDITOR_API UAtlasEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	// Called automatically when the editor starts
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ==================== Job Manager Access ====================

	/**
	 * Get the Job Manager instance.
	 * The job manager is created lazily on first access.
	 * @return The job manager (never null)
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|JobManager")
	UAtlasJobManager* GetJobManager();

	// ==================== File Manager Access ====================

	/**
	 * Get the File Manager instance.
	 * The file manager is created lazily on first access.
	 * Used for uploading/downloading files for workflow execution.
	 * @return The file manager (never null)
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|FileManager")
	UAtlasFileManager* GetFileManager();

	// ==================== Output Manager Access ====================

	/**
	 * Get the Output Manager instance.
	 * The output manager is created lazily on first access.
	 * Used for saving downloaded files to disk and importing them as assets.
	 * @return The output manager (never null)
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output")
	UAtlasOutputManager* GetOutputManager();

	// ==================== Convenience Methods (delegate to JobManager) ====================

	/**
	 * Create a job from a workflow asset.
	 * Shorthand for GetJobManager()->CreateJobFromAsset()
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|JobManager")
	UAtlasJob* CreateJob(UAtlasWorkflowAsset* WorkflowAsset, const FAtlasWorkflowInputs& Inputs);

	/**
	 * Get all active jobs.
	 * Shorthand for GetJobManager()->GetActiveJobs()
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|JobManager")
	TArray<UAtlasJob*> GetActiveJobs() const;

	/**
	 * Cancel all active jobs.
	 * Shorthand for GetJobManager()->CancelAllJobs()
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|JobManager")
	void CancelAllJobs();

	/**
	 * Get all workflow assets in the project.
	 * Shorthand for GetJobManager()->GetAllWorkflowAssets()
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|JobManager")
	TArray<UAtlasWorkflowAsset*> GetAllWorkflowAssets();

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
