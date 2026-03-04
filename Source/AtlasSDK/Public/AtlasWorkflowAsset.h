// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/AtlasSchemaTypes.h"
#include "Types/AtlasHistoryTypes.h"
#include "AtlasWorkflowAsset.generated.h"

class UAtlasJob;
class UAtlasJobManager;

/**
 * Data Asset for storing Atlas workflow configuration.
 * 
 * This asset stores workflow schema imported from the Atlas Platform JSON export.
 * It contains the API identifier, input/output definitions, and other metadata
 * needed to execute the workflow.
 * 
 * Usage:
 * 1. Create via Content Browser: Right-click → Atlas → Workflow Asset
 * 2. Paste workflow JSON into the JsonConfig property
 * 3. Schema will be automatically parsed and validated
 * 4. Use in Blueprints or C++ to execute workflows
 */
UCLASS(BlueprintType)
class ATLASSDK_API UAtlasWorkflowAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UAtlasWorkflowAsset();

	// ==================== Configuration ====================

	/**
	 * Raw JSON configuration from Atlas Platform export.
	 * Paste the workflow JSON here - schema will be parsed automatically.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Configuration", meta = (MultiLine = true))
	FString JsonConfig;

	/**
	 * Parsed workflow schema containing API ID, inputs, and outputs.
	 * Automatically populated when JsonConfig is parsed.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Atlas|Schema")
	FAtlasWorkflowSchema Schema;

	/**
	 * Last parse error message (empty if parse was successful).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Atlas|Schema")
	FString ParseError;

	/**
	 * Whether the schema was successfully parsed from JSON.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Atlas|Schema")
	bool bIsValid = false;

	// ==================== Parsing ====================

	/**
	 * Parse the JsonConfig and populate the Schema.
	 * Called automatically when JsonConfig changes in the editor.
	 * @return True if parsing was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Workflow")
	bool ParseFromJson();

	/**
	 * Parse schema from a JSON string (does not modify JsonConfig).
	 * @param JsonString The JSON to parse
	 * @param OutSchema The parsed schema (if successful)
	 * @param OutError Error message (if parsing failed)
	 * @return True if parsing was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Workflow")
	static bool ParseSchemaFromJson(const FString& JsonString, FAtlasWorkflowSchema& OutSchema, FString& OutError);

	// ==================== Accessors ====================

	/**
	 * Get the API ID for this workflow.
	 * @return The unique workflow identifier
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Workflow")
	FString GetApiId() const;

	/**
	 * Get the base URL for API calls.
	 * @return The base URL (e.g., "https://api.dev.atlas.design")
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Workflow")
	FString GetBaseUrl() const;

	/**
	 * Get the API version for this workflow.
	 * @return The version string (e.g., "0.1")
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Workflow")
	FString GetVersion() const;

	/**
	 * Get the human-readable workflow name.
	 * @return The workflow display name
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Workflow")
	FString GetWorkflowName() const;

	/**
	 * Get the workflow description.
	 * @return Description text
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Workflow")
	FString GetDescription() const;

	// ==================== URL Builders ====================

	/**
	 * Get the upload URL for this workflow.
	 * Pattern: {BaseUrl}/{Version}/upload/{ApiId}
	 * @return The complete upload URL
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Workflow|URLs")
	FString GetUploadUrl() const;

	/**
	 * Get the async execute URL for this workflow.
	 * Pattern: {BaseUrl}/{Version}/api_execute_async/{ApiId}
	 * @return The complete execute URL
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Workflow|URLs")
	FString GetExecuteUrl() const;

	/**
	 * Get the status polling URL for an execution.
	 * Pattern: {BaseUrl}/{Version}/api_status/{ExecutionId}
	 * @param ExecutionId The execution ID returned from execute call
	 * @return The complete status URL
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Workflow|URLs")
	FString GetStatusUrl(const FString& ExecutionId) const;

	/**
	 * Get the download URL for a specific file result.
	 * Pattern: {BaseUrl}/{Version}/download_binary_result/{ApiId}/{FileId}
	 * @param FileId The file ID to download
	 * @return The complete download URL
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Workflow|URLs")
	FString GetDownloadUrl(const FString& FileId) const;

	/**
	 * Check if the workflow asset is valid (has been successfully parsed).
	 * @return True if the schema is valid
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Workflow")
	bool IsValid() const;

	/**
	 * Get the parsed schema.
	 * @return Reference to the workflow schema
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Workflow")
	const FAtlasWorkflowSchema& GetSchema() const;

	/**
	 * Get all input parameter definitions.
	 * @return Array of input parameter definitions
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Workflow")
	TArray<FAtlasParameterDef> GetInputDefinitions() const;

	/**
	 * Get all output parameter definitions.
	 * @return Array of output parameter definitions
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Workflow")
	TArray<FAtlasParameterDef> GetOutputDefinitions() const;

	/**
	 * Create default inputs based on the schema.
	 * @return FAtlasWorkflowInputs populated with default values
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Workflow")
	FAtlasWorkflowInputs CreateDefaultInputs() const;

	/**
	 * Validate inputs against this workflow's schema.
	 * Requires exact 1:1 match - same count, same names, correct types.
	 * @param Inputs The inputs to validate
	 * @param OutErrors Array of validation error messages
	 * @return True if inputs exactly match the schema
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Workflow")
	bool ValidateInputs(const FAtlasWorkflowInputs& Inputs, TArray<FString>& OutErrors) const;

	// ==================== Import/Export ====================

	/**
	 * Import a workflow asset from a JSON file on disk.
	 * Creates a new UAtlasWorkflowAsset in the specified package.
	 * @param FilePath Path to the JSON file
	 * @param PackagePath Content Browser path for the new asset (e.g., "/Game/Workflows/")
	 * @param AssetName Name for the new asset
	 * @param OutError Error message if import fails
	 * @return The created asset, or nullptr if import failed
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Workflow", meta = (DisplayName = "Import Workflow From File"))
	static UAtlasWorkflowAsset* ImportFromFile(const FString& FilePath, const FString& PackagePath, const FString& AssetName, FString& OutError);

	/**
	 * Export the workflow JSON to a file.
	 * @param FilePath Path to save the JSON file
	 * @return True if export was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Workflow")
	bool ExportToFile(const FString& FilePath) const;

	// ==================== Job/History Accessors ====================

	/**
	 * Get all currently active jobs for this workflow.
	 * NOTE: This requires an active Job Manager. In editor, use UAtlasEditorSubsystem
	 * to access jobs. This method queries a globally registered manager if available.
	 * @param JobManager Optional job manager to query (if null, returns empty)
	 * @return Array of active job objects for this workflow's ApiId
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Workflow")
	TArray<UAtlasJob*> GetActiveJobs(UAtlasJobManager* JobManager) const;

	/**
	 * Get job history for this workflow.
	 * Queries the Job Manager's History Manager for completed jobs with matching ApiId.
	 * @param JobManager The job manager to query history from
	 * @return Array of history records for this workflow
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Workflow")
	TArray<FAtlasJobHistoryRecord> GetHistory(UAtlasJobManager* JobManager) const;

#if WITH_EDITOR
	// ==================== Editor Integration ====================

	/** Called when a property changes in the editor */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Internal helper to parse JSON object into schema */
	static bool ParseJsonObject(const TSharedPtr<class FJsonObject>& JsonObject, FAtlasWorkflowSchema& OutSchema, FString& OutError);

	/** Parse a parameter definition from JSON */
	static bool ParseParameterDef(const TSharedPtr<class FJsonObject>& JsonObject, FAtlasParameterDef& OutDef, FString& OutError);

	/** Convert string to EAtlasValueType */
	static EAtlasValueType StringToValueType(const FString& TypeString);
};
