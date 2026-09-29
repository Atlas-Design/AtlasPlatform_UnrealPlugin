// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasTempStorageManager.h"
#include "AtlasSDKSettings.h"
#include "AtlasTempStorageSafety.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"

int64 UAtlasTempStorageManager::GetTempDirectorySize()
{
	const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
	if (!Settings)
	{
		return 0;
	}

	int64 TotalSize = 0;
	TotalSize += GetDirectorySizeRecursive(Settings->GetTempExportFolderPath());
	TotalSize += GetDirectorySizeRecursive(Settings->GetTempImportFolderPath());
	return TotalSize;
}

FString UAtlasTempStorageManager::GetTempDirectorySizeFormatted()
{
	return FormatBytes(GetTempDirectorySize());
}

int32 UAtlasTempStorageManager::CleanupTempDirectory()
{
	const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
	if (!Settings)
	{
		return 0;
	}

	int32 TotalDeleted = 0;
	// Validate raw settings before path normalization can hide ambiguous components.
	TotalDeleted += DeleteDirectoryContents(Settings->TempExportPath.IsEmpty()
		? TEXT("Saved/Atlas/TempExports") : Settings->TempExportPath);
	TotalDeleted += DeleteDirectoryContents(Settings->TempImportPath.IsEmpty()
		? TEXT("Saved/Atlas/TempImports") : Settings->TempImportPath);

	if (TotalDeleted > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasTempStorageManager: Cleaned up %d temp file(s)"), TotalDeleted);
	}

	return TotalDeleted;
}

bool UAtlasTempStorageManager::CheckAndWarnTempStorage()
{
	const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
	if (!Settings)
	{
		return false;
	}

	int64 TotalBytes = GetTempDirectorySize();
	int64 ThresholdBytes = static_cast<int64>(Settings->TempStorageWarningMB) * 1024 * 1024;

	if (TotalBytes <= ThresholdBytes)
	{
		return false;
	}

	FString SizeStr = FormatBytes(TotalBytes);
	FString Message = FString::Printf(
		TEXT("Atlas temp storage is using %s (threshold: %d MB). Consider cleaning up via Project Settings > Atlas SDK."),
		*SizeStr, Settings->TempStorageWarningMB);

	UE_LOG(LogTemp, Warning, TEXT("AtlasTempStorageManager: %s"), *Message);

	return true;
}

// ==================== Internal Helpers ====================

int64 UAtlasTempStorageManager::GetDirectorySizeRecursive(const FString& DirectoryPath)
{
	int64 TotalSize = 0;

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*DirectoryPath))
	{
		return 0;
	}

	PlatformFile.IterateDirectoryRecursively(*DirectoryPath,
		[&TotalSize](const TCHAR* FilePath, bool bIsDirectory) -> bool
		{
			if (!bIsDirectory)
			{
				TotalSize += FPlatformFileManager::Get().GetPlatformFile().FileSize(FilePath);
			}
			return true;
		});

	return TotalSize;
}

int32 UAtlasTempStorageManager::DeleteDirectoryContents(const FString& DirectoryPath)
{
	const AtlasTempStorage::FContext Context{
		FPaths::ProjectDir(), FPaths::ProjectSavedDir(), FPaths::EngineDir(),
		UAtlasSDKSettings::Get()->GetOutputFolderPath() };
	FString Error;
	const int32 Deleted = AtlasTempStorage::CleanupDirectory(DirectoryPath, Context, Error);
	if (!Error.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Atlas temp cleanup [%s]: %s"), *DirectoryPath, *Error);
	}
	return Deleted;
}

FString UAtlasTempStorageManager::FormatBytes(int64 Bytes)
{
	if (Bytes < 1024)
	{
		return FString::Printf(TEXT("%lld B"), Bytes);
	}
	else if (Bytes < 1024 * 1024)
	{
		return FString::Printf(TEXT("%.1f KB"), Bytes / 1024.0);
	}
	else if (Bytes < 1024LL * 1024 * 1024)
	{
		return FString::Printf(TEXT("%.1f MB"), Bytes / (1024.0 * 1024.0));
	}
	else
	{
		return FString::Printf(TEXT("%.2f GB"), Bytes / (1024.0 * 1024.0 * 1024.0));
	}
}
