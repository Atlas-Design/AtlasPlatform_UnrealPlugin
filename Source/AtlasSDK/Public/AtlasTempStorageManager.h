// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AtlasTempStorageManager.generated.h"

/**
 * Static helpers for managing Atlas temp storage directories.
 *
 * Tracks the combined size of TempExports and TempImports,
 * provides cleanup, and fires editor notifications when the
 * total exceeds the configurable threshold.
 */
UCLASS()
class ATLASSDK_API UAtlasTempStorageManager : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Get the total size of all Atlas temp directories in bytes.
	 * Scans TempExports + TempImports recursively.
	 * @return Total size in bytes
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|TempStorage")
	static int64 GetTempDirectorySize();

	/**
	 * Get the total size formatted as a human-readable string.
	 * @return e.g. "245.3 MB"
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|TempStorage")
	static FString GetTempDirectorySizeFormatted();

	/**
	 * Delete all contents of the Atlas temp directories.
	 * The directories themselves are preserved (recreated if absent).
	 * @return Number of files deleted
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|TempStorage")
	static int32 CleanupTempDirectory();

	/**
	 * Check whether temp storage exceeds the warning threshold
	 * defined in UAtlasSDKSettings::TempStorageWarningMB.
	 * If exceeded, shows an editor notification.
	 * @return True if a warning was shown
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|TempStorage")
	static bool CheckAndWarnTempStorage();

private:
	static int64 GetDirectorySizeRecursive(const FString& DirectoryPath);
	static int32 DeleteDirectoryContents(const FString& DirectoryPath);
	static FString FormatBytes(int64 Bytes);
};
