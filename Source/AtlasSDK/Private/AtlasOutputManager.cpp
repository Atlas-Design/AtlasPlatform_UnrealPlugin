// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasOutputManager.h"
#include "AtlasSDKSettings.h"
#include "AtlasJob.h"
#include "Types/AtlasValueTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/TextureFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Editor.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Blueprint.h"
#endif

UAtlasOutputManager::UAtlasOutputManager()
{
}

// ==================== Save Utilities ====================

FAtlasSaveResult UAtlasOutputManager::SaveToOutputFolder(const TArray<uint8>& Bytes, const FString& FileName)
{
	FString TargetFolder = GetOutputFolder();
	
	// If auto-organize is enabled, route to appropriate subfolder
	const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
	if (Settings && Settings->bAutoOrganizeByType)
	{
		EAtlasFileCategory Category = GetFileCategory(FileName);
		if (Category == EAtlasFileCategory::Image)
		{
			TargetFolder = GetImagesFolder();
		}
		else if (Category == EAtlasFileCategory::Mesh)
		{
			TargetFolder = GetMeshesFolder();
		}
	}

	FString FullPath = FPaths::Combine(TargetFolder, FileName);
	return SaveBytesToPath(Bytes, FullPath);
}

FAtlasSaveResult UAtlasOutputManager::SaveToOutputSubfolder(const TArray<uint8>& Bytes, const FString& FileName, const FString& SubFolder)
{
	FString TargetFolder = FPaths::Combine(GetOutputFolder(), SubFolder);
	FString FullPath = FPaths::Combine(TargetFolder, FileName);
	return SaveBytesToPath(Bytes, FullPath);
}

FAtlasSaveResult UAtlasOutputManager::SaveImageToOutputFolder(const TArray<uint8>& Bytes, const FString& FileName)
{
	FString FullPath = FPaths::Combine(GetImagesFolder(), FileName);
	return SaveBytesToPath(Bytes, FullPath);
}

FAtlasSaveResult UAtlasOutputManager::SaveMeshToOutputFolder(const TArray<uint8>& Bytes, const FString& FileName)
{
	FString FullPath = FPaths::Combine(GetMeshesFolder(), FileName);
	return SaveBytesToPath(Bytes, FullPath);
}

// ==================== Job-Based Output Saving ====================

FAtlasSaveResult UAtlasOutputManager::SaveToJobFolder(UAtlasJob* Job, const TArray<uint8>& Bytes, const FString& FileName)
{
	FAtlasSaveResult Result;
	
	if (!Job)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Job is null");
		return Result;
	}
	
	if (FileName.IsEmpty())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("FileName is empty");
		return Result;
	}
	
	FAtlasJobFolderInfo FolderInfo = GetJobFolderInfo(Job);
	if (!FolderInfo.IsValid())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Could not determine job folder path");
		return Result;
	}
	
	FString FullPath = FPaths::Combine(FolderInfo.DiskPath, FileName);
	UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Saving output file to job folder: %s"), *FullPath);
	
	return SaveBytesToPath(Bytes, FullPath);
}

bool UAtlasOutputManager::HasOutputFileForJob(UAtlasJob* Job, const FString& FileName) const
{
	FString FilePath = GetOutputFilePathForJob(Job, FileName);
	return !FilePath.IsEmpty() && FPaths::FileExists(FilePath);
}

FString UAtlasOutputManager::GetOutputFilePathForJob(UAtlasJob* Job, const FString& FileName) const
{
	if (!Job || FileName.IsEmpty())
	{
		return FString();
	}
	
	FAtlasJobFolderInfo FolderInfo = GetJobFolderInfo(Job);
	if (!FolderInfo.IsValid())
	{
		return FString();
	}
	
	return FPaths::Combine(FolderInfo.DiskPath, FileName);
}

TArray<FString> UAtlasOutputManager::GetAllOutputFilesForJob(UAtlasJob* Job) const
{
	TArray<FString> Results;
	
	if (!Job)
	{
		return Results;
	}
	
	FAtlasJobFolderInfo FolderInfo = GetJobFolderInfo(Job);
	if (!FolderInfo.IsValid())
	{
		return Results;
	}
	
	if (!FPaths::DirectoryExists(FolderInfo.DiskPath))
	{
		return Results;
	}
	
	// Find all files in the job folder
	FString SearchPath = FPaths::Combine(FolderInfo.DiskPath, TEXT("*"));
	IFileManager::Get().FindFiles(Results, *SearchPath, true, false);
	
	// Convert to full paths
	for (FString& FileName : Results)
	{
		FileName = FPaths::Combine(FolderInfo.DiskPath, FileName);
	}
	
	return Results;
}

// ==================== History Record Versions (Non-Editor) ====================

bool UAtlasOutputManager::HasOutputFileForJobFromHistory(const FAtlasJobHistoryRecord& HistoryRecord, const FString& FileName) const
{
	FString FilePath = GetOutputFilePathForJobFromHistory(HistoryRecord, FileName);
	return !FilePath.IsEmpty() && FPaths::FileExists(FilePath);
}

FString UAtlasOutputManager::GetOutputFilePathForJobFromHistory(const FAtlasJobHistoryRecord& HistoryRecord, const FString& FileName) const
{
	if (!HistoryRecord.IsValid() || FileName.IsEmpty())
	{
		return FString();
	}
	
	// First check if we have a saved path in OutputFilePaths
	if (const FString* SavedPath = HistoryRecord.OutputFilePaths.Find(FileName))
	{
		return *SavedPath;
	}
	
	// Otherwise compute from folder info
	FAtlasJobFolderInfo FolderInfo = GetJobFolderInfoFromHistory(HistoryRecord);
	if (!FolderInfo.IsValid())
	{
		return FString();
	}
	
	return FPaths::Combine(FolderInfo.DiskPath, FileName);
}

