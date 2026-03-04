// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

/**
 * Atlas SDK Module
 * Provides core types and functionality for executing Atlas Platform workflows.
 */
class FAtlasSDKModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Get the Atlas SDK module instance.
	 * @return Reference to the Atlas SDK module
	 */
	static inline FAtlasSDKModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FAtlasSDKModule>("AtlasSDK");
	}

	/**
	 * Check if the Atlas SDK module is loaded.
	 * @return True if the module is loaded
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("AtlasSDK");
	}
};
