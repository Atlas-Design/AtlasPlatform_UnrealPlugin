// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasWorkflowUIModule.h"

#define LOCTEXT_NAMESPACE "FAtlasWorkflowUIModule"

void FAtlasWorkflowUIModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("AtlasWorkflowUI Module has started"));
}

void FAtlasWorkflowUIModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("AtlasWorkflowUI Module has shut down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAtlasWorkflowUIModule, AtlasWorkflowUI)
