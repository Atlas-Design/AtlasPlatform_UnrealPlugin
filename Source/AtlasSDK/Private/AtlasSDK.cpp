// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasSDK.h"

#define LOCTEXT_NAMESPACE "FAtlasSDKModule"

void FAtlasSDKModule::StartupModule()
{
	// Module startup - types are automatically registered via UHT
}

void FAtlasSDKModule::ShutdownModule()
{
	// Module shutdown
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAtlasSDKModule, AtlasSDK)
