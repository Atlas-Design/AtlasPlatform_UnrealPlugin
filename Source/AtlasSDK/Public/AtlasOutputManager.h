// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Types/AtlasHistoryTypes.h"
#include "AtlasOutputManager.generated.h"

class UAtlasJob;

/**
 * File category for organizing outputs
 */
UENUM(BlueprintType)
enum class EAtlasFileCategory : uint8
{
	/** Image files: .png, .jpg, .jpeg, .bmp, .tga, .exr */
	Image,
	
	/** Mesh files: .glb, .gltf, .fbx, .obj */
	Mesh,
	
	/** Any other file type */
	Other
};

/**
 * Result of a save operation
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasSaveResult
{
	GENERATED_BODY()

	/** Whether the save succeeded */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Output")
	bool bSuccess = false;

	/** Full path where the file was saved */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Output")
	FString FilePath;

	/** Error message if save failed */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Output")
	FString ErrorMessage;

	/** File size in bytes */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Output")
	int64 FileSize = 0;
};

/**
 * Result of an import operation
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasImportResult
{
	GENERATED_BODY()

	/** Whether the import succeeded */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Output")
	bool bSuccess = false;

	/** The imported asset (UTexture2D, UStaticMesh, etc.) */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Output")
	TObjectPtr<UObject> ImportedAsset;

	/** Package path where asset was created */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Output")
	FString PackagePath;

	/** Error message if import failed */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Output")
	FString ErrorMessage;
};

/**
 * Information about a job's output folder path.
 * Used for organizing downloaded files by workflow and job.
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasJobFolderInfo
{
	GENERATED_BODY()

	/** The workflow name (sanitized for filesystem) */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Output")
	FString WorkflowName;

	/** The job folder name (format: YYYY-MM-DD_HHMMSS_shortId) */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Output")
	FString JobFolderName;

	/** Full Content Browser path for this job */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Output")
	FString ContentBrowserPath;

	/** Full disk path for this job's output files */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Output")
	FString DiskPath;

	/** Check if this info is valid */
	bool IsValid() const
	{
		return !WorkflowName.IsEmpty() && !JobFolderName.IsEmpty();
	}
};

/**
 * Manages saving downloaded files to disk and importing them as Unreal assets.
 * 
 * Features:
 * - Save downloaded bytes to configurable output folder
 * - Auto-organize files by type (Images/, Meshes/ subfolders)
 * - Import PNG files as UTexture2D assets
 * - Import GLB/GLTF files as UStaticMesh assets
 * - File type detection from extensions
 * 
 * The OutputManager uses settings from UAtlasSDKSettings (Project Settings -> Plugins -> Atlas SDK).
 */
UCLASS(BlueprintType)
class ATLASSDK_API UAtlasOutputManager : public UObject
{
	GENERATED_BODY()

public:
	UAtlasOutputManager();

	// ==================== Save Utilities ====================

