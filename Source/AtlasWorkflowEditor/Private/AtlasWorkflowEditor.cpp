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
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FAtlasWorkflowEditorModule"

// Set true when Batch Editor and standalone Job History are ready for a public release.
static constexpr bool GAtlasExposeBatchAndJobHistoryUI = false;

// EUW asset paths — update these if you move the widgets
static const TCHAR* GWorkflowEditorPath = TEXT("/AtlasWorkflow/Core/UI/Editor/EUW_AtlasMain.EUW_AtlasMain");
static const TCHAR* GBatchEditorPath    = TEXT("/AtlasWorkflow/Core/UI/Editor/EUW_BatchEditor.EUW_BatchEditor");
static const TCHAR* GJobHistoryPath     = TEXT("/AtlasWorkflow/Core/UI/Editor/EUW_JobHistory.EUW_JobHistory");

// ==================== Module Lifecycle ====================

void FAtlasWorkflowEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	TSharedPtr<IAssetTypeActions> WorkflowAssetActions = MakeShareable(new FAtlasWorkflowAssetActions());
	AssetTools.RegisterAssetTypeActions(WorkflowAssetActions.ToSharedRef());
	RegisteredAssetTypeActions.Add(WorkflowAssetActions);

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAtlasWorkflowEditorModule::RegisterMenus));

	UE_LOG(LogTemp, Log, TEXT("AtlasWorkflowEditor module started"));
}

void FAtlasWorkflowEditorModule::ShutdownModule()
{
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

	UToolMenus::UnregisterOwner(this);
}

// ==================== Menu Registration ====================

void FAtlasWorkflowEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// --- Toolbar combo button ---
	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	FToolMenuSection& ToolbarSection = ToolbarMenu->FindOrAddSection("Atlas");

	FToolMenuEntry SubMenuEntry = FToolMenuEntry::InitSubMenu(
		"AtlasToolbar",
		LOCTEXT("AtlasToolbarLabel", "Atlas"),
		LOCTEXT("AtlasToolbarTooltip", "Atlas Platform workflow editor"),
		FNewToolMenuDelegate::CreateStatic(&FAtlasWorkflowEditorModule::BuildAtlasToolbarMenu),
		false,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.GameSettings")
	);
	SubMenuEntry.StyleNameOverride = "CalloutToolbar";
	ToolbarSection.AddEntry(SubMenuEntry);

	// --- Window > Atlas submenu (secondary access) ---
	UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	FToolMenuSection& WindowSection = WindowMenu->FindOrAddSection("AtlasWorkflow");
	WindowSection.Label = LOCTEXT("AtlasWindowSection", "Atlas");

	WindowSection.AddMenuEntry(
		"OpenWorkflowEditor",
		LOCTEXT("WindowWorkflowEditor", "Workflow Editor"),
		LOCTEXT("WindowWorkflowEditorTip", "Open the single workflow editor"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.GameSettings"),
		FUIAction(FExecuteAction::CreateRaw(this, &FAtlasWorkflowEditorModule::OpenWorkflowEditor))
	);

	if (GAtlasExposeBatchAndJobHistoryUI)
	{
		WindowSection.AddMenuEntry(
			"OpenBatchEditor",
			LOCTEXT("WindowBatchEditor", "Batch Editor"),
			LOCTEXT("WindowBatchEditorTip", "Open the batch workflow editor"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.GameSettings"),
			FUIAction(FExecuteAction::CreateRaw(this, &FAtlasWorkflowEditorModule::OpenBatchEditor))
		);

		WindowSection.AddMenuEntry(
			"OpenJobHistory",
			LOCTEXT("WindowJobHistory", "Job History"),
			LOCTEXT("WindowJobHistoryTip", "Open the job history viewer"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.GameSettings"),
			FUIAction(FExecuteAction::CreateRaw(this, &FAtlasWorkflowEditorModule::OpenJobHistory))
		);
	}
}

void FAtlasWorkflowEditorModule::BuildAtlasToolbarMenu(UToolMenu* Menu)
{
	FToolMenuSection& Section = Menu->FindOrAddSection("AtlasTools");

	Section.AddMenuEntry(
		"WorkflowEditor",
		LOCTEXT("ToolbarWorkflowEditor", "Workflow Editor"),
		LOCTEXT("ToolbarWorkflowEditorTip", "Run a single workflow with full parameter editing"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.GameSettings"),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FModuleManager::GetModuleChecked<FAtlasWorkflowEditorModule>("AtlasWorkflowEditor").OpenWorkflowEditor();
		}))
	);

	if (GAtlasExposeBatchAndJobHistoryUI)
	{
		Section.AddMenuEntry(
			"BatchEditor",
			LOCTEXT("ToolbarBatchEditor", "Batch Editor"),
			LOCTEXT("ToolbarBatchEditorTip", "Run multiple workflow variations as a batch"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.GameSettings"),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FModuleManager::GetModuleChecked<FAtlasWorkflowEditorModule>("AtlasWorkflowEditor").OpenBatchEditor();
			}))
		);

		Section.AddMenuEntry(
			"JobHistory",
			LOCTEXT("ToolbarJobHistory", "Job History"),
			LOCTEXT("ToolbarJobHistoryTip", "View running jobs, history, and batch results"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.GameSettings"),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FModuleManager::GetModuleChecked<FAtlasWorkflowEditorModule>("AtlasWorkflowEditor").OpenJobHistory();
			}))
		);
	}
}

// ==================== EUW Launchers ====================

void FAtlasWorkflowEditorModule::OpenEditorUtilityWidget(const FString& WidgetAssetPath)
{
	const FSoftObjectPath WidgetPath(WidgetAssetPath);
	UObject* LoadedObject = WidgetPath.TryLoad();

	if (UEditorUtilityWidgetBlueprint* WidgetBlueprint = Cast<UEditorUtilityWidgetBlueprint>(LoadedObject))
	{
		if (UEditorUtilitySubsystem* Subsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>())
		{
			Subsystem->SpawnAndRegisterTab(WidgetBlueprint);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Atlas: Could not load EUW at %s"), *WidgetAssetPath);
	}
}

void FAtlasWorkflowEditorModule::OpenWorkflowEditor()
{
	OpenEditorUtilityWidget(GWorkflowEditorPath);
}

void FAtlasWorkflowEditorModule::OpenBatchEditor()
{
	OpenEditorUtilityWidget(GBatchEditorPath);
}

void FAtlasWorkflowEditorModule::OpenJobHistory()
{
	OpenEditorUtilityWidget(GJobHistoryPath);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAtlasWorkflowEditorModule, AtlasWorkflowEditor)
