#include "AtlasWorkflowFunctionLibrary.h"

#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "PixelFormat.h"
#include "Rendering/Texture2DResource.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

// --- Internal helper: read first mip as raw 8-bit BGRA/RGBA ---
static bool ReadTextureFirstMip(UTexture2D* Texture, TArray<uint8>& OutRawPixels, int32& OutW, int32& OutH, ERGBFormat& OutRGBFormat)
{
	if (!Texture)
	{
		return false;
	}

#if WITH_EDITOR
	// If the texture is streamable, you may want to ensure mips are resident before reading.
	// Texture->SetForceMipLevelsToBeResident(30.0f);
	// Texture->WaitForStreaming();
#endif

	// Access cooked platform mips
	if (!Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
	{
		return false;
	}

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	OutW = Mip.SizeX;
	OutH = Mip.SizeY;

	const EPixelFormat PFmt = static_cast<EPixelFormat>(Texture->GetPlatformData()->PixelFormat);

	// We directly support standard 8-bit 4-channel formats
	if (PFmt != PF_B8G8R8A8 && PFmt != PF_R8G8B8A8)
	{
		// (Optional) Add conversions for other formats if you need them (e.g., read via render target).
		return false;
	}

	const int64 BytesPerPixel = 4;
	const int64 ExpectedSize = static_cast<int64>(OutW) * static_cast<int64>(OutH) * BytesPerPixel;

	const void* LockedData = Mip.BulkData.LockReadOnly();
	if (!LockedData)
	{
		return false;
	}

	OutRawPixels.SetNumUninitialized(ExpectedSize);
	FMemory::Memcpy(OutRawPixels.GetData(), LockedData, ExpectedSize);
	Mip.BulkData.Unlock();

	OutRGBFormat = (PFmt == PF_B8G8R8A8) ? ERGBFormat::BGRA : ERGBFormat::RGBA;
	return true;
}

// --- Public: EncodeTextureToPNG ---
TArray<uint8> UAtlasWorkflowFunctionLibrary::EncodeTextureToPNG(UTexture2D* Texture, bool& bSuccess)
{
	bSuccess = false;
	TArray<uint8> OutPNG;

	if (!Texture)
	{
		return OutPNG;
	}

	// Read raw pixels from first mip
	int32 W = 0, H = 0;
	TArray<uint8> RawPixels;
	ERGBFormat RGBFormat = ERGBFormat::BGRA;

	if (!ReadTextureFirstMip(Texture, RawPixels, W, H, RGBFormat))
	{
		return OutPNG;
	}

	// Set up ImageWrapper for PNG
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid())
	{
		return OutPNG;
	}

	const int32 BitDepth = 8;
	if (!ImageWrapper->SetRaw(RawPixels.GetData(), RawPixels.Num(), W, H, RGBFormat, BitDepth))
	{
		return OutPNG;
	}

	// PNG is lossless; quality value is ignored by most encoders (100 is fine)
	OutPNG = ImageWrapper->GetCompressed(100);
	bSuccess = OutPNG.Num() > 0;
	return OutPNG;
}

// --- Public: LoadImageFileToBytes ---
TArray<uint8> UAtlasWorkflowFunctionLibrary::LoadFileIntoArray(const FString& FullFilePath, bool& bSuccess)
{
	bSuccess = false;
	TArray<uint8> OutBytes;

	if (FullFilePath.IsEmpty())
	{
		return OutBytes;
	}

	// Quick existence check (optional)
	if (!IFileManager::Get().FileExists(*FullFilePath))
	{
		return OutBytes;
	}

	if (FFileHelper::LoadFileToArray(OutBytes, *FullFilePath))
	{
		bSuccess = true;
	}
	return OutBytes;
}
