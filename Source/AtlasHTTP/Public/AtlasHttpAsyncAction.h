// Copyright Atlas Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "AtlasHttpTypes.h"
#include "AtlasHttpAsyncAction.generated.h"

class UAtlasJsonObject;

// Delegate for successful response
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAtlasHttpSuccess, UAtlasJsonObject*, Response, int32, StatusCode);

// Delegate for failed response
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAtlasHttpFailure, const FString&, ErrorMessage, int32, StatusCode);

/**
 * Async Blueprint action for HTTP requests.
 * Provides a clean async node with On Success and On Failure exec output pins.
 */
UCLASS()
class ATLASHTTP_API UAtlasHttpAsyncAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Make an HTTP request with automatic JSON parsing.
	 * 
	 * @param URL The URL to request
	 * @param Verb The HTTP method (GET, POST, PUT, DELETE, PATCH)
	 * @param Body Optional JSON body for POST/PUT/PATCH requests
	 * @param ContentType The content type for the request body (defaults to JSON)
	 * @param Headers Optional additional headers as key-value pairs
	 */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Atlas HTTP Request", WorldContext = "WorldContextObject", AutoCreateRefTerm = "Headers"))
	static UAtlasHttpAsyncAction* AsyncHttpRequest(
		UObject* WorldContextObject,
		const FString& URL,
		EAtlasHttpVerb Verb,
		UAtlasJsonObject* Body,
		EAtlasHttpContentType ContentType,
		const TMap<FString, FString>& Headers
	);

	/**
	 * Simple GET request helper.
	 * 
	 * @param URL The URL to request
	 * @param Headers Optional additional headers
	 */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Atlas HTTP GET", WorldContext = "WorldContextObject", AutoCreateRefTerm = "Headers"))
	static UAtlasHttpAsyncAction* AsyncHttpGet(
		UObject* WorldContextObject,
		const FString& URL,
		const TMap<FString, FString>& Headers
	);

	/**
	 * Simple POST request helper.
	 * 
	 * @param URL The URL to request
	 * @param Body JSON body to send
	 * @param Headers Optional additional headers
	 */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Atlas HTTP POST", WorldContext = "WorldContextObject", AutoCreateRefTerm = "Headers"))
	static UAtlasHttpAsyncAction* AsyncHttpPost(
		UObject* WorldContextObject,
		const FString& URL,
		UAtlasJsonObject* Body,
		const TMap<FString, FString>& Headers
	);

	/** Called when the request succeeds (2xx status code) */
	UPROPERTY(BlueprintAssignable)
	FOnAtlasHttpSuccess OnSuccess;

	/** Called when the request fails (non-2xx status code or connection error) */
	UPROPERTY(BlueprintAssignable)
	FOnAtlasHttpFailure OnFailure;

	// UBlueprintAsyncActionBase interface
	virtual void Activate() override;

	/** Cancel the request if it's still in progress */
	void Cancel();

private:
	/** Internal callback when HTTP request completes */
	void OnRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	/** Execute the configured request */
	void ExecuteRequest();

	/** The underlying HTTP request */
	TSharedPtr<IHttpRequest> HttpRequest;

	/** Request configuration */
	FString RequestURL;
	FString RequestVerb;
	FString RequestBody;
	EAtlasHttpContentType RequestContentType;
	TMap<FString, FString> RequestHeaders;

	/** World context for preventing GC */
	UPROPERTY()
	UObject* WorldContext;
};
