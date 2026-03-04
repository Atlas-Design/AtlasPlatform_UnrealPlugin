// Copyright Atlas Games. All Rights Reserved.

#include "AtlasHTTP.h"

#define LOCTEXT_NAMESPACE "FAtlasHTTPModule"

DEFINE_LOG_CATEGORY(LogAtlasHTTP);

void FAtlasHTTPModule::StartupModule()
{
	UE_LOG(LogAtlasHTTP, Log, TEXT("AtlasHTTP module started"));
}

void FAtlasHTTPModule::ShutdownModule()
{
	UE_LOG(LogAtlasHTTP, Log, TEXT("AtlasHTTP module shutdown"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAtlasHTTPModule, AtlasHTTP)