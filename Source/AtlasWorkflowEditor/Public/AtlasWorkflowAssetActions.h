// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class UAtlasWorkflowAsset;

/**
 * Asset type actions for UAtlasWorkflowAsset.
 * Provides Content Browser integration including custom category and color.
 */
class ATLASWORKFLOWEDITOR_API FAtlasWorkflowAssetActions : public FAssetTypeActions_Base
{
public:
	// FAssetTypeActions_Base interface
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor) override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	// End FAssetTypeActions_Base interface

private:
	/** Import workflow from JSON file and replace current content */
	void ExecuteImportFromFile(TArray<TWeakObjectPtr<UAtlasWorkflowAsset>> Assets);
	
	/** Export workflow JSON to file */
	void ExecuteExportToFile(TArray<TWeakObjectPtr<UAtlasWorkflowAsset>> Assets);
	
	/** Re-parse the JSON configuration */
	void ExecuteReparse(TArray<TWeakObjectPtr<UAtlasWorkflowAsset>> Assets);
};
