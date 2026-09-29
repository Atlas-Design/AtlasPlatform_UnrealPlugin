// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasSDKSettings.h"
#include "AtlasCredentialSettings.h"
#include "AtlasRuntimeSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "AtlasPlatformAuth.h"
#include "Misc/Paths.h"
#include "GenericPlatform/GenericPlatformMisc.h"

UAtlasSDKSettings::UAtlasSDKSettings()
{
	// Default output folder: {Project}/Saved/Atlas/Output/
	OutputFolder.Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Atlas"), TEXT("Output"));
	
	// Auto-organize by type is enabled by default
	bAutoOrganizeByType = true;

	// Default temp folders for export/import operations
	TempExportPath = TEXT("Saved/Atlas/TempExports");
	TempImportPath = TEXT("Saved/Atlas/TempImports");
	
	// Default import path in Content Browser
	DefaultImportPath.Path = TEXT("/Game/Atlas/Imported");
	
	// Don't compress by default (let Unreal use its defaults)
	bCompressImportedTextures = false;

	// Authentication (v0.2+)
	WorkspaceApiKey.Empty();
	TrustedApiOrigins = { TEXT("https://api.dev.atlas.design"), TEXT("https://api.prod.atlas.design") };
	bReadApiKeyFromEnvironment = true;

	// Execution settings
	RequestTimeoutSeconds = 120.0f;      // 2 minutes per HTTP request
	StatusPollIntervalSeconds = 5.0f;    // Poll every 5 seconds
	MaxExecutionTimeSeconds = 900.0f;    // 15 minutes max

	// Cache settings
	bEnableUploadCache = true;
	MaxUploadCacheEntries = 100;
	CacheMaxAgeHours = 24;

	// History settings
	MaxHistoryRecordsPerWorkflow = 100;
	StaleJobThresholdHours = 48;
	bAutoSaveOutputFiles = true;

	// Temp storage settings
	TempStorageWarningMB = 500;
	bCleanTempOnEditorStart = true;
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

FString UAtlasSDKSettings::GetTempExportFolderPath() const
{
	const FString RelativePath = !TempExportPath.IsEmpty() 
		? TempExportPath 
		: TEXT("Saved/Atlas/TempExports");
	FString Resolved = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), RelativePath);
	FPaths::NormalizeDirectoryName(Resolved);
	return Resolved;
}

FString UAtlasSDKSettings::GetTempImportFolderPath() const
{
	const FString RelativePath = !TempImportPath.IsEmpty() 
		? TempImportPath 
		: TEXT("Saved/Atlas/TempImports");
	FString Resolved = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), RelativePath);
	FPaths::NormalizeDirectoryName(Resolved);
	return Resolved;
}

const UAtlasSDKSettings* UAtlasSDKSettings::Get()
{
	UAtlasSDKSettings* Settings = GetMutableDefault<UAtlasSDKSettings>();
	const UAtlasRuntimeSettings* Runtime = GetDefault<UAtlasRuntimeSettings>();
	if (Runtime->bUseRuntimeOverrides)
	{
		// Do not mutate the legacy CDO: disabling overrides must restore its existing preferences.
		static TStrongObjectPtr<UAtlasSDKSettings> Effective(NewObject<UAtlasSDKSettings>());
		for (TFieldIterator<FProperty> It(UAtlasSDKSettings::StaticClass()); It; ++It)
			if (It->HasAnyPropertyFlags(CPF_Config)) It->CopyCompleteValue_InContainer(Effective.Get(), Settings);
		Effective->WorkspaceApiKey = Settings->WorkspaceApiKey;
		Settings = Effective.Get();
		Settings->RequestTimeoutSeconds = Runtime->RequestTimeoutSeconds;
		Settings->StatusPollIntervalSeconds = Runtime->StatusPollIntervalSeconds;
		Settings->MaxExecutionTimeSeconds = Runtime->MaxExecutionTimeSeconds;
		Settings->bEnableUploadCache = Runtime->bEnableUploadCache;
		Settings->MaxUploadCacheEntries = Runtime->MaxUploadCacheEntries;
		Settings->CacheMaxAgeHours = Runtime->CacheMaxAgeHours;
		Settings->MaxDownloadCacheMB = Runtime->MaxDownloadCacheMB;
		Settings->MaxPreviewDimension = Runtime->MaxPreviewDimension;
		Settings->MaxPreviewCacheMB = Runtime->MaxPreviewCacheMB;
		Settings->TrustedApiOrigins = Runtime->TrustedApiOrigins;
	}
	return Settings;
}

FString UAtlasSDKSettings::GetResolvedWorkspaceApiKey() const
{
	FString Key;
#if WITH_EDITOR
	Key = GetDefault<UAtlasCredentialSettings>()->WorkspaceApiKey;
	if (Key.IsEmpty()) Key = WorkspaceApiKey; // Read-only migration fallback; never saved back to shared defaults.
#endif
	Key.TrimStartAndEndInline();
	if (!Key.IsEmpty())
	{
		return Key;
	}

	if (bReadApiKeyFromEnvironment)
	{
		FString EnvKey = FPlatformMisc::GetEnvironmentVariable(*FAtlasPlatformAuth::ApiKeyEnvironmentVariableName);
		EnvKey.TrimStartAndEndInline();
		return EnvKey;
	}

	return FString();
}

bool UAtlasSDKSettings::HasWorkspaceApiKey() const
{
	return !GetResolvedWorkspaceApiKey().IsEmpty();
}

FString UAtlasSDKSettings::GetAtlasDefaultExportFolder()
{
	const UAtlasSDKSettings* Settings = Get();
	return Settings ? Settings->GetTempExportFolderPath() : FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/Atlas/TempExports"));
}

FString UAtlasSDKSettings::GetAtlasDefaultImportFolder()
{
	const UAtlasSDKSettings* Settings = Get();
	return Settings ? Settings->GetTempImportFolderPath() : FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/Atlas/TempImports"));
}

void UAtlasSDKSettings::PostInitProperties()
{
	Super::PostInitProperties();
#if WITH_EDITOR
	// Preserve existing installations without rewriting or copying their secret files.
	if (HasAnyFlags(RF_ClassDefaultObject) && GConfig)
		GConfig->GetString(TEXT("/Script/AtlasSDK.AtlasSDKSettings"), TEXT("WorkspaceApiKey"), WorkspaceApiKey, GEditorPerProjectIni);
#endif
}
