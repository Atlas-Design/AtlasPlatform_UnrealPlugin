// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AtlasSDKSettings.generated.h"

/**
 * Atlas SDK Settings - configurable via Project Settings -> Plugins -> Atlas SDK
 * 
 * These settings control where downloaded files are saved and default import paths
 * for assets created from workflow outputs.
 */
UCLASS(Config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "Atlas SDK"))
class ATLASSDK_API UAtlasSDKSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAtlasSDKSettings();

	// ==================== Output Folder Settings ====================

	/**
	 * Root folder where downloaded workflow outputs are saved.
	 * Relative paths are resolved from the project directory.
	 * Default: {Project}/Saved/Atlas/Output/
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Output")
	FDirectoryPath OutputFolder;

	/**
	 * When enabled, automatically organize files into subfolders by type:
	 * - Images go to OutputFolder/Images/
	 * - Meshes go to OutputFolder/Meshes/
	 * When disabled, all files are saved directly to OutputFolder.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Output")
	bool bAutoOrganizeByType;

	// ==================== Import Settings ====================

	/**
	 * Default Content Browser path where imported assets are created.
	 * Example: /Game/Atlas/Imported
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Import", meta = (ContentDir))
	FDirectoryPath DefaultImportPath;

	/**
	 * When importing textures, automatically set these compression settings.
	 * If empty, uses Unreal's default texture settings.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Import|Textures")
	bool bCompressImportedTextures;

	// ==================== Execution Settings ====================

	/**
	 * HTTP request timeout in seconds.
	 * Applies to upload, execute, and download requests.
	 * Default: 120 seconds (2 minutes)
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Execution", meta = (ClampMin = "10", ClampMax = "600"))
	float RequestTimeoutSeconds;

	/**
	 * Status polling interval in seconds during async execution.
	 * Lower values = more responsive but more API calls.
	 * Default: 2 seconds
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Execution", meta = (ClampMin = "0.5", ClampMax = "30"))
	float StatusPollIntervalSeconds;

	/**
	 * Maximum time to wait for job completion in seconds.
	 * Jobs taking longer than this will be considered timed out.
	 * Default: 600 seconds (10 minutes)
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Execution", meta = (ClampMin = "30", ClampMax = "3600"))
	float MaxExecutionTimeSeconds;

	// ==================== Cache Settings ====================

	/**
	 * Enable upload caching by content hash.
	 * When enabled, files with the same content won't be re-uploaded.
	 * Default: true
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cache")
	bool bEnableUploadCache;

	/**
	 * Maximum number of FileId entries to keep in the upload cache.
	 * Older entries are evicted when the limit is reached.
	 * Default: 100
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cache", meta = (ClampMin = "10", ClampMax = "1000", EditCondition = "bEnableUploadCache"))
	int32 MaxUploadCacheEntries;

	/**
	 * Maximum age of cache entries in hours before they're considered stale.
	 * Server may invalidate FileIds after some time.
	 * Default: 24 hours
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cache", meta = (ClampMin = "1", ClampMax = "168", EditCondition = "bEnableUploadCache"))
	int32 CacheMaxAgeHours;

	// ==================== History Settings ====================

	/**
	 * Maximum number of history records to keep per workflow.
	 * Oldest records are automatically removed when limit is exceeded.
	 * Set to 0 for unlimited history.
	 * Default: 100
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "History", meta = (ClampMin = "0", ClampMax = "10000"))
	int32 MaxHistoryRecordsPerWorkflow;

	/**
	 * Automatically save output files when job completes.
	 * If disabled, outputs remain as in-memory bytes until explicitly saved.
	 * Default: true
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "History")
	bool bAutoSaveOutputFiles;

	// ==================== Helpers ====================

	/** Get the resolved output folder path (absolute) */
	UFUNCTION(BlueprintPure, Category = "Atlas|Settings")
	FString GetOutputFolderPath() const;

	/** Get the images subfolder path (absolute) */
	UFUNCTION(BlueprintPure, Category = "Atlas|Settings")
	FString GetImagesFolderPath() const;

	/** Get the meshes subfolder path (absolute) */
	UFUNCTION(BlueprintPure, Category = "Atlas|Settings")
	FString GetMeshesFolderPath() const;

	/** Get the default import path for Content Browser */
	UFUNCTION(BlueprintPure, Category = "Atlas|Settings")
	FString GetDefaultImportPathString() const;

	/** Get the singleton settings instance */
	UFUNCTION(BlueprintPure, Category = "Atlas|Settings", meta = (DisplayName = "Get Atlas SDK Settings"))
	static const UAtlasSDKSettings* Get();

	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override { return FName("Plugins"); }
#if WITH_EDITOR
	virtual FText GetSectionText() const override { return FText::FromString("Atlas SDK"); }
	virtual FText GetSectionDescription() const override { return FText::FromString("Configure Atlas SDK output folders and import settings"); }
#endif
};
