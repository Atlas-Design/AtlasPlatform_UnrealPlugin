// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AtlasErrorTypes.generated.h"

/**
 * Categorized error codes for Atlas operations.
 * Helps callers understand the type of error and respond appropriately.
 */
UENUM(BlueprintType)
enum class EAtlasErrorCode : uint8
{
	/** No error / success */
	None UMETA(DisplayName = "None"),

	/** Unknown or unclassified error */
	Unknown UMETA(DisplayName = "Unknown Error"),

	// ==================== Client-Side Errors ====================

	/** Input validation failed */
	ValidationError UMETA(DisplayName = "Validation Error"),

	/** Required parameter is missing */
	MissingParameter UMETA(DisplayName = "Missing Parameter"),

	/** Parameter type mismatch */
	InvalidParameterType UMETA(DisplayName = "Invalid Parameter Type"),

	/** File read/write error on client */
	FileError UMETA(DisplayName = "File Error"),

	/** Operation was cancelled by user */
	Cancelled UMETA(DisplayName = "Cancelled"),

	/** Operation timed out */
	Timeout UMETA(DisplayName = "Timeout"),

	// ==================== Network Errors ====================

	/** Network connection failed */
	NetworkError UMETA(DisplayName = "Network Error"),

	/** Could not resolve server address */
	DnsError UMETA(DisplayName = "DNS Resolution Error"),

	/** Connection refused by server */
	ConnectionRefused UMETA(DisplayName = "Connection Refused"),

	/** SSL/TLS handshake failed */
	SslError UMETA(DisplayName = "SSL Error"),

	// ==================== Server Errors ====================

	/** Bad request (400) */
	BadRequest UMETA(DisplayName = "Bad Request"),

	/** Authentication failed (401) */
	Unauthorized UMETA(DisplayName = "Unauthorized"),

	/** Access denied (403) */
	Forbidden UMETA(DisplayName = "Forbidden"),

	/** Resource not found (404) */
	NotFound UMETA(DisplayName = "Not Found"),

	/** Rate limit exceeded (429) */
	RateLimited UMETA(DisplayName = "Rate Limited"),

	/** Server error (5xx) */
	ServerError UMETA(DisplayName = "Server Error"),

	/** Service temporarily unavailable */
	ServiceUnavailable UMETA(DisplayName = "Service Unavailable"),

	// ==================== Workflow Errors ====================

	/** Workflow execution failed */
	ExecutionFailed UMETA(DisplayName = "Execution Failed"),

	/** Specific workflow node failed */
	NodeFailed UMETA(DisplayName = "Node Failed"),

	/** Upload failed */
	UploadFailed UMETA(DisplayName = "Upload Failed"),

	/** Download failed */
	DownloadFailed UMETA(DisplayName = "Download Failed"),

	/** Workflow not found or invalid API ID */
	WorkflowNotFound UMETA(DisplayName = "Workflow Not Found"),

	/** Invalid workflow schema/configuration */
	InvalidSchema UMETA(DisplayName = "Invalid Schema"),

	// ==================== Resource Errors ====================

	/** Quota exceeded */
	QuotaExceeded UMETA(DisplayName = "Quota Exceeded"),

	/** Resource limit reached */
	ResourceLimit UMETA(DisplayName = "Resource Limit"),

	/** Invalid or expired file reference */
	InvalidFileId UMETA(DisplayName = "Invalid File ID"),
};

