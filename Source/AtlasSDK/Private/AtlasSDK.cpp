// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasSDK.h"
#include "AtlasFileWork.h"

#define LOCTEXT_NAMESPACE "FAtlasSDKModule"

void FAtlasSDKModule::StartupModule()
{
	// Module startup - types are automatically registered via UHT
}

void FAtlasSDKModule::DrainFileWork() { AtlasFileWork::Drain(); }

void FAtlasSDKModule::ShutdownModule()
{
	AtlasFileWork::Shutdown();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAtlasSDKModule, AtlasSDK)
