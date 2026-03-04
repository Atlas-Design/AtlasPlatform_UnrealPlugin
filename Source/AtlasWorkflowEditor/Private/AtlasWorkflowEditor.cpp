// AtlasWorkflowEditor.cpp

#include "AtlasWorkflowEditor.h"
#include "AtlasWorkflowAssetActions.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"

#define LOCTEXT_NAMESPACE "FAtlasWorkflowEditorModule"

void FAtlasWorkflowEditorModule::StartupModule()
{
    // Register asset type actions
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

    // Register Atlas Workflow Asset actions
    TSharedPtr<IAssetTypeActions> WorkflowAssetActions = MakeShareable(new FAtlasWorkflowAssetActions());
    AssetTools.RegisterAssetTypeActions(WorkflowAssetActions.ToSharedRef());
    RegisteredAssetTypeActions.Add(WorkflowAssetActions);

    UE_LOG(LogTemp, Log, TEXT("AtlasWorkflowEditor Module has started - Asset type actions registered"));
}

void FAtlasWorkflowEditorModule::ShutdownModule()
{
    // Unregister asset type actions
    if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
    {
        IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
        for (TSharedPtr<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
        {
            if (Action.IsValid())
            {
                AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
            }
        }
    }
    RegisteredAssetTypeActions.Empty();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAtlasWorkflowEditorModule, AtlasWorkflowEditor)
