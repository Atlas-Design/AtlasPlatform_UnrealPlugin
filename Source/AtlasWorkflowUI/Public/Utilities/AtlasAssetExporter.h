// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AtlasAssetExporter.generated.h"

class UTexture2D;
class UStaticMesh;

/**
 * Result of an asset export operation.
 */
USTRUCT(BlueprintType)
struct ATLASWORKFLOWUI_API FAtlasExportResult
{
	GENERATED_BODY()

	/** Whether the export was successful */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Export")
	bool bSuccess = false;

	/** The path to the exported file (if successful) */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Export")
	FString FilePath;

	/** Error message if export failed */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Export")
	FString ErrorMessage;

	/** Whether the file is a temp export (should be cleaned up) */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Export")
	bool bIsTempExport = false;
};

/**
 * Utility class for exporting Unreal assets to disk for Atlas platform upload.
 * 
 * When a user selects a Content Browser asset (texture, mesh), we need an actual
 * file path for the Atlas platform. This class handles:
 * 
 * 1. Checking if the original source file still exists
 * 2. If not, exporting the asset to a temp folder
 * 3. Cleaning up old temp exports
 */
UCLASS(BlueprintType)
class ATLASWORKFLOWUI_API UAtlasAssetExporter : public UObject
{
	GENERATED_BODY()

public:
	// ==================== Main Export Functions ====================

	/**
	 * Get a valid file path for an asset.
	 * 
	 * First checks if the original source file exists at the import path.
	 * If not found, exports the asset to a temp folder.
	 * 
	 * @param Asset The asset to get a file path for (UTexture2D or UStaticMesh)
	 * @return Export result with file path or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Export")
	static FAtlasExportResult GetValidFilePath(UObject* Asset);

	/**
	 * Export a texture to PNG format.
	 * @param Texture The texture to export
	 * @param OutputPath Optional specific output path (uses temp folder if empty)
	 * @return Export result
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Export")
	static FAtlasExportResult ExportTextureToPNG(UTexture2D* Texture, const FString& OutputPath = TEXT(""));

	/**
	 * Export a static mesh to FBX format.
	 * @param Mesh The mesh to export
	 * @param OutputPath Optional specific output path (uses temp folder if empty)
	 * @return Export result
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Export")
	static FAtlasExportResult ExportMeshToFBX(UStaticMesh* Mesh, const FString& OutputPath = TEXT(""));

	// ==================== Source File Lookup ====================

	/**
	 * Try to get the original source file path for an asset.
	 * Uses FAssetImportInfo to find the original import location.
	 * @param Asset The asset to look up
	 * @param OutSourcePath The original source path (if found)
	 * @return True if a valid source file was found
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Export")
	static bool GetSourceFilePath(UObject* Asset, FString& OutSourcePath);

	// ==================== Temp Folder Management ====================

	/**
	 * Get the temp export folder path.
	 * Default: {Project}/Saved/Atlas/TempExport/
	 * @return Absolute path to temp export folder
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|Export")
	static FString GetTempExportFolder();

	/**
	 * Clean up old temp exports.
	 * Deletes files older than the specified age.
	 * @param MaxAgeHours Maximum age in hours (default 24)
	 * @return Number of files deleted
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Export")
	static int32 CleanupTempExports(int32 MaxAgeHours = 24);

	/**
	 * Ensure the temp export folder exists.
	 * @return True if folder exists or was created
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Export")
	static bool EnsureTempFolderExists();

private:
	/** Generate a unique filename for temp export */
	static FString GenerateTempFilename(const FString& BaseName, const FString& Extension);
};
