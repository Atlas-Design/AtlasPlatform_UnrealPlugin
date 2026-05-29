// AtlasWorkflowEditor.h

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "AssetTypeCategories.h"

class IAssetTypeActions;

class FAtlasWorkflowEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();

    void OpenEditorUtilityWidget(const FString& WidgetAssetPath);
    void OpenWorkflowEditor();
    void OpenBatchEditor();
    void OpenJobHistory();

    static void BuildAtlasToolbarMenu(UToolMenu* Menu);

    TArray<TSharedPtr<IAssetTypeActions>> RegisteredAssetTypeActions;
};