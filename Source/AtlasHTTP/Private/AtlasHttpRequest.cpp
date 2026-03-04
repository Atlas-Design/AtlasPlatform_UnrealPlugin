// Copyright Atlas Games. All Rights Reserved.

#include "AtlasHttpRequest.h"
#include "AtlasJsonObject.h"
#include "AtlasHTTP.h"
#include "HttpModule.h"

UAtlasHttpRequest::UAtlasHttpRequest()
	: RequestStatus(EAtlasRequestStatus::NotStarted)
	, RequestVerb(TEXT("GET"))
	, TimeoutSeconds(0.0f)
	, BinaryContentType(TEXT("application/octet-stream"))
{
}

UAtlasHttpRequest* UAtlasHttpRequest::CreateRequest()
{
	UAtlasHttpRequest* Request = NewObject<UAtlasHttpRequest>();
	return Request;
}

void UAtlasHttpRequest::SetURL(const FString& URL)
{
	RequestURL = URL;
}

FString UAtlasHttpRequest::GetURL() const
{
	return RequestURL;
}

void UAtlasHttpRequest::SetVerb(EAtlasHttpVerb Verb)
{
	RequestVerb = AtlasHttpHelpers::VerbToString(Verb);
}

void UAtlasHttpRequest::SetCustomVerb(const FString& Verb)
{
	RequestVerb = Verb;
}

void UAtlasHttpRequest::SetContentType(EAtlasHttpContentType ContentType)
{
	if (!HttpRequest.IsValid())
	{
		HttpRequest = FHttpModule::Get().CreateRequest();
	}
	HttpRequest->SetHeader(TEXT("Content-Type"), AtlasHttpHelpers::ContentTypeToString(ContentType));
}

void UAtlasHttpRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	if (!HttpRequest.IsValid())
	{
		HttpRequest = FHttpModule::Get().CreateRequest();
	}
	HttpRequest->SetHeader(HeaderName, HeaderValue);
}

void UAtlasHttpRequest::SetRequestBody(const FString& Body)
{
	if (!HttpRequest.IsValid())
	{
		HttpRequest = FHttpModule::Get().CreateRequest();
	}
	HttpRequest->SetContentAsString(Body);
}

void UAtlasHttpRequest::SetRequestBodyJson(UAtlasJsonObject* JsonObject)
{
	if (!HttpRequest.IsValid())
	{
		HttpRequest = FHttpModule::Get().CreateRequest();
	}
	
	if (JsonObject)
	{
		FString JsonString = JsonObject->EncodeJson();
		HttpRequest->SetContentAsString(JsonString);
		HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	}
}

void UAtlasHttpRequest::SetBinaryContentType(const FString& ContentType)
{
	BinaryContentType = ContentType;
}

void UAtlasHttpRequest::SetBinaryRequestContent(const TArray<uint8>& Content)
{
	RequestBytes = Content;
}

void UAtlasHttpRequest::SetTimeout(float InTimeoutSeconds)
{
	TimeoutSeconds = InTimeoutSeconds;
}

void UAtlasHttpRequest::ProcessRequest()
{
	if (RequestStatus == EAtlasRequestStatus::Processing)
	{
		UE_LOG(LogAtlasHTTP, Warning, TEXT("Request is already in progress"));
		return;
	}

	if (RequestURL.IsEmpty())
	{
		UE_LOG(LogAtlasHTTP, Error, TEXT("Cannot process request: URL is empty"));
		RequestStatus = EAtlasRequestStatus::Failed;
		OnRequestFailed.Broadcast(this, TEXT("URL is empty"), 0);
		return;
	}

	// Create request if not already created
	if (!HttpRequest.IsValid())
	{
		HttpRequest = FHttpModule::Get().CreateRequest();
	}

	// Configure the request
	HttpRequest->SetURL(RequestURL);
	HttpRequest->SetVerb(RequestVerb);

	// Apply binary content if set
	if (RequestBytes.Num() > 0)
	{
		HttpRequest->SetHeader(TEXT("Content-Type"), BinaryContentType);
		HttpRequest->SetContent(RequestBytes);
		UE_LOG(LogAtlasHTTP, Log, TEXT("Setting binary request content (%d bytes, Content-Type: %s)"), RequestBytes.Num(), *BinaryContentType);
	}

	if (TimeoutSeconds > 0.0f)
	{
		HttpRequest->SetTimeout(TimeoutSeconds);
	}

	// Bind completion delegate
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UAtlasHttpRequest::OnHttpRequestComplete);

	// Update status and send
	RequestStatus = EAtlasRequestStatus::Processing;
	
	UE_LOG(LogAtlasHTTP, Log, TEXT("Processing %s request to: %s"), *RequestVerb, *RequestURL);
	
	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogAtlasHTTP, Error, TEXT("Failed to start HTTP request"));
		RequestStatus = EAtlasRequestStatus::Failed;
		OnRequestFailed.Broadcast(this, TEXT("Failed to start HTTP request"), 0);
	}
}