TArray<FString> UAtlasOutputManager::GetAllOutputFilesForJobFromHistory(const FAtlasJobHistoryRecord& HistoryRecord) const
{
	TArray<FString> Results;
	
	if (!HistoryRecord.IsValid())
	{
		return Results;
	}
	
	FAtlasJobFolderInfo FolderInfo = GetJobFolderInfoFromHistory(HistoryRecord);
	if (!FolderInfo.IsValid())
	{
		return Results;
	}
	
	if (!FPaths::DirectoryExists(FolderInfo.DiskPath))
	{
		return Results;
	}
	
	// Find all files in the job folder
	FString SearchPath = FPaths::Combine(FolderInfo.DiskPath, TEXT("*"));
	IFileManager::Get().FindFiles(Results, *SearchPath, true, false);
	
	// Convert to full paths
	for (FString& FileName : Results)
	{
		FileName = FPaths::Combine(FolderInfo.DiskPath, FileName);
	}
	
	return Results;
}

FAtlasSaveResult UAtlasOutputManager::SaveBytesToPath(const TArray<uint8>& Bytes, const FString& FullPath)
{
	FAtlasSaveResult Result;
	Result.FilePath = FullPath;

	if (Bytes.Num() == 0)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Cannot save empty byte array");
		UE_LOG(LogTemp, Warning, TEXT("AtlasOutputManager: %s"), *Result.ErrorMessage);
		return Result;
	}

	// Ensure the directory exists
	FString Directory = FPaths::GetPath(FullPath);
	if (!FPaths::DirectoryExists(Directory))
	{
		if (!IFileManager::Get().MakeDirectory(*Directory, true))
		{
			Result.bSuccess = false;
			Result.ErrorMessage = FString::Printf(TEXT("Failed to create directory: %s"), *Directory);
			UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager: %s"), *Result.ErrorMessage);
			return Result;
		}
	}

	// Save the file
	if (FFileHelper::SaveArrayToFile(Bytes, *FullPath))
	{
		Result.bSuccess = true;
		Result.FileSize = Bytes.Num();
		UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Saved file to %s (%lld bytes)"), *FullPath, Result.FileSize);
	}
	else
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("Failed to write file: %s"), *FullPath);
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager: %s"), *Result.ErrorMessage);
	}

	return Result;
}

// ==================== Folder Access ====================

FString UAtlasOutputManager::GetOutputFolder() const
{
	const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
	if (Settings)
	{
		return Settings->GetOutputFolderPath();
	}
	// Fallback default
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Atlas"), TEXT("Output"));
}

FString UAtlasOutputManager::GetImagesFolder() const
{
	const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
	if (Settings)
	{
		return Settings->GetImagesFolderPath();
	}
	return FPaths::Combine(GetOutputFolder(), TEXT("Images"));
}

FString UAtlasOutputManager::GetMeshesFolder() const
{
	const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
	if (Settings)
	{
		return Settings->GetMeshesFolderPath();
	}
	return FPaths::Combine(GetOutputFolder(), TEXT("Meshes"));
}

bool UAtlasOutputManager::EnsureOutputFolderExists()
{
	bool bSuccess = true;

	// Create root output folder
	FString OutputPath = GetOutputFolder();
	if (!FPaths::DirectoryExists(OutputPath))
	{
		bSuccess &= IFileManager::Get().MakeDirectory(*OutputPath, true);
	}

	// Create Images subfolder
	FString ImagesPath = GetImagesFolder();
	if (!FPaths::DirectoryExists(ImagesPath))
	{
		bSuccess &= IFileManager::Get().MakeDirectory(*ImagesPath, true);
	}

	// Create Meshes subfolder
	FString MeshesPath = GetMeshesFolder();
	if (!FPaths::DirectoryExists(MeshesPath))
	{
		bSuccess &= IFileManager::Get().MakeDirectory(*MeshesPath, true);
	}

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Output folders ready at %s"), *OutputPath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager: Failed to create some output folders"));
	}

	return bSuccess;
}

// ==================== File Type Detection ====================

bool UAtlasOutputManager::IsImageFile(const FString& FileName)
{
	FString Extension = FPaths::GetExtension(FileName).ToLower();
	
	static const TArray<FString> ImageExtensions = {
		TEXT("png"), TEXT("jpg"), TEXT("jpeg"), TEXT("bmp"), 
		TEXT("tga"), TEXT("exr"), TEXT("hdr")
	};
	
	return ImageExtensions.Contains(Extension);
}

bool UAtlasOutputManager::IsMeshFile(const FString& FileName)
{
	FString Extension = FPaths::GetExtension(FileName).ToLower();
	
	static const TArray<FString> MeshExtensions = {
		TEXT("glb"), TEXT("gltf"), TEXT("fbx"), TEXT("obj")
	};
	
	return MeshExtensions.Contains(Extension);
}

EAtlasFileCategory UAtlasOutputManager::GetFileCategory(const FString& FileName)
{
	if (IsImageFile(FileName))
	{
		return EAtlasFileCategory::Image;
	}
	if (IsMeshFile(FileName))
	{
		return EAtlasFileCategory::Mesh;
	}
	return EAtlasFileCategory::Other;
}

// ==================== Thumbnail / Preview Utilities ====================

