// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasPlatformAuth.h"
#include "AtlasSDKSettings.h"
#include "AtlasHttpRequest.h"
#include "GenericPlatform/GenericPlatformMisc.h"

namespace
{
	void ApplyRequestTimeout(UAtlasHttpRequest* Request)
	{
		if (!Request)
		{
			return;
		}

		const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
		if (Settings && Settings->RequestTimeoutSeconds > 0.0f)
		{
			Request->SetTimeout(Settings->RequestTimeoutSeconds);
		}
	}
}

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

void FAtlasPlatformAuth::ApplyPlatformRequestSettings(UAtlasHttpRequest* Request)
{
	ApplyPlatformAuthHeaders(Request);
	ApplyRequestTimeout(Request);
}

FString FAtlasPlatformAuth::GetConfigureApiKeyMessage()
{
	return TEXT(
		"Atlas workspace API key is required. Set it in Project Settings → Plugins → Atlas SDK → "
		"Authentication, or set the ATLAS_API_KEY environment variable.");
}

FString FAtlasPlatformAuth::GetHttpFailureMessage(int32 StatusCode, const FString& FallbackMessage)
{
	switch (StatusCode)
	{
	case 401:
		return FString::Printf(TEXT("Authentication failed (401). %s"), *GetConfigureApiKeyMessage());
	case 403:
		return TEXT("Access denied (403). Verify your API key has access to this workspace.");
	default:
		return FallbackMessage;
	}
}
