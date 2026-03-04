// AtlasWorkflowEditorSettings.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AtlasWorkflowEditorSettings.generated.h"

/**
 * Editor settings for Atlas Workflow plugin.
 * Configure default paths and editor-specific options.
 */
UCLASS(Config = Editor, defaultconfig, meta = (DisplayName = "Atlas Workflow"))
class ATLASWORKFLOWEDITOR_API UAtlasWorkflowEditorSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UAtlasWorkflowEditorSettings()
    {
        DefaultTextureImportPath = TEXT("/Game/Imported/Atlas/Textures");
    }

    /** The default content browser path where new texture assets will be created. */
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Asset Creation")
    FString DefaultTextureImportPath;
};