UTexture2D* UAtlasOutputManager::CreateTextureFromImageBytes(const TArray<uint8>& ImageBytes)
{
	if (ImageBytes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasOutputManager: Cannot create texture from empty bytes"));
		return nullptr;
	}

	// Get the image wrapper module
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));

	// Detect image format from bytes
	EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(ImageBytes.GetData(), ImageBytes.Num());
	if (ImageFormat == EImageFormat::Invalid)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasOutputManager: Could not detect image format from bytes"));
		return nullptr;
	}

	// Create appropriate image wrapper
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasOutputManager: Failed to create image wrapper"));
		return nullptr;
	}

	// Decompress the image
	if (!ImageWrapper->SetCompressed(ImageBytes.GetData(), ImageBytes.Num()))
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasOutputManager: Failed to decompress image bytes"));
		return nullptr;
	}

	// Get raw BGRA data
	TArray<uint8> RawData;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasOutputManager: Failed to get raw image data"));
		return nullptr;
	}

	int32 Width = ImageWrapper->GetWidth();
	int32 Height = ImageWrapper->GetHeight();

	if (Width <= 0 || Height <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasOutputManager: Invalid image dimensions: %dx%d"), Width, Height);
		return nullptr;
	}

	// Create a transient texture (not saved to disk)
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasOutputManager: Failed to create transient texture"));
		return nullptr;
	}

	// Prevent garbage collection while we're setting it up
	Texture->AddToRoot();

	// Lock the texture and copy the data
	void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();

	// Update the texture resource
	Texture->UpdateResource();

	// Allow garbage collection again (caller is responsible for keeping reference)
	Texture->RemoveFromRoot();

	UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Created transient texture %dx%d from bytes"), Width, Height);

	return Texture;
}

UTexture2D* UAtlasOutputManager::LoadTextureFromFile(const FString& FilePath)
{
	if (!FPaths::FileExists(FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasOutputManager: File not found: %s"), *FilePath);
		return nullptr;
	}

	// Load file to bytes
	TArray<uint8> FileBytes;
	if (!FFileHelper::LoadFileToArray(FileBytes, *FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasOutputManager: Failed to load file: %s"), *FilePath);
		return nullptr;
	}

	UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Loading texture from file: %s (%d bytes)"), *FilePath, FileBytes.Num());

	// Use the bytes-based method
	return CreateTextureFromImageBytes(FileBytes);
}

// ==================== Job-Based Path Helpers (Non-Editor) ====================

FString UAtlasOutputManager::GenerateShortId(const FGuid& JobId)
{
	// Get the GUID as a string and take first 8 characters (lowercase)
	FString GuidString = JobId.ToString(EGuidFormats::DigitsLower);
	// Remove hyphens and take first 8 chars
	GuidString.ReplaceInline(TEXT("-"), TEXT(""));
	return GuidString.Left(8);
}

FString UAtlasOutputManager::GenerateJobFolderName(const FDateTime& StartedAt, const FGuid& JobId)
{
	// Format: YYYY-MM-DD_HHMMSS_shortId
	// Example: 2026-02-03_141858_99c451b3
	FString DatePart = FString::Printf(TEXT("%04d-%02d-%02d"),
		StartedAt.GetYear(),
		StartedAt.GetMonth(),
		StartedAt.GetDay());
	
	FString TimePart = FString::Printf(TEXT("%02d%02d%02d"),
		StartedAt.GetHour(),
		StartedAt.GetMinute(),
		StartedAt.GetSecond());
	
	FString ShortId = GenerateShortId(JobId);
	
	return FString::Printf(TEXT("%s_%s_%s"), *DatePart, *TimePart, *ShortId);
}

FString UAtlasOutputManager::SanitizeWorkflowName(const FString& WorkflowName)
{
	if (WorkflowName.IsEmpty())
	{
		return TEXT("UnknownWorkflow");
	}

	FString Sanitized = WorkflowName;
	
	// Replace common invalid characters
	Sanitized.ReplaceInline(TEXT(" "), TEXT("_"));
	Sanitized.ReplaceInline(TEXT("/"), TEXT("_"));
	Sanitized.ReplaceInline(TEXT("\\"), TEXT("_"));
	Sanitized.ReplaceInline(TEXT(":"), TEXT("_"));
	Sanitized.ReplaceInline(TEXT("*"), TEXT("_"));
	Sanitized.ReplaceInline(TEXT("?"), TEXT("_"));
	Sanitized.ReplaceInline(TEXT("\""), TEXT("_"));
	Sanitized.ReplaceInline(TEXT("<"), TEXT("_"));
	Sanitized.ReplaceInline(TEXT(">"), TEXT("_"));
	Sanitized.ReplaceInline(TEXT("|"), TEXT("_"));
	
	// Remove any leading/trailing underscores
	while (Sanitized.StartsWith(TEXT("_")))
	{
		Sanitized.RightChopInline(1);
	}
	while (Sanitized.EndsWith(TEXT("_")))
	{
		Sanitized.LeftChopInline(1);
	}
	
	// Collapse multiple underscores
	while (Sanitized.Contains(TEXT("__")))
	{
		Sanitized.ReplaceInline(TEXT("__"), TEXT("_"));
	}
	
	// If empty after sanitization, use default
	if (Sanitized.IsEmpty())
	{
		return TEXT("UnknownWorkflow");
	}
	
	return Sanitized;
}

FAtlasJobFolderInfo UAtlasOutputManager::GetJobFolderInfo(UAtlasJob* Job) const
{
	FAtlasJobFolderInfo Info;
	
	if (!Job)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasOutputManager: GetJobFolderInfo called with null job"));
		return Info;
	}
	
	// Get sanitized workflow name
	Info.WorkflowName = SanitizeWorkflowName(Job->GetWorkflowName());
	
	// Generate job folder name
	// Use StartedAt if valid, otherwise use CreatedAt
	FDateTime JobTime = Job->GetStartedAt().GetTicks() > 0 ? Job->GetStartedAt() : Job->GetCreatedAt();
	Info.JobFolderName = GenerateJobFolderName(JobTime, Job->GetJobId());
	
	// Build Content Browser path: {DefaultImportPath}/{WorkflowName}/{JobFolder}
	const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
	FString BasePath = Settings ? Settings->GetDefaultImportPathString() : TEXT("/Game/Atlas/Imported");
	Info.ContentBrowserPath = FPaths::Combine(BasePath, Info.WorkflowName, Info.JobFolderName);
	
	// Build disk path for output files
	FString OutputBase = GetOutputFolder();
	Info.DiskPath = FPaths::Combine(OutputBase, Info.WorkflowName, Info.JobFolderName);
	
	return Info;
}

FAtlasJobFolderInfo UAtlasOutputManager::GetJobFolderInfoFromHistory(const FAtlasJobHistoryRecord& HistoryRecord) const
{
	FAtlasJobFolderInfo Info;
	
	if (!HistoryRecord.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasOutputManager: GetJobFolderInfoFromHistory called with invalid history record"));
		return Info;
	}
	
	// Get sanitized workflow name
	Info.WorkflowName = SanitizeWorkflowName(HistoryRecord.WorkflowName);
	
	// Generate job folder name using StartedAt
	Info.JobFolderName = GenerateJobFolderName(HistoryRecord.StartedAt, HistoryRecord.JobId);
	
	// Build Content Browser path: {DefaultImportPath}/{WorkflowName}/{JobFolder}
	const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
	FString BasePath = Settings ? Settings->GetDefaultImportPathString() : TEXT("/Game/Atlas/Imported");
	Info.ContentBrowserPath = FPaths::Combine(BasePath, Info.WorkflowName, Info.JobFolderName);
	
	// Build disk path for output files
	FString OutputBase = GetOutputFolder();
	Info.DiskPath = FPaths::Combine(OutputBase, Info.WorkflowName, Info.JobFolderName);
	
	return Info;
}

// ==================== Import Utilities (Editor Only) ====================

#if WITH_EDITOR

FAtlasImportResult UAtlasOutputManager::ImportTextureAsset(const FString& FilePath, const FString& PackagePath)
{
	FAtlasImportResult Result;
	Result.PackagePath = PackagePath;

	// Verify file exists
	if (!FPaths::FileExists(FilePath))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("File not found: %s"), *FilePath);
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager: %s"), *Result.ErrorMessage);
		return Result;
	}

	// Get asset name from filename
	FString AssetName = FPaths::GetBaseFilename(FilePath);
	FString FullPackagePath = FPaths::Combine(PackagePath, AssetName);

	// Create the texture factory
	UTextureFactory* TextureFactory = NewObject<UTextureFactory>();
	TextureFactory->AddToRoot(); // Prevent GC during import

	// Note: We do NOT call SuppressImportOverwriteDialog()
	// This allows Unreal to show its native overwrite confirmation dialog

	// Import the texture
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	
	TArray<FString> FilesToImport;
	FilesToImport.Add(FilePath);

	TArray<UObject*> ImportedAssets = AssetTools.ImportAssets(FilesToImport, PackagePath, TextureFactory, false);

	TextureFactory->RemoveFromRoot();

	if (ImportedAssets.Num() > 0 && ImportedAssets[0] != nullptr)
	{
		Result.bSuccess = true;
		Result.ImportedAsset = ImportedAssets[0];
		
		UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Imported texture to %s"), *FullPackagePath);
	}
	else
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("Failed to import texture from %s"), *FilePath);
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager: %s"), *Result.ErrorMessage);
	}

	return Result;
}

