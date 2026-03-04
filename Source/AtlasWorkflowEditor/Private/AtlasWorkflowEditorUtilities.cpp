// AtlasWorkflowEditorUtilities.cpp

#include "AtlasWorkflowEditorUtilities.h"
#include "Engine/Engine.h" // Required for GEngine
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture2D.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/Base64.h"
#include "AtlasWorkflowEditorSettings.h"

// Image utils / core
#include "ImageUtils.h"    // FImageUtils, GetTexture2DSourceImage, GetRenderTargetImage, SaveImageByExtension
#include "ImageCore.h"     // FImage, FImageView


#if WITH_EDITOR

// Small helper: allow relative paths (relative to project dir) or absolute
static FString NormalizeOutputPath(const FString& InPath)
{
    if (FPaths::IsRelative(InPath))
    {
        return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), InPath);
    }
    return InPath;
}

// Generic save for an FImage -> PNG/JPG/etc. using file extension
static bool SaveImageAsByExtension(const FImage& Image, const FString& InPath, int32 Quality = 100)
{
    const FString AbsolutePath = NormalizeOutputPath(InPath);
    const FString Directory = FPaths::GetPath(AbsolutePath);
    IFileManager::Get().MakeDirectory(*Directory, /*Tree=*/true);

    // FImageView is a lightweight view on FImage data
    const FImageView ImageView(Image);

    return FImageUtils::SaveImageByExtension(*AbsolutePath, ImageView, Quality);
}

#endif // WITH_EDITOR

bool UAtlasWorkflowEditorUtilities::ExportTexture2DToPNG_Generic(UTexture2D* Texture, const FString& FilePath)
{
#if WITH_EDITOR
    if (!Texture)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExportTexture2DToPNG_Generic: Texture is null."));
        return false;
    }

    FImage Image;
    // This uses the source art, not platform-compressed data
    if (!FImageUtils::GetTexture2DSourceImage(Texture, Image))
    {
        UE_LOG(LogTemp, Warning, TEXT("ExportTexture2DToPNG_Generic: Failed to get source image for %s"), *Texture->GetName());
        return false;
    }

    // As long as FilePath ends with .png, this will produce a PNG
    return SaveImageAsByExtension(Image, FilePath, /*Quality=*/100);
#else
    return false;
#endif
}


static bool DecodeImageToRGBA8(const TArray<uint8>& InBytes, int32& OutW, int32& OutH, TArray<uint8>& OutRGBA)
{
    if (InBytes.Num() == 0) return false;

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");

    // Try common formats; ImageWrapper figures it out
    const EImageFormat FormatsToTry[] = { EImageFormat::PNG, EImageFormat::JPEG, EImageFormat::BMP, EImageFormat::EXR, EImageFormat::TIFF };
    for (EImageFormat Fmt : FormatsToTry)
    {
        TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(Fmt);
        if (Wrapper.IsValid() && Wrapper->SetCompressed(InBytes.GetData(), InBytes.Num()))
        {
            OutW = Wrapper->GetWidth();
            OutH = Wrapper->GetHeight();
            return Wrapper->GetRaw(ERGBFormat::RGBA, 8, OutRGBA);
        }
    }
    return false;
}

UTexture2D* UAtlasWorkflowEditorUtilities::CreateTransientTextureFromBytes(
    const TArray<uint8>& ImageBytes,
    FString& OutError)
{
    OutError.Empty();

    int32 W = 0, H = 0;
    TArray<uint8> RGBA;
    if (!DecodeImageToRGBA8(ImageBytes, W, H, RGBA))
    {
        OutError = TEXT("Failed to decode image bytes.");
        return nullptr;
    }

    // Create a transient texture
    UTexture2D* Tex = UTexture2D::CreateTransient(W, H, PF_R8G8B8A8);
    if (!Tex)
    {
        OutError = TEXT("Failed to create transient texture.");
        return nullptr;
    }

    Tex->CompressionSettings = TC_Default;
    Tex->SRGB = true;
    Tex->MipGenSettings = TMGS_NoMipmaps;
    Tex->NeverStream = true;

#if WITH_EDITORONLY_DATA
    Tex->bUseLegacyGamma = false;
#endif

    // Copy pixels into mip 0
    FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
    void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
    const int64 BytesToCopy = (int64)W * (int64)H * 4;
    FMemory::Memcpy(Data, RGBA.GetData(), BytesToCopy);
    Mip.BulkData.Unlock();

    Tex->UpdateResource(); // push to RHI

    return Tex; // transient (not saved). You can put it in a widget or material, etc.
}

