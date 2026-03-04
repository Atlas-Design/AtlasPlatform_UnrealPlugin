// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasSDKLibrary.h"

// ==================== FAtlasValue Creation ====================

FAtlasValue UAtlasSDKLibrary::MakeStringValue(const FString& Value)
{
	return FAtlasValue::MakeString(Value);
}

FAtlasValue UAtlasSDKLibrary::MakeNumberValue(float Value)
{
	return FAtlasValue::MakeNumber(Value);
}

FAtlasValue UAtlasSDKLibrary::MakeIntegerValue(int32 Value)
{
	return FAtlasValue::MakeInteger(Value);
}

FAtlasValue UAtlasSDKLibrary::MakeBoolValue(bool Value)
{
	return FAtlasValue::MakeBool(Value);
}

FAtlasValue UAtlasSDKLibrary::MakeFileValue(const FString& FilePath)
{
	return FAtlasValue::MakeFile(FilePath);
}

FAtlasValue UAtlasSDKLibrary::MakeFileIdValue(const FString& FileId)
{
	return FAtlasValue::MakeFileId(FileId);
}

FAtlasValue UAtlasSDKLibrary::MakeImageValue(const FString& FilePath)
{
	return FAtlasValue::MakeImage(FilePath);
}

FAtlasValue UAtlasSDKLibrary::MakeMeshValue(const FString& FilePath)
{
	return FAtlasValue::MakeMesh(FilePath);
}

FAtlasValue UAtlasSDKLibrary::MakeJsonValue(const FString& JsonString)
{
	return FAtlasValue::MakeJson(JsonString);
}

// ==================== FAtlasValue Getters ====================

EAtlasValueType UAtlasSDKLibrary::GetValueType(const FAtlasValue& Value)
{
	return Value.Type;
}

bool UAtlasSDKLibrary::IsValueValid(const FAtlasValue& Value)
{
	return Value.IsValid();
}

FString UAtlasSDKLibrary::GetStringFromValue(const FAtlasValue& Value)
{
	return Value.GetString();
}

float UAtlasSDKLibrary::GetNumberFromValue(const FAtlasValue& Value)
{
	return Value.GetNumber();
}

int32 UAtlasSDKLibrary::GetIntegerFromValue(const FAtlasValue& Value)
{
	return Value.GetInteger();
}

bool UAtlasSDKLibrary::GetBoolFromValue(const FAtlasValue& Value)
{
	return Value.GetBool();
}

FString UAtlasSDKLibrary::GetFileIdFromValue(const FAtlasValue& Value)
{
	return Value.GetFileId();
}

FString UAtlasSDKLibrary::GetFilePathFromValue(const FAtlasValue& Value)
{
	return Value.GetFilePath();
}

bool UAtlasSDKLibrary::IsFileType(const FAtlasValue& Value)
{
	return Value.IsFileType();
}

bool UAtlasSDKLibrary::HasFilePath(const FAtlasValue& Value)
{
	return Value.HasFilePath();
}

bool UAtlasSDKLibrary::HasFileData(const FAtlasValue& Value)
{
	return Value.HasFileData();
}

FString UAtlasSDKLibrary::ValueToString(const FAtlasValue& Value)
{
	return Value.ToString();
}

// ==================== FAtlasWorkflowInputs ====================

void UAtlasSDKLibrary::SetStringInput(FAtlasWorkflowInputs& Inputs, const FString& Name, const FString& Value)
{
	Inputs.SetString(Name, Value);
}

void UAtlasSDKLibrary::SetNumberInput(FAtlasWorkflowInputs& Inputs, const FString& Name, float Value)
{
	Inputs.SetNumber(Name, Value);
}

void UAtlasSDKLibrary::SetIntegerInput(FAtlasWorkflowInputs& Inputs, const FString& Name, int32 Value)
{
	Inputs.SetInteger(Name, Value);
}

