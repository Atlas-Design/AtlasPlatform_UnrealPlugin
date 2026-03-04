// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AtlasJobTypes.generated.h"

/**
 * State of a workflow job.
 * Represents the overall lifecycle state of the job.
 */
UENUM(BlueprintType)
enum class EAtlasJobState : uint8
{
	/** Job has been created but not yet started */
	Pending UMETA(DisplayName = "Pending"),

	/** Job is currently executing */
	Running UMETA(DisplayName = "Running"),

	/** Job completed successfully */
	Completed UMETA(DisplayName = "Completed"),

	/** Job failed with an error */
	Failed UMETA(DisplayName = "Failed"),

	/** Job was cancelled by user */
	Cancelled UMETA(DisplayName = "Cancelled")
};

/**
 * Current execution phase of a running job.
 * Provides granular visibility into what the job is doing.
 */
UENUM(BlueprintType)
enum class EAtlasJobPhase : uint8
{
	/** Job not yet started or initializing */
	Initializing UMETA(DisplayName = "Initializing"),

	/** Uploading input files to server */
	Uploading UMETA(DisplayName = "Uploading Files"),

	/** Executing workflow on server */
	Executing UMETA(DisplayName = "Executing Workflow"),

	/** Downloading output files from server */
	Downloading UMETA(DisplayName = "Downloading Results"),

	/** All operations complete */
	Done UMETA(DisplayName = "Done")
};

/**
 * Helper functions for job state/phase
 */
namespace AtlasJobHelpers
{
	/** Check if a state is terminal (job won't change state anymore) */
	inline bool IsTerminalState(EAtlasJobState State)
	{
		return State == EAtlasJobState::Completed ||
		       State == EAtlasJobState::Failed ||
		       State == EAtlasJobState::Cancelled;
	}

	/** Get a human-readable string for a job state */
	inline FString StateToString(EAtlasJobState State)
	{
		switch (State)
		{
		case EAtlasJobState::Pending:   return TEXT("Pending");
		case EAtlasJobState::Running:   return TEXT("Running");
		case EAtlasJobState::Completed: return TEXT("Completed");
		case EAtlasJobState::Failed:    return TEXT("Failed");
		case EAtlasJobState::Cancelled: return TEXT("Cancelled");
		default:                        return TEXT("Unknown");
		}
	}

	/** Get a human-readable string for a job phase */
	inline FString PhaseToString(EAtlasJobPhase Phase)
	{
		switch (Phase)
		{
		case EAtlasJobPhase::Initializing: return TEXT("Initializing");
		case EAtlasJobPhase::Uploading:    return TEXT("Uploading Files");
		case EAtlasJobPhase::Executing:    return TEXT("Executing Workflow");
		case EAtlasJobPhase::Downloading:  return TEXT("Downloading Results");
		case EAtlasJobPhase::Done:         return TEXT("Done");
		default:                           return TEXT("Unknown");
		}
	}
}
