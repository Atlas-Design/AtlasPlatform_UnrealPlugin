// Copyright Atlas Platform. All Rights Reserved.

#include "Utilities/AtlasAssetExporter.h"
#include "AssetExportTask.h"
#include "Exporters/Exporter.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Misc/ScopeExit.h"

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

	// Use the same source-art conversion as AtlasWorkflowEditorUtilities. Source art
	// remains in the asset even when the original imported file is missing on disk.
	FImage Image;
	if (!FImageUtils::GetTexture2DSourceImage(Texture, Image))
	{
		FTexturePlatformData* PlatformData = Texture->GetPlatformData();
		if (!PlatformData || PlatformData->Mips.Num() == 0)
		{
			Result.ErrorMessage = TEXT("Texture has no source image or readable platform mip");
			return Result;
		}
		ERawImageFormat::Type Format;
		int32 BytesPerPixel;
		switch (PlatformData->PixelFormat)
		{
		case PF_B8G8R8A8: Format = ERawImageFormat::BGRA8; BytesPerPixel = 4; break;
		case PF_G8: Format = ERawImageFormat::G8; BytesPerPixel = 1; break;
		case PF_FloatRGBA: Format = ERawImageFormat::RGBA16F; BytesPerPixel = 8; break;
		default:
			Result.ErrorMessage = TEXT("Texture has no source image; its platform format cannot be exported safely. Reimport source art or use a supported uncompressed texture.");
			return Result;
		}
		FTexture2DMipMap& Mip = PlatformData->Mips[0];
		const int64 PixelCount = static_cast<int64>(Mip.SizeX) * Mip.SizeY;
		if (Mip.SizeX <= 0 || Mip.SizeY <= 0 || Mip.SizeZ != 1
			|| PixelCount > MAX_int32 / BytesPerPixel
			|| Mip.BulkData.GetBulkDataSize() != PixelCount * BytesPerPixel)
		{
			Result.ErrorMessage = TEXT("Texture mip dimensions or byte count are invalid for export");
			return Result;
		}
		FByteBulkData& BulkData = Mip.BulkData;
		if (BulkData.IsLocked())
		{
			Result.ErrorMessage = TEXT("Texture mip is already locked; try exporting again later");
			return Result;
		}
		const void* Data = BulkData.LockReadOnly();
		ON_SCOPE_EXIT { BulkData.Unlock(); };
		if (!Data)
		{
			Result.ErrorMessage = TEXT("Texture mip data is unavailable for CPU export");
			return Result;
		}
		const EGammaSpace Gamma = Format == ERawImageFormat::RGBA16F
			? EGammaSpace::Linear : (Texture->SRGB ? EGammaSpace::sRGB : EGammaSpace::Linear);
		Image.Init(Mip.SizeX, Mip.SizeY, Format, Gamma);
		FMemory::Memcpy(Image.RawData.GetData(), Data, PixelCount * BytesPerPixel);
	}

	// Always encode PNG, regardless of the supplied filename extension.
	TArray64<uint8> PNGData;
	if (!FImageUtils::CompressImage(PNGData, TEXT("png"), Image) || PNGData.IsEmpty())
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

	TStrongObjectPtr<UAssetExportTask> Task(NewObject<UAssetExportTask>());
	Task->Object = Mesh;
	Task->Filename = FinalPath;
	Task->bAutomated = true;
	Task->bPrompt = false;
	Task->bReplaceIdentical = true;
	Task->bSelected = false;
	Task->bWriteEmptyFiles = false;
	Result.bSuccess = UExporter::RunAssetExportTask(Task.Get()) && FPaths::FileExists(FinalPath);
	if (Result.bSuccess) Result.FilePath = FinalPath;
	else Result.ErrorMessage = Task->Errors.IsEmpty() ? TEXT("No FBX exporter is available or export failed") : FString::Join(Task->Errors, TEXT("; "));

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