	/**
	 * Save bytes to the output folder.
	 * If bAutoOrganizeByType is enabled in settings, files are auto-sorted into subfolders.
	 * @param Bytes The file content
	 * @param FileName The filename (with extension)
	 * @return Save result with path and success status
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output")
	FAtlasSaveResult SaveToOutputFolder(const TArray<uint8>& Bytes, const FString& FileName);

	/**
	 * Save bytes to a specific subfolder within the output folder.
	 * @param Bytes The file content
	 * @param FileName The filename (with extension)
	 * @param SubFolder Subfolder name (e.g., "MyProject/Results")
	 * @return Save result with path and success status
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output")
	FAtlasSaveResult SaveToOutputSubfolder(const TArray<uint8>& Bytes, const FString& FileName, const FString& SubFolder);

	/**
	 * Save image bytes to the Images subfolder.
	 * Convenience method that always uses the Images/ subfolder regardless of auto-organize setting.
	 * @param Bytes The image file content
	 * @param FileName The filename (with extension, e.g., "result.png")
	 * @return Save result with path and success status
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output")
	FAtlasSaveResult SaveImageToOutputFolder(const TArray<uint8>& Bytes, const FString& FileName);

	/**
	 * Save mesh bytes to the Meshes subfolder.
	 * Convenience method that always uses the Meshes/ subfolder regardless of auto-organize setting.
	 * @param Bytes The mesh file content
	 * @param FileName The filename (with extension, e.g., "model.glb")
	 * @return Save result with path and success status
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output")
	FAtlasSaveResult SaveMeshToOutputFolder(const TArray<uint8>& Bytes, const FString& FileName);

	// ==================== Job-Based Output Saving ====================

	/**
	 * Save file bytes to the job-specific output folder on disk.
	 * Creates folder structure: {OutputFolder}/{WorkflowName}/{JobFolder}/{FileName}
	 * @param Job The job that produced this output
	 * @param Bytes The file content
	 * @param FileName The filename (with extension)
	 * @return Save result with path and success status
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output")
	FAtlasSaveResult SaveToJobFolder(UAtlasJob* Job, const TArray<uint8>& Bytes, const FString& FileName);

	/**
	 * Check if a file exists in a job's output folder on disk.
	 * @param Job The job to check
	 * @param FileName The filename to look for
	 * @return True if the file exists on disk
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output")
	bool HasOutputFileForJob(UAtlasJob* Job, const FString& FileName) const;

	/**
	 * Check if a file exists in a job's output folder on disk (using history record).
	 * @param HistoryRecord The history record to check
	 * @param FileName The filename to look for
	 * @return True if the file exists on disk
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output")
	bool HasOutputFileForJobFromHistory(const FAtlasJobHistoryRecord& HistoryRecord, const FString& FileName) const;

	/**
	 * Get the full disk path to a file in a job's output folder.
	 * @param Job The job
	 * @param FileName The filename
	 * @return Full absolute path to the file (may not exist)
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output")
	FString GetOutputFilePathForJob(UAtlasJob* Job, const FString& FileName) const;

	/**
	 * Get the full disk path to a file in a job's output folder (using history record).
	 * @param HistoryRecord The history record
	 * @param FileName The filename
	 * @return Full absolute path to the file (may not exist)
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output")
	FString GetOutputFilePathForJobFromHistory(const FAtlasJobHistoryRecord& HistoryRecord, const FString& FileName) const;

	/**
	 * List all files in a job's output folder on disk.
	 * @param Job The job to list files for
	 * @return Array of full file paths
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output")
	TArray<FString> GetAllOutputFilesForJob(UAtlasJob* Job) const;

	/**
	 * List all files in a job's output folder on disk (using history record).
	 * @param HistoryRecord The history record to list files for
	 * @return Array of full file paths
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output")
	TArray<FString> GetAllOutputFilesForJobFromHistory(const FAtlasJobHistoryRecord& HistoryRecord) const;

	// ==================== Folder Access ====================

	/**
	 * Get the configured output folder path (absolute).
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output")
	FString GetOutputFolder() const;

	/**
	 * Get the Images subfolder path (absolute).
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output")
	FString GetImagesFolder() const;

	/**
	 * Get the Meshes subfolder path (absolute).
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output")
	FString GetMeshesFolder() const;

	/**
	 * Ensure the output folder and subfolders exist.
	 * Creates directories if they don't exist.
	 * @return True if all folders exist or were created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output")
	bool EnsureOutputFolderExists();

	// ==================== File Type Detection ====================

	/**
	 * Check if a filename represents an image file based on extension.
	 * Supports: .png, .jpg, .jpeg, .bmp, .tga, .exr, .hdr
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output")
	static bool IsImageFile(const FString& FileName);

	/**
	 * Check if a filename represents a mesh file based on extension.
	 * Supports: .glb, .gltf, .fbx, .obj
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output")
	static bool IsMeshFile(const FString& FileName);

	/**
	 * Get the file category based on extension.
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output")
	static EAtlasFileCategory GetFileCategory(const FString& FileName);

	// ==================== Thumbnail / Preview Utilities ====================

	/**
	 * Create a transient UTexture2D from image bytes (PNG, JPEG, etc.).
	 * The texture is NOT saved to the project - it exists only in memory for display purposes.
	 * Useful for showing thumbnails/previews in UI without importing assets.
	 * 
	 * @param ImageBytes Raw image file bytes (PNG, JPEG, BMP, etc.)
	 * @return The created texture, or nullptr if creation failed
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output|Preview")
	UTexture2D* CreateTextureFromImageBytes(const TArray<uint8>& ImageBytes);

	/**
	 * Load a texture from a file on disk for preview purposes.
	 * The texture is NOT imported into the project - it exists only in memory.
	 * Useful for showing thumbnails of saved output files.
	 * 
	 * @param FilePath Absolute path to the image file
	 * @return The loaded texture, or nullptr if loading failed
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output|Preview")
	UTexture2D* LoadTextureFromFile(const FString& FilePath);

	// ==================== Job-Based Path Helpers ====================

	/**
	 * Generate a short ID from a job GUID (first 8 hex characters).
	 * @param JobId The job's unique GUID
	 * @return 8-character lowercase hex string
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output")
	static FString GenerateShortId(const FGuid& JobId);

	/**
	 * Generate the job folder name from job metadata.
	 * Format: YYYY-MM-DD_HHMMSS_shortId (e.g., "2026-02-03_141858_99c451b3")
	 * @param StartedAt When the job started
	 * @param JobId The job's unique GUID
	 * @return The formatted folder name
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output")
	static FString GenerateJobFolderName(const FDateTime& StartedAt, const FGuid& JobId);

	/**
	 * Sanitize a workflow name for use in filesystem paths.
	 * Removes invalid characters and replaces spaces with underscores.
	 * @param WorkflowName The original workflow name
	 * @return Sanitized name safe for filesystem use
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output")
	static FString SanitizeWorkflowName(const FString& WorkflowName);

	/**
	 * Get complete folder information for a job.
	 * Creates the path structure: {DefaultImportPath}/{WorkflowName}/{JobFolder}/
	 * @param Job The job to get folder info for
	 * @return Folder info with paths for Content Browser and disk
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output")
	FAtlasJobFolderInfo GetJobFolderInfo(UAtlasJob* Job) const;

	/**
	 * Get complete folder information from a history record.
	 * Creates the path structure: {DefaultImportPath}/{WorkflowName}/{JobFolder}/
	 * @param HistoryRecord The history record to get folder info for
	 * @return Folder info with paths for Content Browser and disk
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output")
	FAtlasJobFolderInfo GetJobFolderInfoFromHistory(const FAtlasJobHistoryRecord& HistoryRecord) const;

	// ==================== Import Utilities (Editor Only) ====================

#if WITH_EDITOR
	/**
	 * Import a PNG/image file as a UTexture2D asset in the Content Browser.
	 * @param FilePath Absolute path to the image file on disk
	 * @param PackagePath Content Browser path (e.g., "/Game/Atlas/Textures")
	 * @return Import result with the created asset
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output|Import", meta = (CallInEditor = "true"))
	FAtlasImportResult ImportTextureAsset(const FString& FilePath, const FString& PackagePath);

	/**
	 * Import a GLB/GLTF/FBX file as a static mesh asset.
	 * Uses Unreal's Interchange system for GLB/GLTF files.
	 * @param FilePath Absolute path to the mesh file on disk
	 * @param PackagePath Content Browser path (e.g., "/Game/Atlas/Meshes")
	 * @return Import result with the created asset
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output|Import", meta = (CallInEditor = "true"))
	FAtlasImportResult ImportMeshAsset(const FString& FilePath, const FString& PackagePath);

	/**
	 * Import a mesh file and spawn it in the current level.
	 * Combines ImportMeshAsset with spawning a StaticMeshActor in the scene.
	 * @param FilePath Absolute path to the mesh file on disk
	 * @param PackagePath Content Browser path (e.g., "/Game/Atlas/Meshes")
	 * @param SpawnLocation World location to spawn the mesh (default: origin)
	 * @param SpawnRotation World rotation for the spawned mesh (default: no rotation)
	 * @return Import result with the created asset (ImportedAsset will be the StaticMesh, not the Actor)
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output|Import", meta = (CallInEditor = "true"))
	FAtlasImportResult ImportMeshAndSpawnInScene(const FString& FilePath, const FString& PackagePath, 
		FVector SpawnLocation = FVector::ZeroVector, FRotator SpawnRotation = FRotator::ZeroRotator);

	/**
	 * Open Unreal's native import dialog for a file.
	 * User can then configure import settings before importing.
	 * @param FilePath Absolute path to the file
	 * @return True if dialog was opened successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output|Import", meta = (CallInEditor = "true"))
	bool OpenImportDialog(const FString& FilePath);

	/**
	 * Get the default import path from settings.
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output|Import")
	FString GetDefaultImportPath() const;

	/**
	 * Get the Content Browser import path for a specific job.
	 * Path format: {DefaultImportPath}/{WorkflowName}/{YYYY-MM-DD_HHMMSS_shortId}/
	 * @param Job The job to get the import path for
	 * @return Content Browser path (e.g., "/Game/Atlas/Imported/ImageToMesh/2026-02-03_141858_99c451b3")
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output|Import")
	FString GetJobImportPath(UAtlasJob* Job) const;

	// ==================== Simplified Import API ====================

	/**
	 * Import a job output by its output name - THE SIMPLE WAY.
	 * 
	 * This is the recommended function to use. It handles everything automatically:
	 * - Checks if asset already imported → returns existing
	 * - Saves file to disk if needed
	 * - Imports to correct folder structure
	 * - Returns the result
	 * 
	 * @param Job The job that produced the output
	 * @param OutputName The name of the output parameter (from workflow schema, e.g., "mesh", "output_image")
	 * @return Import result with Success, ImportedAsset, PackagePath, ErrorMessage
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output|Import", meta = (CallInEditor = "true"))
	FAtlasImportResult ImportJobOutput(UAtlasJob* Job, const FString& OutputName);

	/**
	 * Import a job output using a history record - THE SIMPLE WAY (for UI).
	 * 
	 * Use this when you have a history record instead of a job object.
	 * This is common in UI elements that display job history.
	 * 
	 * It handles everything automatically:
	 * - Uses file paths already saved in history record if available
	 * - Checks if asset already imported → returns existing
	 * - Saves file to disk if needed
	 * - Imports to correct folder structure
	 * - Returns the result
	 * 
	 * @param HistoryRecord The history record containing job info and outputs
	 * @param OutputName The name of the output parameter (from workflow schema, e.g., "mesh", "output_image")
	 * @return Import result with Success, ImportedAsset, PackagePath, ErrorMessage
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output|Import", meta = (CallInEditor = "true"))
	FAtlasImportResult ImportJobOutputFromHistory(const FAtlasJobHistoryRecord& HistoryRecord, const FString& OutputName);

	/**
	 * Check if a job output is already imported (by output name) - THE SIMPLE WAY.
	 * 
	 * Use this to check if an asset exists without triggering an import.
	 * Same API as ImportJobOutput but only checks, never imports.
	 * 
	 * @param Job The job that produced the output
	 * @param OutputName The name of the output parameter (from workflow schema, e.g., "mesh", "output_image")
	 * @return Import result - Success=true if found, ImportedAsset contains the existing asset
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output|Import")
	FAtlasImportResult FindImportedJobOutput(UAtlasJob* Job, const FString& OutputName) const;

	/**
	 * Check if a job output is already imported (by output name) using history record.
	 * 
	 * Use this to check if an asset exists without triggering an import.
	 * Same API as ImportJobOutputFromHistory but only checks, never imports.
	 * 
	 * @param HistoryRecord The history record containing job info and outputs
	 * @param OutputName The name of the output parameter (from workflow schema, e.g., "mesh", "output_image")
	 * @return Import result - Success=true if found, ImportedAsset contains the existing asset
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output|Import")
	FAtlasImportResult FindImportedJobOutputFromHistory(const FAtlasJobHistoryRecord& HistoryRecord, const FString& OutputName) const;

	// ==================== Lower-Level Import Functions ====================

	/**
	 * Import a texture to the job-specific folder.
	 * Automatically creates workflow/job subfolder structure.
	 * @param Job The job that produced this output
	 * @param FilePath Absolute path to the image file on disk
	 * @return Import result with the created asset
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output|Import", meta = (CallInEditor = "true"))
	FAtlasImportResult ImportTextureForJob(UAtlasJob* Job, const FString& FilePath);

	/**
	 * Import a mesh to the job-specific folder.
	 * Automatically creates workflow/job subfolder structure.
	 * @param Job The job that produced this output
	 * @param FilePath Absolute path to the mesh file on disk
	 * @return Import result with the created asset
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output|Import", meta = (CallInEditor = "true"))
	FAtlasImportResult ImportMeshForJob(UAtlasJob* Job, const FString& FilePath);

	/**
	 * Import a mesh for a job and spawn it in the current level.
	 * @param Job The job that produced this output
	 * @param FilePath Absolute path to the mesh file on disk
	 * @param SpawnLocation World location to spawn the mesh (default: origin)
	 * @param SpawnRotation World rotation for the spawned mesh (default: no rotation)
	 * @return Import result with the created asset
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output|Import", meta = (CallInEditor = "true"))
	FAtlasImportResult ImportMeshForJobAndSpawn(UAtlasJob* Job, const FString& FilePath,
		FVector SpawnLocation = FVector::ZeroVector, FRotator SpawnRotation = FRotator::ZeroRotator);

	// ==================== Asset Lookup Utilities ====================

	/**
	 * Find an imported asset in the job's folder by filename.
	 * Searches the job-specific folder for an asset matching the given filename.
	 * @param Job The job to search for
	 * @param FileName The original filename (e.g., "output.png", "model.glb")
	 * @return The found asset, or nullptr if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output|Import")
	UObject* FindImportedAssetForJob(UAtlasJob* Job, const FString& FileName) const;

	/**
	 * Find an imported asset in the job's folder by filename (using history record).
	 * @param HistoryRecord The history record to search for
	 * @param FileName The original filename (e.g., "output.png", "model.glb")
	 * @return The found asset, or nullptr if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output|Import")
	UObject* FindImportedAssetForJobFromHistory(const FAtlasJobHistoryRecord& HistoryRecord, const FString& FileName) const;

	/**
	 * Check if an imported asset exists for a job.
	 * @param Job The job to check
	 * @param FileName The filename to look for
	 * @return True if an asset exists in the job's import folder
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output|Import")
	bool HasImportedAssetForJob(UAtlasJob* Job, const FString& FileName) const;

	/**
	 * Check if an imported asset exists for a job (using history record).
	 * @param HistoryRecord The history record to check
	 * @param FileName The filename to look for
	 * @return True if an asset exists in the job's import folder
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Output|Import")
	bool HasImportedAssetForJobFromHistory(const FAtlasJobHistoryRecord& HistoryRecord, const FString& FileName) const;

	/**
	 * Get all imported assets in a job's folder.
	 * @param Job The job to get assets for
	 * @return Array of all assets in the job's import folder
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output|Import")
	TArray<UObject*> GetAllImportedAssetsForJob(UAtlasJob* Job) const;

	/**
	 * Get all imported assets in a job's folder (using history record).
	 * @param HistoryRecord The history record to get assets for
	 * @return Array of all assets in the job's import folder
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Output|Import")
	TArray<UObject*> GetAllImportedAssetsForJobFromHistory(const FAtlasJobHistoryRecord& HistoryRecord) const;
#endif

protected:
	/** Internal save implementation */
	FAtlasSaveResult SaveBytesToPath(const TArray<uint8>& Bytes, const FString& FullPath);
};
