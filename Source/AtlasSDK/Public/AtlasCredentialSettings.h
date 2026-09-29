#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AtlasCredentialSettings.generated.h"

/** Local editor credentials. Never use defaultconfig: SaveConfig must target the user-generated INI. */
UCLASS(Config=EditorPerProjectUserSettings, meta=(DisplayName="Atlas Credentials (Local User)"))
class ATLASSDK_API UAtlasCredentialSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, Category="Authentication", meta=(PasswordField=true))
	FString WorkspaceApiKey;
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};