void UAtlasWorkflowEditorUtilities::PrintDebugMessage(const FString& Message)
{
    // Print to the Output Log
    UE_LOG(LogTemp, Warning, TEXT("AtlasWorkflow Utility: %s"), *Message);

    // Also print to the screen for immediate feedback
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,        // Key, -1 means we don't need to worry about overwriting previous messages
            5.0f,      // Time to display for
            FColor::Cyan, // Color of the message
            FString::Printf(TEXT("AtlasWorkflow: %s"), *Message)
        );
    }

}

UTexture2D* UAtlasWorkflowEditorUtilities::CreateTextureFromBytes(
    const TArray<uint8>& ImageBytes,
    const FString& PackagePath,
    const FString& AssetName,
    FString& OutError)
{
    OutError = TEXT("");

    if (ImageBytes.Num() == 0)
    {
        OutError = TEXT("Image data is empty.");
        return nullptr;
    }

    if (PackagePath.IsEmpty() || AssetName.IsEmpty())
    {
        OutError = TEXT("Package path or asset name is empty.");
        return nullptr;
    }

    // --- Decode image ---
    IImageWrapperModule& ImageWrapperModule =
        FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

    EImageFormat ImageFormat =
        ImageWrapperModule.DetectImageFormat(ImageBytes.GetData(), ImageBytes.Num());

    if (ImageFormat == EImageFormat::Invalid)
    {
        OutError = TEXT("Failed to detect image format. The data may be corrupt or an unsupported format.");
        return nullptr;
    }

    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
    if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(ImageBytes.GetData(), ImageBytes.Num()))
    {
        OutError = TEXT("Failed to initialize image wrapper.");
        return nullptr;
    }

    TArray<uint8> UncompressedBGRA;
    if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedBGRA))
    {
        OutError = TEXT("Failed to decompress image data.");
        return nullptr;
    }

    const int32 Width = ImageWrapper->GetWidth();
    const int32 Height = ImageWrapper->GetHeight();

    // --- Create package / asset ---
    const FString FullPackagePath = PackagePath / AssetName;   // e.g. /Game/Imported/Atlas/Textures/TextureColder

    UPackage* Package = CreatePackage(*FullPackagePath);
    if (!Package)
    {
        OutError = TEXT("Failed to create package.");
        return nullptr;
    }

    Package->SetDirtyFlag(true);

    UTexture2D* NewTexture = NewObject<UTexture2D>(
        Package,
        FName(*AssetName),
        RF_Public | RF_Standalone | RF_MarkAsRootSet
    );
    if (!NewTexture)
    {
        OutError = TEXT("Failed to create new texture object.");
        return nullptr;
    }

    // --- Platform data (runtime) ---
    if (!NewTexture->GetPlatformData())
    {
        NewTexture->SetPlatformData(new FTexturePlatformData());
    }

    FTexturePlatformData* PlatformData = NewTexture->GetPlatformData();
    PlatformData->SizeX = Width;
    PlatformData->SizeY = Height;
    PlatformData->PixelFormat = PF_B8G8R8A8;

    FTexture2DMipMap* Mip = new FTexture2DMipMap();
    PlatformData->Mips.Add(Mip);
    Mip->SizeX = Width;
    Mip->SizeY = Height;

    Mip->BulkData.Lock(LOCK_READ_WRITE);
    void* MipData = Mip->BulkData.Realloc(UncompressedBGRA.Num());
    FMemory::Memcpy(MipData, UncompressedBGRA.GetData(), UncompressedBGRA.Num());
    Mip->BulkData.Unlock();

    // --- Source data (editor / cooking) ---
    // This is what fixes "TSF Invalid", dimensions 0x0, Is Source Valid: False
    NewTexture->Source.Init(
        Width,
        Height,
        /*NumSlices*/ 1,
        /*NumMips*/   1,
        TSF_BGRA8,
        UncompressedBGRA.GetData()
    );

    // Basic settings – tweak to taste
    NewTexture->CompressionSettings = TC_Default;
    NewTexture->SRGB = true;
    NewTexture->LODGroup = TEXTUREGROUP_World;
    NewTexture->NeverStream = false;

    // Tell editor / renderer about the new asset
    NewTexture->UpdateResource();
    NewTexture->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewTexture);
    NewTexture->PostEditChange();

    return NewTexture;
}

