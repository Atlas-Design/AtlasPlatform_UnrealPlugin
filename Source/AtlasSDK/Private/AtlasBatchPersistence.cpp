// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasBatchPersistence.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

// ==================== Path Helpers ====================

FString UAtlasBatchPersistence::GetDraftsDirectory()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Atlas"), TEXT("Batches"), TEXT("Drafts"));
}

FString UAtlasBatchPersistence::GetRunsDirectory()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Atlas"), TEXT("Batches"), TEXT("Runs"));
}

FString UAtlasBatchPersistence::GetDraftFilePath(const FString& WorkflowApiId, const FString& DraftName)
{
	FString SafeApiId = SanitizeFileName(WorkflowApiId);
	FString SafeName = SanitizeFileName(DraftName);
	return FPaths::Combine(GetDraftsDirectory(), FString::Printf(TEXT("%s_%s.json"), *SafeApiId, *SafeName));
}

FString UAtlasBatchPersistence::GetManifestFilePath(const FString& BatchId)
{
	return FPaths::Combine(GetRunsDirectory(), FString::Printf(TEXT("%s_manifest.json"), *SanitizeFileName(BatchId)));
}

FString UAtlasBatchPersistence::SanitizeFileName(const FString& Input)
{
	FString Safe = Input;
	Safe.ReplaceInline(TEXT("/"), TEXT("_"));
	Safe.ReplaceInline(TEXT("\\"), TEXT("_"));
	Safe.ReplaceInline(TEXT(":"), TEXT("_"));
	Safe.ReplaceInline(TEXT("*"), TEXT("_"));
	Safe.ReplaceInline(TEXT("?"), TEXT("_"));
	Safe.ReplaceInline(TEXT("\""), TEXT("_"));
	Safe.ReplaceInline(TEXT("<"), TEXT("_"));
	Safe.ReplaceInline(TEXT(">"), TEXT("_"));
	Safe.ReplaceInline(TEXT("|"), TEXT("_"));
	return Safe;
}

// ==================== Status Conversion ====================

FString UAtlasBatchPersistence::RowStatusToString(EAtlasBatchRowStatus Status)
{
	switch (Status)
	{
	case EAtlasBatchRowStatus::Pending:   return TEXT("Pending");
	case EAtlasBatchRowStatus::Running:   return TEXT("Running");
	case EAtlasBatchRowStatus::Succeeded: return TEXT("Succeeded");
	case EAtlasBatchRowStatus::Failed:    return TEXT("Failed");
	case EAtlasBatchRowStatus::Cancelled: return TEXT("Cancelled");
	default:                              return TEXT("Unknown");
	}
}

EAtlasBatchRowStatus UAtlasBatchPersistence::StringToRowStatus(const FString& Str)
{
	if (Str == TEXT("Pending"))   return EAtlasBatchRowStatus::Pending;
	if (Str == TEXT("Running"))   return EAtlasBatchRowStatus::Running;
	if (Str == TEXT("Succeeded")) return EAtlasBatchRowStatus::Succeeded;
	if (Str == TEXT("Failed"))    return EAtlasBatchRowStatus::Failed;
	if (Str == TEXT("Cancelled")) return EAtlasBatchRowStatus::Cancelled;
	return EAtlasBatchRowStatus::Pending;
}

// ==================== Drafts ====================

bool UAtlasBatchPersistence::SaveDraft(const FString& WorkflowApiId, const FString& DraftName, const FAtlasBatchDefinition& Batch)
{
	if (WorkflowApiId.IsEmpty() || DraftName.IsEmpty())
	{
		return false;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString DraftsDir = GetDraftsDirectory();
	if (!PlatformFile.DirectoryExists(*DraftsDir))
	{
		PlatformFile.CreateDirectoryTree(*DraftsDir);
	}

	TSharedPtr<FJsonObject> RootJson = MakeShared<FJsonObject>();
	RootJson->SetStringField(TEXT("workflowApiId"), WorkflowApiId);
	RootJson->SetStringField(TEXT("draftName"), DraftName);
	RootJson->SetStringField(TEXT("savedAt"), FDateTime::Now().ToIso8601());
	RootJson->SetObjectField(TEXT("batch"), BatchDefToJson(Batch));

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	if (!FJsonSerializer::Serialize(RootJson.ToSharedRef(), Writer))
	{
		return false;
	}

	FString FilePath = GetDraftFilePath(WorkflowApiId, DraftName);
	bool bSaved = FFileHelper::SaveStringToFile(JsonString, *FilePath);

	if (bSaved)
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasBatchPersistence: Saved draft '%s' for %s (%d rows)"),
			*DraftName, *WorkflowApiId, Batch.Rows.Num());
	}

	return bSaved;
}

