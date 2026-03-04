// Copyright Atlas Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Dom/JsonValue.h"
#include "AtlasJsonValue.generated.h"

class UAtlasJsonObject;

/**
 * Type of JSON value
 */
UENUM(BlueprintType)
enum class EAtlasJsonValueType : uint8
{
	None		UMETA(DisplayName = "None"),
	Null		UMETA(DisplayName = "Null"),
	String		UMETA(DisplayName = "String"),
	Number		UMETA(DisplayName = "Number"),
	Boolean		UMETA(DisplayName = "Boolean"),
	Array		UMETA(DisplayName = "Array"),
	Object		UMETA(DisplayName = "Object")
};

/**
 * Blueprint-friendly wrapper for JSON values.
 * Wraps Unreal's native FJsonValue for Blueprint exposure.
 */
UCLASS(BlueprintType, Blueprintable)
class ATLASHTTP_API UAtlasJsonValue : public UObject
{
	GENERATED_BODY()

public:
	UAtlasJsonValue();

	//~ Creation functions

	/** Create a JSON value from a string */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON", meta = (DisplayName = "Make JSON String Value"))
	static UAtlasJsonValue* MakeString(const FString& Value);

	/** Create a JSON value from a number */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON", meta = (DisplayName = "Make JSON Number Value"))
	static UAtlasJsonValue* MakeNumber(float Value);

	/** Create a JSON value from a boolean */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON", meta = (DisplayName = "Make JSON Bool Value"))
	static UAtlasJsonValue* MakeBool(bool Value);

	/** Create a JSON null value */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON", meta = (DisplayName = "Make JSON Null Value"))
	static UAtlasJsonValue* MakeNull();

	/** Create a JSON value from an array of JSON values */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON", meta = (DisplayName = "Make JSON Array Value"))
	static UAtlasJsonValue* MakeArray(const TArray<UAtlasJsonValue*>& Values);

	/** Create a JSON value from a JSON object */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON", meta = (DisplayName = "Make JSON Object Value"))
	static UAtlasJsonValue* MakeObject(UAtlasJsonObject* Object);

	//~ Type checking

	/** Get the type of this JSON value */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	EAtlasJsonValueType GetType() const;

	/** Check if this value is null */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	bool IsNull() const;

	//~ Value getters

	/** Get this value as a string. Returns empty string if not a string type. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	FString AsString() const;

	/** Get this value as a number. Returns 0 if not a number type. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	float AsNumber() const;

	/** Get this value as an integer. Returns 0 if not a number type. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	int32 AsInteger() const;

	/** Get this value as a boolean. Returns false if not a boolean type. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	bool AsBool() const;

	/** Get this value as an array of JSON values. Returns empty array if not an array type. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	TArray<UAtlasJsonValue*> AsArray() const;

	/** Get this value as a JSON object. Returns null if not an object type. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	UAtlasJsonObject* AsObject() const;

	//~ Internal

	/** Set the underlying JSON value (for internal use) */
	void SetJsonValue(TSharedPtr<FJsonValue> InValue);

	/** Get the underlying JSON value (for internal use) */
	TSharedPtr<FJsonValue> GetJsonValue() const { return JsonValue; }

private:
	/** The underlying Unreal JSON value */
	TSharedPtr<FJsonValue> JsonValue;
};
