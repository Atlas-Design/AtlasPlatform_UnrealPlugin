// Copyright Atlas Games. All Rights Reserved.

#include "AtlasJsonObject.h"
#include "AtlasJsonValue.h"
#include "AtlasHTTP.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

UAtlasJsonObject::UAtlasJsonObject()
{
	JsonObject = MakeShared<FJsonObject>();
}

UAtlasJsonObject* UAtlasJsonObject::MakeJsonObject()
{
	return NewObject<UAtlasJsonObject>();
}

bool UAtlasJsonObject::HasField(const FString& FieldName) const
{
	if (!JsonObject.IsValid())
	{
		return false;
	}
	return JsonObject->HasField(FieldName);
}

TArray<FString> UAtlasJsonObject::GetFieldNames() const
{
	TArray<FString> FieldNames;
	if (JsonObject.IsValid())
	{
		JsonObject->Values.GetKeys(FieldNames);
	}
	return FieldNames;
}

bool UAtlasJsonObject::RemoveField(const FString& FieldName)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}
	
	if (JsonObject->HasField(FieldName))
	{
		JsonObject->RemoveField(FieldName);
		return true;
	}
	return false;
}

// String fields

FString UAtlasJsonObject::GetStringField(const FString& FieldName) const
{
	if (!JsonObject.IsValid() || !JsonObject->HasField(FieldName))
	{
		return FString();
	}
	return JsonObject->GetStringField(FieldName);
}

void UAtlasJsonObject::SetStringField(const FString& FieldName, const FString& Value)
{
	if (JsonObject.IsValid())
	{
		JsonObject->SetStringField(FieldName, Value);
	}
}

// Number fields

float UAtlasJsonObject::GetNumberField(const FString& FieldName) const
{
	if (!JsonObject.IsValid() || !JsonObject->HasField(FieldName))
	{
		return 0.0f;
	}
	return static_cast<float>(JsonObject->GetNumberField(FieldName));
}

void UAtlasJsonObject::SetNumberField(const FString& FieldName, float Value)
{
	if (JsonObject.IsValid())
	{
		JsonObject->SetNumberField(FieldName, static_cast<double>(Value));
	}
}

// Integer fields

int32 UAtlasJsonObject::GetIntegerField(const FString& FieldName) const
{
	if (!JsonObject.IsValid() || !JsonObject->HasField(FieldName))
	{
		return 0;
	}
	return static_cast<int32>(JsonObject->GetIntegerField(FieldName));
}

void UAtlasJsonObject::SetIntegerField(const FString& FieldName, int32 Value)
{
	if (JsonObject.IsValid())
	{
		JsonObject->SetNumberField(FieldName, static_cast<double>(Value));
	}
}

// Boolean fields

bool UAtlasJsonObject::GetBoolField(const FString& FieldName) const
{
	if (!JsonObject.IsValid() || !JsonObject->HasField(FieldName))
	{
		return false;
	}
	return JsonObject->GetBoolField(FieldName);
}

void UAtlasJsonObject::SetBoolField(const FString& FieldName, bool Value)
{
	if (JsonObject.IsValid())
	{
		JsonObject->SetBoolField(FieldName, Value);
	}
}

// Object fields

UAtlasJsonObject* UAtlasJsonObject::GetObjectField(const FString& FieldName) const
{
	if (!JsonObject.IsValid() || !JsonObject->HasTypedField<EJson::Object>(FieldName))
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> NestedObject = JsonObject->GetObjectField(FieldName);
	if (!NestedObject.IsValid())
	{
		return nullptr;
	}

	UAtlasJsonObject* Result = NewObject<UAtlasJsonObject>();
	Result->SetJsonObject(NestedObject);
	return Result;
}

void UAtlasJsonObject::SetObjectField(const FString& FieldName, UAtlasJsonObject* Value)
{
	if (!JsonObject.IsValid())
	{
		return;
	}

	if (Value && Value->JsonObject.IsValid())
	{
		JsonObject->SetObjectField(FieldName, Value->JsonObject);
	}
	else
	{
		JsonObject->SetField(FieldName, MakeShared<FJsonValueNull>());
	}
}

// Array fields

TArray<UAtlasJsonValue*> UAtlasJsonObject::GetArrayField(const FString& FieldName) const
{
	TArray<UAtlasJsonValue*> OutArray;

	if (!JsonObject.IsValid() || !JsonObject->HasTypedField<EJson::Array>(FieldName))
	{
		return OutArray;
	}

	const TArray<TSharedPtr<FJsonValue>>& JsonArray = JsonObject->GetArrayField(FieldName);
	for (const TSharedPtr<FJsonValue>& Val : JsonArray)
	{
		UAtlasJsonValue* NewValue = NewObject<UAtlasJsonValue>();
		NewValue->SetJsonValue(Val);
		OutArray.Add(NewValue);
	}

	return OutArray;
}

