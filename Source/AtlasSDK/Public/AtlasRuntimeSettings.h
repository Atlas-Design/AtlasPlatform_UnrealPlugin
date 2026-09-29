#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AtlasRuntimeSettings.generated.h"

/** Shared runtime policy, safe to include in packaged builds. Contains no credentials. */
UCLASS(Config=Game, defaultconfig, meta=(DisplayName="Atlas Runtime"))
class ATLASSDK_API UAtlasRuntimeSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	/** Opt in after reviewing these settings. False preserves existing Atlas SDK configuration. */
	UPROPERTY(Config, EditAnywhere, Category="Compatibility")
	bool bUseRuntimeOverrides = false;
	UPROPERTY(Config, EditAnywhere, Category="Authentication")
	TArray<FString> TrustedApiOrigins = { TEXT("https://api.dev.atlas.design"), TEXT("https://api.prod.atlas.design") };
	UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="0"))
	float RequestTimeoutSeconds = 120.f;
	UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="0"))
	float StatusPollIntervalSeconds = 5.f;
	UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="0"))
	float MaxExecutionTimeSeconds = 900.f;
	UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="0"))
	bool bEnableUploadCache = true;
	UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="0"))
	int32 MaxUploadCacheEntries = 100;
	UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="0"))
	int32 CacheMaxAgeHours = 24;
	UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="0"))
	int32 MaxDownloadCacheMB = 128;
	UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="0"))
	int32 MaxPreviewDimension = 512;
	UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="0"))
	int32 MaxPreviewCacheMB = 32;
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};
