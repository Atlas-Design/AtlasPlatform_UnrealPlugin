// Copyright Atlas Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AtlasHttpTypes.generated.h"

/**
 * HTTP request method/verb
 */
UENUM(BlueprintType)
enum class EAtlasHttpVerb : uint8
{
	GET		UMETA(DisplayName = "GET"),
	POST	UMETA(DisplayName = "POST"),
	PUT		UMETA(DisplayName = "PUT"),
	DELETE	UMETA(DisplayName = "DELETE"),
	PATCH	UMETA(DisplayName = "PATCH"),
	CUSTOM	UMETA(DisplayName = "Custom")
};

/**
 * Content type for HTTP request body
 */
UENUM(BlueprintType)
enum class EAtlasHttpContentType : uint8
{
	/** application/json */
	JSON				UMETA(DisplayName = "JSON"),
	
	/** application/x-www-form-urlencoded */
	FormUrlEncoded		UMETA(DisplayName = "Form URL Encoded"),
	
	/** text/plain */
	Text				UMETA(DisplayName = "Text"),
	
	/** application/octet-stream */
	Binary				UMETA(DisplayName = "Binary")
};

/**
 * Current status of an HTTP request
 */
UENUM(BlueprintType)
enum class EAtlasRequestStatus : uint8
{
	/** Request has not been started */
	NotStarted		UMETA(DisplayName = "Not Started"),
	
	/** Request is currently in progress */
	Processing		UMETA(DisplayName = "Processing"),
	
	/** Request completed successfully */
	Succeeded		UMETA(DisplayName = "Succeeded"),
	
	/** Request failed */
	Failed			UMETA(DisplayName = "Failed")
};

/**
 * Helper functions for HTTP types
 */
namespace AtlasHttpHelpers
{
	/** Convert EAtlasHttpVerb to FString */
	inline FString VerbToString(EAtlasHttpVerb Verb)
	{
		switch (Verb)
		{
			case EAtlasHttpVerb::GET:		return TEXT("GET");
			case EAtlasHttpVerb::POST:		return TEXT("POST");
			case EAtlasHttpVerb::PUT:		return TEXT("PUT");
			case EAtlasHttpVerb::DELETE:	return TEXT("DELETE");
			case EAtlasHttpVerb::PATCH:		return TEXT("PATCH");
			default:						return TEXT("GET");
		}
	}

	/** Convert EAtlasHttpContentType to content type string */
	inline FString ContentTypeToString(EAtlasHttpContentType ContentType)
	{
		switch (ContentType)
		{
			case EAtlasHttpContentType::JSON:			return TEXT("application/json");
			case EAtlasHttpContentType::FormUrlEncoded:	return TEXT("application/x-www-form-urlencoded");
			case EAtlasHttpContentType::Text:			return TEXT("text/plain");
			case EAtlasHttpContentType::Binary:			return TEXT("application/octet-stream");
			default:									return TEXT("application/json");
		}
	}
}
