// Copyright Atlas Platform. All Rights Reserved.
#include "AtlasLibraryProfiler.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

namespace
{
	TAutoConsoleVariable<int32> Enabled(TEXT("Atlas.LibraryProfile"), 0,
		TEXT("Collect aggregate game-thread library timings. Reset and Dump commands control capture."));
	struct FSample { int64 Calls = 0; int64 Items = 0; double TotalMs = 0; double MaxMs = 0; };
	TMap<FString, FSample> Samples;
	FAutoConsoleCommand ResetCommand(TEXT("Atlas.LibraryProfile.Reset"), TEXT("Reset aggregate library timings."),
		FConsoleCommandDelegate::CreateStatic(&FAtlasLibraryProfiler::Reset));
	FAutoConsoleCommand DumpCommand(TEXT("Atlas.LibraryProfile.Dump"), TEXT("Save aggregate timings to Saved/AtlasProfiling/Manual.json."),
		FConsoleCommandDelegate::CreateLambda([] { FAtlasLibraryProfiler::Save(FPaths::ProjectSavedDir() / TEXT("AtlasProfiling/Manual.json")); }));
}

bool FAtlasLibraryProfiler::IsEnabled()
{
	static const bool bCommandLine = FParse::Param(FCommandLine::Get(), TEXT("AtlasLibraryProfile"));
	return IsInGameThread() && (bCommandLine || Enabled.GetValueOnGameThread() != 0);
}
void FAtlasLibraryProfiler::Reset() { if (IsInGameThread()) Samples.Empty(); }
void FAtlasLibraryProfiler::Add(const TCHAR* Name, double Milliseconds, int64 Items)
{
	if (!IsEnabled()) return;
	FSample& Sample = Samples.FindOrAdd(Name);
	++Sample.Calls;
	Sample.Items += Items;
	Sample.TotalMs += Milliseconds;
	Sample.MaxMs = FMath::Max(Sample.MaxMs, Milliseconds);
}
bool FAtlasLibraryProfiler::Save(const FString& Path, const TMap<FString, double>& Measurements)
{
	if (!IsInGameThread()) return false;
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("timingSemantics"), TEXT("Inclusive game-thread scopes; nested totals must not be added together."));
	TSharedRef<FJsonObject> Metrics = MakeShared<FJsonObject>();
	for (const auto& Pair : Measurements) Metrics->SetNumberField(Pair.Key, Pair.Value);
	Root->SetObjectField(TEXT("measurements"), Metrics);
	TSharedRef<FJsonObject> Events = MakeShared<FJsonObject>();
	for (const auto& Pair : Samples)
	{
		TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
		Event->SetNumberField(TEXT("calls"), Pair.Value.Calls);
		Event->SetNumberField(TEXT("items"), Pair.Value.Items);
		Event->SetNumberField(TEXT("totalMs"), Pair.Value.TotalMs);
		Event->SetNumberField(TEXT("maxMs"), Pair.Value.MaxMs);
		Events->SetObjectField(Pair.Key, Event);
	}
	Root->SetObjectField(TEXT("events"), Events);
	FString Json;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
	return IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true) && FFileHelper::SaveStringToFile(Json, *Path);
}
FAtlasLibraryProfileScope::FAtlasLibraryProfileScope(const TCHAR* InName)
	: Name(InName), Started(0), bEnabled(FAtlasLibraryProfiler::IsEnabled())
{
	if (bEnabled) Started = FPlatformTime::Seconds();
}
FAtlasLibraryProfileScope::~FAtlasLibraryProfileScope()
{
	if (bEnabled) FAtlasLibraryProfiler::Add(Name, (FPlatformTime::Seconds() - Started) * 1000.0);
}
