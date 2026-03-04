// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasWorkflowAssetFactory.h"
#include "AtlasWorkflowAsset.h"
#include "Misc/FileHelper.h"

// ==================== UAtlasWorkflowAssetFactory ====================

UAtlasWorkflowAssetFactory::UAtlasWorkflowAssetFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UAtlasWorkflowAsset::StaticClass();
}

UObject* UAtlasWorkflowAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UAtlasWorkflowAsset* NewAsset = NewObject<UAtlasWorkflowAsset>(InParent, Class, Name, Flags);
	return NewAsset;
}

// ==================== UAtlasWorkflowAssetFactoryImport ====================

UAtlasWorkflowAssetFactoryImport::UAtlasWorkflowAssetFactoryImport()
{
	bCreateNew = false;
	bEditorImport = true;
	bText = true;
	SupportedClass = UAtlasWorkflowAsset::StaticClass();
	Formats.Add(TEXT("json;Atlas Workflow JSON"));
	Formats.Add(TEXT("atlasworkflow;Atlas Workflow"));
}

UObject* UAtlasWorkflowAssetFactoryImport::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;

	// Read file contents
	FString JsonContents;
	if (!FFileHelper::LoadFileToString(JsonContents, *Filename))
	{
		if (Warn)
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("Failed to read file: %s"), *Filename);
		}
		return nullptr;
	}

	// Create the asset
	UAtlasWorkflowAsset* NewAsset = NewObject<UAtlasWorkflowAsset>(InParent, InClass, InName, Flags);
	if (NewAsset)
	{
		NewAsset->JsonConfig = JsonContents;
		if (!NewAsset->ParseFromJson())
		{
			if (Warn)
			{
				Warn->Logf(ELogVerbosity::Warning, TEXT("JSON parsed with warnings: %s"), *NewAsset->ParseError);
			}
		}
	}

	return NewAsset;
}

bool UAtlasWorkflowAssetFactoryImport::FactoryCanImport(const FString& Filename)
{
	// Accept .json files that might contain workflow definitions
	return Filename.EndsWith(TEXT(".json")) || Filename.EndsWith(TEXT(".atlasworkflow"));
}
