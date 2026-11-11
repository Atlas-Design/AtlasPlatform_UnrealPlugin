// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/AtlasValueTypes.h"
#include "Types/AtlasWorkflowTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
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

	/** Original platform token; transport families do not imply native import support. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|Schema")
	FString OriginalType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|Schema")
	bool bIsArray = false;
	/** Legacy custom exports without required metadata retain their previous required behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atlas|Schema")
	bool bRequired = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Atlas|Schema")
	bool bHasRequiredMetadata = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Atlas|Schema")
	bool bHasDefaultValue = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Atlas|Schema")
	bool bDefaultIsNull = false;
	/** Exact JSON representation preserves list/default presence until typed list support is available. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Atlas|Schema")
	FString DefaultValueJson;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	bool bHasMinimum = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	bool bHasMaximum = false;

	/** Available options for enum/dropdown types */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	TArray<FString> Options;

	/** Allowed file extensions for file types (e.g., "png", "jpg", "glb") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	TArray<FString> AllowedExtensions;

	/** File format hint from schema (e.g., "mp3", "wav") - used for audio/file outputs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Schema")
	FString Format;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Atlas|Schema") bool bHasExplicitFormat=false;

	// ==================== Helper Methods (C++) ====================

    /** One extension policy for selection and execution. Explicit format wins over fallbacks. */
    TArray<FString> GetSelectionExtensions() const
    {
        TArray<FString> Result;
        if (bHasExplicitFormat && !Format.IsEmpty()) Result={Format};
        else if (!AllowedExtensions.IsEmpty()) Result=AllowedExtensions;
        else
        {
            const FString Token=OriginalType.ToLower();
            if (Token==TEXT("agent_context")) Result={TEXT("atlas-context.json"),TEXT("json")};
            else if (Token==TEXT("video")) Result={TEXT("mp4"),TEXT("mov"),TEXT("webm"),TEXT("avi"),TEXT("mkv")};
            else if (Token==TEXT("scene")) Result={TEXT("blend")};
            else if (Token==TEXT("rig_animation")) Result={TEXT("glb")};
            else if (Token==TEXT("jpeg")) Result={TEXT("jpg")};
            else if (Token==TEXT("tiff")) Result={TEXT("tiff")};
            else if (TArray<FString>{TEXT("exr"),TEXT("webp"),TEXT("avif"),TEXT("tga"),TEXT("svg"),TEXT("fbx"),TEXT("usdz"),TEXT("pdf")}.Contains(Token)) Result={Token};
            else if (Type==EAtlasValueType::Image) Result={TEXT("png"),TEXT("jpg"),TEXT("bmp"),TEXT("tga"),TEXT("tiff"),TEXT("exr"),TEXT("webp"),TEXT("avif")};
            else if (Type==EAtlasValueType::Mesh) Result={TEXT("glb")};
            else if (Type==EAtlasValueType::Audio) Result={TEXT("wav"),TEXT("mp3"),TEXT("ogg"),TEXT("aif"),TEXT("aiff"),TEXT("flac"),TEXT("m4a")};
        }
        for (auto& Ext : Result) { Ext=Ext.TrimStartAndEnd().ToLower(); Ext.RemoveFromStart(TEXT(".")); }
        if (Result.Contains(TEXT("jpg")) || Result.Contains(TEXT("jpeg"))) { Result.AddUnique(TEXT("jpg")); Result.AddUnique(TEXT("jpeg")); }
        if (Result.Contains(TEXT("tif")) || Result.Contains(TEXT("tiff"))) { Result.AddUnique(TEXT("tif")); Result.AddUnique(TEXT("tiff")); }
        return Result;
    }
    bool ValidateFilePath(const FString& Path,FString& Error) const
    {
        if (Path.IsEmpty() || !FPaths::FileExists(Path)) { Error=TEXT("File is missing or is a folder; choose an existing file"); return false; }
        const auto Extensions=GetSelectionExtensions();
        if (!Extensions.IsEmpty() && !Extensions.ContainsByPredicate([&](const FString& Ext) { return Path.EndsWith(TEXT(".")+Ext,ESearchCase::IgnoreCase); }))
        { Error=TEXT("Expected ")+FString::Join(Extensions,TEXT(", ")); return false; }
        return true;
    }

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

	/** Check if this is a file-type parameter (File, Image, Mesh, or Audio) */
	bool IsFileType() const
	{
		return Type == EAtlasValueType::File || 
		       Type == EAtlasValueType::Image || 
		       Type == EAtlasValueType::Mesh ||
		       Type == EAtlasValueType::Audio;
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
		Def.Type = Value.Type; Def.OriginalType=Value.OriginalType; Def.bIsArray=Value.bIsArray; Def.Format=Value.FileFormat;
		
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
		if (bDefaultIsNull || (!bHasDefaultValue && DefaultValue.IsEmpty()))
		{
			return FAtlasValue();
		}

        if (bIsArray)
        {
            TArray<TSharedPtr<FJsonValue>> Items;
            if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(DefaultValueJson), Items)) return FAtlasValue();
            FAtlasValue Result=FAtlasValue::MakeFileList(Type, {}); Result.OriginalType=OriginalType; Result.FileFormat=Format;
            for (const auto& Item : Items)
            {
                FAtlasFileReference Ref;
                if (!Item->TryGetString(Ref.FileId)) return FAtlasValue();
                Result.FileItems.Add(Ref);
            }
            return Result;
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
		case EAtlasValueType::Image:
		case EAtlasValueType::Mesh:
		case EAtlasValueType::Audio:
		case EAtlasValueType::File:
		case EAtlasValueType::FileId:
			{
			FAtlasValue Result = DefaultValue.IsEmpty() ? FAtlasValue() : FAtlasValue::MakeFileId(DefaultValue);
			Result.OriginalType = OriginalType; Result.FileFormat = Format;
			return Result;
		}
		default:
			return FAtlasValue();
		}
	}

	/** Validate a value against this parameter definition */
	bool ValidateValue(const FAtlasValue& Value, FString& OutError) const
	{
		if (Value.bOmitted) { OutError=TEXT("Omitted history markers must be removed from request inputs"); return false; }
        if (Type==EAtlasValueType::None) { OutError=TEXT("Unsupported input type: ")+OriginalType; return false; }
        // None is an explicit invalid value. Optional omission is handled by the schema map check.
        if (bIsArray != Value.bIsArray)
        {
            OutError=FString::Printf(TEXT("Input '%s' expects %s"), *Name, bIsArray ? TEXT("a file list") : TEXT("a scalar")); return false;
        }
        if (bIsArray)
        {
            if (!IsFileType() || Value.Type != Type)
            { OutError=FString::Printf(TEXT("Input '%s' has an incompatible file-list type"), *Name); return false; }
            FAtlasParameterDef Scalar=*this; Scalar.bIsArray=false;
            for (int32 I=0; I<Value.FileItems.Num(); ++I)
            {
                const auto& Ref=Value.FileItems[I]; FAtlasValue Item; Item.Type=Type;
                Item.FileId=Ref.FileId; Item.FilePath=Ref.FilePath; Item.FileName=Ref.FileName;
                // Presence only; don't copy potentially large buffers just to validate.
                if (Item.FileId.IsEmpty() && Item.FilePath.IsEmpty() && !Ref.FileData.IsEmpty()) continue;
                FString Detail;
                if (!Scalar.ValidateValue(Item, Detail))
                { OutError=FString::Printf(TEXT("Input '%s' item %d: %s"), *Name, I+1, *Detail); return false; }
            }
            return true;
        }

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

		if (Type == EAtlasValueType::Number || Type == EAtlasValueType::Integer)
		{
			const double Number = Type == EAtlasValueType::Number ? Value.NumberValue : Value.IntValue;
			const bool bLegacyRange = bHasRange && !bHasMinimum && !bHasMaximum;
			if (!FMath::IsFinite(Number) || ((bHasMinimum || bLegacyRange) && Number < MinValue)
				|| ((bHasMaximum || bLegacyRange) && Number > MaxValue))
			{
				OutError = FString::Printf(TEXT("Input '%s' is not finite or is outside its declared bounds"), *Name);
				return false;
			}
		}
		if (IsFileType() || Type == EAtlasValueType::FileId)
		{
			if (Value.FileId.IsEmpty() && Value.FileData.IsEmpty() && (Value.FilePath.IsEmpty() || !FPaths::FileExists(Value.FilePath)))
			{
				OutError = FString::Printf(TEXT("Input '%s' requires a file ID, readable file or inline bytes"), *Name);
				return false;
			}
		}

        if (IsFileType() && Value.FileId.IsEmpty() && !Value.FilePath.IsEmpty() && !ValidateFilePath(Value.FilePath,OutError)) return false;
		if (Type == EAtlasValueType::Json)
		{
			TSharedPtr<FJsonValue> Json;
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Value.JsonValue), Json) || !Json.IsValid()
				|| (Json->Type != EJson::Object && Json->Type != EJson::Array))
			{ OutError = FString::Printf(TEXT("Input '%s' requires a JSON object or array"), *Name); return false; }
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

	// ==================== API Version Helpers (C++) ====================

	/**
	 * Parse "major.minor" API version strings from exported workflow JSON.
	 * @return False when Version is empty or not parseable (treated as legacy v0.1 file API).
	 */
	static bool TryParseApiVersionString(const FString& VersionString, int32& OutMajor, int32& OutMinor)
	{
		OutMajor = 0;
		OutMinor = 0;

		FString Trimmed = VersionString;
		Trimmed.TrimStartAndEndInline();
		if (Trimmed.IsEmpty())
		{
			return false;
		}

		FString MajorStr;
		FString MinorStr;
		if (!Trimmed.Split(TEXT("."), &MajorStr, &MinorStr, ESearchCase::IgnoreCase, ESearchDir::FromStart))
		{
			if (Trimmed.IsNumeric())
			{
				OutMajor = FCString::Atoi(*Trimmed);
				OutMinor = 0;
				return true;
			}
			return false;
		}

		MajorStr.TrimStartAndEndInline();
		MinorStr.TrimStartAndEndInline();
		if (!MajorStr.IsNumeric())
		{
			return false;
		}

		OutMajor = FCString::Atoi(*MajorStr);
		OutMinor = MinorStr.IsNumeric() ? FCString::Atoi(*MinorStr) : 0;
		return true;
	}

	/**
	 * v0.2+ uses workspace-scoped upload/download paths (no api_id in URL).
	 * Legacy v0.1 embeds api_id in upload and download URLs.
	 */
	bool UsesWorkspaceScopedFileApi() const
	{
		int32 Major = 0;
		int32 Minor = 0;
		if (!TryParseApiVersionString(Version, Major, Minor))
		{
			return false;
		}

		return Major > 0 || (Major == 0 && Minor >= 2);
	}

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
	 * v0.1: {BaseUrl}/{Version}/upload/{ApiId}
	 * v0.2+: {BaseUrl}/{Version}/upload
	 */
	FString GetUploadUrl() const
	{
		if (UsesWorkspaceScopedFileApi())
		{
			return FString::Printf(TEXT("%s/%s/upload"), *GetCleanBaseUrl(), *Version);
		}

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
	 * v0.1: {BaseUrl}/{Version}/download_binary_result/{ApiId}/{FileId}
	 * v0.2+: {BaseUrl}/{Version}/download_binary_result/{FileId}
	 */
	FString GetDownloadUrl(const FString& FileId) const
	{
		if (UsesWorkspaceScopedFileApi())
		{
			return FString::Printf(TEXT("%s/%s/download_binary_result/%s"), *GetCleanBaseUrl(), *Version, *FileId);
		}

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

	/** Validate provided inputs; only explicitly optional inputs may be omitted. */
	bool ValidateInputs(const FAtlasWorkflowInputs& InputValues, TArray<FString>& OutErrors) const
	{
		OutErrors.Empty();
		bool bValid = true;

		for (const auto& Output : Outputs) if (Output.Type==EAtlasValueType::None) { OutErrors.Add(TEXT("Unsupported output type: ")+Output.OriginalType); bValid=false; }
		// Check each schema-defined input exists and has correct type
		for (const FAtlasParameterDef& InputDef : Inputs)
		{
			if (!InputDef.bRequired && !InputValues.Values.Contains(InputDef.Name)) continue;
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
			const FAtlasValue Value = InputDef.GetDefaultValueAsAtlasValue();
			if (Value.Type != EAtlasValueType::None) DefaultInputs.SetValue(InputDef.Name, Value);
		}

		return DefaultInputs;
	}
};
