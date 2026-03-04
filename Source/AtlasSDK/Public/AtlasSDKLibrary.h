// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Types/AtlasValueTypes.h"
#include "Types/AtlasWorkflowTypes.h"
#include "Types/AtlasSchemaTypes.h"
#include "Types/AtlasErrorTypes.h"
#include "Types/AtlasJobTypes.h"
#include "Types/AtlasHistoryTypes.h"
#include "AtlasSDKLibrary.generated.h"

/**
 * Blueprint Function Library for Atlas SDK types.
 * Provides Blueprint-callable functions for creating and manipulating
 * Atlas value types, workflow inputs/outputs, and error handling.
 */
UCLASS()
class ATLASSDK_API UAtlasSDKLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ==================== FAtlasValue Creation ====================

	/** Create a string value */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Make String Value"))
	static FAtlasValue MakeStringValue(const FString& Value);

	/** Create a number (float) value */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Make Number Value"))
	static FAtlasValue MakeNumberValue(float Value);

	/** Create an integer value */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Make Integer Value"))
	static FAtlasValue MakeIntegerValue(int32 Value);

	/** Create a boolean value */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Make Bool Value"))
	static FAtlasValue MakeBoolValue(bool Value);

	/** Create a file input value from a file path */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Make File Value"))
	static FAtlasValue MakeFileValue(const FString& FilePath);

	/** Create a file ID reference value (server-side file reference) */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Make File ID Value"))
	static FAtlasValue MakeFileIdValue(const FString& FileId);

	/** Create an image input value from a file path */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Make Image Value"))
	static FAtlasValue MakeImageValue(const FString& FilePath);

	/** Create a mesh input value from a file path */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Make Mesh Value"))
	static FAtlasValue MakeMeshValue(const FString& FilePath);

	/** Create a JSON value from a JSON string */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Make JSON Value"))
	static FAtlasValue MakeJsonValue(const FString& JsonString);

	// ==================== FAtlasValue Getters ====================

	/** Get the type of an Atlas value */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Get Value Type"))
	static EAtlasValueType GetValueType(const FAtlasValue& Value);

	/** Check if value is valid (not None) */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Is Valid"))
	static bool IsValueValid(const FAtlasValue& Value);

	/** Get string from value */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Get String"))
	static FString GetStringFromValue(const FAtlasValue& Value);

	/** Get number from value */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Get Number"))
	static float GetNumberFromValue(const FAtlasValue& Value);

	/** Get integer from value */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Get Integer"))
	static int32 GetIntegerFromValue(const FAtlasValue& Value);

	/** Get boolean from value */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Get Bool"))
	static bool GetBoolFromValue(const FAtlasValue& Value);

	/** Get file ID from value */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Get File ID"))
	static FString GetFileIdFromValue(const FAtlasValue& Value);

	/** Get file path from value (for input file types) */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Get File Path"))
	static FString GetFilePathFromValue(const FAtlasValue& Value);

	/** Check if value is a file type (File, Image, or Mesh) */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Is File Type"))
	static bool IsFileType(const FAtlasValue& Value);

	/** Check if file value has a file path set (for inputs) */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Has File Path"))
	static bool HasFilePath(const FAtlasValue& Value);

	/** Check if file value has byte data (for outputs) */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "Has File Data"))
	static bool HasFileData(const FAtlasValue& Value);

	/** Convert value to human-readable string */
	UFUNCTION(BlueprintPure, Category = "Atlas|Value", meta = (DisplayName = "To String"))
	static FString ValueToString(const FAtlasValue& Value);

	// ==================== FAtlasWorkflowInputs ====================

	/** Set a string input */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Inputs", meta = (DisplayName = "Set String Input"))
	static void SetStringInput(UPARAM(ref) FAtlasWorkflowInputs& Inputs, const FString& Name, const FString& Value);

	/** Set a number input */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Inputs", meta = (DisplayName = "Set Number Input"))
	static void SetNumberInput(UPARAM(ref) FAtlasWorkflowInputs& Inputs, const FString& Name, float Value);

	/** Set an integer input */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Inputs", meta = (DisplayName = "Set Integer Input"))
	static void SetIntegerInput(UPARAM(ref) FAtlasWorkflowInputs& Inputs, const FString& Name, int32 Value);

	/** Set a boolean input */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Inputs", meta = (DisplayName = "Set Bool Input"))
	static void SetBoolInput(UPARAM(ref) FAtlasWorkflowInputs& Inputs, const FString& Name, bool Value);

	/** Set a file input from a file path */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Inputs", meta = (DisplayName = "Set File Input"))
	static void SetFileInput(UPARAM(ref) FAtlasWorkflowInputs& Inputs, const FString& Name, const FString& FilePath);

	/** Set a file ID input (server-side file reference) */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Inputs", meta = (DisplayName = "Set File ID Input"))
	static void SetFileIdInput(UPARAM(ref) FAtlasWorkflowInputs& Inputs, const FString& Name, const FString& FileId);

	/** Set an image input from a file path */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Inputs", meta = (DisplayName = "Set Image Input"))
	static void SetImageInput(UPARAM(ref) FAtlasWorkflowInputs& Inputs, const FString& Name, const FString& FilePath);

	/** Set a mesh input from a file path */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Inputs", meta = (DisplayName = "Set Mesh Input"))
	static void SetMeshInput(UPARAM(ref) FAtlasWorkflowInputs& Inputs, const FString& Name, const FString& FilePath);

	/** Set a JSON input */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Inputs", meta = (DisplayName = "Set JSON Input"))
	static void SetJsonInput(UPARAM(ref) FAtlasWorkflowInputs& Inputs, const FString& Name, const FString& JsonString);

	/** Set a value input */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Inputs", meta = (DisplayName = "Set Value Input"))
	static void SetValueInput(UPARAM(ref) FAtlasWorkflowInputs& Inputs, const FString& Name, const FAtlasValue& Value);

	/** Get all input names */
	UFUNCTION(BlueprintPure, Category = "Atlas|Inputs", meta = (DisplayName = "Get Input Names"))
	static TArray<FString> GetInputNames(const FAtlasWorkflowInputs& Inputs);

	/** Get input count */
	UFUNCTION(BlueprintPure, Category = "Atlas|Inputs", meta = (DisplayName = "Get Input Count"))
	static int32 GetInputCount(const FAtlasWorkflowInputs& Inputs);

	/** Check if input exists */
	UFUNCTION(BlueprintPure, Category = "Atlas|Inputs", meta = (DisplayName = "Has Input"))
	static bool HasInput(const FAtlasWorkflowInputs& Inputs, const FString& Name);

	/** Get input value by name */
	UFUNCTION(BlueprintPure, Category = "Atlas|Inputs", meta = (DisplayName = "Get Input Value"))
	static bool GetInputValue(const FAtlasWorkflowInputs& Inputs, const FString& Name, FAtlasValue& OutValue);

	/** Clear all inputs */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Inputs", meta = (DisplayName = "Clear Inputs"))
	static void ClearInputs(UPARAM(ref) FAtlasWorkflowInputs& Inputs);

	// ==================== FAtlasWorkflowOutputs ====================

	/** Get all output names */
	UFUNCTION(BlueprintPure, Category = "Atlas|Outputs", meta = (DisplayName = "Get Output Names"))
	static TArray<FString> GetOutputNames(const FAtlasWorkflowOutputs& Outputs);

	/** Get output count */
	UFUNCTION(BlueprintPure, Category = "Atlas|Outputs", meta = (DisplayName = "Get Output Count"))
	static int32 GetOutputCount(const FAtlasWorkflowOutputs& Outputs);

	/** Check if output exists */
	UFUNCTION(BlueprintPure, Category = "Atlas|Outputs", meta = (DisplayName = "Has Output"))
	static bool HasOutput(const FAtlasWorkflowOutputs& Outputs, const FString& Name);

	/** Get output value by name */
	UFUNCTION(BlueprintPure, Category = "Atlas|Outputs", meta = (DisplayName = "Get Output Value"))
	static bool GetOutputValue(const FAtlasWorkflowOutputs& Outputs, const FString& Name, FAtlasValue& OutValue);

	/** Get string output */
	UFUNCTION(BlueprintPure, Category = "Atlas|Outputs", meta = (DisplayName = "Get String Output"))
	static bool GetStringOutput(const FAtlasWorkflowOutputs& Outputs, const FString& Name, FString& OutValue);

	/** Get number output */
	UFUNCTION(BlueprintPure, Category = "Atlas|Outputs", meta = (DisplayName = "Get Number Output"))
	static bool GetNumberOutput(const FAtlasWorkflowOutputs& Outputs, const FString& Name, float& OutValue);

	/** Get integer output */
	UFUNCTION(BlueprintPure, Category = "Atlas|Outputs", meta = (DisplayName = "Get Integer Output"))
	static bool GetIntegerOutput(const FAtlasWorkflowOutputs& Outputs, const FString& Name, int32& OutValue);

	/** Get boolean output */
	UFUNCTION(BlueprintPure, Category = "Atlas|Outputs", meta = (DisplayName = "Get Bool Output"))
	static bool GetBoolOutput(const FAtlasWorkflowOutputs& Outputs, const FString& Name, bool& OutValue);

	/** Get file ID output */
	UFUNCTION(BlueprintPure, Category = "Atlas|Outputs", meta = (DisplayName = "Get File ID Output"))
	static bool GetFileIdOutput(const FAtlasWorkflowOutputs& Outputs, const FString& Name, FString& OutFileId);

	// ==================== FAtlasError ====================

	/** Check if error represents a failure */
	UFUNCTION(BlueprintPure, Category = "Atlas|Error", meta = (DisplayName = "Is Error"))
	static bool IsError(const FAtlasError& Error);

	/** Check if error is success (no error) */
	UFUNCTION(BlueprintPure, Category = "Atlas|Error", meta = (DisplayName = "Is Success"))
	static bool IsSuccess(const FAtlasError& Error);

	/** Check if error is retryable */
	UFUNCTION(BlueprintPure, Category = "Atlas|Error", meta = (DisplayName = "Is Retryable"))
	static bool IsRetryable(const FAtlasError& Error);

	/** Check if operation was cancelled */
	UFUNCTION(BlueprintPure, Category = "Atlas|Error", meta = (DisplayName = "Was Cancelled"))
	static bool WasCancelled(const FAtlasError& Error);

	/** Get error code */
	UFUNCTION(BlueprintPure, Category = "Atlas|Error", meta = (DisplayName = "Get Error Code"))
	static EAtlasErrorCode GetErrorCode(const FAtlasError& Error);

	/** Get error message */
	UFUNCTION(BlueprintPure, Category = "Atlas|Error", meta = (DisplayName = "Get Error Message"))
	static FString GetErrorMessage(const FAtlasError& Error);

	/** Get error summary for UI display */
	UFUNCTION(BlueprintPure, Category = "Atlas|Error", meta = (DisplayName = "Get Error Summary"))
	static FString GetErrorSummary(const FAtlasError& Error);

	/** Convert error to detailed string */
	UFUNCTION(BlueprintPure, Category = "Atlas|Error", meta = (DisplayName = "Error To String"))
	static FString ErrorToString(const FAtlasError& Error);

	// ==================== FAtlasWorkflowSchema ====================

	/** Check if schema is valid */
	UFUNCTION(BlueprintPure, Category = "Atlas|Schema", meta = (DisplayName = "Is Schema Valid"))
	static bool IsSchemaValid(const FAtlasWorkflowSchema& Schema);

	/** Get schema display name */
	UFUNCTION(BlueprintPure, Category = "Atlas|Schema", meta = (DisplayName = "Get Schema Display Name"))
	static FString GetSchemaDisplayName(const FAtlasWorkflowSchema& Schema);

	/** Get all input definitions */
	UFUNCTION(BlueprintPure, Category = "Atlas|Schema", meta = (DisplayName = "Get Input Definitions"))
	static TArray<FAtlasParameterDef> GetInputDefinitions(const FAtlasWorkflowSchema& Schema);

	/** Get all output definitions */
	UFUNCTION(BlueprintPure, Category = "Atlas|Schema", meta = (DisplayName = "Get Output Definitions"))
	static TArray<FAtlasParameterDef> GetOutputDefinitions(const FAtlasWorkflowSchema& Schema);

	/** Validate inputs against schema - requires exact 1:1 match */
	UFUNCTION(BlueprintPure, Category = "Atlas|Schema", meta = (DisplayName = "Validate Inputs"))
	static bool ValidateInputs(const FAtlasWorkflowSchema& Schema, const FAtlasWorkflowInputs& Inputs, TArray<FString>& OutErrors);

	/** Create default inputs from schema */
	UFUNCTION(BlueprintPure, Category = "Atlas|Schema", meta = (DisplayName = "Create Default Inputs"))
	static FAtlasWorkflowInputs CreateDefaultInputs(const FAtlasWorkflowSchema& Schema);

	// ==================== Schema URL Builders ====================

	/**
	 * Get the upload URL from schema.
	 * Pattern: {BaseUrl}/{Version}/upload/{ApiId}
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Schema|URLs", meta = (DisplayName = "Get Upload URL"))
	static FString GetSchemaUploadUrl(const FAtlasWorkflowSchema& Schema);

	/**
	 * Get the execute URL from schema.
	 * Pattern: {BaseUrl}/{Version}/execute/{ApiId}
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Schema|URLs", meta = (DisplayName = "Get Execute URL"))
	static FString GetSchemaExecuteUrl(const FAtlasWorkflowSchema& Schema);

	/**
	 * Get the download URL for a file from schema.
	 * Pattern: {BaseUrl}/{Version}/download/{FileId}
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Schema|URLs", meta = (DisplayName = "Get Download URL"))
	static FString GetSchemaDownloadUrl(const FAtlasWorkflowSchema& Schema, const FString& FileId);

	// ==================== FAtlasParameterDef ====================

	/** Get parameter display name */
	UFUNCTION(BlueprintPure, Category = "Atlas|Schema", meta = (DisplayName = "Get Parameter Display Name"))
	static FString GetParameterDisplayName(const FAtlasParameterDef& Parameter);

	/** Check if parameter is file type */
	UFUNCTION(BlueprintPure, Category = "Atlas|Schema", meta = (DisplayName = "Is File Type"))
	static bool IsParameterFileType(const FAtlasParameterDef& Parameter);

	/** Check if parameter has options */
	UFUNCTION(BlueprintPure, Category = "Atlas|Schema", meta = (DisplayName = "Has Options"))
	static bool ParameterHasOptions(const FAtlasParameterDef& Parameter);

	// ==================== Job State Helpers ====================

	/** Get human-readable string for job state */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job", meta = (DisplayName = "State To String"))
	static FString JobStateToString(EAtlasJobState State);

	/** Get human-readable string for job phase */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job", meta = (DisplayName = "Phase To String"))
	static FString JobPhaseToString(EAtlasJobPhase Phase);

	/** Check if job state is terminal (finished) */
	UFUNCTION(BlueprintPure, Category = "Atlas|Job", meta = (DisplayName = "Is Terminal State"))
	static bool IsTerminalState(EAtlasJobState State);

	// ==================== History Helpers ====================

	/** Check if history record is valid */
	UFUNCTION(BlueprintPure, Category = "Atlas|History", meta = (DisplayName = "Is Record Valid"))
	static bool IsHistoryRecordValid(const FAtlasJobHistoryRecord& Record);

	/** Check if history record is successful */
	UFUNCTION(BlueprintPure, Category = "Atlas|History", meta = (DisplayName = "Is Success"))
	static bool IsHistoryRecordSuccess(const FAtlasJobHistoryRecord& Record);

	/** Get job status string from history record */
	UFUNCTION(BlueprintPure, Category = "Atlas|History", meta = (DisplayName = "Get Status String"))
	static FString GetHistoryStatusString(const FAtlasJobHistoryRecord& Record);

	/** Get formatted duration string (e.g., "01:23") */
	UFUNCTION(BlueprintPure, Category = "Atlas|History", meta = (DisplayName = "Get Duration String"))
	static FString GetHistoryDurationString(const FAtlasJobHistoryRecord& Record);

	/** Get time ago string (e.g., "2h ago", "Yesterday") */
	UFUNCTION(BlueprintPure, Category = "Atlas|History", meta = (DisplayName = "Get Time Ago String"))
	static FString GetHistoryTimeAgoString(const FAtlasJobHistoryRecord& Record);

	/** Get date category for grouping (Today, Yesterday, Older) */
	UFUNCTION(BlueprintPure, Category = "Atlas|History", meta = (DisplayName = "Get Date Category"))
	static FString GetHistoryDateCategory(const FAtlasJobHistoryRecord& Record);

	/** Get formatted timestamp with duration (e.g., "Jan 29, 19:33 • 01:25") */
	UFUNCTION(BlueprintPure, Category = "Atlas|History", meta = (DisplayName = "Get Formatted Timestamp"))
	static FString GetHistoryFormattedTimestamp(const FAtlasJobHistoryRecord& Record);

	/** Create a history query for all records */
	UFUNCTION(BlueprintPure, Category = "Atlas|History", meta = (DisplayName = "Make All History Query"))
	static FAtlasHistoryQuery MakeAllHistoryQuery();

	/** Create a history query for a specific workflow */
	UFUNCTION(BlueprintPure, Category = "Atlas|History", meta = (DisplayName = "Make Workflow History Query"))
	static FAtlasHistoryQuery MakeWorkflowHistoryQuery(const FString& ApiId);

	/** Create a history query for today's records */
	UFUNCTION(BlueprintPure, Category = "Atlas|History", meta = (DisplayName = "Make Today History Query"))
	static FAtlasHistoryQuery MakeTodayHistoryQuery();

	/** Create a history query for successful records only */
	UFUNCTION(BlueprintPure, Category = "Atlas|History", meta = (DisplayName = "Make Success Only Query"))
	static FAtlasHistoryQuery MakeSuccessOnlyQuery();

	/** Create a history query for failed records only */
	UFUNCTION(BlueprintPure, Category = "Atlas|History", meta = (DisplayName = "Make Failed Only Query"))
	static FAtlasHistoryQuery MakeFailedOnlyQuery();

	/** Create a history query for the last 7 days */
	UFUNCTION(BlueprintPure, Category = "Atlas|History", meta = (DisplayName = "Make Last 7 Days Query"))
	static FAtlasHistoryQuery MakeLast7DaysQuery();

	/** Create a history query for the last 30 days */
	UFUNCTION(BlueprintPure, Category = "Atlas|History", meta = (DisplayName = "Make Last 30 Days Query"))
	static FAtlasHistoryQuery MakeLast30DaysQuery();

	/** Set status filter on a query */
	UFUNCTION(BlueprintCallable, Category = "Atlas|History", meta = (DisplayName = "Set Query Status Filter"))
	static void SetQueryStatusFilter(UPARAM(ref) FAtlasHistoryQuery& Query, EAtlasJobStatus Status);

	/** Set workflow filter on a query */
	UFUNCTION(BlueprintCallable, Category = "Atlas|History", meta = (DisplayName = "Set Query Workflow Filter"))
	static void SetQueryWorkflowFilter(UPARAM(ref) FAtlasHistoryQuery& Query, const FString& ApiId);

	/** Set date range filter on a query (Today, Last7Days, Last30Days) */
	UFUNCTION(BlueprintCallable, Category = "Atlas|History", meta = (DisplayName = "Set Query Date Filter"))
	static void SetQueryDateFilter(UPARAM(ref) FAtlasHistoryQuery& Query, bool bFilterByDate, const FDateTime& DateFrom, const FDateTime& DateTo);

	/** Clear all filters from a query */
	UFUNCTION(BlueprintCallable, Category = "Atlas|History", meta = (DisplayName = "Clear Query Filters"))
	static void ClearQueryFilters(UPARAM(ref) FAtlasHistoryQuery& Query);
};