FAtlasImportResult UAtlasOutputManager::ImportMeshAsset(const FString& FilePath, const FString& PackagePath)
{
	FAtlasImportResult Result;
	Result.PackagePath = PackagePath;

	// Verify file exists
	if (!FPaths::FileExists(FilePath))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("File not found: %s"), *FilePath);
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager: %s"), *Result.ErrorMessage);
		return Result;
	}

	// Use standard asset import for all mesh types (GLB/GLTF/FBX/OBJ)
	// UE5 handles GLB/GLTF natively through its standard import pipeline
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	
	TArray<FString> FilesToImport;
	FilesToImport.Add(FilePath);

	TArray<UObject*> ImportedAssets = AssetTools.ImportAssets(FilesToImport, PackagePath, nullptr, false);

	if (ImportedAssets.Num() == 0)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("Failed to import mesh from %s"), *FilePath);
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager: %s"), *Result.ErrorMessage);
		return Result;
	}

	// GLB/GLTF imports multiple assets (mesh, materials, textures)
	// Find the StaticMesh among all imported assets
	UStaticMesh* StaticMesh = nullptr;
	for (UObject* Asset : ImportedAssets)
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
		{
			StaticMesh = Mesh;
			break;
		}
	}

	if (StaticMesh)
	{
		Result.bSuccess = true;
		Result.ImportedAsset = StaticMesh;
		UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Imported mesh '%s' to %s (total assets: %d)"), 
			*StaticMesh->GetName(), *PackagePath, ImportedAssets.Num());
	}
	else
	{
		// No StaticMesh found, return first asset (might be SkeletalMesh, Blueprint, etc.)
		Result.bSuccess = true;
		Result.ImportedAsset = ImportedAssets[0];
		UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Imported '%s' (%s) to %s - no StaticMesh found"), 
			*ImportedAssets[0]->GetName(), *ImportedAssets[0]->GetClass()->GetName(), *PackagePath);
	}

	return Result;
}

