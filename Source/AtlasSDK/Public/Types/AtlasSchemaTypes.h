// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/AtlasValueTypes.h"
#include "Types/AtlasWorkflowTypes.h"
#include "AtlasSchemaTypes.generated.h"

/**
 * Schema definition for a single workflow input or output parameter.
 * Describes the expected type, constraints, and UI presentation for a parameter.
 * Used to generate dynamic UI and validate inputs before execution.
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasParameterDef
{
	GENERATED_BODY()

	/** Internal parameter identifier (used in API calls) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	FString Name;

	/** Human-readable display name for UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	FString DisplayName;

	/** Expected value type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	EAtlasValueType Type = EAtlasValueType::String;

	/** Default value as string (parsed based on Type) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	FString DefaultValue;

	/** Description/tooltip for the parameter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	FString Description;

	/** Minimum value for numeric types */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	float MinValue = 0.0f;

	/** Maximum value for numeric types */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	float MaxValue = 1.0f;

	/** Whether min/max constraints are enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	bool bHasRange = false;

	/** Available options for enum/dropdown types */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	TArray<FString> Options;

	/** Allowed file extensions for file types (e.g., "png", "jpg", "glb") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	TArray<FString> AllowedExtensions;

	// ==================== Helper Methods (C++) ====================

	/** Get the display name, falling back to Name if DisplayName is empty */
	FString GetDisplayName() const
	{
		return DisplayName.IsEmpty() ? Name : DisplayName;
	}

	/** Check if this parameter has available options (for dropdown UI) */
	bool HasOptions() const
	{
		return Options.Num() > 0;
	}

	/** Check if this is a file-type parameter (File, Image, or Mesh) */
	bool IsFileType() const
	{
		return Type == EAtlasValueType::File || 
		       Type == EAtlasValueType::Image || 
		       Type == EAtlasValueType::Mesh;
	}

	/** 
	 * Create a minimal ParameterDef from an existing value.
	 * Useful for displaying output values or dynamic content where schema isn't available.
	 * @param Value The value to infer from
	 * @param InName The parameter name to use
	 * @param InDisplayName Optional display name (defaults to InName)
	 * @return A ParameterDef with type inferred from the value
	 */
	static FAtlasParameterDef FromValue(const FAtlasValue& Value, const FString& InName, const FString& InDisplayName = TEXT(""))
	{
		FAtlasParameterDef Def;
		Def.Name = InName;
		Def.DisplayName = InDisplayName.IsEmpty() ? InName : InDisplayName;
		Def.Type = Value.Type;
		
		// For file types, try to infer allowed extensions from filename
		if (Value.IsFileType() && !Value.FileName.IsEmpty())
		{
			FString Extension = FPaths::GetExtension(Value.FileName).ToLower();
			if (!Extension.IsEmpty())
			{
				Def.AllowedExtensions.Add(Extension);
			}
		}
		
		return Def;
	}

	/** Parse the default value string into an FAtlasValue */
	FAtlasValue GetDefaultValueAsAtlasValue() const
	{
		if (DefaultValue.IsEmpty())
		{
			return FAtlasValue();
		}

		switch (Type)
		{
		case EAtlasValueType::String:
			return FAtlasValue::MakeString(DefaultValue);
		case EAtlasValueType::Number:
			return FAtlasValue::MakeNumber(FCString::Atof(*DefaultValue));
		case EAtlasValueType::Integer:
			return FAtlasValue::MakeInteger(FCString::Atoi(*DefaultValue));
		case EAtlasValueType::Boolean:
			return FAtlasValue::MakeBool(DefaultValue.ToBool());
		case EAtlasValueType::Json:
			return FAtlasValue::MakeJson(DefaultValue);
		default:
			return FAtlasValue();
		}
	}

	/** Validate a value against this parameter definition */
	bool ValidateValue(const FAtlasValue& Value, FString& OutError) const
	{
		// All inputs are required - no value means failure
		if (Value.Type == EAtlasValueType::None)
		{
			OutError = FString::Printf(TEXT("Input '%s' is missing"), *GetDisplayName());
			return false;
		}

		// Special handling: FileId is acceptable for file types (already uploaded)
		bool bTypeMatches = (Value.Type == Type);
		if (!bTypeMatches && Value.Type == EAtlasValueType::FileId && IsFileType())
		{
			bTypeMatches = true;
		}

		if (!bTypeMatches)
		{
			OutError = FString::Printf(TEXT("Input '%s' expects type %s but got %s"),
				*GetDisplayName(),
				*UEnum::GetValueAsString(Type),
				*UEnum::GetValueAsString(Value.Type));
			return false;
		}

		// Range validation for numeric types
		if (bHasRange)
		{
			if (Type == EAtlasValueType::Number)
			{
				float NumValue = Value.GetNumber();
				if (NumValue < MinValue || NumValue > MaxValue)
				{
					OutError = FString::Printf(TEXT("Input '%s' value %.2f is outside range [%.2f, %.2f]"),
						*GetDisplayName(), NumValue, MinValue, MaxValue);
					return false;
				}
			}
			else if (Type == EAtlasValueType::Integer)
			{
				int32 IntValue = Value.GetInteger();
				if (IntValue < (int32)MinValue || IntValue > (int32)MaxValue)
				{
					OutError = FString::Printf(TEXT("Input '%s' value %d is outside range [%d, %d]"),
						*GetDisplayName(), IntValue, (int32)MinValue, (int32)MaxValue);
					return false;
				}
			}
		}

		// Options validation for string types with dropdown
		if (Options.Num() > 0 && Type == EAtlasValueType::String)
		{
			if (!Options.Contains(Value.GetString()))
			{
				OutError = FString::Printf(TEXT("Input '%s' value '%s' is not in allowed options"),
					*GetDisplayName(), *Value.GetString());
				return false;
			}
		}

		return true;
	}
};

