// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasWorkflowUIModule.h"
#include "AtlasSDK.h"

#define LOCTEXT_NAMESPACE "FAtlasWorkflowUIModule"

void FAtlasWorkflowUIModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("AtlasWorkflowUI Module has started"));
}

void FAtlasWorkflowUIModule::ShutdownModule()
{
	FAtlasSDKModule::DrainFileWork();
	UE_LOG(LogTemp, Log, TEXT("AtlasWorkflowUI Module has shut down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAtlasWorkflowUIModule, AtlasWorkflowUI)
