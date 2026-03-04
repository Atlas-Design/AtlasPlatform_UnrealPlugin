// Copyright Atlas Platform. All Rights Reserved.

#include "Utilities/AtlasAssetExporter.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"

FAtlasExportResult UAtlasAssetExporter::GetValidFilePath(UObject* Asset)
{
	FAtlasExportResult Result;

	if (!Asset)
	{
		Result.ErrorMessage = TEXT("Asset is null");
		return Result;
	}

	// First, try to get the original source file
	FString SourcePath;
	if (GetSourceFilePath(Asset, SourcePath))
	{
		// Source file exists, use it directly
		Result.bSuccess = true;
		Result.FilePath = SourcePath;
		Result.bIsTempExport = false;
		return Result;
	}

	// Source not found, need to export
	if (UTexture2D* Texture = Cast<UTexture2D>(Asset))
	{
		return ExportTextureToPNG(Texture);
	}
	else if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
	{
		return ExportMeshToFBX(Mesh);
	}

	Result.ErrorMessage = FString::Printf(TEXT("Unsupported asset type: %s"), *Asset->GetClass()->GetName());
	return Result;
}

FAtlasExportResult UAtlasAssetExporter::ExportTextureToPNG(UTexture2D* Texture, const FString& OutputPath)
{
	FAtlasExportResult Result;

	if (!Texture)
	{
		Result.ErrorMessage = TEXT("Texture is null");
		return Result;
	}

	// Determine output path
	FString FinalPath = OutputPath;
	if (FinalPath.IsEmpty())
	{
		if (!EnsureTempFolderExists())
		{
			Result.ErrorMessage = TEXT("Failed to create temp export folder");
			return Result;
		}
		FinalPath = GenerateTempFilename(Texture->GetName(), TEXT("png"));
		Result.bIsTempExport = true;
	}

	// Get texture data
	FTexturePlatformData* PlatformData = Texture->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0)
	{
		Result.ErrorMessage = TEXT("Texture has no platform data");
		return Result;
	}

	// Lock the mip for reading
	FByteBulkData& BulkData = PlatformData->Mips[0].BulkData;
	const void* Data = BulkData.LockReadOnly();
	if (!Data)
	{
		Result.ErrorMessage = TEXT("Failed to lock texture data");
		return Result;
	}

	int32 Width = PlatformData->Mips[0].SizeX;
	int32 Height = PlatformData->Mips[0].SizeY;

	// Create image from raw data
	TArray<FColor> Pixels;
	Pixels.SetNum(Width * Height);

	// Copy pixel data (assuming BGRA8 format)
	FMemory::Memcpy(Pixels.GetData(), Data, Width * Height * 4);
	BulkData.Unlock();

	// Export to PNG using FImageUtils
	TArray<uint8> PNGData;
	FImageUtils::ThumbnailCompressImageArray(Width, Height, Pixels, PNGData);

	if (PNGData.Num() == 0)
	{
		Result.ErrorMessage = TEXT("Failed to compress image to PNG");
		return Result;
	}

	// Write to file
	if (!FFileHelper::SaveArrayToFile(PNGData, *FinalPath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to write file: %s"), *FinalPath);
		return Result;
	}

	Result.bSuccess = true;
	Result.FilePath = FinalPath;
	UE_LOG(LogTemp, Log, TEXT("[AtlasAssetExporter] Exported texture to: %s"), *FinalPath);
	return Result;
}

FAtlasExportResult UAtlasAssetExporter::ExportMeshToFBX(UStaticMesh* Mesh, const FString& OutputPath)
{
	FAtlasExportResult Result;

	if (!Mesh)
	{
		Result.ErrorMessage = TEXT("Mesh is null");
		return Result;
	}

	// Determine output path
	FString FinalPath = OutputPath;
	if (FinalPath.IsEmpty())
	{
		if (!EnsureTempFolderExists())
		{
			Result.ErrorMessage = TEXT("Failed to create temp export folder");
			return Result;
		}
		FinalPath = GenerateTempFilename(Mesh->GetName(), TEXT("fbx"));
		Result.bIsTempExport = true;
	}

	// For mesh export, we'll use the Unreal exporter system
	// This is more complex and requires the FBX SDK
	// For now, return error indicating this needs implementation
	Result.ErrorMessage = TEXT("Mesh export not yet implemented - please use the original source file");
	return Result;
}

bool UAtlasAssetExporter::GetSourceFilePath(UObject* Asset, FString& OutSourcePath)
{
	if (!Asset)
	{
		return false;
	}

	// Try to get AssetImportData
	UAssetImportData* ImportData = nullptr;

	if (UTexture2D* Texture = Cast<UTexture2D>(Asset))
	{
		ImportData = Texture->AssetImportData;
	}
	else if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
	{
		ImportData = Mesh->AssetImportData;
	}

	if (!ImportData)
	{
		return false;
	}

	// Get the source file path
	TArray<FString> SourceFiles;
	ImportData->ExtractFilenames(SourceFiles);

	if (SourceFiles.Num() == 0)
	{
		return false;
	}

	// Check if the file exists
	FString SourcePath = SourceFiles[0];
	if (FPaths::FileExists(SourcePath))
	{
		OutSourcePath = SourcePath;
		return true;
	}

	// File doesn't exist at original location
	return false;
}

FString UAtlasAssetExporter::GetTempExportFolder()
{
	return FPaths::ProjectSavedDir() / TEXT("Atlas") / TEXT("TempExport");
}

int32 UAtlasAssetExporter::CleanupTempExports(int32 MaxAgeHours)
{
	FString TempFolder = GetTempExportFolder();
	if (!FPaths::DirectoryExists(TempFolder))
	{
		return 0;
	}

	int32 DeletedCount = 0;
	FDateTime CutoffTime = FDateTime::UtcNow() - FTimespan::FromHours(MaxAgeHours);

	IFileManager& FileManager = IFileManager::Get();
	TArray<FString> Files;
	FileManager.FindFiles(Files, *(TempFolder / TEXT("*.*")), true, false);

	for (const FString& Filename : Files)
	{
		FString FullPath = TempFolder / Filename;
		FDateTime ModTime = FileManager.GetTimeStamp(*FullPath);
		
		if (ModTime < CutoffTime)
		{
			if (FileManager.Delete(*FullPath))
			{
				DeletedCount++;
				UE_LOG(LogTemp, Log, TEXT("[AtlasAssetExporter] Deleted old temp file: %s"), *Filename);
			}
		}
	}

	return DeletedCount;
}

bool UAtlasAssetExporter::EnsureTempFolderExists()
{
	FString TempFolder = GetTempExportFolder();
	
	if (FPaths::DirectoryExists(TempFolder))
	{
		return true;
	}

	return IFileManager::Get().MakeDirectory(*TempFolder, true);
}

FString UAtlasAssetExporter::GenerateTempFilename(const FString& BaseName, const FString& Extension)
{
	FString TempFolder = GetTempExportFolder();
	FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
	FString SafeName = BaseName;
	SafeName.ReplaceInline(TEXT(" "), TEXT("_"));
	
	return TempFolder / FString::Printf(TEXT("%s_%s.%s"), *SafeName, *Timestamp, *Extension);
}