/**
 * Complete schema definition for an Atlas workflow.
 * Contains the API identifier, human-readable name, and definitions
 * for all inputs and outputs.
 */
USTRUCT(BlueprintType)
struct ATLASSDK_API FAtlasWorkflowSchema
{
	GENERATED_BODY()

	/** Unique API identifier for this workflow (UUID format) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	FString ApiId;

	/** Human-readable workflow name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	FString Name;

	/** Description of what this workflow does */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	FString Description;

	/** Version string for the workflow */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	FString Version;

	/** Input parameter definitions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	TArray<FAtlasParameterDef> Inputs;

	/** Output parameter definitions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	TArray<FAtlasParameterDef> Outputs;

	/** Base URL for API calls (e.g., "https://api.dev.atlas.design") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	FString BaseUrl;

	// ==================== URL Builder Methods (C++) ====================

	/** Get the base URL with trailing slash removed */
	FString GetCleanBaseUrl() const
	{
		FString CleanUrl = BaseUrl;
		if (CleanUrl.EndsWith(TEXT("/")))
		{
			CleanUrl = CleanUrl.LeftChop(1);
		}
		return CleanUrl;
	}

	/**
	 * Build the upload URL for this workflow.
	 * Pattern: {BaseUrl}/{Version}/upload/{ApiId}
	 */
	FString GetUploadUrl() const
	{
		return FString::Printf(TEXT("%s/%s/upload/%s"), *GetCleanBaseUrl(), *Version, *ApiId);
	}

	/**
	 * Build the async execute URL for this workflow.
	 * Pattern: {BaseUrl}/{Version}/api_execute_async/{ApiId}
	 */
	FString GetExecuteUrl() const
	{
		return FString::Printf(TEXT("%s/%s/api_execute_async/%s"), *GetCleanBaseUrl(), *Version, *ApiId);
	}

	/**
	 * Build the status polling URL for an execution.
	 * Pattern: {BaseUrl}/{Version}/api_status/{ExecutionId}
	 */
	FString GetStatusUrl(const FString& ExecutionId) const
	{
		return FString::Printf(TEXT("%s/%s/api_status/%s"), *GetCleanBaseUrl(), *Version, *ExecutionId);
	}

