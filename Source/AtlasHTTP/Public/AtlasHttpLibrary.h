// Copyright Atlas Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AtlasHttpLibrary.generated.h"

/**
 * Static utility functions for HTTP and encoding operations.
 */
UCLASS()
class ATLASHTTP_API UAtlasHttpLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//~ URL Encoding

	/** 
	 * Percent-encode a string for use in URLs.
	 * Encodes special characters like spaces, &, =, etc.
	 */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|Encoding")
	static FString PercentEncode(const FString& Source);

	/** 
	 * Decode a percent-encoded string.
	 */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|Encoding")
	static FString PercentDecode(const FString& Source);

	//~ Base64 String Encoding

	/** 
	 * Encode a string to Base64.
	 */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|Encoding")
	static FString Base64Encode(const FString& Source);

	/** 
	 * Decode a Base64 string.
	 * Returns empty string if decoding fails.
	 */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|Encoding")
	static FString Base64Decode(const FString& Source);

	//~ Base64 Binary Encoding

	/** 
	 * Encode binary data to Base64 string.
	 */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|Encoding")
	static FString Base64EncodeData(const TArray<uint8>& Data);

	/** 
	 * Decode a Base64 string to binary data.
	 * Returns empty array if decoding fails.
	 */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|Encoding")
	static TArray<uint8> Base64DecodeData(const FString& Source);

	//~ Hashing

	/** 
	 * Generate MD5 hash of a string.
	 * Returns lowercase hex string.
	 */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|Hashing")
	static FString StringToMd5(const FString& Source);

	/** 
	 * Generate SHA1 hash of a string.
	 * Returns lowercase hex string.
	 */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|Hashing")
	static FString StringToSha1(const FString& Source);

	//~ URL Helpers

	/**
	 * Build a URL with query parameters.
	 * @param BaseURL The base URL (e.g., "https://api.example.com/endpoint")
	 * @param QueryParams Map of query parameter key-value pairs
	 * @return Full URL with encoded query string
	 */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|URL")
	static FString BuildURLWithParams(const FString& BaseURL, const TMap<FString, FString>& QueryParams);
};