void UAtlasHttpRequest::CancelRequest()
{
	if (HttpRequest.IsValid() && RequestStatus == EAtlasRequestStatus::Processing)
	{
		HttpRequest->CancelRequest();
		RequestStatus = EAtlasRequestStatus::Failed;
		UE_LOG(LogAtlasHTTP, Log, TEXT("Request cancelled"));
	}
}

int32 UAtlasHttpRequest::GetResponseCode() const
{
	if (CachedResponse.IsValid())
	{
		return CachedResponse->GetResponseCode();
	}
	return 0;
}

FString UAtlasHttpRequest::GetResponseContent() const
{
	if (CachedResponse.IsValid())
	{
		return CachedResponse->GetContentAsString();
	}
	return FString();
}

const TArray<uint8>& UAtlasHttpRequest::GetResponseContentBinary() const
{
	if (CachedResponse.IsValid())
	{
		CachedResponseBytes = CachedResponse->GetContent();
	}
	return CachedResponseBytes;
}

UAtlasJsonObject* UAtlasHttpRequest::GetResponseJson() const
{
	if (!CachedResponse.IsValid())
	{
		return nullptr;
	}

	FString Content = CachedResponse->GetContentAsString();
	if (Content.IsEmpty())
	{
		return nullptr;
	}

	UAtlasJsonObject* JsonObj = UAtlasJsonObject::FromJsonString(Content);
	return JsonObj;
}

FString UAtlasHttpRequest::GetResponseHeader(const FString& HeaderName) const
{
	if (CachedResponse.IsValid())
	{
		return CachedResponse->GetHeader(HeaderName);
	}
	return FString();
}

TMap<FString, FString> UAtlasHttpRequest::GetAllResponseHeaders() const
{
	TMap<FString, FString> Headers;
	
	if (CachedResponse.IsValid())
	{
		TArray<FString> AllHeaders = CachedResponse->GetAllHeaders();
		for (const FString& Header : AllHeaders)
		{
			FString Key, Value;
			if (Header.Split(TEXT(": "), &Key, &Value))
			{
				Headers.Add(Key, Value);
			}
		}
	}
	
	return Headers;
}

void UAtlasHttpRequest::OnHttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	CachedResponse = Response;

	if (bWasSuccessful && Response.IsValid())
	{
		int32 ResponseCode = Response->GetResponseCode();
		
		// Consider 2xx status codes as success
		if (ResponseCode >= 200 && ResponseCode < 300)
		{
			RequestStatus = EAtlasRequestStatus::Succeeded;
			UE_LOG(LogAtlasHTTP, Log, TEXT("Request succeeded with code %d"), ResponseCode);
			
			UAtlasJsonObject* ResponseJson = GetResponseJson();
			OnRequestComplete.Broadcast(this, ResponseJson, ResponseCode);
		}
		else
		{
			RequestStatus = EAtlasRequestStatus::Failed;
			FString ErrorMsg = FString::Printf(TEXT("HTTP Error: %d"), ResponseCode);
			UE_LOG(LogAtlasHTTP, Warning, TEXT("%s"), *ErrorMsg);
			
			OnRequestFailed.Broadcast(this, ErrorMsg, ResponseCode);
		}
	}
	else
	{
		RequestStatus = EAtlasRequestStatus::Failed;
		FString ErrorMsg = TEXT("Request failed - no response or connection error");
		
		if (Response.IsValid())
		{
			ErrorMsg = FString::Printf(TEXT("Request failed with code %d"), Response->GetResponseCode());
		}
		
		UE_LOG(LogAtlasHTTP, Error, TEXT("%s"), *ErrorMsg);
		OnRequestFailed.Broadcast(this, ErrorMsg, Response.IsValid() ? Response->GetResponseCode() : 0);
	}
}