bool UAtlasBatchPersistence::LoadDraft(const FString& WorkflowApiId, const FString& DraftName, FAtlasBatchDefinition& OutBatch)
{
	FString FilePath = GetDraftFilePath(WorkflowApiId, DraftName);

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootJson;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootJson) || !RootJson.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* BatchJson;
	if (!RootJson->TryGetObjectField(TEXT("batch"), BatchJson))
	{
		return false;
	}

	return JsonToBatchDef(*BatchJson, OutBatch);
}

TArray<FAtlasBatchDraftInfo> UAtlasBatchPersistence::ListDrafts(const FString& WorkflowApiId)
{
	TArray<FAtlasBatchDraftInfo> Results;

	FString DraftsDir = GetDraftsDirectory();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	TArray<FString> Files;
	PlatformFile.FindFiles(Files, *DraftsDir, TEXT("json"));

	for (const FString& FilePath : Files)
	{
		FString JsonString;
		if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
		{
			continue;
		}

		TSharedPtr<FJsonObject> RootJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, RootJson) || !RootJson.IsValid())
		{
			continue;
		}

		FString FileApiId;
		RootJson->TryGetStringField(TEXT("workflowApiId"), FileApiId);

		if (!WorkflowApiId.IsEmpty() && FileApiId != WorkflowApiId)
		{
			continue;
		}

		FAtlasBatchDraftInfo Info;
		Info.WorkflowApiId = FileApiId;
		RootJson->TryGetStringField(TEXT("draftName"), Info.DraftName);
		Info.FilePath = FilePath;

		FString SavedAtStr;
		if (RootJson->TryGetStringField(TEXT("savedAt"), SavedAtStr))
		{
			FDateTime::ParseIso8601(*SavedAtStr, Info.SavedAt);
		}

		const TSharedPtr<FJsonObject>* BatchJson;
		if (RootJson->TryGetObjectField(TEXT("batch"), BatchJson))
		{
			const TArray<TSharedPtr<FJsonValue>>* RowsArray;
			if ((*BatchJson)->TryGetArrayField(TEXT("rows"), RowsArray))
			{
				Info.RowCount = RowsArray->Num();
			}
		}

		Results.Add(Info);
	}

	return Results;
}

