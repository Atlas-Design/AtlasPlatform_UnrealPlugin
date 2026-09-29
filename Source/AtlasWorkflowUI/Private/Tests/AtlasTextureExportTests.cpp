// Copyright Atlas Platform. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "Utilities/AtlasAssetExporter.h"
#include "Engine/Texture2D.h"
#include "EditorFramework/AssetImportData.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFile.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	FString TestPath()
	{
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/TextureExport"));
		IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*Directory);
		return FPaths::Combine(Directory, FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".png"));
	}
	void SetMip(UTexture2D* Texture, EPixelFormat Format, int32 Width, int32 Height, int64 Bytes, const void* Data = nullptr)
	{
		auto* Platform = new FTexturePlatformData();
		Platform->SizeX = Width;
		Platform->SizeY = Height;
		Platform->SetNumSlices(1);
		Platform->PixelFormat = Format;
		FTexture2DMipMap* Mip = new FTexture2DMipMap();
		Platform->Mips.Add(Mip);
		Mip->SizeX = Width;
		Mip->SizeY = Height;
		Mip->SizeZ = 1;
		Mip->BulkData.Lock(LOCK_READ_WRITE);
		void* Buffer = Mip->BulkData.Realloc(Bytes);
		if (Bytes > 0)
		{
			if (Data) FMemory::Memcpy(Buffer, Data, Bytes);
			else FMemory::Memzero(Buffer, Bytes);
		}
		Mip->BulkData.Unlock();
		Texture->SetPlatformData(Platform);
	}
	bool CheckPNG(FAutomationTestBase& Test, const FAtlasExportResult& Result, const FColor& Expected)
	{
		if (!Test.TestTrue(*Result.ErrorMessage, Result.bSuccess)) return false;
		TArray64<uint8> Bytes;
		FImage Image;
		if (!Test.TestTrue(TEXT("Read PNG"), FFileHelper::LoadFileToArray(Bytes, *Result.FilePath))
			|| !Test.TestTrue(TEXT("Decode PNG"), FImageUtils::DecompressImage(Bytes.GetData(), Bytes.Num(), Image))) return false;
		Test.TestEqual(TEXT("Width"), Image.SizeX, 4);
		Test.TestEqual(TEXT("Height"), Image.SizeY, 4);
		FImage BGRA;
		Image.CopyTo(BGRA, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		const FColor* Pixels = reinterpret_cast<const FColor*>(BGRA.RawData.GetData());
		for (int32 Index = 0; Index < 16; ++Index)
		{
			if (!Test.TestEqual(TEXT("Decoded pixel and channels"), Pixels[Index], Expected)) return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasTextureSourceExport, "Atlas.TextureExport.SourceArt", Flags)
bool FAtlasTextureSourceExport::RunTest(const FString& Parameters)
{
	for (ERawImageFormat::Type Format : {ERawImageFormat::BGRA8, ERawImageFormat::G8, ERawImageFormat::RGBA16F})
	{
		TStrongObjectPtr<UTexture2D> Texture(NewObject<UTexture2D>());
		FImage Source(4, 4, Format);
		FColor Expected = FColor::Red;
		if (Format == ERawImageFormat::BGRA8)
		{
			for (int32 I = 0; I < 16; ++I) reinterpret_cast<FColor*>(Source.RawData.GetData())[I] = Expected;
		}
		else if (Format == ERawImageFormat::G8)
		{
			Expected = FColor::White;
			FMemory::Memset(Source.RawData.GetData(), 255, Source.RawData.Num());
		}
		else
		{
			for (int32 I = 0; I < 16; ++I) reinterpret_cast<FFloat16Color*>(Source.RawData.GetData())[I] = FFloat16Color(FLinearColor::Red);
		}
		Texture->Source.Init(Source);
		// A tiny block-compressed platform mip must never be treated as 64 BGRA bytes.
		SetMip(Texture.Get(), PF_DXT1, 4, 4, 8);
		Texture->AssetImportData = NewObject<UAssetImportData>(Texture.Get());
		Texture->AssetImportData->UpdateFilenameOnly(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("missing-original-texture.png")));
		CheckPNG(*this, UAtlasAssetExporter::GetValidFilePath(Texture.Get()), Expected);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasTextureRawExport, "Atlas.TextureExport.UncompressedFallback", Flags)
bool FAtlasTextureRawExport::RunTest(const FString& Parameters)
{
	for (EPixelFormat Format : {PF_B8G8R8A8, PF_G8, PF_FloatRGBA})
	{
		TStrongObjectPtr<UTexture2D> Texture(NewObject<UTexture2D>());
		FImage Image(4, 4, Format == PF_G8 ? ERawImageFormat::G8 : (Format == PF_FloatRGBA ? ERawImageFormat::RGBA16F : ERawImageFormat::BGRA8));
		FColor Expected = FColor::Blue;
		if (Format == PF_B8G8R8A8)
		{
			for (int32 I = 0; I < 16; ++I) reinterpret_cast<FColor*>(Image.RawData.GetData())[I] = Expected;
		}
		else if (Format == PF_G8)
		{
			Expected = FColor::Black;
			FMemory::Memzero(Image.RawData.GetData(), Image.RawData.Num());
		}
		else
		{
			for (int32 I = 0; I < 16; ++I) reinterpret_cast<FFloat16Color*>(Image.RawData.GetData())[I] = FFloat16Color(FLinearColor::Blue);
		}
		SetMip(Texture.Get(), Format, 4, 4, Image.RawData.Num(), Image.RawData.GetData());
		CheckPNG(*this, UAtlasAssetExporter::ExportTextureToPNG(Texture.Get(), TestPath()), Expected);
		TestFalse(TEXT("Mip lock released"), Texture->GetPlatformData()->Mips[0].BulkData.IsLocked());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasTextureInvalidExport, "Atlas.TextureExport.InvalidData", Flags)
bool FAtlasTextureInvalidExport::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Null texture rejected"), UAtlasAssetExporter::ExportTextureToPNG(nullptr, TestPath()).bSuccess);
	for (int32 Case = 0; Case < 6; ++Case)
	{
		TStrongObjectPtr<UTexture2D> Texture(NewObject<UTexture2D>());
		if (Case == 0) SetMip(Texture.Get(), PF_DXT1, 4, 4, 8);
		if (Case == 1) SetMip(Texture.Get(), PF_B8G8R8A8, 4, 4, 4); // Truncated.
		if (Case == 2) SetMip(Texture.Get(), PF_B8G8R8A8, MAX_int32, MAX_int32, 0);
		if (Case == 3) SetMip(Texture.Get(), PF_B8G8R8A8, 0, 4, 0);
		// Case 4 has no source or platform data.
		if (Case == 5)
		{
			SetMip(Texture.Get(), PF_B8G8R8A8, 4, 4, 64);
			Texture->GetPlatformData()->Mips[0].BulkData.LockReadOnly();
		}
		const FString Path = TestPath();
		const FAtlasExportResult Result = UAtlasAssetExporter::ExportTextureToPNG(Texture.Get(), Path);
		TestFalse(TEXT("Unsafe data rejected"), Result.bSuccess);
		TestFalse(TEXT("Actionable error"), Result.ErrorMessage.IsEmpty());
		TestFalse(TEXT("No output written"), FPaths::FileExists(Path));
		if (Case == 5) Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		if (Texture->GetPlatformData()) TestFalse(TEXT("No lock leaked"), Texture->GetPlatformData()->Mips[0].BulkData.IsLocked());
	}
	return true;
}
#endif
