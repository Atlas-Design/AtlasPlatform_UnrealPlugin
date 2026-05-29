// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UAtlasHttpRequest;

/**
 * Resolves workspace API keys and applies Atlas Platform v0.2 authentication headers.
 */
namespace FAtlasPlatformAuth
{
	/** Environment variable checked when project settings key is empty (if enabled). */
	ATLASSDK_API extern const FString ApiKeyEnvironmentVariableName;

	/** True when a non-empty API key is available from settings and/or environment. */
	ATLASSDK_API bool HasConfiguredApiKey();

	/** Workspace API key from project settings, else ATLAS_API_KEY when configured to read env. */
	ATLASSDK_API FString GetResolvedApiKey();

	/** Sets Authorization: Bearer on the request when a key is configured. */
	ATLASSDK_API void ApplyPlatformAuthHeaders(UAtlasHttpRequest* Request);
}