bool UAtlasBatchPersistence::DeleteDraft(const FString& WorkflowApiId, const FString& DraftName)
{
	FString FilePath = GetDraftFilePath(WorkflowApiId, DraftName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	return PlatformFile.DeleteFile(*FilePath);
}

// ==================== Manifests ====================

bool UAtlasBatchPersistence::SaveManifest(const FAtlasBatchManifest& Manifest)
{
	if (!Manifest.IsValid())
	{
		return false;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString RunsDir = GetRunsDirectory();
	if (!PlatformFile.DirectoryExists(*RunsDir))
	{
		PlatformFile.CreateDirectoryTree(*RunsDir);
	}

	TSharedPtr<FJsonObject> Json = ManifestToJson(Manifest);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	if (!FJsonSerializer::Serialize(Json.ToSharedRef(), Writer))
	{
		return false;
	}

	FString FilePath = GetManifestFilePath(Manifest.BatchId);
	return FFileHelper::SaveStringToFile(JsonString, *FilePath);
}

bool UAtlasBatchPersistence::LoadManifest(const FString& BatchId, FAtlasBatchManifest& OutManifest)
{
	FString FilePath = GetManifestFilePath(BatchId);

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
	{
		return false;
	}

	return JsonToManifest(Json, OutManifest);
}

bool UAtlasBatchPersistence::UpdateManifestRow(const FString& BatchId, int32 RowIndex, const FGuid& NewJobId, EAtlasBatchRowStatus NewStatus, const FString& ErrorMessage, const FString& JobFolderPath, const FString& JobJsonPath)
{
	FAtlasBatchManifest Manifest;
	if (!LoadManifest(BatchId, Manifest))
	{
		return false;
	}

	for (FAtlasBatchManifestRow& Row : Manifest.Rows)
	{
		if (Row.RowIndex == RowIndex)
		{
			const bool bJobChanged = Row.JobId != NewJobId;
			Row.JobId = NewJobId;
			Row.Status = NewStatus;
			Row.ErrorMessage = ErrorMessage;
			if (!JobFolderPath.IsEmpty())
			{
				Row.JobFolderPath = JobFolderPath;
			}
			else if (bJobChanged)
			{
				Row.JobFolderPath.Empty();
			}
			if (!JobJsonPath.IsEmpty())
			{
				Row.JobJsonPath = JobJsonPath;
			}
			else if (bJobChanged)
			{
				Row.JobJsonPath.Empty();
			}
			return SaveManifest(Manifest);
		}
	}

	return false;
}

TArray<FAtlasBatchManifest> UAtlasBatchPersistence::ListManifests()
{
	TArray<FAtlasBatchManifest> Results;

	FString RunsDir = GetRunsDirectory();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	TArray<FString> Files;
	PlatformFile.FindFiles(Files, *RunsDir, TEXT("json"));

	for (const FString& FilePath : Files)
	{
		FString JsonString;
		if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Json;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
		{
			continue;
		}

		FAtlasBatchManifest Manifest;
		if (JsonToManifest(Json, Manifest))
		{
			Results.Add(Manifest);
		}
	}

	return Results;
}

// ==================== JSON Serialization — BatchDefinition ====================

TSharedPtr<FJsonObject> UAtlasBatchPersistence::BatchDefToJson(const FAtlasBatchDefinition& Batch)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("batchName"), Batch.BatchName);
	Json->SetNumberField(TEXT("maxConcurrentJobs"), Batch.MaxConcurrentJobs);
	Json->SetNumberField(TEXT("maxRetriesPerRow"), Batch.MaxRetriesPerRow);

	TArray<TSharedPtr<FJsonValue>> RowsArray;
	for (const FAtlasBatchRow& Row : Batch.Rows)
	{
		TSharedPtr<FJsonObject> RowJson = MakeShared<FJsonObject>();
		RowJson->SetNumberField(TEXT("rowIndex"), Row.RowIndex);

		TSharedPtr<FJsonObject> ValuesJson = MakeShared<FJsonObject>();
		for (const auto& Pair : Row.Values)
		{
			TSharedPtr<FJsonObject> ValueJson = MakeShared<FJsonObject>();
			ValueJson->SetStringField(TEXT("type"), UEnum::GetValueAsString(Pair.Value.Type));

			switch (Pair.Value.Type)
			{
			case EAtlasValueType::String:
				ValueJson->SetStringField(TEXT("value"), Pair.Value.StringValue);
				break;
			case EAtlasValueType::Number:
				ValueJson->SetNumberField(TEXT("value"), Pair.Value.NumberValue);
				break;
			case EAtlasValueType::Integer:
				ValueJson->SetNumberField(TEXT("value"), Pair.Value.IntValue);
				break;
			case EAtlasValueType::Boolean:
				ValueJson->SetBoolField(TEXT("value"), Pair.Value.BoolValue);
				break;
			case EAtlasValueType::Image:
			case EAtlasValueType::Mesh:
			case EAtlasValueType::Audio:
			case EAtlasValueType::File:
				ValueJson->SetStringField(TEXT("filePath"), Pair.Value.FilePath);
				ValueJson->SetStringField(TEXT("fileId"), Pair.Value.FileId);
				break;
			case EAtlasValueType::FileId:
				ValueJson->SetStringField(TEXT("fileId"), Pair.Value.FileId);
				break;
			case EAtlasValueType::Json:
				ValueJson->SetStringField(TEXT("value"), Pair.Value.JsonValue);
				break;
			default:
				break;
			}

			ValuesJson->SetObjectField(Pair.Key, ValueJson);
		}

		RowJson->SetObjectField(TEXT("values"), ValuesJson);
		RowsArray.Add(MakeShared<FJsonValueObject>(RowJson));
	}

	Json->SetArrayField(TEXT("rows"), RowsArray);
	return Json;
}

