// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasBatchValidator.h"
#include "AtlasWorkflowAsset.h"

FAtlasBatchValidationResult UAtlasBatchValidator::ValidateBatch(
	const FAtlasWorkflowSchema& Schema,
	const FAtlasBatchDefinition& Batch)
{
	FAtlasBatchValidationResult Result;
	Result.bAllValid = true;

	if (!Schema.IsValid())
	{
		Result.bAllValid = false;
		Result.BatchErrors.Add(TEXT("Workflow schema is invalid or not loaded"));
		return Result;
	}

	if (Batch.IsEmpty())
	{
		Result.bAllValid = false;
		Result.BatchErrors.Add(TEXT("Batch has no rows"));
		return Result;
	}

	Result.Rows.Reserve(Batch.Rows.Num());

	for (int32 i = 0; i < Batch.Rows.Num(); ++i)
	{
		const FAtlasBatchRow& Row = Batch.Rows[i];
		FAtlasBatchRowValidation RowResult = ValidateRow(Schema, Row);
		RowResult.RowIndex = i;

		if (!RowResult.bIsValid)
		{
			Result.bAllValid = false;
			Result.InvalidRowCount++;
		}

		Result.Rows.Add(MoveTemp(RowResult));
	}

	return Result;
}

FAtlasBatchRowValidation UAtlasBatchValidator::ValidateRow(
	const FAtlasWorkflowSchema& Schema,
	const FAtlasBatchRow& Row)
{
	FAtlasBatchRowValidation Result;
	Result.RowIndex = Row.RowIndex;
	Result.bIsValid = true;

	for (const FAtlasParameterDef& InputDef : Schema.Inputs)
	{
		const FAtlasValue* ValuePtr = Row.Values.Find(InputDef.Name);

		if (!ValuePtr || ValuePtr->Type == EAtlasValueType::None)
		{
			Result.bIsValid = false;
			Result.Errors.Add(FString::Printf(
				TEXT("Required input '%s' is missing"), *InputDef.GetDisplayName()));
			continue;
		}

		// Audio inputs are not yet supported in batch mode (matching Unity)
		if (InputDef.Type == EAtlasValueType::Audio)
		{
			Result.bIsValid = false;
			Result.Errors.Add(FString::Printf(
				TEXT("Audio input '%s' is not supported in batch mode"), *InputDef.GetDisplayName()));
			continue;
		}

		FString Error;
		if (!InputDef.ValidateValue(*ValuePtr, Error))
		{
			Result.bIsValid = false;
			Result.Errors.Add(Error);
		}
	}

	// Check for unknown keys not in schema
	for (const auto& Pair : Row.Values)
	{
		bool bFound = false;
		for (const FAtlasParameterDef& InputDef : Schema.Inputs)
		{
			if (InputDef.Name == Pair.Key)
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			Result.bIsValid = false;
			Result.Errors.Add(FString::Printf(
				TEXT("Unknown input '%s' — not defined in workflow schema"), *Pair.Key));
		}
	}

	return Result;
}

FAtlasBatchValidationResult UAtlasBatchValidator::ValidateBatchAgainstAsset(
	UAtlasWorkflowAsset* WorkflowAsset,
	const FAtlasBatchDefinition& Batch)
{
	if (!IsValid(WorkflowAsset) || !WorkflowAsset->IsValid())
	{
		FAtlasBatchValidationResult Result;
		Result.bAllValid = false;
		Result.BatchErrors.Add(TEXT("Workflow asset is invalid or not loaded"));
		return Result;
	}

	return ValidateBatch(WorkflowAsset->GetSchema(), Batch);
}
