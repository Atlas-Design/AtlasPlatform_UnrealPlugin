// AtlasWorkflowFunctionLibrary.h

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AtlasWorkflowFunctionLibrary.generated.h"

// Forward declare the UTexture2D class
class UTexture2D;

UCLASS()
class ATLASWORKFLOW_API UAtlasWorkflowFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Encodes a UTexture2D object into a byte array representing a PNG file.
     * @param Texture The source texture to encode.
     * @param bSuccess True if the encoding was successful.
     * @return A byte array containing the PNG data.
     */
    UFUNCTION(BlueprintCallable, Category = "AtlasWorkflow Utilities|File")
    static TArray<uint8> EncodeTextureToPNG(UTexture2D* Texture, bool& bSuccess);

    /**
     * Loads any file from disk directly into a byte array.
     * @param FullFilePath The absolute path to the file.
     * @param bSuccess True if the file was loaded successfully.
     * @return A byte array containing the file's data.
     */
    UFUNCTION(BlueprintCallable, Category = "AtlasWorkflow Utilities|File")
    static TArray<uint8> LoadFileIntoArray(const FString& FullFilePath, bool& bSuccess);
};