FAtlasImportResult UAtlasOutputManager::ImportMeshAndSpawnInScene(const FString& FilePath, const FString& PackagePath, 
	FVector SpawnLocation, FRotator SpawnRotation)
{
	FAtlasImportResult Result;
	Result.PackagePath = PackagePath;

	// Verify file exists
	if (!FPaths::FileExists(FilePath))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("File not found: %s"), *FilePath);
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager: %s"), *Result.ErrorMessage);
		return Result;
	}

	// Import all assets from the file (GLB imports multiple: mesh, materials, textures)
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	
	TArray<FString> FilesToImport;
	FilesToImport.Add(FilePath);

	TArray<UObject*> ImportedAssets = AssetTools.ImportAssets(FilesToImport, PackagePath, nullptr, false);

	if (ImportedAssets.Num() == 0)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("Failed to import any assets from %s"), *FilePath);
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager: %s"), *Result.ErrorMessage);
		return Result;
	}

	// Find the StaticMesh among all imported assets
	UStaticMesh* StaticMesh = nullptr;
	for (UObject* Asset : ImportedAssets)
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
		{
			StaticMesh = Mesh;
			break;
		}
	}

	// Log what was imported for debugging
	UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Imported %d assets:"), ImportedAssets.Num());
	for (UObject* Asset : ImportedAssets)
	{
		UE_LOG(LogTemp, Log, TEXT("  - %s (%s)"), *Asset->GetName(), *Asset->GetClass()->GetName());
	}

	if (!StaticMesh)
	{
		// No StaticMesh found - might be a SkeletalMesh or Blueprint
		Result.bSuccess = true; // Import succeeded, just can't auto-spawn
		Result.ImportedAsset = ImportedAssets[0]; // Return first asset
		Result.ErrorMessage = TEXT("No StaticMesh found in imported assets - drag into scene manually");
		UE_LOG(LogTemp, Warning, TEXT("AtlasOutputManager: %s"), *Result.ErrorMessage);
		return Result;
	}

	Result.bSuccess = true;
	Result.ImportedAsset = StaticMesh;

	// Get the current world
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasOutputManager: No editor world available, cannot spawn mesh in scene."));
		Result.ErrorMessage = TEXT("No editor world - asset imported but not spawned");
		return Result;
	}

	// Spawn a StaticMeshActor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	
	AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), 
		SpawnLocation, SpawnRotation, SpawnParams);
	
	if (MeshActor)
	{
		// Set the mesh
		MeshActor->GetStaticMeshComponent()->SetStaticMesh(StaticMesh);
		
		// Set a meaningful label
		FString MeshName = StaticMesh->GetName();
		MeshActor->SetActorLabel(MeshName);
		
		// Select the new actor in the editor
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(MeshActor, true, true);
		
		UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Spawned mesh '%s' in scene at (%s)"), 
			*MeshName, *SpawnLocation.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasOutputManager: Failed to spawn StaticMeshActor"));
		Result.ErrorMessage = TEXT("Asset imported but failed to spawn in scene");
	}

	return Result;
}

bool UAtlasOutputManager::OpenImportDialog(const FString& FilePath)
{
	if (!FPaths::FileExists(FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager: File not found for import dialog: %s"), *FilePath);
		return false;
	}

	// Get the Content Browser module
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	
	// Get default import path
	FString ImportPath = GetDefaultImportPath();
	
	// Import assets with dialog
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	
	TArray<FString> FilesToImport;
	FilesToImport.Add(FilePath);
	
	// Import with UI (bSyncToBrowser = true opens the dialog)
	TArray<UObject*> ImportedAssets = AssetTools.ImportAssets(FilesToImport, ImportPath, nullptr, true);
	
	return ImportedAssets.Num() > 0;
}

FString UAtlasOutputManager::GetDefaultImportPath() const
{
	const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
	if (Settings)
	{
		return Settings->GetDefaultImportPathString();
	}
	return TEXT("/Game/Atlas/Imported");
}

// ==================== Simplified Import API ====================

FAtlasImportResult UAtlasOutputManager::ImportJobOutput(UAtlasJob* Job, const FString& OutputName)
{
	FAtlasImportResult Result;
	
	// Validate inputs
	if (!Job)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Job is null");
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager::ImportJobOutput - Job is null"));
		return Result;
	}
	
	if (OutputName.IsEmpty())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("OutputName is empty");
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager::ImportJobOutput - OutputName is empty"));
		return Result;
	}
	
	// Get the output value from the job
	FAtlasValue OutputValue;
	if (!Job->Outputs.GetValue(OutputName, OutputValue))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("Output '%s' not found in job"), *OutputName);
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager::ImportJobOutput - Output '%s' not found"), *OutputName);
		return Result;
	}
	
	// Check if it's a file type
	if (!OutputValue.IsFileType())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("Output '%s' is not a file type (type: %s)"), 
			*OutputName, *UEnum::GetValueAsString(OutputValue.Type));
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager::ImportJobOutput - %s"), *Result.ErrorMessage);
		return Result;
	}
	
	// Determine the filename
	FString FileName = OutputValue.FileName;
	if (FileName.IsEmpty())
	{
		// Generate filename from output name + default extension based on type
		FString Extension;
		switch (OutputValue.Type)
		{
		case EAtlasValueType::Image:
			Extension = TEXT("png");
			break;
		case EAtlasValueType::Mesh:
			Extension = TEXT("glb");
			break;
		default:
			Extension = TEXT("bin");
			break;
		}
		FileName = FString::Printf(TEXT("%s.%s"), *OutputName, *Extension);
	}
	
	UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager::ImportJobOutput - Processing output '%s' with filename '%s'"), 
		*OutputName, *FileName);
	
	// Check if already imported
	if (UObject* ExistingAsset = FindImportedAssetForJob(Job, FileName))
	{
		Result.bSuccess = true;
		Result.ImportedAsset = ExistingAsset;
		Result.PackagePath = GetJobImportPath(Job);
		UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager::ImportJobOutput - Asset already imported, returning existing"));
		return Result;
	}
	
	// Get the file path on disk (check if already saved)
	FString FilePath = GetOutputFilePathForJob(Job, FileName);
	
	// If file doesn't exist on disk, save it first
	if (!FPaths::FileExists(FilePath))
	{
		// Check if we have the bytes
		if (!OutputValue.HasFileData())
		{
			Result.bSuccess = false;
			Result.ErrorMessage = FString::Printf(TEXT("Output '%s' has no file data and file not on disk"), *OutputName);
			UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager::ImportJobOutput - %s"), *Result.ErrorMessage);
			return Result;
		}
		
		// Save bytes to disk
		FAtlasSaveResult SaveResult = SaveToJobFolder(Job, OutputValue.FileData, FileName);
		if (!SaveResult.bSuccess)
		{
			Result.bSuccess = false;
			Result.ErrorMessage = FString::Printf(TEXT("Failed to save file: %s"), *SaveResult.ErrorMessage);
			UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager::ImportJobOutput - %s"), *Result.ErrorMessage);
			return Result;
		}
		
		FilePath = SaveResult.FilePath;
		UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager::ImportJobOutput - Saved file to disk: %s"), *FilePath);
	}
	
	// Import based on type
	switch (OutputValue.Type)
	{
	case EAtlasValueType::Image:
		Result = ImportTextureForJob(Job, FilePath);
		break;
		
	case EAtlasValueType::Mesh:
		Result = ImportMeshForJob(Job, FilePath);
		break;
		
	default:
		// For generic files, try to use standard import
		Result = ImportMeshForJob(Job, FilePath); // Let Unreal figure out the type
		break;
	}
	
	if (Result.bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager::ImportJobOutput - Successfully imported '%s'"), *OutputName);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager::ImportJobOutput - Import failed: %s"), *Result.ErrorMessage);
	}
	
	return Result;
}

