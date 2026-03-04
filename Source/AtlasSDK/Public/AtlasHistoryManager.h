// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Types/AtlasHistoryTypes.h"
#include "AtlasHistoryManager.generated.h"

class UAtlasJob;

/**
 * Manages persistent job history storage and queries.
 * 
 * History is stored as JSON files in Saved/Atlas/History/{ApiId}.json
 * Each workflow has its own history file for efficient access.
 * 
 * Owned by UAtlasJobManager, not intended for direct use.
 */
UCLASS(BlueprintType)
class ATLASSDK_API UAtlasHistoryManager : public UObject
{
	GENERATED_BODY()

public:
	UAtlasHistoryManager();

	// ==================== Save Operations ====================

	/**
	 * Save a completed job to history.
	 * Called automatically by JobManager when jobs complete.
	 * @param Job The completed job to save
	 * @return True if saved successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|History")
	bool SaveJobToHistory(UAtlasJob* Job);

	/**
	 * Save a completed job to history and return the created record.
	 * @param Job The completed job to save
	 * @param OutRecord The created history record (output)
	 * @return True if saved successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|History")
	bool SaveJobToHistoryWithRecord(UAtlasJob* Job, FAtlasJobHistoryRecord& OutRecord);

	/**
	 * Save a history record directly.
	 * @param Record The record to save
	 * @return True if saved successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|History")
	bool SaveRecord(const FAtlasJobHistoryRecord& Record);

	// ==================== Query Operations ====================

	/**
	 * Get all history for a specific workflow.
	 * @param ApiId The workflow API ID
	 * @return Array of history records, newest first
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|History")
	TArray<FAtlasJobHistoryRecord> GetHistoryForWorkflow(const FString& ApiId);

	/**
	 * Query history with flexible filters.
	 * @param Query The query parameters
	 * @return Array of matching history records
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|History")
	TArray<FAtlasJobHistoryRecord> QueryHistory(const FAtlasHistoryQuery& Query);

	/**
	 * Get all history across all workflows.
	 * @return Array of all history records, newest first
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|History")
	TArray<FAtlasJobHistoryRecord> GetAllHistory();

	/**
	 * Get the count of records matching a query.
	 * Useful for pagination UI without loading all data.
	 * @param Query The query parameters (Offset/Limit ignored)
	 * @return Number of matching records
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|History")
	int32 GetHistoryCount(const FAtlasHistoryQuery& Query);

	/**
	 * Find a specific record by JobId.
	 * @param JobId The job GUID
	 * @param OutRecord Output: the found record
	 * @return True if found
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|History")
	bool FindRecord(const FGuid& JobId, FAtlasJobHistoryRecord& OutRecord);

	// ==================== Management Operations ====================

	/**
	 * Clear history for a specific workflow.
	 * @param ApiId The workflow API ID (empty = clear ALL history)
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|History")
	void ClearHistory(const FString& ApiId = TEXT(""));

	/**
	 * Delete a specific record from history.
	 * @param JobId The job GUID to delete
	 * @return True if deleted
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|History")
	bool DeleteRecord(const FGuid& JobId);

	/**
	 * Get the list of all workflow IDs that have history.
	 * @return Array of API IDs
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|History")
	TArray<FString> GetWorkflowsWithHistory();

	/**
	 * Get workflow info (API ID + display name) for all workflows with history.
	 * Useful for populating filter dropdowns that need to show names but filter by ID.
	 * @return Array of workflow info structs
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|History")
	TArray<FAtlasWorkflowInfo> GetWorkflowInfoWithHistory();

	// ==================== Utility ====================

	/**
	 * Get the base directory for history storage.
	 * @return Path to Saved/Atlas/History/
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|History")
	FString GetHistoryDirectory() const;

	/**
	 * Get the history file path for a workflow.
	 * @param ApiId The workflow API ID
	 * @return Path to the JSON file
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|History")
	FString GetHistoryFilePath(const FString& ApiId) const;

	/**
	 * Ensure the history directory exists.
	 */
	void EnsureHistoryDirectoryExists();

protected:
	// ==================== Internal Methods ====================

	/** Load history records from a file */
	TArray<FAtlasJobHistoryRecord> LoadHistoryFile(const FString& FilePath);

	/** Save history records to a file */
	bool SaveHistoryFile(const FString& FilePath, const TArray<FAtlasJobHistoryRecord>& Records);

	/** Convert a job to a history record */
	FAtlasJobHistoryRecord JobToRecord(UAtlasJob* Job);

	/** Convert a record to JSON */
	TSharedPtr<FJsonObject> RecordToJson(const FAtlasJobHistoryRecord& Record);

	/** Convert JSON to a record */
	bool JsonToRecord(const TSharedPtr<FJsonObject>& JsonObj, FAtlasJobHistoryRecord& OutRecord);

	/** Convert FAtlasWorkflowInputs/Outputs to JSON (without bytes) */
	TSharedPtr<FJsonObject> InputsToJson(const FAtlasWorkflowInputs& Inputs);
	TSharedPtr<FJsonObject> OutputsToJson(const FAtlasWorkflowOutputs& Outputs);

	/** Convert JSON to FAtlasWorkflowInputs/Outputs */
	bool JsonToInputs(const TSharedPtr<FJsonObject>& JsonObj, FAtlasWorkflowInputs& OutInputs);
	bool JsonToOutputs(const TSharedPtr<FJsonObject>& JsonObj, FAtlasWorkflowOutputs& OutOutputs);

private:
	/** Cache of loaded history (ApiId -> Records) */
	TMap<FString, TArray<FAtlasJobHistoryRecord>> HistoryCache;

	/** Whether a file has been modified and needs re-save */
	TSet<FString> DirtyFiles;
};
