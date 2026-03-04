// Copyright Atlas Games. All Rights Reserved.

#include "AtlasHttpAsyncAction.h"
#include "AtlasJsonObject.h"
#include "AtlasHTTP.h"
#include "HttpModule.h"

UAtlasHttpAsyncAction* UAtlasHttpAsyncAction::AsyncHttpRequest(
	UObject* WorldContextObject,
	const FString& URL,
	EAtlasHttpVerb Verb,
	UAtlasJsonObject* Body,
	EAtlasHttpContentType ContentType,
	const TMap<FString, FString>& Headers)
{
	UAtlasHttpAsyncAction* Action = NewObject<UAtlasHttpAsyncAction>();
	Action->WorldContext = WorldContextObject;
	Action->RequestURL = URL;
	Action->RequestVerb = AtlasHttpHelpers::VerbToString(Verb);
	Action->RequestContentType = ContentType;
	Action->RequestHeaders = Headers;
	
	if (Body)
	{
		Action->RequestBody = Body->EncodeJson();
	}
	
	Action->RegisterWithGameInstance(WorldContextObject);
	
	return Action;
}

UAtlasHttpAsyncAction* UAtlasHttpAsyncAction::AsyncHttpGet(
	UObject* WorldContextObject,
	const FString& URL,
	const TMap<FString, FString>& Headers)
{
	return AsyncHttpRequest(WorldContextObject, URL, EAtlasHttpVerb::GET, nullptr, EAtlasHttpContentType::JSON, Headers);
}

UAtlasHttpAsyncAction* UAtlasHttpAsyncAction::AsyncHttpPost(
	UObject* WorldContextObject,
	const FString& URL,
	UAtlasJsonObject* Body,
	const TMap<FString, FString>& Headers)
{
	return AsyncHttpRequest(WorldContextObject, URL, EAtlasHttpVerb::POST, Body, EAtlasHttpContentType::JSON, Headers);
}

void UAtlasHttpAsyncAction::Activate()
{
	ExecuteRequest();
}

void UAtlasHttpAsyncAction::Cancel()
{
	if (HttpRequest.IsValid())
	{
		HttpRequest->CancelRequest();
		UE_LOG(LogAtlasHTTP, Log, TEXT("Async HTTP request cancelled"));
	}
}

void UAtlasHttpAsyncAction::ExecuteRequest()
{
	if (RequestURL.IsEmpty())
	{
		UE_LOG(LogAtlasHTTP, Error, TEXT("AsyncHttpRequest: URL is empty"));
		OnFailure.Broadcast(TEXT("URL is empty"), 0);
		SetReadyToDestroy();
		return;
	}

	// Create the HTTP request
	HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(RequestURL);
	HttpRequest->SetVerb(RequestVerb);

	// Set content type and body if we have a body
	if (!RequestBody.IsEmpty())
	{
		HttpRequest->SetHeader(TEXT("Content-Type"), AtlasHttpHelpers::ContentTypeToString(RequestContentType));
		HttpRequest->SetContentAsString(RequestBody);
	}

	// Apply custom headers (can override content-type if needed)
	for (const auto& Header : RequestHeaders)
	{
		HttpRequest->SetHeader(Header.Key, Header.Value);
	}

	// Bind completion delegate
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UAtlasHttpAsyncAction::OnRequestComplete);

	UE_LOG(LogAtlasHTTP, Log, TEXT("Async %s request to: %s"), *RequestVerb, *RequestURL);

	// Send the request
	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogAtlasHTTP, Error, TEXT("Failed to start async HTTP request"));
		OnFailure.Broadcast(TEXT("Failed to start HTTP request"), 0);
		SetReadyToDestroy();
	}
}

void UAtlasHttpAsyncAction::OnRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bWasSuccessful && Response.IsValid())
	{
		int32 StatusCode = Response->GetResponseCode();
		FString Content = Response->GetContentAsString();

		// Consider 2xx status codes as success
		if (StatusCode >= 200 && StatusCode < 300)
		{
			UE_LOG(LogAtlasHTTP, Log, TEXT("Async request succeeded with code %d"), StatusCode);

			// Try to parse as JSON
			UAtlasJsonObject* ResponseJson = nullptr;
			if (!Content.IsEmpty())
			{
				ResponseJson = UAtlasJsonObject::FromJsonString(Content);
				if (!ResponseJson)
				{
					// Content wasn't valid JSON, create empty object
					// Could optionally store raw content somewhere
					UE_LOG(LogAtlasHTTP, Warning, TEXT("Response was not valid JSON, returning empty object"));
					ResponseJson = UAtlasJsonObject::MakeJsonObject();
				}
			}
			else
			{
				// Empty response, return empty JSON object
				ResponseJson = UAtlasJsonObject::MakeJsonObject();
			}

			OnSuccess.Broadcast(ResponseJson, StatusCode);
		}
		else
		{
			FString ErrorMsg = FString::Printf(TEXT("HTTP Error: %d"), StatusCode);
			UE_LOG(LogAtlasHTTP, Warning, TEXT("Async request failed: %s"), *ErrorMsg);
			
			OnFailure.Broadcast(ErrorMsg, StatusCode);
		}
	}
	else
	{
		FString ErrorMsg = TEXT("Request failed - no response or connection error");
		int32 StatusCode = 0;

		if (Response.IsValid())
		{
			StatusCode = Response->GetResponseCode();
			ErrorMsg = FString::Printf(TEXT("Request failed with code %d"), StatusCode);
		}

		UE_LOG(LogAtlasHTTP, Error, TEXT("Async request failed: %s"), *ErrorMsg);
		OnFailure.Broadcast(ErrorMsg, StatusCode);
	}

	// Mark for cleanup
	SetReadyToDestroy();
}
