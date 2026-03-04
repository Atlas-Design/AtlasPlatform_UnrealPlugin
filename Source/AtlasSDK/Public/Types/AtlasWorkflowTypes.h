// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/AtlasValueTypes.h"
#include "AtlasWorkflowTypes.generated.h"

/**
 * Container for workflow input values.
 * Provides type-safe setters for each supported value type.
 * Uses a TMap internally to support dynamic/parametric workflows where
 * input names are not known at compile time.
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasWorkflowInputs
{
	GENERATED_BODY()

	/** Map of input name to value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Inputs")
	TMap<FString, FAtlasValue> Values;

	// ==================== Setters (C++) ====================

	/** Set a string input value */
	void SetString(const FString& Name, const FString& Value)
	{
		Values.Add(Name, FAtlasValue::MakeString(Value));
	}

	/** Set a number (float) input value */
	void SetNumber(const FString& Name, float Value)
	{
		Values.Add(Name, FAtlasValue::MakeNumber(Value));
	}

	/** Set an integer input value */
	void SetInteger(const FString& Name, int32 Value)
	{
		Values.Add(Name, FAtlasValue::MakeInteger(Value));
	}

	/** Set a boolean input value */
	void SetBool(const FString& Name, bool Value)
	{
		Values.Add(Name, FAtlasValue::MakeBool(Value));
	}

	/** Set a file input value from a file path */
	void SetFile(const FString& Name, const FString& FilePath)
	{
		Values.Add(Name, FAtlasValue::MakeFile(FilePath));
	}

	/** Set a file ID reference (for already-uploaded files) */
	void SetFileId(const FString& Name, const FString& FileId)
	{
		Values.Add(Name, FAtlasValue::MakeFileId(FileId));
	}

	/** Set an image input value from a file path */
	void SetImage(const FString& Name, const FString& FilePath)
	{
		Values.Add(Name, FAtlasValue::MakeImage(FilePath));
	}

	/** Set a mesh input value from a file path */
	void SetMesh(const FString& Name, const FString& FilePath)
	{
		Values.Add(Name, FAtlasValue::MakeMesh(FilePath));
	}

	/** Set a JSON input value */
	void SetJson(const FString& Name, const FString& JsonString)
	{
		Values.Add(Name, FAtlasValue::MakeJson(JsonString));
	}

	/** Set a raw FAtlasValue directly */
	void SetValue(const FString& Name, const FAtlasValue& Value)
	{
		Values.Add(Name, Value);
	}

	// ==================== Getters (C++) ====================

	/** Check if an input with the given name exists */
	bool Contains(const FString& Name) const
	{
		return Values.Contains(Name);
	}

	/** Get a value by name */
	bool GetValue(const FString& Name, FAtlasValue& OutValue) const
	{
		if (const FAtlasValue* Found = Values.Find(Name))
		{
			OutValue = *Found;
			return true;
		}
		return false;
	}

	/** Get all input names */
	TArray<FString> GetInputNames() const
	{
		TArray<FString> Names;
		Values.GetKeys(Names);
		return Names;
	}

	/** Get the number of inputs */
	int32 Num() const
	{
		return Values.Num();
	}

	/** Remove an input by name */
	bool Remove(const FString& Name)
	{
		return Values.Remove(Name) > 0;
	}

	/** Clear all inputs */
	void Clear()
	{
		Values.Empty();
	}
};

/**
 * Container for workflow output values.
 * Provides type-safe getters for each supported value type.
 * Populated by the SDK after workflow execution completes.
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasWorkflowOutputs
{
	GENERATED_BODY()

	/** Map of output name to value */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atlas|Outputs")
	TMap<FString, FAtlasValue> Values;

	// ==================== Value Access (C++) ====================

	/** Check if an output with the given name exists */
	bool Contains(const FString& Name) const
	{
		return Values.Contains(Name);
	}

	/** Get a value by name */
	bool GetValue(const FString& Name, FAtlasValue& OutValue) const
	{
		if (const FAtlasValue* Found = Values.Find(Name))
		{
			OutValue = *Found;
			return true;
		}
		return false;
	}

	/** Get all output names */
	TArray<FString> GetOutputNames() const
	{
		TArray<FString> Names;
		Values.GetKeys(Names);
		return Names;
	}

	/** Get the number of outputs */
	int32 Num() const
	{
		return Values.Num();
	}

	// ==================== Type-Safe Getters (C++) ====================

	/** Get a string output value */
	bool GetString(const FString& Name, FString& OutValue) const
	{
		if (const FAtlasValue* Found = Values.Find(Name))
		{
			if (Found->IsString())
			{
				OutValue = Found->GetString();
				return true;
			}
		}
		return false;
	}

	/** Get a number (float) output value */
	bool GetNumber(const FString& Name, float& OutValue) const
	{
		if (const FAtlasValue* Found = Values.Find(Name))
		{
			if (Found->IsNumber())
			{
				OutValue = Found->GetNumber();
				return true;
			}
		}
		return false;
	}

	/** Get an integer output value */
	bool GetInteger(const FString& Name, int32& OutValue) const
	{
		if (const FAtlasValue* Found = Values.Find(Name))
		{
			if (Found->IsInteger())
			{
				OutValue = Found->GetInteger();
				return true;
			}
		}
		return false;
	}

	/** Get a boolean output value */
	bool GetBool(const FString& Name, bool& OutValue) const
	{
		if (const FAtlasValue* Found = Values.Find(Name))
		{
			if (Found->IsBool())
			{
				OutValue = Found->GetBool();
				return true;
			}
		}
		return false;
	}

	/** Get a file ID output value */
	bool GetFileId(const FString& Name, FString& OutFileId) const
	{
		if (const FAtlasValue* Found = Values.Find(Name))
		{
			if (Found->IsFileId())
			{
				OutFileId = Found->GetFileId();
				return true;
			}
		}
		return false;
	}

	/** Get file data for an output (if file was downloaded) */
	bool GetFileData(const FString& Name, TArray<uint8>& OutBytes) const
	{
		if (const FAtlasValue* Found = Values.Find(Name))
		{
			if (Found->IsFileType() && Found->FileData.Num() > 0)
			{
				OutBytes = Found->FileData;
				return true;
			}
		}
		return false;
	}

	/** Get a JSON string output value */
	bool GetJson(const FString& Name, FString& OutJson) const
	{
		if (const FAtlasValue* Found = Values.Find(Name))
		{
			if (Found->IsJson())
			{
				OutJson = Found->GetJson();
				return true;
			}
		}
		return false;
	}

	// ==================== Internal Methods ====================

	/** Add an output value (used internally by SDK) */
	void AddValue(const FString& Name, const FAtlasValue& Value)
	{
		Values.Add(Name, Value);
	}

	/** Clear all outputs */
	void Clear()
	{
		Values.Empty();
	}
};
