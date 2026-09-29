// Copyright Atlas Platform. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "AtlasAsyncActions.h"
#include "AtlasBatchOrchestrator.h"
#include "AtlasLifetimeTestObserver.generated.h"

UCLASS(Transient, NotBlueprintable)
class UAtlasLifetimeTestObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 ResultCount = 0;
	int32 BatchCount = 0;
	TFunction<void(const FAtlasExecutionResult&)> ResultCallback;
	TFunction<void(UAtlasBatchOrchestrator*)> BatchCallback;
	UFUNCTION()
	void ResultReceived(const FAtlasExecutionResult& Result)
	{
		++ResultCount;
		if (ResultCallback) ResultCallback(Result);
	}
	UFUNCTION()
	void BatchReceived(UAtlasBatchOrchestrator* Orchestrator, const FAtlasBatchProgress& Progress)
	{
		++BatchCount;
		if (BatchCallback) BatchCallback(Orchestrator);
	}
};
