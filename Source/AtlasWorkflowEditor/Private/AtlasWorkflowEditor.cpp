// AtlasWorkflowEditor.cpp

#include "AtlasWorkflowEditor.h"
#include "AtlasWorkflowAssetActions.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ToolMenus.h"
#include "LevelEditor.h"
#include "EditorUtilityWidget.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "FAtlasWorkflowEditorModule"

void FAtlasWorkflowEditorModule::StartupModule()
{
    // Register asset type actions
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

    // Register Atlas Workflow Asset actions
    TSharedPtr<IAssetTypeActions> WorkflowAssetActions = MakeShareable(new FAtlasWorkflowAssetActions());
    AssetTools.RegisterAssetTypeActions(WorkflowAssetActions.ToSharedRef());
    RegisteredAssetTypeActions.Add(WorkflowAssetActions);

    // Register Window menu extension
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAtlasWorkflowEditorModule::RegisterMenus));

    UE_LOG(LogTemp, Log, TEXT("AtlasWorkflowEditor Module has started - Asset type actions registered"));
}

void FAtlasWorkflowEditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    // Extend the Window menu
    UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
    
    FToolMenuSection& Section = WindowMenu->FindOrAddSection("AtlasWorkflow");
    Section.Label = LOCTEXT("AtlasWorkflowSection", "Atlas");
    
    Section.AddMenuEntry(
        "OpenAtlasWorkflow",
        LOCTEXT("OpenAtlasWorkflow", "Atlas Workflow"),
        LOCTEXT("OpenAtlasWorkflowTooltip", "Open the Atlas Workflow editor window"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"),
        FUIAction(FExecuteAction::CreateRaw(this, &FAtlasWorkflowEditorModule::OpenAtlasWorkflowWindow))
    );
}

void FAtlasWorkflowEditorModule::OpenAtlasWorkflowWindow()
{
    // Path to the Editor Utility Widget Blueprint
    const FSoftObjectPath WidgetPath(TEXT("/AtlasWorkflow/EUW_AtlasMain.EUW_AtlasMain"));
    
    UObject* LoadedObject = WidgetPath.TryLoad();
    if (UEditorUtilityWidgetBlueprint* WidgetBlueprint = Cast<UEditorUtilityWidgetBlueprint>(LoadedObject))
    {
        if (UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>())
        {
            EditorUtilitySubsystem->SpawnAndRegisterTab(WidgetBlueprint);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Atlas Workflow: Could not find EUW_AtlasMain widget at %s"), *WidgetPath.ToString());
    }
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