FAtlasImportResult UAtlasOutputManager::ImportJobOutputFromHistory(const FAtlasJobHistoryRecord& HistoryRecord, const FString& OutputName)
{
	FAtlasImportResult Result;
	
	// Validate inputs
	if (!HistoryRecord.IsValid())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("History record is invalid");
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager::ImportJobOutputFromHistory - History record is invalid"));
		return Result;
	}
	
	if (OutputName.IsEmpty())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("OutputName is empty");
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager::ImportJobOutputFromHistory - OutputName is empty"));
		return Result;
	}
	
	// Get folder info from history record
	FAtlasJobFolderInfo FolderInfo = GetJobFolderInfoFromHistory(HistoryRecord);
	if (!FolderInfo.IsValid())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Could not determine folder info from history record");
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager::ImportJobOutputFromHistory - Invalid folder info"));
		return Result;
	}
	
	// Get the output value from the history record
	FAtlasValue OutputValue;
	if (!HistoryRecord.Outputs.GetValue(OutputName, OutputValue))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("Output '%s' not found in history record"), *OutputName);
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager::ImportJobOutputFromHistory - Output '%s' not found"), *OutputName);
		return Result;
	}
	
	// Check if it's a file type
	if (!OutputValue.IsFileType())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("Output '%s' is not a file type (type: %s)"), 
			*OutputName, *UEnum::GetValueAsString(OutputValue.Type));
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager::ImportJobOutputFromHistory - %s"), *Result.ErrorMessage);
		return Result;
	}
	
	// Determine the filename
	FString FileName = OutputValue.FileName;
	if (FileName.IsEmpty())
	{
		// Generate filename from output name + default extension based on type
		FString Extension;
		switch (OutputValue.Type)
		{
		case EAtlasValueType::Image:
			Extension = TEXT("png");
			break;
		case EAtlasValueType::Mesh:
			Extension = TEXT("glb");
			break;
		default:
			Extension = TEXT("bin");
			break;
		}
		FileName = FString::Printf(TEXT("%s.%s"), *OutputName, *Extension);
	}
	
	UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager::ImportJobOutputFromHistory - Processing output '%s' with filename '%s'"), 
		*OutputName, *FileName);
	
	// Check if already imported in Content Browser
	FString AssetName = FPaths::GetBaseFilename(FileName);
	FString AssetPath = FPaths::Combine(FolderInfo.ContentBrowserPath, AssetName);
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssetsByPath(FName(*FolderInfo.ContentBrowserPath), AssetDataList, false);
	
	for (const FAssetData& AssetData : AssetDataList)
	{
		if (AssetData.AssetName.ToString() == AssetName)
		{
			UObject* ExistingAsset = AssetData.GetAsset();
			if (ExistingAsset)
			{
				Result.bSuccess = true;
				Result.ImportedAsset = ExistingAsset;
				Result.PackagePath = FolderInfo.ContentBrowserPath;
				UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager::ImportJobOutputFromHistory - Asset already imported, returning existing"));
				return Result;
			}
		}
	}
	
	// Determine file path on disk
	FString FilePath;
	
	// First, check if we have a saved path in OutputFilePaths
	if (const FString* SavedPath = HistoryRecord.OutputFilePaths.Find(OutputName))
	{
		if (FPaths::FileExists(*SavedPath))
		{
			FilePath = *SavedPath;
			UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager::ImportJobOutputFromHistory - Using saved path from history: %s"), *FilePath);
		}
	}
	
	// If no saved path, check the expected location
	if (FilePath.IsEmpty())
	{
		FString ExpectedPath = FPaths::Combine(FolderInfo.DiskPath, FileName);
		if (FPaths::FileExists(ExpectedPath))
		{
			FilePath = ExpectedPath;
			UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager::ImportJobOutputFromHistory - Found file at expected location: %s"), *FilePath);
		}
	}
	
	// If still no file, try to save it from the output value's file data
	if (FilePath.IsEmpty())
	{
		if (!OutputValue.HasFileData())
		{
			Result.bSuccess = false;
			Result.ErrorMessage = FString::Printf(TEXT("Output '%s' has no file data and file not on disk"), *OutputName);
			UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager::ImportJobOutputFromHistory - %s"), *Result.ErrorMessage);
			return Result;
		}
		
		// Ensure directory exists
		IFileManager::Get().MakeDirectory(*FolderInfo.DiskPath, true);
		
		// Save the file
		FilePath = FPaths::Combine(FolderInfo.DiskPath, FileName);
		if (!FFileHelper::SaveArrayToFile(OutputValue.FileData, *FilePath))
		{
			Result.bSuccess = false;
			Result.ErrorMessage = FString::Printf(TEXT("Failed to save file to: %s"), *FilePath);
			UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager::ImportJobOutputFromHistory - %s"), *Result.ErrorMessage);
			return Result;
		}
		
		UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager::ImportJobOutputFromHistory - Saved file to disk: %s"), *FilePath);
	}
	
	// Import based on type
	switch (OutputValue.Type)
	{
	case EAtlasValueType::Image:
		Result = ImportTextureAsset(FilePath, FolderInfo.ContentBrowserPath);
		break;
		
	case EAtlasValueType::Mesh:
		Result = ImportMeshAsset(FilePath, FolderInfo.ContentBrowserPath);
		break;
		
	default:
		// For generic files, try mesh import and let Unreal figure out the type
		Result = ImportMeshAsset(FilePath, FolderInfo.ContentBrowserPath);
		break;
	}
	
	if (Result.bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager::ImportJobOutputFromHistory - Successfully imported '%s'"), *OutputName);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasOutputManager::ImportJobOutputFromHistory - Import failed: %s"), *Result.ErrorMessage);
	}
	
	return Result;
}

