// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Types/AtlasWorkflowTypes.h"
#include "Types/AtlasErrorTypes.h"
#include "AtlasAsyncActions.generated.h"

class UAtlasWorkflowAsset;
class UAtlasJob;
class UAtlasJobManager;
class UAtlasFileManager;

/**
 * Result of an Atlas workflow execution.
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasExecutionResult
{
	GENERATED_BODY()

	/** Whether execution succeeded */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas")
	bool bSuccess = false;

	/** The job that was executed */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas")
	UAtlasJob* Job = nullptr;

	/** Output values from the workflow (valid if bSuccess) */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas")
	FAtlasWorkflowOutputs Outputs;

	/** Error details (valid if !bSuccess) */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas")
	FAtlasError Error;
};

// Delegate for execution complete
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAtlasExecutionComplete, const FAtlasExecutionResult&, Result);

/**
 * Async Blueprint node: Execute Atlas Workflow
 * 
 * This is a high-level convenience node that handles the full workflow execution:
 * 1. Validates inputs
 * 2. Creates a job
 * 3. Uploads any file inputs
 * 4. Executes the workflow (async polling)
 * 5. Downloads results
 * 6. Returns outputs
 * 
 * Usage in Blueprint:
 *   "Execute Atlas Workflow" node with:
 *   - Input: Workflow Asset, Inputs
 *   - Output pins: On Success, On Failure
 */
UCLASS()
class ATLASSDK_API UAtlasExecuteWorkflowAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Execute an Atlas workflow asynchronously.
	 * 
	 * This is the recommended way to run workflows from Blueprint.
	 * Handles all steps: validation, upload, execute, download.
	 * 
	 * @param WorldContextObject World context for timer access
	 * @param WorkflowAsset The workflow to execute
	 * @param Inputs Input values for the workflow
	 * @param UserIndex Optional: User-defined integer to track context in callbacks (e.g., enum value)
	 * @param UserTag Optional: User-defined name to track context in callbacks
	 * @param UserObject Optional: User-defined object reference to track context in callbacks
	 * @param JobManager Optional: Job Manager to use (creates internal one if null)
	 * @param FileManager Optional: File Manager for uploads/downloads (creates internal one if null)
	 * @return Async action node
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Workflow", 
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", 
		DisplayName = "Execute Atlas Workflow",
		AdvancedDisplay = "UserTag,UserObject,JobManager,FileManager"))
	static UAtlasExecuteWorkflowAction* ExecuteWorkflow(
		UObject* WorldContextObject,
		UAtlasWorkflowAsset* WorkflowAsset,
		const FAtlasWorkflowInputs& Inputs,
		int32 UserIndex = 0,
		FName UserTag = NAME_None,
		UObject* UserObject = nullptr,
		UAtlasJobManager* JobManager = nullptr,
		UAtlasFileManager* FileManager = nullptr);

	/** Called when execution completes successfully */
	UPROPERTY(BlueprintAssignable)
	FOnAtlasExecutionComplete OnSuccess;

	/** Called when execution fails */
	UPROPERTY(BlueprintAssignable)
	FOnAtlasExecutionComplete OnFailure;

	/** Called on completion (success or failure) */
	UPROPERTY(BlueprintAssignable)
	FOnAtlasExecutionComplete OnComplete;

	// UBlueprintAsyncActionBase interface
	virtual void Activate() override;

protected:
	/** Handle job state changes */
	UFUNCTION()
	void OnJobStateChanged(UAtlasJob* ChangedJob, EAtlasJobState NewState);

	/** Complete with success */
	void CompleteWithSuccess();

	/** Complete with failure */
	void CompleteWithError(const FAtlasError& InError);

	/** Cleanup internal resources */
	void Cleanup();

private:
	/** World context for timers */
	UPROPERTY()
	TObjectPtr<UObject> WorldContext;

	/** The workflow asset to execute */
	UPROPERTY()
	TObjectPtr<UAtlasWorkflowAsset> Workflow;

	/** Input values */
	UPROPERTY()
	FAtlasWorkflowInputs InputValues;

	/** The job manager (may be provided or created internally) */
	UPROPERTY()
	TObjectPtr<UAtlasJobManager> Manager;

	/** The file manager (may be provided or created internally) */
	UPROPERTY()
	TObjectPtr<UAtlasFileManager> FileMgr;

	/** The executing job */
	UPROPERTY()
	TObjectPtr<UAtlasJob> CurrentJob;

	/** User data to set on the job */
	int32 StoredUserIndex = 0;
	FName StoredUserTag = NAME_None;
	UPROPERTY()
	TObjectPtr<UObject> StoredUserObject = nullptr;

	/** Whether we created the managers internally (need cleanup) */
	bool bOwnManagers = false;

	/** Whether action has completed */
	bool bHasCompleted = false;
};
