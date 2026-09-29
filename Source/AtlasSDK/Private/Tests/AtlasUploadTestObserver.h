// Copyright Atlas Platform. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "AtlasJob.h"
#include "AtlasUploadTestObserver.generated.h"

// Native automation receiver for the same dynamic delegates consumed by Blueprint.
UCLASS(Transient, NotBlueprintable)
class UAtlasUploadTestObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 FailureCount = 0;
	int32 CompletionCount = 0;
	bool bCancelOnUploading = false;

	UFUNCTION()
	void Failed(UAtlasJob* Job, const FAtlasError& Error) { ++FailureCount; }
	UFUNCTION()
	void Completed(UAtlasJob* Job, const FAtlasWorkflowOutputs& Outputs) { ++CompletionCount; }
	UFUNCTION()
	void Progressed(UAtlasJob* Job, float Progress, EAtlasJobPhase Phase)
	{
		if (bCancelOnUploading && Phase == EAtlasJobPhase::Uploading) Job->Cancel();
	}
};
