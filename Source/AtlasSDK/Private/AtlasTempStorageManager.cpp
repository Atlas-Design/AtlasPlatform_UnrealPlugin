// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasTempStorageManager.h"
#include "AtlasSDKSettings.h"
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
	TotalDeleted += DeleteDirectoryContents(Settings->GetTempExportFolderPath());
	TotalDeleted += DeleteDirectoryContents(Settings->GetTempImportFolderPath());

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
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*DirectoryPath))
	{
		return 0;
	}

	int32 DeletedCount = 0;

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *DirectoryPath, TEXT("*"), true, false);

	for (const FString& FilePath : Files)
	{
		if (PlatformFile.DeleteFile(*FilePath))
		{
			DeletedCount++;
		}
	}

	// Clean up empty subdirectories
	TArray<FString> Directories;
	IFileManager::Get().FindFilesRecursive(Directories, *DirectoryPath, TEXT("*"), false, true);

	// Delete deepest directories first
	Directories.Sort([](const FString& A, const FString& B) { return A.Len() > B.Len(); });
	for (const FString& Dir : Directories)
	{
		PlatformFile.DeleteDirectory(*Dir);
	}

	return DeletedCount;
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
