// Copyright Atlas Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Dom/JsonObject.h"
#include "AtlasJsonObject.generated.h"

class UAtlasJsonValue;

/**
 * Blueprint-friendly wrapper for JSON objects.
 * Wraps Unreal's native FJsonObject for Blueprint exposure.
 */
UCLASS(BlueprintType, Blueprintable)
class ATLASHTTP_API UAtlasJsonObject : public UObject
{
	GENERATED_BODY()

public:
	UAtlasJsonObject();

	//~ Creation

	/** Create a new empty JSON object */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON", meta = (DisplayName = "Make JSON Object"))
	static UAtlasJsonObject* MakeJsonObject();

	//~ Field checking

	/** Check if this object has a field with the given name */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	bool HasField(const FString& FieldName) const;

	/** Get all field names in this object */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	TArray<FString> GetFieldNames() const;

	/** Remove a field from this object. Returns true if the field existed. */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	bool RemoveField(const FString& FieldName);

	//~ String fields

	/** Get a string field value. Returns empty string if field doesn't exist or isn't a string. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	FString GetStringField(const FString& FieldName) const;

	/** Set a string field value */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	void SetStringField(const FString& FieldName, const FString& Value);

	//~ Number fields

	/** Get a number field value. Returns 0 if field doesn't exist or isn't a number. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	float GetNumberField(const FString& FieldName) const;

	/** Set a number field value */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	void SetNumberField(const FString& FieldName, float Value);

	//~ Integer fields

	/** Get an integer field value. Returns 0 if field doesn't exist or isn't a number. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	int32 GetIntegerField(const FString& FieldName) const;

	/** Set an integer field value */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	void SetIntegerField(const FString& FieldName, int32 Value);

	//~ Boolean fields

	/** Get a boolean field value. Returns false if field doesn't exist or isn't a boolean. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	bool GetBoolField(const FString& FieldName) const;

	/** Set a boolean field value */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	void SetBoolField(const FString& FieldName, bool Value);

	//~ Object fields

	/** Get a nested JSON object field. Returns null if field doesn't exist or isn't an object. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	UAtlasJsonObject* GetObjectField(const FString& FieldName) const;

	/** Set a nested JSON object field */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	void SetObjectField(const FString& FieldName, UAtlasJsonObject* Value);

	//~ Array fields

	/** Get an array field as array of JSON values. Returns empty array if field doesn't exist or isn't an array. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	TArray<UAtlasJsonValue*> GetArrayField(const FString& FieldName) const;

	/** Set an array field from array of JSON values */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	void SetArrayField(const FString& FieldName, const TArray<UAtlasJsonValue*>& Values);

	/** Get an array of strings from a field. Returns empty array if field doesn't exist or isn't a string array. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	TArray<FString> GetStringArrayField(const FString& FieldName) const;

	/** Set an array of strings field */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	void SetStringArrayField(const FString& FieldName, const TArray<FString>& Values);

	/** Get an array of JSON objects from a field. Returns empty array if field doesn't exist or isn't an object array. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	TArray<UAtlasJsonObject*> GetObjectArrayField(const FString& FieldName) const;

	/** Set an array of JSON objects field */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	void SetObjectArrayField(const FString& FieldName, const TArray<UAtlasJsonObject*>& Values);

	//~ Generic value field

	/** Get a field as a JSON value. Returns null if field doesn't exist. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	UAtlasJsonValue* GetField(const FString& FieldName) const;

	/** Set a field from a JSON value */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	void SetField(const FString& FieldName, UAtlasJsonValue* Value);

	//~ Serialization

	/** Encode this object to a JSON string */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	FString EncodeJson() const;

	/** Encode this object to a prettified JSON string */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|JSON")
	FString EncodeJsonPretty() const;

	/** Decode a JSON string into this object. Returns true if successful. */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON")
	bool DecodeJson(const FString& JsonString);

	/** Create a JSON object from a JSON string. Returns null if parsing fails. */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|JSON", meta = (DisplayName = "Parse JSON String"))
	static UAtlasJsonObject* FromJsonString(const FString& JsonString);

	//~ Internal

	/** Set the underlying JSON object (for internal use) */
	void SetJsonObject(TSharedPtr<FJsonObject> InObject);

	/** Get the underlying JSON object (for internal use) */
	TSharedPtr<FJsonObject> GetJsonObject() const { return JsonObject; }

private:
	/** The underlying Unreal JSON object */
	TSharedPtr<FJsonObject> JsonObject;
};
