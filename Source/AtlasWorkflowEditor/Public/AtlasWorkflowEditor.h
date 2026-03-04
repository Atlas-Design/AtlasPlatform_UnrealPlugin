// AtlasWorkflowEditor.h

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "AssetTypeCategories.h"

class IAssetTypeActions;

class FAtlasWorkflowEditorModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    /** Register menu extensions */
    void RegisterMenus();
    
    /** Open the Atlas Workflow editor window */
    void OpenAtlasWorkflowWindow();

    /** Registered asset type actions */
    TArray<TSharedPtr<IAssetTypeActions>> RegisteredAssetTypeActions;
};