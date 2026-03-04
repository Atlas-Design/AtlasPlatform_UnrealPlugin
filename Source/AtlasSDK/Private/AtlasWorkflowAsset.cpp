// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasWorkflowAsset.h"
#include "AtlasJobManager.h"
#include "AtlasJob.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "UObject/Package.h"
#include "AssetRegistry/AssetRegistryModule.h"

UAtlasWorkflowAsset::UAtlasWorkflowAsset()
{
	bIsValid = false;
}

// ==================== Parsing ====================

bool UAtlasWorkflowAsset::ParseFromJson()
{
	ParseError.Empty();
	bIsValid = false;
	Schema = FAtlasWorkflowSchema();

	if (JsonConfig.IsEmpty())
	{
		ParseError = TEXT("JSON configuration is empty");
		return false;
	}

	bIsValid = ParseSchemaFromJson(JsonConfig, Schema, ParseError);
	return bIsValid;
}

bool UAtlasWorkflowAsset::ParseSchemaFromJson(const FString& JsonString, FAtlasWorkflowSchema& OutSchema, FString& OutError)
{
	OutSchema = FAtlasWorkflowSchema();
	OutError.Empty();

	if (JsonString.IsEmpty())
	{
		OutError = TEXT("JSON string is empty");
		return false;
	}

	// Parse JSON string
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		OutError = FString::Printf(TEXT("Failed to parse JSON: %s"), *Reader->GetErrorMessage());
		return false;
	}

	return ParseJsonObject(JsonObject, OutSchema, OutError);
}

bool UAtlasWorkflowAsset::ParseJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FAtlasWorkflowSchema& OutSchema, FString& OutError)
{
	// Parse required fields
	if (!JsonObject->TryGetStringField(TEXT("api_id"), OutSchema.ApiId) && 
	    !JsonObject->TryGetStringField(TEXT("apiId"), OutSchema.ApiId) &&
	    !JsonObject->TryGetStringField(TEXT("id"), OutSchema.ApiId))
	{
		OutError = TEXT("Missing required field: api_id (or apiId, id)");
		return false;
	}

	// Parse optional fields
	JsonObject->TryGetStringField(TEXT("name"), OutSchema.Name);
	JsonObject->TryGetStringField(TEXT("description"), OutSchema.Description);
	JsonObject->TryGetStringField(TEXT("version"), OutSchema.Version);
	JsonObject->TryGetStringField(TEXT("base_url"), OutSchema.BaseUrl);
	if (OutSchema.BaseUrl.IsEmpty())
	{
		JsonObject->TryGetStringField(TEXT("baseUrl"), OutSchema.BaseUrl);
	}

	// Parse inputs array
	const TArray<TSharedPtr<FJsonValue>>* InputsArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("inputs"), InputsArray) && InputsArray)
	{
		for (const TSharedPtr<FJsonValue>& InputValue : *InputsArray)
		{
			if (InputValue.IsValid() && InputValue->Type == EJson::Object)
			{
				FAtlasParameterDef ParamDef;
				FString ParamError;
				if (ParseParameterDef(InputValue->AsObject(), ParamDef, ParamError))
				{
					OutSchema.Inputs.Add(ParamDef);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Failed to parse input parameter: %s"), *ParamError);
				}
			}
		}
	}

	// Parse outputs array
	const TArray<TSharedPtr<FJsonValue>>* OutputsArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("outputs"), OutputsArray) && OutputsArray)
	{
		for (const TSharedPtr<FJsonValue>& OutputValue : *OutputsArray)
		{
			if (OutputValue.IsValid() && OutputValue->Type == EJson::Object)
			{
				FAtlasParameterDef ParamDef;
				FString ParamError;
				if (ParseParameterDef(OutputValue->AsObject(), ParamDef, ParamError))
				{
					OutSchema.Outputs.Add(ParamDef);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Failed to parse output parameter: %s"), *ParamError);
				}
			}
		}
	}

	return true;
}