	/**
	 * Build a download URL for a specific file result.
	 * Pattern: {BaseUrl}/{Version}/download_binary_result/{ApiId}/{FileId}
	 */
	FString GetDownloadUrl(const FString& FileId) const
	{
		return FString::Printf(TEXT("%s/%s/download_binary_result/%s/%s"), *GetCleanBaseUrl(), *Version, *ApiId, *FileId);
	}

	// ==================== Query Methods (C++) ====================

	/** Check if the schema has been populated */
	bool IsValid() const
	{
		return !ApiId.IsEmpty();
	}

	/** Get the display name for this workflow */
	FString GetDisplayName() const
	{
		return Name.IsEmpty() ? ApiId : Name;
	}

	/** Find an input parameter definition by name */
	bool FindInput(const FString& InputName, FAtlasParameterDef& OutDef) const
	{
		for (const FAtlasParameterDef& Input : Inputs)
		{
			if (Input.Name == InputName)
			{
				OutDef = Input;
				return true;
			}
		}
		return false;
	}

	/** Find an output parameter definition by name */
	bool FindOutput(const FString& OutputName, FAtlasParameterDef& OutDef) const
	{
		for (const FAtlasParameterDef& Output : Outputs)
		{
			if (Output.Name == OutputName)
			{
				OutDef = Output;
				return true;
			}
		}
		return false;
	}

	/** Get all input parameter names */
	TArray<FString> GetInputNames() const
	{
		TArray<FString> Names;
		for (const FAtlasParameterDef& Input : Inputs)
		{
			Names.Add(Input.Name);
		}
		return Names;
	}

	/** Get all output parameter names */
	TArray<FString> GetOutputNames() const
	{
		TArray<FString> Names;
		for (const FAtlasParameterDef& Output : Outputs)
		{
			Names.Add(Output.Name);
		}
		return Names;
	}

	/** Validate inputs against this schema - requires exact 1:1 match */
	bool ValidateInputs(const FAtlasWorkflowInputs& InputValues, TArray<FString>& OutErrors) const
	{
		OutErrors.Empty();
		bool bValid = true;

		// First check: count must match exactly
		int32 ExpectedCount = Inputs.Num();
		int32 ProvidedCount = InputValues.Num();
		if (ProvidedCount != ExpectedCount)
		{
			OutErrors.Add(FString::Printf(TEXT("Input count mismatch: expected %d, got %d"), ExpectedCount, ProvidedCount));
			bValid = false;
		}

		// Check each schema-defined input exists and has correct type
		for (const FAtlasParameterDef& InputDef : Inputs)
		{
			FAtlasValue Value;
			InputValues.GetValue(InputDef.Name, Value);

			FString Error;
			if (!InputDef.ValidateValue(Value, Error))
			{
				OutErrors.Add(Error);
				bValid = false;
			}
		}

		// Check for unknown inputs (inputs provided that aren't in schema)
		TArray<FString> ProvidedNames = InputValues.GetInputNames();
		for (const FString& ProvidedName : ProvidedNames)
		{
			bool bFound = false;
			for (const FAtlasParameterDef& InputDef : Inputs)
			{
				if (InputDef.Name == ProvidedName)
				{
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				OutErrors.Add(FString::Printf(TEXT("Unknown input '%s' - not defined in workflow schema"), *ProvidedName));
				bValid = false;
			}
		}

		return bValid;
	}

	/** Create default inputs based on schema definitions */
	FAtlasWorkflowInputs CreateDefaultInputs() const
	{
		FAtlasWorkflowInputs DefaultInputs;

		for (const FAtlasParameterDef& InputDef : Inputs)
		{
			if (!InputDef.DefaultValue.IsEmpty())
			{
				DefaultInputs.SetValue(InputDef.Name, InputDef.GetDefaultValueAsAtlasValue());
			}
		}

		return DefaultInputs;
	}
};