bool UAtlasBatchPersistence::JsonToBatchDef(const TSharedPtr<FJsonObject>& Json, FAtlasBatchDefinition& OutBatch)
{
	if (!Json.IsValid())
	{
		return false;
	}

	Json->TryGetStringField(TEXT("batchName"), OutBatch.BatchName);

	double TempNum;
	if (Json->TryGetNumberField(TEXT("maxConcurrentJobs"), TempNum))
	{
		OutBatch.MaxConcurrentJobs = FMath::RoundToInt(TempNum);
	}
	if (Json->TryGetNumberField(TEXT("maxRetriesPerRow"), TempNum))
	{
		OutBatch.MaxRetriesPerRow = FMath::RoundToInt(TempNum);
	}

	const TArray<TSharedPtr<FJsonValue>>* RowsArray;
	if (!Json->TryGetArrayField(TEXT("rows"), RowsArray))
	{
		return true; // Valid but empty
	}

	for (const TSharedPtr<FJsonValue>& RowValue : *RowsArray)
	{
		const TSharedPtr<FJsonObject>* RowJson;
		if (!RowValue->TryGetObject(RowJson))
		{
			continue;
		}

		FAtlasBatchRow Row;
		if ((*RowJson)->TryGetNumberField(TEXT("rowIndex"), TempNum))
		{
			Row.RowIndex = FMath::RoundToInt(TempNum);
		}

		const TSharedPtr<FJsonObject>* ValuesJson;
		if ((*RowJson)->TryGetObjectField(TEXT("values"), ValuesJson))
		{
			for (const auto& Pair : (*ValuesJson)->Values)
			{
				const TSharedPtr<FJsonObject>* ValueJson;
				if (!Pair.Value->TryGetObject(ValueJson))
				{
					continue;
				}

				FAtlasValue Value;
				FString TypeStr;
				if ((*ValueJson)->TryGetStringField(TEXT("type"), TypeStr))
				{
					if (TypeStr.Contains(TEXT("String")))       Value.Type = EAtlasValueType::String;
					else if (TypeStr.Contains(TEXT("Number")))  Value.Type = EAtlasValueType::Number;
					else if (TypeStr.Contains(TEXT("Integer"))) Value.Type = EAtlasValueType::Integer;
					else if (TypeStr.Contains(TEXT("Boolean"))) Value.Type = EAtlasValueType::Boolean;
					else if (TypeStr.Contains(TEXT("Image")))   Value.Type = EAtlasValueType::Image;
					else if (TypeStr.Contains(TEXT("Mesh")))    Value.Type = EAtlasValueType::Mesh;
					else if (TypeStr.Contains(TEXT("Audio")))   Value.Type = EAtlasValueType::Audio;
					else if (TypeStr.Contains(TEXT("FileId")))  Value.Type = EAtlasValueType::FileId;
					else if (TypeStr.Contains(TEXT("File")))    Value.Type = EAtlasValueType::File;
					else if (TypeStr.Contains(TEXT("Json")))    Value.Type = EAtlasValueType::Json;
				}

				switch (Value.Type)
				{
				case EAtlasValueType::String:
					(*ValueJson)->TryGetStringField(TEXT("value"), Value.StringValue);
					break;
				case EAtlasValueType::Number:
					(*ValueJson)->TryGetNumberField(TEXT("value"), Value.NumberValue);
					break;
				case EAtlasValueType::Integer:
					{
						double Temp;
						if ((*ValueJson)->TryGetNumberField(TEXT("value"), Temp))
							Value.IntValue = FMath::RoundToInt(Temp);
					}
					break;
				case EAtlasValueType::Boolean:
					(*ValueJson)->TryGetBoolField(TEXT("value"), Value.BoolValue);
					break;
				case EAtlasValueType::Image:
				case EAtlasValueType::Mesh:
				case EAtlasValueType::Audio:
				case EAtlasValueType::File:
					(*ValueJson)->TryGetStringField(TEXT("filePath"), Value.FilePath);
					(*ValueJson)->TryGetStringField(TEXT("fileId"), Value.FileId);
					break;
				case EAtlasValueType::FileId:
					(*ValueJson)->TryGetStringField(TEXT("fileId"), Value.FileId);
					break;
				case EAtlasValueType::Json:
					(*ValueJson)->TryGetStringField(TEXT("value"), Value.JsonValue);
					break;
				default:
					break;
				}

				Row.Values.Add(Pair.Key, Value);
			}
		}

		OutBatch.Rows.Add(Row);
	}

	return true;
}

// ==================== JSON Serialization — Manifest ====================