bool UAtlasWorkflowAsset::ParseParameterDef(const TSharedPtr<FJsonObject>& JsonObject, FAtlasParameterDef& OutDef, FString& OutError)
{
	if (!JsonObject.IsValid())
	{
		OutError = TEXT("Invalid JSON object for parameter");
		return false;
	}

	// Required: name (try multiple field names: "name", "id")
	if (!JsonObject->TryGetStringField(TEXT("name"), OutDef.Name))
	{
		if (!JsonObject->TryGetStringField(TEXT("id"), OutDef.Name))
		{
			OutError = TEXT("Parameter missing required 'name' or 'id' field");
			return false;
		}
	}

	// Optional fields - display name (try multiple field names)
	JsonObject->TryGetStringField(TEXT("display_name"), OutDef.DisplayName);
	if (OutDef.DisplayName.IsEmpty())
	{
		JsonObject->TryGetStringField(TEXT("displayName"), OutDef.DisplayName);
	}
	if (OutDef.DisplayName.IsEmpty())
	{
		JsonObject->TryGetStringField(TEXT("label"), OutDef.DisplayName);
	}

	JsonObject->TryGetStringField(TEXT("description"), OutDef.Description);

	// Type (default to string) - parse this first so we know how to handle default_value
	FString TypeString;
	if (JsonObject->TryGetStringField(TEXT("type"), TypeString))
	{
		OutDef.Type = StringToValueType(TypeString);
	}

	// Default value - handle multiple field names and non-string types
	// Try "default", "defaultValue", "default_value"
	const TSharedPtr<FJsonValue>* DefaultField = nullptr;
	if (JsonObject->Values.Contains(TEXT("default_value")))
	{
		DefaultField = JsonObject->Values.Find(TEXT("default_value"));
	}
	else if (JsonObject->Values.Contains(TEXT("default")))
	{
		DefaultField = JsonObject->Values.Find(TEXT("default"));
	}
	else if (JsonObject->Values.Contains(TEXT("defaultValue")))
	{
		DefaultField = JsonObject->Values.Find(TEXT("defaultValue"));
	}

	if (DefaultField && DefaultField->IsValid())
	{
		const TSharedPtr<FJsonValue>& DefaultValue = *DefaultField;
		switch (DefaultValue->Type)
		{
		case EJson::String:
			OutDef.DefaultValue = DefaultValue->AsString();
			break;
		case EJson::Number:
			OutDef.DefaultValue = FString::SanitizeFloat(DefaultValue->AsNumber());
			break;
		case EJson::Boolean:
			OutDef.DefaultValue = DefaultValue->AsBool() ? TEXT("true") : TEXT("false");
			break;
		case EJson::Null:
			OutDef.DefaultValue.Empty();
			break;
		default:
			// For objects/arrays, try to serialize back to string
			OutDef.DefaultValue.Empty();
			break;
		}
	}

	// Numeric constraints
	double MinVal, MaxVal;
	if (JsonObject->TryGetNumberField(TEXT("min"), MinVal))
	{
		OutDef.MinValue = (float)MinVal;
		OutDef.bHasRange = true;
	}
	if (JsonObject->TryGetNumberField(TEXT("max"), MaxVal))
	{
		OutDef.MaxValue = (float)MaxVal;
		OutDef.bHasRange = true;
	}

	// Options for enum/dropdown
	const TArray<TSharedPtr<FJsonValue>>* OptionsArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("options"), OptionsArray) && OptionsArray)
	{
		for (const TSharedPtr<FJsonValue>& OptionValue : *OptionsArray)
		{
			if (OptionValue.IsValid() && OptionValue->Type == EJson::String)
			{
				OutDef.Options.Add(OptionValue->AsString());
			}
		}
	}

	// Allowed extensions for file types - try "format" field first (single value)
	FString FormatStr;
	if (JsonObject->TryGetStringField(TEXT("format"), FormatStr))
	{
		OutDef.AllowedExtensions.Add(FormatStr);
	}

	// Also try array fields for extensions
	const TArray<TSharedPtr<FJsonValue>>* ExtensionsArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("extensions"), ExtensionsArray) && ExtensionsArray)
	{
		for (const TSharedPtr<FJsonValue>& ExtValue : *ExtensionsArray)
		{
			if (ExtValue.IsValid() && ExtValue->Type == EJson::String)
			{
				OutDef.AllowedExtensions.Add(ExtValue->AsString());
			}
		}
	}
	// Also try "allowed_extensions"
	if (JsonObject->TryGetArrayField(TEXT("allowed_extensions"), ExtensionsArray) && ExtensionsArray)
	{
		for (const TSharedPtr<FJsonValue>& ExtValue : *ExtensionsArray)
		{
			if (ExtValue.IsValid() && ExtValue->Type == EJson::String)
			{
				OutDef.AllowedExtensions.AddUnique(ExtValue->AsString());
			}
		}
	}

	return true;
}

