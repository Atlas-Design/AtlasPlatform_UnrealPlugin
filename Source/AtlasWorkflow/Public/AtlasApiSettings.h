#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AtlasApiSettings.generated.h"

/**
 * Runtime settings for Atlas Workflow plugin.
 * Configure network timeouts and file paths.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Atlas Workflow"))
class ATLASWORKFLOW_API UAtlasApiSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Default timeout for network requests in seconds */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Network", meta = (ClampMin = "1.0"))
	float DefaultTimeoutSec = 400.f;

	/** Relative folder inside the project directory for temporary exports */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Export")
	FString DefaultExportPath = TEXT("Saved/Atlas/TempExports");

	/** Relative folder inside the project directory for temporary imports */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Export")
	FString DefaultImportPath = TEXT("Saved/Atlas/TempImports");

	/** Returns the absolute path for the default export folder */
	UFUNCTION(BlueprintPure, Category = "Atlas|Paths")
	static FString GetAtlasDefaultExportFolder();

	/** Returns the absolute path for the default import folder */
	UFUNCTION(BlueprintPure, Category = "Atlas|Paths")
	static FString GetAtlasDefaultImportFolder();
};