void UAtlasSDKLibrary::SetBoolInput(FAtlasWorkflowInputs& Inputs, const FString& Name, bool Value)
{
	Inputs.SetBool(Name, Value);
}

void UAtlasSDKLibrary::SetFileInput(FAtlasWorkflowInputs& Inputs, const FString& Name, const FString& FilePath)
{
	Inputs.SetFile(Name, FilePath);
}

void UAtlasSDKLibrary::SetFileIdInput(FAtlasWorkflowInputs& Inputs, const FString& Name, const FString& FileId)
{
	Inputs.SetFileId(Name, FileId);
}

void UAtlasSDKLibrary::SetImageInput(FAtlasWorkflowInputs& Inputs, const FString& Name, const FString& FilePath)
{
	Inputs.SetImage(Name, FilePath);
}

void UAtlasSDKLibrary::SetMeshInput(FAtlasWorkflowInputs& Inputs, const FString& Name, const FString& FilePath)
{
	Inputs.SetMesh(Name, FilePath);
}

void UAtlasSDKLibrary::SetJsonInput(FAtlasWorkflowInputs& Inputs, const FString& Name, const FString& JsonString)
{
	Inputs.SetJson(Name, JsonString);
}

void UAtlasSDKLibrary::SetValueInput(FAtlasWorkflowInputs& Inputs, const FString& Name, const FAtlasValue& Value)
{
	Inputs.SetValue(Name, Value);
}

TArray<FString> UAtlasSDKLibrary::GetInputNames(const FAtlasWorkflowInputs& Inputs)
{
	return Inputs.GetInputNames();
}

int32 UAtlasSDKLibrary::GetInputCount(const FAtlasWorkflowInputs& Inputs)
{
	return Inputs.Num();
}

bool UAtlasSDKLibrary::HasInput(const FAtlasWorkflowInputs& Inputs, const FString& Name)
{
	return Inputs.Contains(Name);
}

bool UAtlasSDKLibrary::GetInputValue(const FAtlasWorkflowInputs& Inputs, const FString& Name, FAtlasValue& OutValue)
{
	return Inputs.GetValue(Name, OutValue);
}

void UAtlasSDKLibrary::ClearInputs(FAtlasWorkflowInputs& Inputs)
{
	Inputs.Clear();
}

// ==================== FAtlasWorkflowOutputs ====================

TArray<FString> UAtlasSDKLibrary::GetOutputNames(const FAtlasWorkflowOutputs& Outputs)
{
	return Outputs.GetOutputNames();
}

int32 UAtlasSDKLibrary::GetOutputCount(const FAtlasWorkflowOutputs& Outputs)
{
	return Outputs.Num();
}

bool UAtlasSDKLibrary::HasOutput(const FAtlasWorkflowOutputs& Outputs, const FString& Name)
{
	return Outputs.Contains(Name);
}

bool UAtlasSDKLibrary::GetOutputValue(const FAtlasWorkflowOutputs& Outputs, const FString& Name, FAtlasValue& OutValue)
{
	return Outputs.GetValue(Name, OutValue);
}

bool UAtlasSDKLibrary::GetStringOutput(const FAtlasWorkflowOutputs& Outputs, const FString& Name, FString& OutValue)
{
	return Outputs.GetString(Name, OutValue);
}

bool UAtlasSDKLibrary::GetNumberOutput(const FAtlasWorkflowOutputs& Outputs, const FString& Name, float& OutValue)
{
	return Outputs.GetNumber(Name, OutValue);
}

bool UAtlasSDKLibrary::GetIntegerOutput(const FAtlasWorkflowOutputs& Outputs, const FString& Name, int32& OutValue)
{
	return Outputs.GetInteger(Name, OutValue);
}

bool UAtlasSDKLibrary::GetBoolOutput(const FAtlasWorkflowOutputs& Outputs, const FString& Name, bool& OutValue)
{
	return Outputs.GetBool(Name, OutValue);
}

