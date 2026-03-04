// Copyright Atlas Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AtlasHttpTypes.h"
#include "AtlasHttpSubsystem.generated.h"

class UAtlasHttpRequest;
class UAtlasJsonObject;
class UAtlasJsonValue;

/**
 * Game Instance Subsystem for AtlasHTTP.
 * Provides factory methods for creating HTTP and JSON objects.
 * Access via GetGameInstance()->GetSubsystem<UAtlasHttpSubsystem>()
 */
UCLASS()
class ATLASHTTP_API UAtlasHttpSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Get the AtlasHTTP subsystem from a world context */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP", meta = (WorldContext = "WorldContextObject"))
	static UAtlasHttpSubsystem* Get(UObject* WorldContextObject);

	//~ HTTP Request Factory

	/** Create a new HTTP request object */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Request")
	UAtlasHttpRequest* ConstructHttpRequest();

	//~ JSON Object Factory

	/** Create a new empty JSON object */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	UAtlasJsonObject* ConstructJsonObject();

	/** Create a JSON object from a JSON string */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	UAtlasJsonObject* ConstructJsonObjectFromString(const FString& JsonString);

	//~ JSON Value Factory

	/** Create a JSON string value */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	UAtlasJsonValue* ConstructJsonValueString(const FString& Value);

	/** Create a JSON number value */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	UAtlasJsonValue* ConstructJsonValueNumber(float Value);

	/** Create a JSON boolean value */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	UAtlasJsonValue* ConstructJsonValueBool(bool Value);

	/** Create a JSON null value */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	UAtlasJsonValue* ConstructJsonValueNull();

	/** Create a JSON array value */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	UAtlasJsonValue* ConstructJsonValueArray(const TArray<UAtlasJsonValue*>& Values);

	/** Create a JSON object value */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	UAtlasJsonValue* ConstructJsonValueObject(UAtlasJsonObject* Object);
};
