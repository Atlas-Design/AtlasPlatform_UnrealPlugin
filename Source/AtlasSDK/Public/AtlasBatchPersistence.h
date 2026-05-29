// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Types/AtlasBatchTypes.h"
#include "AtlasBatchPersistence.generated.h"

/**
 * A single row entry in a batch run manifest.
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasBatchManifestRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	int32 RowIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	FGuid JobId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	EAtlasBatchRowStatus Status = EAtlasBatchRowStatus::Pending;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	FString ErrorMessage;

	/** Per-run archive folder for the row's current job attempt */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	FString JobFolderPath;

	/** Per-run metadata file for the row's current job attempt */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	FString JobJsonPath;
};

/**
 * Manifest for a batch run — records which job ID was assigned to each row
 * and its final status. Written to disk during execution and updated
 * as rows complete.
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasBatchManifest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	FString BatchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	FString BatchName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	FString WorkflowApiId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	FString WorkflowName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	FDateTime StartedAt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	FDateTime CompletedAt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	int32 TotalRows = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	TArray<FAtlasBatchManifestRow> Rows;

	bool IsValid() const { return !BatchId.IsEmpty(); }
};

/**
 * Info for a saved draft (returned by ListDrafts without loading full data).
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasBatchDraftInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Batch")
	FString DraftName;

	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Batch")
	FString WorkflowApiId;

	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Batch")
	FString FilePath;

	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Batch")
	int32 RowCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Batch")
	FDateTime SavedAt;
};

/**
 * Static helpers for batch persistence: saving/loading drafts and
 * writing run manifests to disk. All functions are Blueprint-callable.
 *
 * File layout:
 *   Saved/Atlas/Batches/Drafts/{WorkflowApiId}_{DraftName}.json
 *   Saved/Atlas/Batches/Runs/{BatchId}_manifest.json
 */
UCLASS()
class ATLASSDK_API UAtlasBatchPersistence : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ==================== Drafts ====================

	/**
	 * Save a batch definition as a draft.
	 * @param WorkflowApiId The workflow this draft belongs to
	 * @param DraftName Human-readable draft name (used in filename)
	 * @param Batch The batch definition to save
	 * @return True if saved successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Batch|Drafts")
	static bool SaveDraft(const FString& WorkflowApiId, const FString& DraftName, const FAtlasBatchDefinition& Batch);

	/**
	 * Load a draft from disk.
	 * @param WorkflowApiId The workflow this draft belongs to
	 * @param DraftName The draft name to load
	 * @param OutBatch The loaded batch definition
	 * @return True if loaded successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Batch|Drafts")
	static bool LoadDraft(const FString& WorkflowApiId, const FString& DraftName, FAtlasBatchDefinition& OutBatch);

	/**
	 * List all saved drafts, optionally filtered by workflow.
	 * @param WorkflowApiId Filter by workflow (empty = all drafts)
	 * @return Array of draft info
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Batch|Drafts")
	static TArray<FAtlasBatchDraftInfo> ListDrafts(const FString& WorkflowApiId = TEXT(""));

	/**
	 * Delete a saved draft.
	 * @param WorkflowApiId The workflow this draft belongs to
	 * @param DraftName The draft name to delete
	 * @return True if deleted
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Batch|Drafts")
	static bool DeleteDraft(const FString& WorkflowApiId, const FString& DraftName);

	// ==================== Manifests ====================

	/**
	 * Create and save an initial manifest for a batch run.
	 * Called when a batch starts execution.
	 * @param Manifest The manifest to save
	 * @return True if saved
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Batch|Manifests")
	static bool SaveManifest(const FAtlasBatchManifest& Manifest);

	/**
	 * Load a manifest for a batch run.
	 * @param BatchId The batch ID
	 * @param OutManifest The loaded manifest
	 * @return True if loaded
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Batch|Manifests")
	static bool LoadManifest(const FString& BatchId, FAtlasBatchManifest& OutManifest);

	/**
	 * Update a single row in an existing manifest.
	 * @param BatchId The batch ID
	 * @param RowIndex Which row to update
	 * @param NewJobId The job ID for this row (may change on retry)
	 * @param NewStatus The new status
	 * @param ErrorMessage Error message if failed
	 * @param JobFolderPath Optional per-run archive folder for the current job attempt
	 * @param JobJsonPath Optional per-run metadata file for the current job attempt
	 * @return True if updated
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Batch|Manifests")
	static bool UpdateManifestRow(const FString& BatchId, int32 RowIndex, const FGuid& NewJobId, EAtlasBatchRowStatus NewStatus, const FString& ErrorMessage = TEXT(""), const FString& JobFolderPath = TEXT(""), const FString& JobJsonPath = TEXT(""));

	/**
	 * List all run manifests on disk.
	 * @return Array of manifests (lightweight load — metadata only)
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Batch|Manifests")
	static TArray<FAtlasBatchManifest> ListManifests();

	// ==================== Path Helpers ====================

	UFUNCTION(BlueprintPure, Category = "Atlas|Batch|Paths")
	static FString GetDraftsDirectory();

	UFUNCTION(BlueprintPure, Category = "Atlas|Batch|Paths")
	static FString GetRunsDirectory();

private:
	static FString GetDraftFilePath(const FString& WorkflowApiId, const FString& DraftName);
	static FString GetManifestFilePath(const FString& BatchId);
	static FString SanitizeFileName(const FString& Input);

	static TSharedPtr<FJsonObject> BatchDefToJson(const FAtlasBatchDefinition& Batch);
	static bool JsonToBatchDef(const TSharedPtr<FJsonObject>& Json, FAtlasBatchDefinition& OutBatch);

	static TSharedPtr<FJsonObject> ManifestToJson(const FAtlasBatchManifest& Manifest);
	static bool JsonToManifest(const TSharedPtr<FJsonObject>& Json, FAtlasBatchManifest& OutManifest);

	static FString RowStatusToString(EAtlasBatchRowStatus Status);
	static EAtlasBatchRowStatus StringToRowStatus(const FString& Str);
};
