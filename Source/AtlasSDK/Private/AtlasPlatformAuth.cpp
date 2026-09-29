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

bool FAtlasPlatformAuth::IsTrustedDestination(const FString& Url)
{
	auto Origin = [](FString Value) -> FString
	{
		if (!Value.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase)) return FString();
		Value.RightChopInline(8);
		int32 End = Value.Len();
		for (int32 i = 0; i < Value.Len(); ++i)
		{
			if (Value[i] == TEXT('/') || Value[i] == TEXT('?') || Value[i] == TEXT('#')) { End = i; break; }
		}
		Value = Value.Left(End).ToLower();
		if (Value.IsEmpty()) return FString();
		for (TCHAR Ch : Value)
		{
			if (!(FChar::IsAlnum(Ch) || Ch == TEXT('.') || Ch == TEXT('-') || Ch == TEXT(':'))) return FString();
		}
		Value.RemoveFromEnd(TEXT(":443"));
		return TEXT("https://") + Value;
	};
	const FString Target = Origin(Url);
	if (Target.IsEmpty()) return false;
	for (const FString& Allowed : UAtlasSDKSettings::Get()->TrustedApiOrigins)
	{
		if (Origin(Allowed) == Target) return true;
	}
	return false;
}

void FAtlasPlatformAuth::ApplyPlatformAuthHeaders(UAtlasHttpRequest* Request)
{
	if (!Request)
	{
		return;
	}

	if (!IsTrustedDestination(Request->GetURL()))
	{
		Request->SetPreflightError(TEXT("Destination is not a trusted HTTPS API origin. Configure TrustedApiOrigins in Atlas SDK settings."));
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
