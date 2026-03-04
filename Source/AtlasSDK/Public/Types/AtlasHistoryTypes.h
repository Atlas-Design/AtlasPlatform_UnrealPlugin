// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AtlasWorkflowTypes.h"
#include "AtlasHistoryTypes.generated.h"

/**
 * Simple struct pairing workflow API ID with its display name.
 * Used for populating filter dropdowns in UI.
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasWorkflowInfo
{
	GENERATED_BODY()

	/** The workflow API ID (used for filtering/queries) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	FString ApiId;

	/** Human-readable workflow name (for display) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	FString DisplayName;

	FAtlasWorkflowInfo() {}
	FAtlasWorkflowInfo(const FString& InApiId, const FString& InDisplayName)
		: ApiId(InApiId), DisplayName(InDisplayName) {}
};

/**
 * Status of a completed job in history.
 */
UENUM(BlueprintType)
enum class EAtlasJobStatus : uint8
{
	/** Job completed successfully */
	Success UMETA(DisplayName = "Success"),

	/** Job failed with an error */
	Failed UMETA(DisplayName = "Failed"),

	/** Job was cancelled */
	Cancelled UMETA(DisplayName = "Cancelled")
};

/**
 * A record of a completed job, stored in history.
 * 
 * Inputs/Outputs store values by reference (FileIds) not raw bytes,
 * making history files small and human-readable.
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasJobHistoryRecord
{
	GENERATED_BODY()

	/** Unique identifier of the job execution */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	FGuid JobId;

	/** API identifier of the workflow */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	FString ApiId;

	/** Human-readable workflow name (for display) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	FString WorkflowName;

	/** When the job started executing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	FDateTime StartedAt;

	/** When the job completed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	FDateTime CompletedAt;

	/** Duration in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	float DurationSeconds = 0.0f;

	/** Final status of the job */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	EAtlasJobStatus Status = EAtlasJobStatus::Success;

	/** Input values (file inputs stored as FileIds/paths, not bytes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	FAtlasWorkflowInputs Inputs;

	/** Output values (file outputs stored as FileIds, not bytes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	FAtlasWorkflowOutputs Outputs;

	/** Error message if status is Failed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	FString ErrorMessage;

	/** Path to saved output file on disk (if downloaded and saved) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	TMap<FString, FString> OutputFilePaths;

	// ==================== Helper Methods ====================

	/** Check if this is a valid record */
	bool IsValid() const
	{
		return JobId.IsValid() && !ApiId.IsEmpty();
	}

	/** Check if job succeeded */
	bool IsSuccess() const
	{
		return Status == EAtlasJobStatus::Success;
	}

	/** Check if job failed */
	bool IsFailed() const
	{
		return Status == EAtlasJobStatus::Failed;
	}

	/** Check if job was cancelled */
	bool IsCancelled() const
	{
		return Status == EAtlasJobStatus::Cancelled;
	}

	/** Get formatted duration string (e.g., "00:20") */
	FString GetDurationString() const
	{
		int32 TotalSeconds = FMath::RoundToInt(DurationSeconds);
		int32 Minutes = TotalSeconds / 60;
		int32 Seconds = TotalSeconds % 60;
		return FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
	}

	/** Get formatted timestamp with duration (e.g., "Jan 29, 19:33 • 01:25") */
	FString GetFormattedTimestamp() const
	{
		// Unreal doesn't support %b, so we manually get month abbreviation
		static const TCHAR* MonthNames[] = {
			TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"),
			TEXT("May"), TEXT("Jun"), TEXT("Jul"), TEXT("Aug"),
			TEXT("Sep"), TEXT("Oct"), TEXT("Nov"), TEXT("Dec")
		};
		
		const TCHAR* MonthName = MonthNames[StartedAt.GetMonth() - 1];
		FString DatePart = FString::Printf(TEXT("%s %d, %02d:%02d"),
			MonthName,
			StartedAt.GetDay(),
			StartedAt.GetHour(),
			StartedAt.GetMinute());
		
		FString DurationPart = GetDurationString();
		return FString::Printf(TEXT("%s \u2022 %s"), *DatePart, *DurationPart);
	}

	/** Get time ago string (e.g., "1h ago", "Yesterday") */
	FString GetTimeAgoString() const
	{
		FTimespan TimeSince = FDateTime::Now() - CompletedAt;  // Use local time for consistency
		
		if (TimeSince.GetTotalMinutes() < 1)
		{
			return TEXT("Just now");
		}
		else if (TimeSince.GetTotalMinutes() < 60)
		{
			int32 Minutes = FMath::RoundToInt(TimeSince.GetTotalMinutes());
			return FString::Printf(TEXT("%dm ago"), Minutes);
		}
		else if (TimeSince.GetTotalHours() < 24)
		{
			int32 Hours = FMath::RoundToInt(TimeSince.GetTotalHours());
			return FString::Printf(TEXT("%dh ago"), Hours);
		}
		else if (TimeSince.GetTotalDays() < 2)
		{
			return TEXT("Yesterday");
		}
		else if (TimeSince.GetTotalDays() < 7)
		{
			int32 Days = FMath::RoundToInt(TimeSince.GetTotalDays());
			return FString::Printf(TEXT("%dd ago"), Days);
		}
		else
		{
			return CompletedAt.ToString(TEXT("%b %d, %Y"));
		}
	}

	/** Get date category for grouping (Today, Yesterday, Older) */
	FString GetDateCategory() const
	{
		FDateTime Today = FDateTime::Today();
		FDateTime RecordDate = FDateTime(CompletedAt.GetYear(), CompletedAt.GetMonth(), CompletedAt.GetDay());
		
		if (RecordDate == Today)
		{
			return TEXT("Today");
		}
		else if (RecordDate == Today - FTimespan::FromDays(1))
		{
			return TEXT("Yesterday");
		}
		else
		{
			return TEXT("Older");
		}
	}
};

