// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasWorkflowAssetActions.h"
#include "AtlasWorkflowAsset.h"
#include "ToolMenus.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "AtlasWorkflowAssetActions"

FText FAtlasWorkflowAssetActions::GetName() const
{
	return LOCTEXT("AssetName", "Atlas Workflow");
}

FColor FAtlasWorkflowAssetActions::GetTypeColor() const
{
	// Atlas brand color - purple/blue
	return FColor(100, 80, 200);
}

UClass* FAtlasWorkflowAssetActions::GetSupportedClass() const
{
	return UAtlasWorkflowAsset::StaticClass();
}

uint32 FAtlasWorkflowAssetActions::GetCategories()
{
	// Use Misc category - could register custom category for "Atlas" later
	return EAssetTypeCategories::Misc;
}

void FAtlasWorkflowAssetActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	// Use default property editor
	FAssetTypeActions_Base::OpenAssetEditor(InObjects, EditWithinLevelEditor);
}

void FAtlasWorkflowAssetActions::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<TWeakObjectPtr<UAtlasWorkflowAsset>> Assets;
	for (UObject* Object : InObjects)
	{
		if (UAtlasWorkflowAsset* Asset = Cast<UAtlasWorkflowAsset>(Object))
		{
			Assets.Add(Asset);
		}
	}

	// Import from File action
	Section.AddMenuEntry(
		"AtlasWorkflow_ImportFromFile",
		LOCTEXT("ImportFromFile", "Import from JSON File..."),
		LOCTEXT("ImportFromFileTooltip", "Import workflow configuration from a JSON file"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAtlasWorkflowAssetActions::ExecuteImportFromFile, Assets),
			FCanExecuteAction()
		)
	);

	// Export to File action
	Section.AddMenuEntry(
		"AtlasWorkflow_ExportToFile",
		LOCTEXT("ExportToFile", "Export to JSON File..."),
		LOCTEXT("ExportToFileTooltip", "Export workflow configuration to a JSON file"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAtlasWorkflowAssetActions::ExecuteExportToFile, Assets),
			FCanExecuteAction()
		)
	);

	// Reparse action
	Section.AddMenuEntry(
		"AtlasWorkflow_Reparse",
		LOCTEXT("Reparse", "Reparse JSON"),
		LOCTEXT("ReparseTooltip", "Re-parse the JSON configuration"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAtlasWorkflowAssetActions::ExecuteReparse, Assets),
			FCanExecuteAction()
		)
	);
}

void FAtlasWorkflowAssetActions::ExecuteImportFromFile(TArray<TWeakObjectPtr<UAtlasWorkflowAsset>> Assets)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return;
	}

	TArray<FString> OutFiles;
	const bool bOpened = DesktopPlatform->OpenFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		TEXT("Import Atlas Workflow"),
		FPaths::ProjectDir(),
		TEXT(""),
		TEXT("JSON Files (*.json)|*.json|All Files (*.*)|*.*"),
		EFileDialogFlags::None,
		OutFiles
	);

	if (bOpened && OutFiles.Num() > 0)
	{
		FString JsonContents;
		if (FFileHelper::LoadFileToString(JsonContents, *OutFiles[0]))
		{
			for (TWeakObjectPtr<UAtlasWorkflowAsset>& AssetPtr : Assets)
			{
				if (UAtlasWorkflowAsset* Asset = AssetPtr.Get())
				{
					Asset->JsonConfig = JsonContents;
					Asset->ParseFromJson();
					Asset->MarkPackageDirty();
				}
			}
		}
	}
}

void FAtlasWorkflowAssetActions::ExecuteExportToFile(TArray<TWeakObjectPtr<UAtlasWorkflowAsset>> Assets)
{
	if (Assets.Num() == 0)
	{
		return;
	}

	UAtlasWorkflowAsset* FirstAsset = Assets[0].Get();
	if (!FirstAsset)
	{
		return;
	}

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return;
	}

	// Suggest filename based on asset name
	FString SuggestedName = FirstAsset->GetName() + TEXT(".json");

	TArray<FString> OutFiles;
	const bool bSaved = DesktopPlatform->SaveFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		TEXT("Export Atlas Workflow"),
		FPaths::ProjectDir(),
		SuggestedName,
		TEXT("JSON Files (*.json)|*.json"),
		EFileDialogFlags::None,
		OutFiles
	);

	if (bSaved && OutFiles.Num() > 0)
	{
		FirstAsset->ExportToFile(OutFiles[0]);
	}
}

void FAtlasWorkflowAssetActions::ExecuteReparse(TArray<TWeakObjectPtr<UAtlasWorkflowAsset>> Assets)
{
	for (TWeakObjectPtr<UAtlasWorkflowAsset>& AssetPtr : Assets)
	{
		if (UAtlasWorkflowAsset* Asset = AssetPtr.Get())
		{
			Asset->ParseFromJson();
			Asset->MarkPackageDirty();
		}
	}
}

#undef LOCTEXT_NAMESPACE
