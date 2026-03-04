// Copyright Atlas Games. All Rights Reserved.

#include "AtlasHttpSubsystem.h"
#include "AtlasHttpRequest.h"
#include "AtlasJsonObject.h"
#include "AtlasJsonValue.h"
#include "AtlasHTTP.h"
#include "Engine/GameInstance.h"

UAtlasHttpSubsystem* UAtlasHttpSubsystem::Get(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<UAtlasHttpSubsystem>();
}

UAtlasHttpRequest* UAtlasHttpSubsystem::ConstructHttpRequest()
{
	return UAtlasHttpRequest::CreateRequest();
}

UAtlasJsonObject* UAtlasHttpSubsystem::ConstructJsonObject()
{
	return UAtlasJsonObject::MakeJsonObject();
}

UAtlasJsonObject* UAtlasHttpSubsystem::ConstructJsonObjectFromString(const FString& JsonString)
{
	return UAtlasJsonObject::FromJsonString(JsonString);
}

UAtlasJsonValue* UAtlasHttpSubsystem::ConstructJsonValueString(const FString& Value)
{
	return UAtlasJsonValue::MakeString(Value);
}

UAtlasJsonValue* UAtlasHttpSubsystem::ConstructJsonValueNumber(float Value)
{
	return UAtlasJsonValue::MakeNumber(Value);
}

UAtlasJsonValue* UAtlasHttpSubsystem::ConstructJsonValueBool(bool Value)
{
	return UAtlasJsonValue::MakeBool(Value);
}

UAtlasJsonValue* UAtlasHttpSubsystem::ConstructJsonValueNull()
{
	return UAtlasJsonValue::MakeNull();
}

UAtlasJsonValue* UAtlasHttpSubsystem::ConstructJsonValueArray(const TArray<UAtlasJsonValue*>& Values)
{
	return UAtlasJsonValue::MakeArray(Values);
}

UAtlasJsonValue* UAtlasHttpSubsystem::ConstructJsonValueObject(UAtlasJsonObject* Object)
{
	return UAtlasJsonValue::MakeObject(Object);
}
