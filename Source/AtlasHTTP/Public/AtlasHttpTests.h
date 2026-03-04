// Copyright Atlas Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AtlasHttpTests.generated.h"

/**
 * Test utility for AtlasHTTP plugin.
 * Provides functions to verify all plugin features are working correctly.
 */
UCLASS()
class ATLASHTTP_API UAtlasHttpTests : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Run all AtlasHTTP tests and log results.
	 * Tests JSON, encoding, hashing, and prints a summary.
	 * Note: HTTP tests require network and are run separately.
	 * @return True if all tests passed
	 */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Tests")
	static bool RunAllTests();

	/**
	 * Run JSON object tests.
	 * @return True if all JSON tests passed
	 */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Tests")
	static bool TestJsonObject();

	/**
	 * Run JSON value tests.
	 * @return True if all JSON value tests passed
	 */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Tests")
	static bool TestJsonValue();

	/**
	 * Run Base64 encoding tests.
	 * @return True if all Base64 tests passed
	 */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Tests")
	static bool TestBase64();

	/**
	 * Run URL encoding tests.
	 * @return True if all URL encoding tests passed
	 */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Tests")
	static bool TestUrlEncoding();

	/**
	 * Run hashing tests (MD5, SHA1).
	 * @return True if all hashing tests passed
	 */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Tests")
	static bool TestHashing();

	/**
	 * Run HTTP GET test (requires network).
	 * This is an async test - check log for results.
	 * @param WorldContextObject World context for async action
	 */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Tests", meta = (WorldContext = "WorldContextObject"))
	static void TestHttpGet(UObject* WorldContextObject);

	/**
	 * Run HTTP POST test (requires network).
	 * This is an async test - check log for results.
	 * @param WorldContextObject World context for async action
	 */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Tests", meta = (WorldContext = "WorldContextObject"))
	static void TestHttpPost(UObject* WorldContextObject);

private:
	/** Log a test result */
	static void LogTestResult(const FString& TestName, bool bPassed, const FString& Details = TEXT(""));
	
	/** Test counter for summary */
	static int32 TestsPassed;
	static int32 TestsFailed;
};
