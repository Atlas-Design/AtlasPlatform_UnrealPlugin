// Copyright Atlas Platform. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

/** Opt-in, game-thread-only aggregate diagnostics. Never records paths or payloads. */
class ATLASSDK_API FAtlasLibraryProfiler
{
public:
	static bool IsEnabled();
	static void Reset();
	static void Add(const TCHAR* Name, double Milliseconds = 0.0, int64 Items = 0);
	static bool Save(const FString& Path, const TMap<FString, double>& Measurements = {});
};

class ATLASSDK_API FAtlasLibraryProfileScope
{
public:
	explicit FAtlasLibraryProfileScope(const TCHAR* InName);
	~FAtlasLibraryProfileScope();
private:
	const TCHAR* Name;
	double Started;
	bool bEnabled;
};
