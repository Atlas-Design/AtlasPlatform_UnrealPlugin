// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Types/AtlasBatchTypes.h"
#include "Types/AtlasSchemaTypes.h"
#include "AtlasBatchValidator.generated.h"

class UAtlasWorkflowAsset;

/**
 * Validates batch rows against a workflow schema before execution.
 * All functions are static and Blueprint-callable so EUW widgets can
 * validate without C++ code.
 */
UCLASS()
class ATLASSDK_API UAtlasBatchValidator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Validate an entire batch definition against a workflow schema.
	 * Checks every row for missing/invalid values and returns per-row results.
	 * @param Schema The workflow schema to validate against
	 * @param Batch The batch definition to validate
	 * @return Validation result with per-row details
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Batch|Validation", meta = (DisplayName = "Validate Batch"))
	static FAtlasBatchValidationResult ValidateBatch(const FAtlasWorkflowSchema& Schema, const FAtlasBatchDefinition& Batch);

	/**
	 * Validate a single batch row against a workflow schema.
	 * @param Schema The workflow schema to validate against
	 * @param Row The row to validate
	 * @return Validation result for this row
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Batch|Validation", meta = (DisplayName = "Validate Row"))
	static FAtlasBatchRowValidation ValidateRow(const FAtlasWorkflowSchema& Schema, const FAtlasBatchRow& Row);

	/**
	 * Validate a batch definition against a workflow asset.
	 * Convenience wrapper that extracts the schema from the asset.
	 * @param WorkflowAsset The workflow asset to validate against
	 * @param Batch The batch definition to validate
	 * @return Validation result with per-row details
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|Batch|Validation", meta = (DisplayName = "Validate Batch Against Asset"))
	static FAtlasBatchValidationResult ValidateBatchAgainstAsset(UAtlasWorkflowAsset* WorkflowAsset, const FAtlasBatchDefinition& Batch);
};
