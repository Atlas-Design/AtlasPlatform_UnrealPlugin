// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/AtlasValueTypes.h"
#include "AtlasBatchTypes.generated.h"

class UAtlasWorkflowAsset;

/**
 * Status of a single row within a batch execution.
 */
UENUM(BlueprintType)
enum class EAtlasBatchRowStatus : uint8
{
	Pending   UMETA(DisplayName = "Pending"),
	Running   UMETA(DisplayName = "Running"),
	Succeeded UMETA(DisplayName = "Succeeded"),
	Failed    UMETA(DisplayName = "Failed"),
	Cancelled UMETA(DisplayName = "Cancelled")
};

/**
 * A single row in a batch definition.
 * Maps parameter names to values, exactly like a single-run input set.
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasBatchRow
{
	GENERATED_BODY()

	/** Row index within the batch (0-based) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	int32 RowIndex = 0;

	/** Input values for this row, keyed by parameter name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	TMap<FString, FAtlasValue> Values;

	/** Current status during execution (only meaningful while a batch is running) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	EAtlasBatchRowStatus Status = EAtlasBatchRowStatus::Pending;

	/** Error message if Status == Failed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	FString ErrorMessage;

	/** Job ID assigned to this row during execution (invalid until row starts) */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Batch")
	FGuid JobId;
};

/**
 * Validation result for a single batch row.
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasBatchRowValidation
{
	GENERATED_BODY()

	/** Row index that was validated */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Batch")
	int32 RowIndex = 0;

	/** Whether the row passed validation */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Batch")
	bool bIsValid = false;

	/** Per-parameter error messages (empty if row is valid) */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Batch")
	TArray<FString> Errors;
};

/**
 * Complete result of validating a batch definition.
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasBatchValidationResult
{
	GENERATED_BODY()

	/** Whether all rows passed validation */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Batch")
	bool bAllValid = false;

	/** Per-row validation results */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Batch")
	TArray<FAtlasBatchRowValidation> Rows;

	/** Batch-level errors (e.g. empty batch, missing workflow) */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Batch")
	TArray<FString> BatchErrors;

	/** Total number of invalid rows */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Batch")
	int32 InvalidRowCount = 0;
};

/**
 * Definition of a batch workflow run.
 * Contains the rows to execute and concurrency/retry settings.
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasBatchDefinition
{
	GENERATED_BODY()

	/** Human-readable name for this batch (used in history/manifests) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	FString BatchName;

	/** Rows to execute */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	TArray<FAtlasBatchRow> Rows;

	/** Maximum number of jobs running simultaneously (1 = sequential) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxConcurrentJobs = 2;

	/** Maximum retry attempts per row for transient failures (0 = no retries) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch", meta = (ClampMin = "0", ClampMax = "5"))
	int32 MaxRetriesPerRow = 1;

	/** Number of rows in this batch */
	int32 Num() const { return Rows.Num(); }

	/** Check if the batch has any rows */
	bool IsEmpty() const { return Rows.Num() == 0; }
};

/**
 * Aggregate progress of a running batch.
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasBatchProgress
{
	GENERATED_BODY()

	/** Unique ID for this batch run */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	FString BatchId;

	/** Total number of rows */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	int32 TotalRows = 0;

	/** Number of rows still waiting to start */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	int32 PendingCount = 0;

	/** Number of rows currently executing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	int32 RunningCount = 0;

	/** Number of rows that completed successfully */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	int32 SucceededCount = 0;

	/** Number of rows that failed (after retries exhausted) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	int32 FailedCount = 0;

	/** Number of rows cancelled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Batch")
	int32 CancelledCount = 0;

	/** Overall progress from 0.0 to 1.0 (completed rows / total rows) */
	float GetProgress() const
	{
		if (TotalRows <= 0) return 0.0f;
		return static_cast<float>(SucceededCount + FailedCount + CancelledCount) / TotalRows;
	}

	/** Whether the batch has finished (no pending or running rows) */
	bool IsComplete() const
	{
		return PendingCount == 0 && RunningCount == 0;
	}
};
