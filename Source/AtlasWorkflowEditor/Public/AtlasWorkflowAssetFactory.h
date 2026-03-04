// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "AtlasWorkflowAssetFactory.generated.h"

/**
 * Factory for creating UAtlasWorkflowAsset objects in the Content Browser.
 * Enables: Right-click → Miscellaneous → Atlas Workflow Asset
 */
UCLASS()
class ATLASWORKFLOWEDITOR_API UAtlasWorkflowAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UAtlasWorkflowAssetFactory();

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ShouldShowInNewMenu() const override { return true; }
	// End UFactory interface
};

/**
 * Factory for importing UAtlasWorkflowAsset from JSON files.
 * Enables: Drag & drop JSON files, or Import button
 */
UCLASS()
class ATLASWORKFLOWEDITOR_API UAtlasWorkflowAssetFactoryImport : public UFactory
{
	GENERATED_BODY()

public:
	UAtlasWorkflowAssetFactoryImport();

	// UFactory interface
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	// End UFactory interface
};