bool UAtlasSDKLibrary::GetFileIdOutput(const FAtlasWorkflowOutputs& Outputs, const FString& Name, FString& OutFileId)
{
	return Outputs.GetFileId(Name, OutFileId);
}

// ==================== FAtlasError ====================

bool UAtlasSDKLibrary::IsError(const FAtlasError& Error)
{
	return Error.IsError();
}

bool UAtlasSDKLibrary::IsSuccess(const FAtlasError& Error)
{
	return Error.IsSuccess();
}

bool UAtlasSDKLibrary::IsRetryable(const FAtlasError& Error)
{
	return Error.IsRetryable();
}

bool UAtlasSDKLibrary::WasCancelled(const FAtlasError& Error)
{
	return Error.WasCancelled();
}

EAtlasErrorCode UAtlasSDKLibrary::GetErrorCode(const FAtlasError& Error)
{
	return Error.Code;
}

FString UAtlasSDKLibrary::GetErrorMessage(const FAtlasError& Error)
{
	return Error.Message;
}

FString UAtlasSDKLibrary::GetErrorSummary(const FAtlasError& Error)
{
	return Error.GetSummary();
}

FString UAtlasSDKLibrary::ErrorToString(const FAtlasError& Error)
{
	return Error.ToString();
}

// ==================== FAtlasWorkflowSchema ====================

bool UAtlasSDKLibrary::IsSchemaValid(const FAtlasWorkflowSchema& Schema)
{
	return Schema.IsValid();
}

FString UAtlasSDKLibrary::GetSchemaDisplayName(const FAtlasWorkflowSchema& Schema)
{
	return Schema.GetDisplayName();
}

TArray<FAtlasParameterDef> UAtlasSDKLibrary::GetInputDefinitions(const FAtlasWorkflowSchema& Schema)
{
	return Schema.Inputs;
}

TArray<FAtlasParameterDef> UAtlasSDKLibrary::GetOutputDefinitions(const FAtlasWorkflowSchema& Schema)
{
	return Schema.Outputs;
}

bool UAtlasSDKLibrary::ValidateInputs(const FAtlasWorkflowSchema& Schema, const FAtlasWorkflowInputs& Inputs, TArray<FString>& OutErrors)
{
	return Schema.ValidateInputs(Inputs, OutErrors);
}

FAtlasWorkflowInputs UAtlasSDKLibrary::CreateDefaultInputs(const FAtlasWorkflowSchema& Schema)
{
	return Schema.CreateDefaultInputs();
}

// ==================== Schema URL Builders ====================

FString UAtlasSDKLibrary::GetSchemaUploadUrl(const FAtlasWorkflowSchema& Schema)
{
	return Schema.GetUploadUrl();
}

FString UAtlasSDKLibrary::GetSchemaExecuteUrl(const FAtlasWorkflowSchema& Schema)
{
	return Schema.GetExecuteUrl();
}

FString UAtlasSDKLibrary::GetSchemaDownloadUrl(const FAtlasWorkflowSchema& Schema, const FString& FileId)
{
	return Schema.GetDownloadUrl(FileId);
}

// ==================== FAtlasParameterDef ====================

FString UAtlasSDKLibrary::GetParameterDisplayName(const FAtlasParameterDef& Parameter)
{
	return Parameter.GetDisplayName();
}

bool UAtlasSDKLibrary::IsParameterFileType(const FAtlasParameterDef& Parameter)
{
	return Parameter.IsFileType();
}

bool UAtlasSDKLibrary::ParameterHasOptions(const FAtlasParameterDef& Parameter)
{
	return Parameter.HasOptions();
}

// ==================== Job State Helpers ====================

FString UAtlasSDKLibrary::JobStateToString(EAtlasJobState State)
{
	return AtlasJobHelpers::StateToString(State);
}

FString UAtlasSDKLibrary::JobPhaseToString(EAtlasJobPhase Phase)
{
	return AtlasJobHelpers::PhaseToString(Phase);
}