/**
 * Structured error information for Atlas operations.
 * Provides detailed context about what went wrong, including
 * HTTP status, error messages, and workflow node information.
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasError
{
	GENERATED_BODY()

	/** Categorized error code */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atlas|Error")
	EAtlasErrorCode Code = EAtlasErrorCode::None;

	/** HTTP status code (0 if not applicable) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atlas|Error")
	int32 HttpStatusCode = 0;

	/** Human-readable error message */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atlas|Error")
	FString Message;

	/** Detailed error description (may include technical details) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atlas|Error")
	FString Details;

	/** Name of the workflow node that failed (if applicable) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atlas|Error")
	FString NodeName;

	/** Type of the workflow node that failed (if applicable) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atlas|Error")
	FString NodeType;

	/** ID of the workflow node that failed (if applicable) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atlas|Error")
	FString NodeId;

	/** Raw server response for debugging */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atlas|Error")
	FString RawResponse;

	/** Timestamp when the error occurred */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atlas|Error")
	FDateTime Timestamp;

	// ==================== Constructors ====================

	/** Default constructor - no error */
	FAtlasError()
		: Code(EAtlasErrorCode::None)
		, HttpStatusCode(0)
		, Timestamp(FDateTime::Now())
	{
	}

	/** Construct with code and message */
	FAtlasError(EAtlasErrorCode InCode, const FString& InMessage)
		: Code(InCode)
		, HttpStatusCode(0)
		, Message(InMessage)
		, Timestamp(FDateTime::Now())
	{
	}

	/** Construct with code, message, and HTTP status */
	FAtlasError(EAtlasErrorCode InCode, const FString& InMessage, int32 InHttpStatus)
		: Code(InCode)
		, HttpStatusCode(InHttpStatus)
		, Message(InMessage)
		, Timestamp(FDateTime::Now())
	{
	}

	// ==================== Static Factory Methods (C++) ====================

	/** Create a validation error */
	static FAtlasError ValidationFailed(const FString& InMessage)
	{
		return FAtlasError(EAtlasErrorCode::ValidationError, InMessage);
	}

	/** Create a missing parameter error */
	static FAtlasError MissingParameter(const FString& ParameterName)
	{
		return FAtlasError(EAtlasErrorCode::MissingParameter,
			FString::Printf(TEXT("Required parameter '%s' is missing"), *ParameterName));
	}

	/** Create a network error */
	static FAtlasError NetworkFailed(const FString& InDetails)
	{
		FAtlasError Error(EAtlasErrorCode::NetworkError, TEXT("Network request failed"));
		Error.Details = InDetails;
		return Error;
	}

	/** Create a timeout error */
	static FAtlasError TimedOut(const FString& Operation)
	{
		return FAtlasError(EAtlasErrorCode::Timeout,
			FString::Printf(TEXT("Operation '%s' timed out"), *Operation));
	}

	/** Create a cancellation error */
	static FAtlasError Cancelled()
	{
		return FAtlasError(EAtlasErrorCode::Cancelled, TEXT("Operation was cancelled"));
	}

	/** Create an error from HTTP status code */
	static FAtlasError FromHttpStatus(int32 StatusCode, const FString& ResponseBody)
	{
		EAtlasErrorCode ErrorCode;
		FString ErrorMessage;

		switch (StatusCode)
		{
		case 400:
			ErrorCode = EAtlasErrorCode::BadRequest;
			ErrorMessage = TEXT("Bad request");
			break;
		case 401:
			ErrorCode = EAtlasErrorCode::Unauthorized;
			ErrorMessage = TEXT("Authentication required");
			break;
		case 403:
			ErrorCode = EAtlasErrorCode::Forbidden;
			ErrorMessage = TEXT("Access denied");
			break;
		case 404:
			ErrorCode = EAtlasErrorCode::NotFound;
			ErrorMessage = TEXT("Resource not found");
			break;
		case 429:
			ErrorCode = EAtlasErrorCode::RateLimited;
			ErrorMessage = TEXT("Rate limit exceeded");
			break;
		case 500:
		case 502:
		case 503:
		case 504:
			ErrorCode = EAtlasErrorCode::ServerError;
			ErrorMessage = FString::Printf(TEXT("Server error (%d)"), StatusCode);
			break;
		default:
			ErrorCode = StatusCode >= 400 && StatusCode < 500 
				? EAtlasErrorCode::BadRequest 
				: EAtlasErrorCode::ServerError;
			ErrorMessage = FString::Printf(TEXT("HTTP error %d"), StatusCode);
			break;
		}

		FAtlasError Error(ErrorCode, ErrorMessage, StatusCode);
		Error.RawResponse = ResponseBody;
		return Error;
	}

	/** Create a node failure error */
	static FAtlasError NodeFailure(const FString& InNodeName, const FString& InNodeType, const FString& InNodeId, const FString& ErrorMessage)
	{
		FAtlasError Error(EAtlasErrorCode::NodeFailed,
			FString::Printf(TEXT("Workflow node '%s' failed: %s"), *InNodeName, *ErrorMessage));
		Error.NodeName = InNodeName;
		Error.NodeType = InNodeType;
		Error.NodeId = InNodeId;
		return Error;
	}

	// ==================== Query Methods (C++) ====================

	/** Check if this represents an error (Code != None) */
	bool IsError() const
	{
		return Code != EAtlasErrorCode::None;
	}

	/** Check if this is a success (no error) */
	bool IsSuccess() const
	{
		return Code == EAtlasErrorCode::None;
	}

	/** Check if this error is recoverable (can retry) */
	bool IsRetryable() const
	{
		switch (Code)
		{
		case EAtlasErrorCode::NetworkError:
		case EAtlasErrorCode::Timeout:
		case EAtlasErrorCode::RateLimited:
		case EAtlasErrorCode::ServiceUnavailable:
		case EAtlasErrorCode::ServerError:
			return true;
		default:
			return false;
		}
	}

	/** Check if this error is due to user action (cancelled) */
	bool WasCancelled() const
	{
		return Code == EAtlasErrorCode::Cancelled;
	}

	/** Check if this is a client-side error (validation, input, etc.) */
	bool IsClientError() const
	{
		switch (Code)
		{
		case EAtlasErrorCode::ValidationError:
		case EAtlasErrorCode::MissingParameter:
		case EAtlasErrorCode::InvalidParameterType:
		case EAtlasErrorCode::FileError:
		case EAtlasErrorCode::Cancelled:
			return true;
		default:
			return false;
		}
	}

	/** Check if this is a network-related error */
	bool IsNetworkError() const
	{
		switch (Code)
		{
		case EAtlasErrorCode::NetworkError:
		case EAtlasErrorCode::DnsError:
		case EAtlasErrorCode::ConnectionRefused:
		case EAtlasErrorCode::SslError:
		case EAtlasErrorCode::Timeout:
			return true;
		default:
			return false;
		}
	}

	/** Check if a specific workflow node failed */
	bool HasNodeInfo() const
	{
		return !NodeName.IsEmpty() || !NodeId.IsEmpty();
	}

	/** Get a formatted error string for display/logging */
	FString ToString() const
	{
		if (Code == EAtlasErrorCode::None)
		{
			return TEXT("No error");
		}

		FString Result = FString::Printf(TEXT("[%s] %s"),
			*UEnum::GetValueAsString(Code),
			*Message);

		if (HttpStatusCode > 0)
		{
			Result += FString::Printf(TEXT(" (HTTP %d)"), HttpStatusCode);
		}

		if (HasNodeInfo())
		{
			Result += FString::Printf(TEXT(" [Node: %s]"), *NodeName);
		}

		return Result;
	}

	/** Get a short summary suitable for UI display */
	FString GetSummary() const
	{
		if (Code == EAtlasErrorCode::None)
		{
			return TEXT("Success");
		}
		return Message.IsEmpty() ? UEnum::GetDisplayValueAsText(Code).ToString() : Message;
	}
};
