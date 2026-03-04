// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AtlasWorkflowEditorUtilities.generated.h"


class UTexture2D;


/**
 * 
 */
UCLASS()
class ATLASWORKFLOWEDITOR_API UAtlasWorkflowEditorUtilities : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
    public:

        /** Export a Texture2D asset to a PNG file on disk (editor-only, format-agnostic). */
        UFUNCTION(BlueprintCallable, Category = "MyPlugin|ImageExport", meta = (DisplayName = "Export Texture2D to PNG (Generic)"))
        static bool ExportTexture2DToPNG_Generic(UTexture2D* Texture, const FString& FilePath);


        /**
         * Prints a debug message to the output log and the screen. Only works in the editor.
         * @param Message The message to display.
         */
        UFUNCTION(BlueprintCallable, Category = "AtlasWorkflow Utilities")
        static void PrintDebugMessage(const FString& Message);

        UFUNCTION(BlueprintCallable, Category = "AtlasWorkflow Utilities|Asset Preview")
        static UTexture2D* CreateTransientTextureFromBytes(
            const TArray<uint8>& ImageBytes,
            FString& OutError);

        /**
     * Creates a Texture2D asset from a byte array of compressed image data (e.g., PNG, JPG).
     * @param ImageBytes The raw byte data of the image file.
     * @param PackagePath The full content browser path where the asset will be created (e.g., "/Game/Imported/Atlas").
     * @param AssetName The name of the new texture asset (e.g., "Tex_001").
     * @param OutError A string containing any error message if the operation fails.
     * @return The newly created UTexture2D asset, or nullptr if it fails.
     */
        UFUNCTION(BlueprintCallable, Category = "AtlasWorkflow Utilities|Asset Creation")
        static UTexture2D* CreateTextureFromBytes(const TArray<uint8>& ImageBytes,
            const FString& PackagePath,
            const FString& AssetName,
            FString& OutError);

        /**
         * Creates a Texture2D asset from a byte array using the default path from Project Settings.
         * @param ImageBytes The raw byte data of the image file.
         * @param AssetName The name of the new texture asset (e.g., "Tex_001").
         * @param OutError A string containing any error message if the operation fails.
         * @return The newly created UTexture2D asset, or nullptr if it fails.
         */
        UFUNCTION(BlueprintCallable, Category = "AtlasWorkflow Utilities|Asset Creation", meta = (DisplayName = "Create Texture From Bytes (Default Path)"))
        static UTexture2D* CreateTextureFromBytesWithDefaults(const TArray<uint8>& ImageBytes,
            const FString& AssetName,
            FString& OutError);

        /**
         * Creates a Texture2D asset from a Base64 encoded string of image data.
         * @param Base64Data The Base64 encoded string.
         * @param PackagePath The full content browser path (e.g., "/Game/Imported/Atlas").
         * @param AssetName The name of the new texture asset (e.g., "Tex_001").
         * @param OutError A string containing any error message if the operation fails.
         * @return The newly created UTexture2D asset, or nullptr if it fails.
         */
        UFUNCTION(BlueprintCallable, Category = "AtlasWorkflow Utilities|Asset Creation")
        static UTexture2D* CreateTextureFromBase64(const FString& Base64Data,
            const FString& PackagePath,
            const FString& AssetName,
            FString& OutError);



        /**
         * Saves a GLB byte array to a unique temp file under Project/Saved/Atlas/TempImports
         * @param GlbBytes Raw .glb data
         * @param OutAbsoluteFilePath Filled with the absolute path to the written .glb file on success
         * @param OutError Error message on failure
         * @param FileNameHint Optional name seed (no extension)
         * @return true on success, false otherwise
         */
        UFUNCTION(BlueprintCallable, Category = "AtlasWorkflow Utilities|Files")
        static bool SaveBytesAsTempGLB(
            const TArray<uint8>& GlbBytes,
            FString& OutAbsoluteFilePath,
            FString& OutError,
            const FString& FileNameHint = TEXT("ImportedModel"));




#if WITH_EDITOR
        /**
         * Import a .glb file into the Content Browser using UE's Interchange pipeline.
         * Interchange is chosen automatically by file extension; no explicit Factory is set.
         *
         * @param AbsoluteFilePath Full absolute path to a .glb on disk.
         * @param DestinationPath Content path, e.g. "/Game/Imported". (Folder need not pre-exist.)
         * @param bSave If true, asks the import task to save created assets.
         * @param OutImportedObjectPaths Returned object paths of created assets (e.g. "/Game/Imported/MyMesh")
         * @param OutError Error description if the function returns false.
         */
        UFUNCTION(BlueprintCallable, Category = "AtlasWorkflow Utilities|Import")
        static bool ImportGLBToContent(
            const FString& AbsoluteFilePath,
            const FString& DestinationPath,
            bool bSave,
            TArray<FString>& OutImportedObjectPaths,
            FString& OutError);
#endif


};