TSharedPtr<FJsonObject> UAtlasBatchPersistence::ManifestToJson(const FAtlasBatchManifest& Manifest)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("batchId"), Manifest.BatchId);
	Json->SetStringField(TEXT("batchName"), Manifest.BatchName);
	Json->SetStringField(TEXT("workflowApiId"), Manifest.WorkflowApiId);
	Json->SetStringField(TEXT("workflowName"), Manifest.WorkflowName);
	Json->SetStringField(TEXT("startedAt"), Manifest.StartedAt.ToIso8601());

	if (Manifest.CompletedAt.GetTicks() > 0)
	{
		Json->SetStringField(TEXT("completedAt"), Manifest.CompletedAt.ToIso8601());
	}

	Json->SetNumberField(TEXT("totalRows"), Manifest.TotalRows);

	TArray<TSharedPtr<FJsonValue>> RowsArray;
	for (const FAtlasBatchManifestRow& Row : Manifest.Rows)
	{
		TSharedPtr<FJsonObject> RowJson = MakeShared<FJsonObject>();
		RowJson->SetNumberField(TEXT("rowIndex"), Row.RowIndex);
		RowJson->SetStringField(TEXT("jobId"), Row.JobId.ToString(EGuidFormats::DigitsWithHyphens));
		RowJson->SetStringField(TEXT("status"), RowStatusToString(Row.Status));
		if (!Row.JobFolderPath.IsEmpty())
		{
			RowJson->SetStringField(TEXT("jobFolderPath"), Row.JobFolderPath);
		}
		if (!Row.JobJsonPath.IsEmpty())
		{
			RowJson->SetStringField(TEXT("jobJsonPath"), Row.JobJsonPath);
		}
		if (!Row.ErrorMessage.IsEmpty())
		{
			RowJson->SetStringField(TEXT("errorMessage"), Row.ErrorMessage);
		}
		RowsArray.Add(MakeShared<FJsonValueObject>(RowJson));
	}

	Json->SetArrayField(TEXT("rows"), RowsArray);
	return Json;
}

bool UAtlasBatchPersistence::JsonToManifest(const TSharedPtr<FJsonObject>& Json, FAtlasBatchManifest& OutManifest)
{
	if (!Json.IsValid())
	{
		return false;
	}

	Json->TryGetStringField(TEXT("batchId"), OutManifest.BatchId);
	Json->TryGetStringField(TEXT("batchName"), OutManifest.BatchName);
	Json->TryGetStringField(TEXT("workflowApiId"), OutManifest.WorkflowApiId);
	Json->TryGetStringField(TEXT("workflowName"), OutManifest.WorkflowName);

	FString TimeStr;
	if (Json->TryGetStringField(TEXT("startedAt"), TimeStr))
	{
		FDateTime::ParseIso8601(*TimeStr, OutManifest.StartedAt);
	}
	if (Json->TryGetStringField(TEXT("completedAt"), TimeStr))
	{
		FDateTime::ParseIso8601(*TimeStr, OutManifest.CompletedAt);
	}

	double TempNum;
	if (Json->TryGetNumberField(TEXT("totalRows"), TempNum))
	{
		OutManifest.TotalRows = FMath::RoundToInt(TempNum);
	}

	const TArray<TSharedPtr<FJsonValue>>* RowsArray;
	if (Json->TryGetArrayField(TEXT("rows"), RowsArray))
	{
		for (const TSharedPtr<FJsonValue>& RowValue : *RowsArray)
		{
			const TSharedPtr<FJsonObject>* RowJson;
			if (!RowValue->TryGetObject(RowJson))
			{
				continue;
			}

			FAtlasBatchManifestRow Row;
			if ((*RowJson)->TryGetNumberField(TEXT("rowIndex"), TempNum))
			{
				Row.RowIndex = FMath::RoundToInt(TempNum);
			}

			FString JobIdStr;
			if ((*RowJson)->TryGetStringField(TEXT("jobId"), JobIdStr))
			{
				FGuid::Parse(JobIdStr, Row.JobId);
			}

			FString StatusStr;
			if ((*RowJson)->TryGetStringField(TEXT("status"), StatusStr))
			{
				Row.Status = StringToRowStatus(StatusStr);
			}

			(*RowJson)->TryGetStringField(TEXT("errorMessage"), Row.ErrorMessage);
			(*RowJson)->TryGetStringField(TEXT("jobFolderPath"), Row.JobFolderPath);
			(*RowJson)->TryGetStringField(TEXT("jobJsonPath"), Row.JobJsonPath);

			OutManifest.Rows.Add(Row);
		}
	}

	return OutManifest.IsValid();
}