EAtlasValueType UAtlasWorkflowAsset::StringToValueType(const FString& TypeString)
{
	FString LowerType = TypeString.ToLower();

	if (LowerType == TEXT("string") || LowerType == TEXT("text"))
	{
		return EAtlasValueType::String;
	}
	if (LowerType == TEXT("number") || LowerType == TEXT("float") || LowerType == TEXT("double"))
	{
		return EAtlasValueType::Number;
	}
	if (LowerType == TEXT("integer") || LowerType == TEXT("int"))
	{
		return EAtlasValueType::Integer;
	}
	if (LowerType == TEXT("boolean") || LowerType == TEXT("bool"))
	{
		return EAtlasValueType::Boolean;
	}
	if (LowerType == TEXT("file"))
	{
		return EAtlasValueType::File;
	}
	if (LowerType == TEXT("file_id") || LowerType == TEXT("fileid"))
	{
		return EAtlasValueType::FileId;
	}
	if (LowerType == TEXT("image") || LowerType == TEXT("img") || LowerType == TEXT("picture"))
	{
		return EAtlasValueType::Image;
	}
	if (LowerType == TEXT("mesh") || LowerType == TEXT("3d") || LowerType == TEXT("model"))
	{
		return EAtlasValueType::Mesh;
	}
	if (LowerType == TEXT("json") || LowerType == TEXT("object") || LowerType == TEXT("array"))
	{
		return EAtlasValueType::Json;
	}

	// Default to string
	return EAtlasValueType::String;
}

// ==================== Accessors ====================

FString UAtlasWorkflowAsset::GetApiId() const
{
	return Schema.ApiId;
}

FString UAtlasWorkflowAsset::GetBaseUrl() const
{
	return Schema.BaseUrl;
}

FString UAtlasWorkflowAsset::GetVersion() const
{
	return Schema.Version;
}

FString UAtlasWorkflowAsset::GetWorkflowName() const
{
	return Schema.GetDisplayName();
}

FString UAtlasWorkflowAsset::GetDescription() const
{
	return Schema.Description;
}

// ==================== URL Builders ====================

FString UAtlasWorkflowAsset::GetUploadUrl() const
{
	return Schema.GetUploadUrl();
}

FString UAtlasWorkflowAsset::GetExecuteUrl() const
{
	return Schema.GetExecuteUrl();
}

FString UAtlasWorkflowAsset::GetStatusUrl(const FString& ExecutionId) const
{
	return Schema.GetStatusUrl(ExecutionId);
}

FString UAtlasWorkflowAsset::GetDownloadUrl(const FString& FileId) const
{
	return Schema.GetDownloadUrl(FileId);
}

bool UAtlasWorkflowAsset::IsValid() const
{
	return bIsValid && Schema.IsValid();
}

const FAtlasWorkflowSchema& UAtlasWorkflowAsset::GetSchema() const
{
	return Schema;
}

TArray<FAtlasParameterDef> UAtlasWorkflowAsset::GetInputDefinitions() const
{
	return Schema.Inputs;
}

TArray<FAtlasParameterDef> UAtlasWorkflowAsset::GetOutputDefinitions() const
{
	return Schema.Outputs;
}