bool UAtlasSDKLibrary::IsTerminalState(EAtlasJobState State)
{
	return AtlasJobHelpers::IsTerminalState(State);
}

// ==================== History Helpers ====================

bool UAtlasSDKLibrary::IsHistoryRecordValid(const FAtlasJobHistoryRecord& Record)
{
	return Record.IsValid();
}

bool UAtlasSDKLibrary::IsHistoryRecordSuccess(const FAtlasJobHistoryRecord& Record)
{
	return Record.IsSuccess();
}

FString UAtlasSDKLibrary::GetHistoryStatusString(const FAtlasJobHistoryRecord& Record)
{
	return AtlasHistoryHelpers::StatusToString(Record.Status);
}

FString UAtlasSDKLibrary::GetHistoryDurationString(const FAtlasJobHistoryRecord& Record)
{
	return Record.GetDurationString();
}

FString UAtlasSDKLibrary::GetHistoryTimeAgoString(const FAtlasJobHistoryRecord& Record)
{
	return Record.GetTimeAgoString();
}

FString UAtlasSDKLibrary::GetHistoryDateCategory(const FAtlasJobHistoryRecord& Record)
{
	return Record.GetDateCategory();
}

FString UAtlasSDKLibrary::GetHistoryFormattedTimestamp(const FAtlasJobHistoryRecord& Record)
{
	return Record.GetFormattedTimestamp();
}

FAtlasHistoryQuery UAtlasSDKLibrary::MakeAllHistoryQuery()
{
	return FAtlasHistoryQuery::All();
}

FAtlasHistoryQuery UAtlasSDKLibrary::MakeWorkflowHistoryQuery(const FString& ApiId)
{
	return FAtlasHistoryQuery::ForWorkflow(ApiId);
}

FAtlasHistoryQuery UAtlasSDKLibrary::MakeTodayHistoryQuery()
{
	return FAtlasHistoryQuery::Today();
}

FAtlasHistoryQuery UAtlasSDKLibrary::MakeSuccessOnlyQuery()
{
	return FAtlasHistoryQuery::SuccessOnly();
}

FAtlasHistoryQuery UAtlasSDKLibrary::MakeFailedOnlyQuery()
{
	return FAtlasHistoryQuery::FailedOnly();
}

FAtlasHistoryQuery UAtlasSDKLibrary::MakeLast7DaysQuery()
{
	FAtlasHistoryQuery Query;
	Query.bFilterByDate = true;
	Query.DateFrom = FDateTime::Now() - FTimespan::FromDays(7);
	Query.DateTo = FDateTime::Now() + FTimespan::FromDays(1); // Include today fully
	return Query;
}

FAtlasHistoryQuery UAtlasSDKLibrary::MakeLast30DaysQuery()
{
	FAtlasHistoryQuery Query;
	Query.bFilterByDate = true;
	Query.DateFrom = FDateTime::Now() - FTimespan::FromDays(30);
	Query.DateTo = FDateTime::Now() + FTimespan::FromDays(1); // Include today fully
	return Query;
}

void UAtlasSDKLibrary::SetQueryStatusFilter(FAtlasHistoryQuery& Query, EAtlasJobStatus Status)
{
	Query.bFilterByStatus = true;
	Query.StatusFilter = Status;
}

void UAtlasSDKLibrary::SetQueryWorkflowFilter(FAtlasHistoryQuery& Query, const FString& ApiId)
{
	Query.ApiId = ApiId;
}

void UAtlasSDKLibrary::SetQueryDateFilter(FAtlasHistoryQuery& Query, bool bFilterByDate, const FDateTime& DateFrom, const FDateTime& DateTo)
{
	Query.bFilterByDate = bFilterByDate;
	Query.DateFrom = DateFrom;
	Query.DateTo = DateTo;
}

void UAtlasSDKLibrary::ClearQueryFilters(FAtlasHistoryQuery& Query)
{
	Query = FAtlasHistoryQuery();
}
