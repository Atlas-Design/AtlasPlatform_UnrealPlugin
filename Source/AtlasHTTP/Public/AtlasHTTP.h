// Copyright Atlas Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "AtlasHttpTypes.h"

ATLASHTTP_API DECLARE_LOG_CATEGORY_EXTERN(LogAtlasHTTP, Log, All);

class FAtlasHTTPModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/**
	 * Singleton-like access to this module's interface.
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FAtlasHTTPModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FAtlasHTTPModule>("AtlasHTTP");
	}
	
	/**
	 * Checks to see if this module is loaded and ready.
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("AtlasHTTP");
	}
};
