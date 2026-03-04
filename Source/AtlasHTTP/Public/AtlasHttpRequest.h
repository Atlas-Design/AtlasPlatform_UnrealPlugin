// Copyright Atlas Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "AtlasHttpTypes.h"
#include "AtlasHttpRequest.generated.h"

class UAtlasJsonObject;

// Delegate declarations
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAtlasRequestComplete, UAtlasHttpRequest*, Request, UAtlasJsonObject*, ResponseJson, int32, StatusCode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAtlasRequestFailed, UAtlasHttpRequest*, Request, const FString&, ErrorMessage, int32, StatusCode);

/**
 * HTTP Request object for making web requests from Blueprints.
 * Wraps Unreal's native HTTP module for Blueprint exposure.
 */
UCLASS(BlueprintType, Blueprintable)
class ATLASHTTP_API UAtlasHttpRequest : public UObject
{
	GENERATED_BODY()

public:
	UAtlasHttpRequest();

	//~ Creation

	/** Create a new HTTP request */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Request", meta = (DisplayName = "Create HTTP Request"))
	static UAtlasHttpRequest* CreateRequest();

	//~ Request configuration

	/** Set the URL for this request */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Request")
	void SetURL(const FString& URL);

	/** Get the URL for this request */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|Request")
	FString GetURL() const;

	/** Set the HTTP verb/method (GET, POST, PUT, DELETE, PATCH) */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Request")
	void SetVerb(EAtlasHttpVerb Verb);

	/** Set a custom HTTP verb string */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Request")
	void SetCustomVerb(const FString& Verb);

	/** Set the content type for this request */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Request")
	void SetContentType(EAtlasHttpContentType ContentType);

	/** Set a request header */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Request")
	void SetHeader(const FString& HeaderName, const FString& HeaderValue);

	/** Set the request body as a string */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Request")
	void SetRequestBody(const FString& Body);

	/** Set the request body from a JSON object */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Request")
	void SetRequestBodyJson(UAtlasJsonObject* JsonObject);

	/** Set the content type for binary requests (default: application/octet-stream) */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Request")
	void SetBinaryContentType(const FString& ContentType);

	/** Set the request body as binary data */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Request")
	void SetBinaryRequestContent(const TArray<uint8>& Content);

	/** Set the request timeout in seconds (0 = no timeout) */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Request")
	void SetTimeout(float TimeoutSeconds);

	//~ Request execution

	/** Execute the HTTP request. Bind to OnRequestComplete/OnRequestFailed before calling. */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Request")
	void ProcessRequest();

	/** Cancel an in-progress request */
	UFUNCTION(BlueprintCallable, Category = "AtlasHTTP|Request")
	void CancelRequest();

	/** Get the current status of this request */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|Request")
	EAtlasRequestStatus GetStatus() const { return RequestStatus; }

	//~ Response accessors

	/** Get the HTTP response code (200, 404, etc). Returns 0 if request hasn't completed. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|Request")
	int32 GetResponseCode() const;

	/** Get the raw response content as a string */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|Request")
	FString GetResponseContent() const;

	/** Get the raw response content as binary data */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|Request")
	const TArray<uint8>& GetResponseContentBinary() const;

	/** Get the response content parsed as JSON. Returns null if not valid JSON. */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|Request")
	UAtlasJsonObject* GetResponseJson() const;

	/** Get a response header value by name */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|Request")
	FString GetResponseHeader(const FString& HeaderName) const;

	/** Get all response headers */
	UFUNCTION(BlueprintPure, Category = "AtlasHTTP|Request")
	TMap<FString, FString> GetAllResponseHeaders() const;

	//~ Delegates

	/** Called when the request completes successfully */
	UPROPERTY(BlueprintAssignable, Category = "AtlasHTTP|Request")
	FOnAtlasRequestComplete OnRequestComplete;

	/** Called when the request fails */
	UPROPERTY(BlueprintAssignable, Category = "AtlasHTTP|Request")
	FOnAtlasRequestFailed OnRequestFailed;

private:
	/** Internal callback when HTTP request completes */
	void OnHttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	/** The underlying HTTP request */
	TSharedPtr<IHttpRequest> HttpRequest;

	/** Cached response for accessor functions */
	FHttpResponsePtr CachedResponse;

	/** Current request status */
	EAtlasRequestStatus RequestStatus;

	/** Configured URL */
	FString RequestURL;

	/** Configured verb */
	FString RequestVerb;

	/** Configured timeout */
	float TimeoutSeconds;

	/** Binary request content */
	TArray<uint8> RequestBytes;

	/** Content type for binary requests */
	FString BinaryContentType;

	/** Cached binary response content */
	mutable TArray<uint8> CachedResponseBytes;
};