UTexture2D* UAtlasWorkflowEditorUtilities::CreateTextureFromBytesWithDefaults(const TArray<uint8>& ImageBytes, const FString& AssetName, FString& OutError)
{
    const UAtlasWorkflowEditorSettings* Settings = GetDefault<UAtlasWorkflowEditorSettings>();
    if (!Settings)
    {
        OutError = TEXT("Could not load Atlas Workflow Editor settings.");
        return nullptr;
    }

    return CreateTextureFromBytes(ImageBytes, Settings->DefaultTextureImportPath, AssetName, OutError);
}

UTexture2D* UAtlasWorkflowEditorUtilities::CreateTextureFromBase64(const FString& Base64Data, const FString& PackagePath, const FString& AssetName, FString& OutError)
{
    TArray<uint8> ImageBytes;
    if (!FBase64::Decode(Base64Data, ImageBytes))
    {
        OutError = TEXT("Failed to decode Base64 string. It may be invalid.");
        return nullptr;
    }

    return CreateTextureFromBytes(ImageBytes, PackagePath, AssetName, OutError);
}

bool UAtlasWorkflowEditorUtilities::SaveBytesAsTempGLB(
    const TArray<uint8>& GlbBytes,
    FString& OutAbsoluteFilePath,
    FString& OutError,
    const FString& FileNameHint)
{
    OutError.Empty();
    OutAbsoluteFilePath.Empty();

    if (GlbBytes.Num() == 0)
    {
        OutError = TEXT("GLB byte array is empty.");
        return false;
    }

    // Saved/Atlas/TempImports
    const FString BaseDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Atlas"), TEXT("TempImports"));
    IFileManager& FileMgr = IFileManager::Get();
    if (!FileMgr.MakeDirectory(*BaseDir, /*Tree*/true))
    {
        OutError = FString::Printf(TEXT("Failed to create directory: %s"), *BaseDir);
        return false;
    }

    // Unique filename: <Hint>_<GUID>.glb
    const FString FileName = FString::Printf(TEXT("%s_%s.glb"),
        *FileNameHint, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    const FString AbsPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(BaseDir, FileName));

    if (!FFileHelper::SaveArrayToFile(GlbBytes, *AbsPath))
    {
        OutError = FString::Printf(TEXT("Failed to write file: %s"), *AbsPath);
        return false;
    }

    OutAbsoluteFilePath = AbsPath;
    return true;
}

#if WITH_EDITOR
#include "Misc/Paths.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#endif

bool UAtlasWorkflowEditorUtilities::ImportGLBToContent(
    const FString& AbsoluteFilePath,
    const FString& DestinationPath,
    bool bSave,
    TArray<FString>& OutImportedObjectPaths,
    FString& OutError)
{
#if !WITH_EDITOR
    OutError = TEXT("ImportGLBToContent can only be used in the Editor.");
    return false;
#else
    OutImportedObjectPaths.Reset();
    OutError.Empty();

    // Basic checks
    if (AbsoluteFilePath.IsEmpty())
    {
        OutError = TEXT("AbsoluteFilePath is empty.");
        return false;
    }
    if (!FPaths::FileExists(AbsoluteFilePath))
    {
        OutError = FString::Printf(TEXT("File does not exist: %s"), *AbsoluteFilePath);
        return false;
    }
    if (DestinationPath.IsEmpty() || !DestinationPath.StartsWith(TEXT("/")))
    {
        OutError = TEXT("DestinationPath must be a valid content path, e.g. \"/Game/Imported\".");
        return false;
    }

    // Prepare the import task (no explicit Factory -> Interchange selects by extension)
    UAssetImportTask* Task = NewObject<UAssetImportTask>();
    Task->Filename = AbsoluteFilePath;
    Task->DestinationPath = DestinationPath;
    Task->bAutomated = true;
    Task->bSave = bSave;
    Task->Factory = nullptr;

    TArray<UAssetImportTask*> Tasks;
    Tasks.Add(Task);

    // Run the task
    FAssetToolsModule& AssetTools = FAssetToolsModule::GetModule();
    AssetTools.Get().ImportAssetTasks(Tasks);

    // Collect results (use whichever field your engine version supports)
#if ENGINE_MAJOR_VERSION >= 5
    if (Task->ImportedObjectPaths.Num() > 0)
    {
        OutImportedObjectPaths = Task->ImportedObjectPaths;
    }
#endif
    if (OutImportedObjectPaths.Num() == 0)
    {
        OutError = TEXT("Import finished, but no assets were reported (ImportedObjectPaths empty). "
            "Ensure Interchange GLTF plugins are enabled and check the Output Log.");
        return false;
    }

    if (OutImportedObjectPaths.Num() == 0)
    {
        OutError = TEXT("Import finished, but no assets were reported. Check the Output Log for Interchange messages.");
        return false;
    }

    return true;
#endif
}