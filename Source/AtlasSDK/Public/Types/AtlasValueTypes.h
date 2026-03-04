// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AtlasValueTypes.generated.h"

/**
 * Enumeration of value types supported by Atlas workflows.
 * Determines how values are stored, serialized, and transmitted.
 */
UENUM(BlueprintType)
enum class EAtlasValueType : uint8
{
	/** No value / uninitialized */
	None UMETA(DisplayName = "None"),
	
	/** Text string value */
	String UMETA(DisplayName = "String"),
	
	/** Floating-point number */
	Number UMETA(DisplayName = "Number"),
	
	/** Integer number */
	Integer UMETA(DisplayName = "Integer"),
	
	/** Boolean true/false */
	Boolean UMETA(DisplayName = "Boolean"),
	
	/** Raw file bytes (for upload) */
	File UMETA(DisplayName = "File"),
	
	/** Server-side file reference ID */
	FileId UMETA(DisplayName = "File ID"),
	
	/** Image file (PNG, JPEG, etc.) */
	Image UMETA(DisplayName = "Image"),
	
	/** 3D mesh file */
	Mesh UMETA(DisplayName = "Mesh"),
	
	/** JSON object or array */
	Json UMETA(DisplayName = "JSON")
};

/**
 * Variant container that can hold any Atlas workflow value type.
 * Used for both inputs and outputs in dynamic/parametric workflows.
 * 
 * For INPUTS (workflow configuration):
 * - File/Image/Mesh types store a FilePath - the system reads and uploads when executing
 * - Other types store their value directly
 * 
 * For OUTPUTS (workflow results):
 * - File/Image/Mesh types contain FileData (bytes) received from API
 * - FileId contains server-side reference if applicable
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasValue
{
	GENERATED_BODY()

	/** The type of value stored in this container */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Value")
	EAtlasValueType Type = EAtlasValueType::None;

	/** String value (when Type == String) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Value")
	FString StringValue;

	/** Floating-point number value (when Type == Number) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Value")
	float NumberValue = 0.0f;

	/** Integer value (when Type == Integer) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Value")
	int32 IntValue = 0;

	/** Boolean value (when Type == Boolean) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Value")
	bool BoolValue = false;

	/** File path for INPUT file types (Image, Mesh, File) - path to local file to upload */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Value")
	FString FilePath;

	/** Raw file bytes for OUTPUT file types - populated when receiving results from API */
	UPROPERTY(BlueprintReadWrite, Category = "Atlas|Value")
	TArray<uint8> FileData;

	/** Server-side file reference ID (when Type == FileId, or populated after upload) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Value")
	FString FileId;

	/** Original filename (used for outputs to know file extension, or extracted from FilePath for inputs) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Value")
	FString FileName;

	/** JSON string content (when Type == Json) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Value")
	FString JsonValue;

	// ==================== Constructors ====================

	/** Default constructor - creates an empty/None value */
	FAtlasValue()
		: Type(EAtlasValueType::None)
		, NumberValue(0.0f)
		, IntValue(0)
		, BoolValue(false)
	{
	}

	/** Construct a string value */
	explicit FAtlasValue(const FString& InString)
		: Type(EAtlasValueType::String)
		, StringValue(InString)
		, NumberValue(0.0f)
		, IntValue(0)
		, BoolValue(false)
	{
	}

	/** Construct a number value */
	explicit FAtlasValue(float InNumber)
		: Type(EAtlasValueType::Number)
		, NumberValue(InNumber)
		, IntValue(0)
		, BoolValue(false)
	{
	}

	/** Construct an integer value */
	explicit FAtlasValue(int32 InInt)
		: Type(EAtlasValueType::Integer)
		, NumberValue(0.0f)
		, IntValue(InInt)
		, BoolValue(false)
	{
	}

	/** Construct a boolean value */
	explicit FAtlasValue(bool bInBool)
		: Type(EAtlasValueType::Boolean)
		, NumberValue(0.0f)
		, IntValue(0)
		, BoolValue(bInBool)
	{
	}

	// ==================== Static Factory Methods (C++) ====================

	/** Create a string value */
	static FAtlasValue MakeString(const FString& Value)
	{
		return FAtlasValue(Value);
	}

	/** Create a number value */
	static FAtlasValue MakeNumber(float Value)
	{
		return FAtlasValue(Value);
	}

	/** Create an integer value */
	static FAtlasValue MakeInteger(int32 Value)
	{
		return FAtlasValue(Value);
	}

	/** Create a boolean value */
	static FAtlasValue MakeBool(bool Value)
	{
		return FAtlasValue(Value);
	}

	/** Create a file input value from a file path (for workflow inputs) */
	static FAtlasValue MakeFile(const FString& InFilePath)
	{
		FAtlasValue Result;
		Result.Type = EAtlasValueType::File;
		Result.FilePath = InFilePath;
		Result.FileName = FPaths::GetCleanFilename(InFilePath);
		return Result;
	}

	/** Create a file ID reference value (server-side file reference) */
	static FAtlasValue MakeFileId(const FString& InFileId)
	{
		FAtlasValue Result;
		Result.Type = EAtlasValueType::FileId;
		Result.FileId = InFileId;
		return Result;
	}

	/** Create an image input value from a file path (for workflow inputs) */
	static FAtlasValue MakeImage(const FString& InFilePath)
	{
		FAtlasValue Result;
		Result.Type = EAtlasValueType::Image;
		Result.FilePath = InFilePath;
		Result.FileName = FPaths::GetCleanFilename(InFilePath);
		return Result;
	}

	/** Create a mesh input value from a file path (for workflow inputs) */
	static FAtlasValue MakeMesh(const FString& InFilePath)
	{
		FAtlasValue Result;
		Result.Type = EAtlasValueType::Mesh;
		Result.FilePath = InFilePath;
		Result.FileName = FPaths::GetCleanFilename(InFilePath);
		return Result;
	}

	// ==================== Output Factory Methods (for API responses) ====================

	/** Create an image output value from byte data (used when receiving API results) */
	static FAtlasValue MakeImageFromBytes(const TArray<uint8>& Bytes, const FString& InFileName)
	{
		FAtlasValue Result;
		Result.Type = EAtlasValueType::Image;
		Result.FileData = Bytes;
		Result.FileName = InFileName;
		return Result;
	}

	/** Create a mesh output value from byte data (used when receiving API results) */
	static FAtlasValue MakeMeshFromBytes(const TArray<uint8>& Bytes, const FString& InFileName)
	{
		FAtlasValue Result;
		Result.Type = EAtlasValueType::Mesh;
		Result.FileData = Bytes;
		Result.FileName = InFileName;
		return Result;
	}

	/** Create a file output value from byte data (used when receiving API results) */
	static FAtlasValue MakeFileFromBytes(const TArray<uint8>& Bytes, const FString& InFileName)
	{
		FAtlasValue Result;
		Result.Type = EAtlasValueType::File;
		Result.FileData = Bytes;
		Result.FileName = InFileName;
		return Result;
	}

	/** Create a JSON value from a JSON string */
	static FAtlasValue MakeJson(const FString& InJsonString)
	{
		FAtlasValue Result;
		Result.Type = EAtlasValueType::Json;
		Result.JsonValue = InJsonString;
		return Result;
	}

	// ==================== Type Checking ====================

	/** Check if this value contains valid data */
	bool IsValid() const
	{
		return Type != EAtlasValueType::None;
	}

	/** Check if this value is a string */
	bool IsString() const { return Type == EAtlasValueType::String; }

	/** Check if this value is a number */
	bool IsNumber() const { return Type == EAtlasValueType::Number; }

	/** Check if this value is an integer */
	bool IsInteger() const { return Type == EAtlasValueType::Integer; }

	/** Check if this value is a boolean */
	bool IsBool() const { return Type == EAtlasValueType::Boolean; }

	/** Check if this is a file-type value (File, Image, or Mesh) */
	bool IsFileType() const
	{
		return Type == EAtlasValueType::File || Type == EAtlasValueType::Image || Type == EAtlasValueType::Mesh;
	}

	/** Check if this file-type value has a file path set (for inputs) */
	bool HasFilePath() const
	{
		return IsFileType() && !FilePath.IsEmpty();
	}

	/** Check if this file-type value has byte data (for outputs) */
	bool HasFileData() const
	{
		return IsFileType() && FileData.Num() > 0;
	}

	/** Check if this value is a file ID reference */
	bool IsFileId() const { return Type == EAtlasValueType::FileId; }

	/** Check if this value is JSON */
	bool IsJson() const { return Type == EAtlasValueType::Json; }

	// ==================== Getters with Validation ====================

	/** Get the string value. Returns empty string if not a string type. */
	FString GetString() const
	{
		return IsString() ? StringValue : FString();
	}

	/** Get the number value. Returns 0 if not a number type. */
	float GetNumber() const
	{
		return IsNumber() ? NumberValue : 0.0f;
	}

	/** Get the integer value. Returns 0 if not an integer type. */
	int32 GetInteger() const
	{
		return IsInteger() ? IntValue : 0;
	}

	/** Get the boolean value. Returns false if not a boolean type. */
	bool GetBool() const
	{
		return IsBool() ? BoolValue : false;
	}

	/** Get the file ID. Returns empty string if not a file ID type. */
	FString GetFileId() const
	{
		return IsFileId() ? FileId : FString();
	}

	/** Get the file path (for input file types). Returns empty string if not set. */
	FString GetFilePath() const
	{
		return IsFileType() ? FilePath : FString();
	}

	/** Get the JSON string. Returns empty string if not a JSON type. */
	FString GetJson() const
	{
		return IsJson() ? JsonValue : FString();
	}

	/** Get a human-readable string representation of this value */
	FString ToString() const
	{
		switch (Type)
		{
		case EAtlasValueType::None:
			return TEXT("(None)");
		case EAtlasValueType::String:
			return StringValue;
		case EAtlasValueType::Number:
			return FString::SanitizeFloat(NumberValue);
		case EAtlasValueType::Integer:
			return FString::FromInt(IntValue);
		case EAtlasValueType::Boolean:
			return BoolValue ? TEXT("true") : TEXT("false");
		case EAtlasValueType::File:
		case EAtlasValueType::Image:
		case EAtlasValueType::Mesh:
			if (!FilePath.IsEmpty())
			{
				return FString::Printf(TEXT("[%s: %s]"), 
					*UEnum::GetValueAsString(Type), *FilePath);
			}
			else if (FileData.Num() > 0)
			{
				return FString::Printf(TEXT("[%s: %s, %d bytes]"), 
					*UEnum::GetValueAsString(Type), *FileName, FileData.Num());
			}
			return FString::Printf(TEXT("[%s: empty]"), *UEnum::GetValueAsString(Type));
		case EAtlasValueType::FileId:
			return FString::Printf(TEXT("[FileId: %s]"), *FileId);
		case EAtlasValueType::Json:
			return JsonValue.Len() > 50 ? JsonValue.Left(50) + TEXT("...") : JsonValue;
		default:
			return TEXT("(Unknown)");
		}
	}
};