void UAtlasJsonObject::SetArrayField(const FString& FieldName, const TArray<UAtlasJsonValue*>& Values)
{
	if (!JsonObject.IsValid())
	{
		return;
	}

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (UAtlasJsonValue* Val : Values)
	{
		if (Val && Val->GetJsonValue().IsValid())
		{
			JsonArray.Add(Val->GetJsonValue());
		}
		else
		{
			JsonArray.Add(MakeShared<FJsonValueNull>());
		}
	}

	JsonObject->SetArrayField(FieldName, JsonArray);
}

TArray<FString> UAtlasJsonObject::GetStringArrayField(const FString& FieldName) const
{
	TArray<FString> OutArray;

	if (!JsonObject.IsValid() || !JsonObject->HasTypedField<EJson::Array>(FieldName))
	{
		return OutArray;
	}

	const TArray<TSharedPtr<FJsonValue>>& JsonArray = JsonObject->GetArrayField(FieldName);
	for (const TSharedPtr<FJsonValue>& Val : JsonArray)
	{
		FString StringVal;
		if (Val.IsValid() && Val->TryGetString(StringVal))
		{
			OutArray.Add(StringVal);
		}
	}

	return OutArray;
}

void UAtlasJsonObject::SetStringArrayField(const FString& FieldName, const TArray<FString>& Values)
{
	if (!JsonObject.IsValid())
	{
		return;
	}

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (const FString& Val : Values)
	{
		JsonArray.Add(MakeShared<FJsonValueString>(Val));
	}

	JsonObject->SetArrayField(FieldName, JsonArray);
}

TArray<UAtlasJsonObject*> UAtlasJsonObject::GetObjectArrayField(const FString& FieldName) const
{
	TArray<UAtlasJsonObject*> OutArray;

	if (!JsonObject.IsValid() || !JsonObject->HasTypedField<EJson::Array>(FieldName))
	{
		return OutArray;
	}

	const TArray<TSharedPtr<FJsonValue>>& JsonArray = JsonObject->GetArrayField(FieldName);
	for (const TSharedPtr<FJsonValue>& Val : JsonArray)
	{
		if (Val.IsValid() && Val->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
			if (Val->TryGetObject(ObjPtr) && ObjPtr && ObjPtr->IsValid())
			{
				UAtlasJsonObject* NewObj = NewObject<UAtlasJsonObject>();
				NewObj->SetJsonObject(*ObjPtr);
				OutArray.Add(NewObj);
			}
		}
	}

	return OutArray;
}

void UAtlasJsonObject::SetObjectArrayField(const FString& FieldName, const TArray<UAtlasJsonObject*>& Values)
{
	if (!JsonObject.IsValid())
	{
		return;
	}

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (UAtlasJsonObject* Obj : Values)
	{
		if (Obj && Obj->GetJsonObject().IsValid())
		{
			JsonArray.Add(MakeShared<FJsonValueObject>(Obj->GetJsonObject()));
		}
		else
		{
			JsonArray.Add(MakeShared<FJsonValueNull>());
		}
	}

	JsonObject->SetArrayField(FieldName, JsonArray);
}

// Generic value field

UAtlasJsonValue* UAtlasJsonObject::GetField(const FString& FieldName) const
{
	if (!JsonObject.IsValid() || !JsonObject->HasField(FieldName))
	{
		return nullptr;
	}

	TSharedPtr<FJsonValue> FieldValue = JsonObject->TryGetField(FieldName);
	if (!FieldValue.IsValid())
	{
		return nullptr;
	}

	UAtlasJsonValue* NewValue = NewObject<UAtlasJsonValue>();
	NewValue->SetJsonValue(FieldValue);
	return NewValue;
}

void UAtlasJsonObject::SetField(const FString& FieldName, UAtlasJsonValue* Value)
{
	if (!JsonObject.IsValid())
	{
		return;
	}

	if (Value && Value->GetJsonValue().IsValid())
	{
		JsonObject->SetField(FieldName, Value->GetJsonValue());
	}
	else
	{
		JsonObject->SetField(FieldName, MakeShared<FJsonValueNull>());
	}
}

// Serialization

FString UAtlasJsonObject::EncodeJson() const
{
	if (!JsonObject.IsValid())
	{
		return TEXT("{}");
	}

	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = 
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
	
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	return OutputString;
}

FString UAtlasJsonObject::EncodeJsonPretty() const
{
	if (!JsonObject.IsValid())
	{
		return TEXT("{}");
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	return OutputString;
}

bool UAtlasJsonObject::DecodeJson(const FString& JsonString)
{
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	TSharedPtr<FJsonObject> ParsedObject;

	if (FJsonSerializer::Deserialize(Reader, ParsedObject) && ParsedObject.IsValid())
	{
		JsonObject = ParsedObject;
		return true;
	}

	UE_LOG(LogAtlasHTTP, Warning, TEXT("Failed to parse JSON string: %s"), *JsonString);
	return false;
}

UAtlasJsonObject* UAtlasJsonObject::FromJsonString(const FString& JsonString)
{
	UAtlasJsonObject* Result = NewObject<UAtlasJsonObject>();
	if (Result->DecodeJson(JsonString))
	{
		return Result;
	}
	return nullptr;
}

void UAtlasJsonObject::SetJsonObject(TSharedPtr<FJsonObject> InObject)
{
	JsonObject = InObject;
}
