// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Atlas Workflow UI Module
 * 
 * Editor-only module providing UI widgets for the Atlas SDK.
 * Contains reusable C++ base classes for Blueprint widgets.
 */
class FAtlasWorkflowUIModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Get the Atlas Workflow UI module instance.
	 * @return Reference to the module
	 */
	static inline FAtlasWorkflowUIModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FAtlasWorkflowUIModule>("AtlasWorkflowUI");
	}

	/**
	 * Check if the Atlas Workflow UI module is loaded.
	 * @return True if the module is loaded
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("AtlasWorkflowUI");
	}
};