FAtlasImportResult UAtlasOutputManager::FindImportedJobOutput(UAtlasJob* Job, const FString& OutputName) const
{
	FAtlasImportResult Result;
	
	// Validate inputs
	if (!Job)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Job is null");
		return Result;
	}
	
	if (OutputName.IsEmpty())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("OutputName is empty");
		return Result;
	}
	
	// Get the output value from the job
	FAtlasValue OutputValue;
	if (!Job->Outputs.GetValue(OutputName, OutputValue))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("Output '%s' not found in job"), *OutputName);
		return Result;
	}
	
	// Determine the filename
	FString FileName = OutputValue.FileName;
	if (FileName.IsEmpty())
	{
		FString Extension;
		switch (OutputValue.Type)
		{
		case EAtlasValueType::Image:
			Extension = TEXT("png");
			break;
		case EAtlasValueType::Mesh:
			Extension = TEXT("glb");
			break;
		default:
			Extension = TEXT("bin");
			break;
		}
		FileName = FString::Printf(TEXT("%s.%s"), *OutputName, *Extension);
	}
	
	// Check if already imported
	if (UObject* ExistingAsset = FindImportedAssetForJob(Job, FileName))
	{
		Result.bSuccess = true;
		Result.ImportedAsset = ExistingAsset;
		Result.PackagePath = GetJobImportPath(Job);
		return Result;
	}
	
	// Not found
	Result.bSuccess = false;
	Result.ErrorMessage = FString::Printf(TEXT("Asset for output '%s' not found in Content Browser"), *OutputName);
	return Result;
}

FAtlasImportResult UAtlasOutputManager::FindImportedJobOutputFromHistory(const FAtlasJobHistoryRecord& HistoryRecord, const FString& OutputName) const
{
	FAtlasImportResult Result;
	
	// Validate inputs
	if (!HistoryRecord.IsValid())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("History record is invalid");
		return Result;
	}
	
	if (OutputName.IsEmpty())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("OutputName is empty");
		return Result;
	}
	
	// Get folder info from history record
	FAtlasJobFolderInfo FolderInfo = GetJobFolderInfoFromHistory(HistoryRecord);
	if (!FolderInfo.IsValid())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Could not determine folder info from history record");
		return Result;
	}
	
	// Get the output value from the history record
	FAtlasValue OutputValue;
	if (!HistoryRecord.Outputs.GetValue(OutputName, OutputValue))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("Output '%s' not found in history record"), *OutputName);
		return Result;
	}
	
	// Determine the filename
	FString FileName = OutputValue.FileName;
	if (FileName.IsEmpty())
	{
		FString Extension;
		switch (OutputValue.Type)
		{
		case EAtlasValueType::Image:
			Extension = TEXT("png");
			break;
		case EAtlasValueType::Mesh:
			Extension = TEXT("glb");
			break;
		default:
			Extension = TEXT("bin");
			break;
		}
		FileName = FString::Printf(TEXT("%s.%s"), *OutputName, *Extension);
	}
	
	// Check if already imported in Content Browser
	FString AssetName = FPaths::GetBaseFilename(FileName);
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssetsByPath(FName(*FolderInfo.ContentBrowserPath), AssetDataList, false);
	
	for (const FAssetData& AssetData : AssetDataList)
	{
		if (AssetData.AssetName.ToString() == AssetName)
		{
			UObject* ExistingAsset = AssetData.GetAsset();
			if (ExistingAsset)
			{
				Result.bSuccess = true;
				Result.ImportedAsset = ExistingAsset;
				Result.PackagePath = FolderInfo.ContentBrowserPath;
				return Result;
			}
		}
	}
	
	// Not found
	Result.bSuccess = false;
	Result.ErrorMessage = FString::Printf(TEXT("Asset for output '%s' not found in Content Browser"), *OutputName);
	return Result;
}

// ==================== Job-Based Import Paths (Editor Only) ====================

FString UAtlasOutputManager::GetJobImportPath(UAtlasJob* Job) const
{
	FAtlasJobFolderInfo Info = GetJobFolderInfo(Job);
	return Info.ContentBrowserPath;
}

FAtlasImportResult UAtlasOutputManager::ImportTextureForJob(UAtlasJob* Job, const FString& FilePath)
{
	FAtlasImportResult Result;
	
	if (!Job)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Job is null");
		return Result;
	}
	
	FString ImportPath = GetJobImportPath(Job);
	UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Importing texture for job to %s"), *ImportPath);
	
	return ImportTextureAsset(FilePath, ImportPath);
}

FAtlasImportResult UAtlasOutputManager::ImportMeshForJob(UAtlasJob* Job, const FString& FilePath)
{
	FAtlasImportResult Result;
	
	if (!Job)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Job is null");
		return Result;
	}
	
	FString ImportPath = GetJobImportPath(Job);
	UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Importing mesh for job to %s"), *ImportPath);
	
	return ImportMeshAsset(FilePath, ImportPath);
}

