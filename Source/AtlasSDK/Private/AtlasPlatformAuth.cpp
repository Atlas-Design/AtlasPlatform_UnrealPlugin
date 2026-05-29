// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasPlatformAuth.h"
#include "AtlasSDKSettings.h"
#include "AtlasHttpRequest.h"
#include "GenericPlatform/GenericPlatformMisc.h"

const FString FAtlasPlatformAuth::ApiKeyEnvironmentVariableName(TEXT("ATLAS_API_KEY"));

bool FAtlasPlatformAuth::HasConfiguredApiKey()
{
	return !GetResolvedApiKey().IsEmpty();
}

FString FAtlasPlatformAuth::GetResolvedApiKey()
{
	const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
	return Settings ? Settings->GetResolvedWorkspaceApiKey() : FString();
}

void FAtlasPlatformAuth::ApplyPlatformAuthHeaders(UAtlasHttpRequest* Request)
{
	if (!Request)
	{
		return;
	}

	const FString ApiKey = GetResolvedApiKey();
	if (ApiKey.IsEmpty())
	{
		return;
	}

	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
}