/**
 * Query parameters for filtering job history.
 * All filters are optional - empty/false means "no filter".
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasHistoryQuery
{
	GENERATED_BODY()

	/** Filter by workflow API ID (empty = all workflows) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	FString ApiId;

	/** Whether to apply status filter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	bool bFilterByStatus = false;

	/** Status to filter by (only used if bFilterByStatus is true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	EAtlasJobStatus StatusFilter = EAtlasJobStatus::Success;

	/** Whether to apply date range filter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	bool bFilterByDate = false;

	/** Start of date range (inclusive, only used if bFilterByDate is true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	FDateTime DateFrom;

	/** End of date range (inclusive, only used if bFilterByDate is true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	FDateTime DateTo;

	/** Pagination: offset into results (default 0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	int32 Offset = 0;

	/** Pagination: max results to return (default 100, 0 = no limit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	int32 Limit = 100;

	/** Sort order: true = newest first (default), false = oldest first */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|History")
	bool bNewestFirst = true;

	// ==================== Helper Methods ====================

	/** Create a query for all history (no filters) */
	static FAtlasHistoryQuery All()
	{
		return FAtlasHistoryQuery();
	}

	/** Create a query for a specific workflow */
	static FAtlasHistoryQuery ForWorkflow(const FString& InApiId)
	{
		FAtlasHistoryQuery Query;
		Query.ApiId = InApiId;
		return Query;
	}

	/** Create a query for today's jobs */
	static FAtlasHistoryQuery Today()
	{
		FAtlasHistoryQuery Query;
		Query.bFilterByDate = true;
		Query.DateFrom = FDateTime::Today();
		Query.DateTo = FDateTime::Today() + FTimespan::FromDays(1);
		return Query;
	}

	/** Create a query for successful jobs only */
	static FAtlasHistoryQuery SuccessOnly()
	{
		FAtlasHistoryQuery Query;
		Query.bFilterByStatus = true;
		Query.StatusFilter = EAtlasJobStatus::Success;
		return Query;
	}

	/** Create a query for failed jobs only */
	static FAtlasHistoryQuery FailedOnly()
	{
		FAtlasHistoryQuery Query;
		Query.bFilterByStatus = true;
		Query.StatusFilter = EAtlasJobStatus::Failed;
		return Query;
	}

	/** Check if a record matches this query's filters */
	bool Matches(const FAtlasJobHistoryRecord& Record) const
	{
		// API ID filter
		if (!ApiId.IsEmpty() && Record.ApiId != ApiId)
		{
			return false;
		}

		// Status filter
		if (bFilterByStatus && Record.Status != StatusFilter)
		{
			return false;
		}

		// Date filter
		if (bFilterByDate)
		{
			if (Record.CompletedAt < DateFrom || Record.CompletedAt > DateTo)
			{
				return false;
			}
		}

		return true;
	}
};

/**
 * Helper functions for history types.
 */
namespace AtlasHistoryHelpers
{
	/** Convert EAtlasJobStatus to string */
	inline FString StatusToString(EAtlasJobStatus Status)
	{
		switch (Status)
		{
		case EAtlasJobStatus::Success:
			return TEXT("Success");
		case EAtlasJobStatus::Failed:
			return TEXT("Failed");
		case EAtlasJobStatus::Cancelled:
			return TEXT("Cancelled");
		default:
			return TEXT("Unknown");
		}
	}

	/** Convert string to EAtlasJobStatus */
	inline EAtlasJobStatus StringToStatus(const FString& StatusStr)
	{
		if (StatusStr == TEXT("Success"))
		{
			return EAtlasJobStatus::Success;
		}
		else if (StatusStr == TEXT("Failed"))
		{
			return EAtlasJobStatus::Failed;
		}
		else if (StatusStr == TEXT("Cancelled"))
		{
			return EAtlasJobStatus::Cancelled;
		}
		return EAtlasJobStatus::Failed;
	}
}