FAtlasImportResult UAtlasOutputManager::ImportMeshForJobAndSpawn(UAtlasJob* Job, const FString& FilePath,
	FVector SpawnLocation, FRotator SpawnRotation)
{
	FAtlasImportResult Result;
	
	if (!Job)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Job is null");
		return Result;
	}
	
	FString ImportPath = GetJobImportPath(Job);
	UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Importing and spawning mesh for job to %s"), *ImportPath);
	
	return ImportMeshAndSpawnInScene(FilePath, ImportPath, SpawnLocation, SpawnRotation);
}

// ==================== Asset Lookup Utilities ====================

UObject* UAtlasOutputManager::FindImportedAssetForJob(UAtlasJob* Job, const FString& FileName) const
{
	if (!Job || FileName.IsEmpty())
	{
		return nullptr;
	}
	
	FString ImportPath = GetJobImportPath(Job);
	FString AssetName = FPaths::GetBaseFilename(FileName);
	FString FullAssetPath = FPaths::Combine(ImportPath, AssetName);
	
	// Try to find the asset in the asset registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	// Build the object path (Content Browser path format)
	FString ObjectPath = FullAssetPath + TEXT(".") + AssetName;
	
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
	
	if (AssetData.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Found imported asset: %s"), *ObjectPath);
		return AssetData.GetAsset();
	}
	
	// Asset not found - try searching the folder
	TArray<FAssetData> AssetsInFolder;
	AssetRegistry.GetAssetsByPath(FName(*ImportPath), AssetsInFolder, true);
	
	// Look for an asset with matching name
	for (const FAssetData& Asset : AssetsInFolder)
	{
		if (Asset.AssetName.ToString() == AssetName)
		{
			UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Found imported asset by name: %s"), *Asset.GetObjectPathString());
			return Asset.GetAsset();
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: No imported asset found for job at %s"), *ImportPath);
	return nullptr;
}

bool UAtlasOutputManager::HasImportedAssetForJob(UAtlasJob* Job, const FString& FileName) const
{
	return FindImportedAssetForJob(Job, FileName) != nullptr;
}

TArray<UObject*> UAtlasOutputManager::GetAllImportedAssetsForJob(UAtlasJob* Job) const
{
	TArray<UObject*> Results;
	
	if (!Job)
	{
		return Results;
	}
	
	FString ImportPath = GetJobImportPath(Job);
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	TArray<FAssetData> AssetsInFolder;
	AssetRegistry.GetAssetsByPath(FName(*ImportPath), AssetsInFolder, true);
	
	for (const FAssetData& AssetData : AssetsInFolder)
	{
		if (UObject* Asset = AssetData.GetAsset())
		{
			Results.Add(Asset);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Found %d imported assets for job at %s"), Results.Num(), *ImportPath);
	
	return Results;
}

// ==================== History Record Versions (Asset Lookup) ====================

UObject* UAtlasOutputManager::FindImportedAssetForJobFromHistory(const FAtlasJobHistoryRecord& HistoryRecord, const FString& FileName) const
{
	if (!HistoryRecord.IsValid() || FileName.IsEmpty())
	{
		return nullptr;
	}
	
	FAtlasJobFolderInfo FolderInfo = GetJobFolderInfoFromHistory(HistoryRecord);
	if (!FolderInfo.IsValid())
	{
		return nullptr;
	}
	
	FString ImportPath = FolderInfo.ContentBrowserPath;
	FString AssetName = FPaths::GetBaseFilename(FileName);
	FString FullAssetPath = FPaths::Combine(ImportPath, AssetName);
	
	// Try to find the asset in the asset registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	// Build the object path (Content Browser path format)
	FString ObjectPath = FullAssetPath + TEXT(".") + AssetName;
	
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
	
	if (AssetData.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Found imported asset from history: %s"), *ObjectPath);
		return AssetData.GetAsset();
	}
	
	// Asset not found - try searching the folder
	TArray<FAssetData> AssetsInFolder;
	AssetRegistry.GetAssetsByPath(FName(*ImportPath), AssetsInFolder, true);
	
	// Look for an asset with matching name
	for (const FAssetData& Asset : AssetsInFolder)
	{
		if (Asset.AssetName.ToString() == AssetName)
		{
			UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Found imported asset by name from history: %s"), *Asset.GetObjectPathString());
			return Asset.GetAsset();
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: No imported asset found from history at %s"), *ImportPath);
	return nullptr;
}

bool UAtlasOutputManager::HasImportedAssetForJobFromHistory(const FAtlasJobHistoryRecord& HistoryRecord, const FString& FileName) const
{
	return FindImportedAssetForJobFromHistory(HistoryRecord, FileName) != nullptr;
}

TArray<UObject*> UAtlasOutputManager::GetAllImportedAssetsForJobFromHistory(const FAtlasJobHistoryRecord& HistoryRecord) const
{
	TArray<UObject*> Results;
	
	if (!HistoryRecord.IsValid())
	{
		return Results;
	}
	
	FAtlasJobFolderInfo FolderInfo = GetJobFolderInfoFromHistory(HistoryRecord);
	if (!FolderInfo.IsValid())
	{
		return Results;
	}
	
	FString ImportPath = FolderInfo.ContentBrowserPath;
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	TArray<FAssetData> AssetsInFolder;
	AssetRegistry.GetAssetsByPath(FName(*ImportPath), AssetsInFolder, true);
	
	for (const FAssetData& AssetData : AssetsInFolder)
	{
		if (UObject* Asset = AssetData.GetAsset())
		{
			Results.Add(Asset);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("AtlasOutputManager: Found %d imported assets from history at %s"), Results.Num(), *ImportPath);
	
	return Results;
}

#endif // WITH_EDITOR
