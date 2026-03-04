// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasSDKSettings.h"
#include "Misc/Paths.h"

UAtlasSDKSettings::UAtlasSDKSettings()
{
	// Default output folder: {Project}/Saved/Atlas/Output/
	OutputFolder.Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Atlas"), TEXT("Output"));
	
	// Auto-organize by type is enabled by default
	bAutoOrganizeByType = true;
	
	// Default import path in Content Browser
	DefaultImportPath.Path = TEXT("/Game/Atlas/Imported");
	
	// Don't compress by default (let Unreal use its defaults)
	bCompressImportedTextures = false;

	// Execution settings
	RequestTimeoutSeconds = 120.0f;      // 2 minutes
	StatusPollIntervalSeconds = 2.0f;    // Poll every 2 seconds
	MaxExecutionTimeSeconds = 600.0f;    // 10 minutes max

	// Cache settings
	bEnableUploadCache = true;
	MaxUploadCacheEntries = 100;
	CacheMaxAgeHours = 24;

	// History settings
	MaxHistoryRecordsPerWorkflow = 100;
	bAutoSaveOutputFiles = true;
}

FString UAtlasSDKSettings::GetOutputFolderPath() const
{
	FString ResolvedPath = OutputFolder.Path;
	
	// Convert to absolute path (handles both relative and absolute paths correctly)
	// This properly resolves paths like "../../Users/..." without doubling them
	FPaths::CollapseRelativeDirectories(ResolvedPath);
	ResolvedPath = FPaths::ConvertRelativePathToFull(ResolvedPath);
	
	// Normalize the path
	FPaths::NormalizeDirectoryName(ResolvedPath);
	
	return ResolvedPath;
}

FString UAtlasSDKSettings::GetImagesFolderPath() const
{
	if (bAutoOrganizeByType)
	{
		return FPaths::Combine(GetOutputFolderPath(), TEXT("Images"));
	}
	return GetOutputFolderPath();
}

FString UAtlasSDKSettings::GetMeshesFolderPath() const
{
	if (bAutoOrganizeByType)
	{
		return FPaths::Combine(GetOutputFolderPath(), TEXT("Meshes"));
	}
	return GetOutputFolderPath();
}

FString UAtlasSDKSettings::GetDefaultImportPathString() const
{
	return DefaultImportPath.Path;
}

const UAtlasSDKSettings* UAtlasSDKSettings::Get()
{
	return GetDefault<UAtlasSDKSettings>();
}
