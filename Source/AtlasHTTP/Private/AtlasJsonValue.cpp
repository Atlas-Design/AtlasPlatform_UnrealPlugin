// Copyright Atlas Games. All Rights Reserved.

#include "AtlasJsonValue.h"
#include "AtlasJsonObject.h"
#include "AtlasHTTP.h"

UAtlasJsonValue::UAtlasJsonValue()
{
	JsonValue = MakeShared<FJsonValueNull>();
}

UAtlasJsonValue* UAtlasJsonValue::MakeString(const FString& Value)
{
	UAtlasJsonValue* NewValue = NewObject<UAtlasJsonValue>();
	NewValue->JsonValue = MakeShared<FJsonValueString>(Value);
	return NewValue;
}

UAtlasJsonValue* UAtlasJsonValue::MakeNumber(float Value)
{
	UAtlasJsonValue* NewValue = NewObject<UAtlasJsonValue>();
	NewValue->JsonValue = MakeShared<FJsonValueNumber>(Value);
	return NewValue;
}

UAtlasJsonValue* UAtlasJsonValue::MakeBool(bool Value)
{
	UAtlasJsonValue* NewValue = NewObject<UAtlasJsonValue>();
	NewValue->JsonValue = MakeShared<FJsonValueBoolean>(Value);
	return NewValue;
}

UAtlasJsonValue* UAtlasJsonValue::MakeNull()
{
	UAtlasJsonValue* NewValue = NewObject<UAtlasJsonValue>();
	NewValue->JsonValue = MakeShared<FJsonValueNull>();
	return NewValue;
}

UAtlasJsonValue* UAtlasJsonValue::MakeArray(const TArray<UAtlasJsonValue*>& Values)
{
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (UAtlasJsonValue* Val : Values)
	{
		if (Val && Val->JsonValue.IsValid())
		{
			JsonArray.Add(Val->JsonValue);
		}
		else
		{
			JsonArray.Add(MakeShared<FJsonValueNull>());
		}
	}

	UAtlasJsonValue* NewValue = NewObject<UAtlasJsonValue>();
	NewValue->JsonValue = MakeShared<FJsonValueArray>(JsonArray);
	return NewValue;
}

UAtlasJsonValue* UAtlasJsonValue::MakeObject(UAtlasJsonObject* Object)
{
	UAtlasJsonValue* NewValue = NewObject<UAtlasJsonValue>();
	
	if (Object && Object->GetJsonObject().IsValid())
	{
		NewValue->JsonValue = MakeShared<FJsonValueObject>(Object->GetJsonObject());
	}
	else
	{
		NewValue->JsonValue = MakeShared<FJsonValueNull>();
	}
	
	return NewValue;
}

EAtlasJsonValueType UAtlasJsonValue::GetType() const
{
	if (!JsonValue.IsValid())
	{
		return EAtlasJsonValueType::None;
	}

	switch (JsonValue->Type)
	{
		case EJson::None:
			return EAtlasJsonValueType::None;
		case EJson::Null:
			return EAtlasJsonValueType::Null;
		case EJson::String:
			return EAtlasJsonValueType::String;
		case EJson::Number:
			return EAtlasJsonValueType::Number;
		case EJson::Boolean:
			return EAtlasJsonValueType::Boolean;
		case EJson::Array:
			return EAtlasJsonValueType::Array;
		case EJson::Object:
			return EAtlasJsonValueType::Object;
		default:
			return EAtlasJsonValueType::None;
	}
}

bool UAtlasJsonValue::IsNull() const
{
	return !JsonValue.IsValid() || JsonValue->IsNull();
}

FString UAtlasJsonValue::AsString() const
{
	if (!JsonValue.IsValid())
	{
		return FString();
	}

	FString OutString;
	if (JsonValue->TryGetString(OutString))
	{
		return OutString;
	}

	// Try to convert other types to string
	if (JsonValue->Type == EJson::Number)
	{
		return FString::SanitizeFloat(JsonValue->AsNumber());
	}
	if (JsonValue->Type == EJson::Boolean)
	{
		return JsonValue->AsBool() ? TEXT("true") : TEXT("false");
	}

	return FString();
}

float UAtlasJsonValue::AsNumber() const
{
	if (!JsonValue.IsValid())
	{
		return 0.0f;
	}

	double OutNumber;
	if (JsonValue->TryGetNumber(OutNumber))
	{
		return static_cast<float>(OutNumber);
	}

	return 0.0f;
}

int32 UAtlasJsonValue::AsInteger() const
{
	return FMath::RoundToInt(AsNumber());
}

bool UAtlasJsonValue::AsBool() const
{
	if (!JsonValue.IsValid())
	{
		return false;
	}

	bool OutBool;
	if (JsonValue->TryGetBool(OutBool))
	{
		return OutBool;
	}

	return false;
}

TArray<UAtlasJsonValue*> UAtlasJsonValue::AsArray() const
{
	TArray<UAtlasJsonValue*> OutArray;

	if (!JsonValue.IsValid())
	{
		return OutArray;
	}

	const TArray<TSharedPtr<FJsonValue>>* JsonArray;
	if (JsonValue->TryGetArray(JsonArray))
	{
		for (const TSharedPtr<FJsonValue>& Val : *JsonArray)
		{
			UAtlasJsonValue* NewValue = NewObject<UAtlasJsonValue>();
			NewValue->SetJsonValue(Val);
			OutArray.Add(NewValue);
		}
	}

	return OutArray;
}

UAtlasJsonObject* UAtlasJsonValue::AsObject() const
{
	if (!JsonValue.IsValid())
	{
		return nullptr;
	}

	const TSharedPtr<FJsonObject>* JsonObj;
	if (JsonValue->TryGetObject(JsonObj) && JsonObj->IsValid())
	{
		UAtlasJsonObject* Result = NewObject<UAtlasJsonObject>();
		Result->SetJsonObject(*JsonObj);
		return Result;
	}

	return nullptr;
}

void UAtlasJsonValue::SetJsonValue(TSharedPtr<FJsonValue> InValue)
{
	JsonValue = InValue;
}
