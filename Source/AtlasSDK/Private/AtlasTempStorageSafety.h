// Copyright Atlas Platform. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

namespace AtlasTempStorage
{
	struct FContext
	{
		FString ProjectDirectory;
		FString SavedDirectory;
		FString EngineDirectory;
		FString OutputDirectory;
	};

	// Pure path policy; never creates or deletes anything.
	bool ValidateDirectory(const FString& Directory, const FContext& Context, FString& OutDirectory, FString& OutError);

	// Only empty directories may be adopted. Existing data requires an ownership marker.
	int32 CleanupDirectory(const FString& Directory, const FContext& Context, FString& OutError);
}