FAtlasWorkflowInputs UAtlasWorkflowAsset::CreateDefaultInputs() const
{
	return Schema.CreateDefaultInputs();
}

bool UAtlasWorkflowAsset::ValidateInputs(const FAtlasWorkflowInputs& Inputs, TArray<FString>& OutErrors) const
{
	return Schema.ValidateInputs(Inputs, OutErrors);
}

// ==================== Job Accessors ====================

TArray<UAtlasJob*> UAtlasWorkflowAsset::GetActiveJobs(UAtlasJobManager* JobManager) const
{
	if (!JobManager || !::IsValid(JobManager))
	{
		return TArray<UAtlasJob*>();
	}

	return JobManager->GetJobsForWorkflow(Schema.ApiId);
}

TArray<FAtlasJobHistoryRecord> UAtlasWorkflowAsset::GetHistory(UAtlasJobManager* JobManager) const
{
	if (!JobManager || !::IsValid(JobManager))
	{
		return TArray<FAtlasJobHistoryRecord>();
	}

	return JobManager->GetHistoryForWorkflow(Schema.ApiId);
}

// ==================== Import/Export ====================

UAtlasWorkflowAsset* UAtlasWorkflowAsset::ImportFromFile(const FString& FilePath, const FString& PackagePath, const FString& AssetName, FString& OutError)
{
	OutError.Empty();

	// Read file contents
	FString JsonContents;
	if (!FFileHelper::LoadFileToString(JsonContents, *FilePath))
	{
		OutError = FString::Printf(TEXT("Failed to read file: %s"), *FilePath);
		return nullptr;
	}

	// Validate JSON can be parsed
	FAtlasWorkflowSchema TestSchema;
	FString ParseErr;
	if (!ParseSchemaFromJson(JsonContents, TestSchema, ParseErr))
	{
		OutError = FString::Printf(TEXT("Invalid JSON: %s"), *ParseErr);
		return nullptr;
	}

	// Check if a workflow with this API ID already exists
	if (!TestSchema.ApiId.IsEmpty())
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		
		TArray<FAssetData> AllWorkflowAssets;
		AssetRegistry.GetAssetsByClass(UAtlasWorkflowAsset::StaticClass()->GetClassPathName(), AllWorkflowAssets);
		
		for (const FAssetData& AssetData : AllWorkflowAssets)
		{
			if (UAtlasWorkflowAsset* ExistingAsset = Cast<UAtlasWorkflowAsset>(AssetData.GetAsset()))
			{
				if (ExistingAsset->GetApiId() == TestSchema.ApiId)
				{
					OutError = FString::Printf(TEXT("A workflow with API ID '%s' already exists: %s"), 
						*TestSchema.ApiId, *AssetData.GetObjectPathString());
					return nullptr;
				}
			}
		}
	}

	// Create package
	FString FullPackagePath = PackagePath / AssetName;
	UPackage* Package = CreatePackage(*FullPackagePath);
	if (!Package)
	{
		OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPackagePath);
		return nullptr;
	}

	// Create asset
	UAtlasWorkflowAsset* NewAsset = NewObject<UAtlasWorkflowAsset>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!NewAsset)
	{
		OutError = TEXT("Failed to create asset object");
		return nullptr;
	}

	// Set JSON and parse
	NewAsset->JsonConfig = JsonContents;
	NewAsset->ParseFromJson();

	// Mark package dirty
	Package->MarkPackageDirty();

	// Notify asset registry
	FAssetRegistryModule::AssetCreated(NewAsset);

	return NewAsset;
}

bool UAtlasWorkflowAsset::ExportToFile(const FString& FilePath) const
{
	if (JsonConfig.IsEmpty())
	{
		return false;
	}

	return FFileHelper::SaveStringToFile(JsonConfig, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

#if WITH_EDITOR
void UAtlasWorkflowAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Auto-parse when JsonConfig changes
	FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAtlasWorkflowAsset, JsonConfig))
	{
		ParseFromJson();
	}
}
#endif
