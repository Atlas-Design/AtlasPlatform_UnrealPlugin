// Copyright Atlas Platform. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "AtlasLibraryProfiler.h"
#include "AtlasWorkflowEditor.h"
#include "AtlasHistoryManager.h"
#include "AtlasSDKSettings.h"
#include "Editor.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "EditorUtilityWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMemory.h"
#include "UObject/UObjectIterator.h"
#include "UObject/StrongObjectPtr.h"

struct FAtlasLibraryBenchmarkAccess
{
	static void Open() { FModuleManager::GetModuleChecked<FAtlasWorkflowEditorModule>("AtlasWorkflowEditor").OpenWorkflowEditor(); }
};
namespace
{
	const TCHAR* MainPath = TEXT("/AtlasWorkflow/Core/UI/Editor/EUW_AtlasMain.EUW_AtlasMain");
	FString ReportPath(const FString& Name) { return FPaths::ProjectSavedDir() / TEXT("AtlasProfiling") / (Name + TEXT(".json")); }
	template <typename T> int32 CountObjects()
	{
		int32 Count = 0;
		for (TObjectIterator<T> It; It; ++It) if (!It->IsTemplate()) ++Count;
		return Count;
	}
	void ExportGraphs()
	{
		FString Text;
		for (TObjectIterator<UBlueprint> It; It; ++It)
		{
			if (!It->GetPathName().StartsWith(TEXT("/AtlasWorkflow/"))) continue;
			Text += TEXT("\nASSET ") + It->GetPathName() + TEXT("\n");
			TArray<UEdGraph*> Graphs;
			It->GetAllGraphs(Graphs);
			for (UEdGraph* Graph : Graphs)
			{
				Text += TEXT("GRAPH ") + Graph->GetName() + TEXT("\n");
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					if (!Node) continue;
					Text += Node->NodeGuid.ToString() + TEXT(" ") + Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Replace(TEXT("\n"), TEXT(" | ")) + TEXT("\n");
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin->Direction != EGPD_Output) continue;
						for (UEdGraphPin* Linked : Pin->LinkedTo)
							Text += TEXT("  ") + Pin->PinName.ToString() + TEXT(" -> ") + Linked->GetOwningNode()->NodeGuid.ToString() + TEXT(".") + Linked->PinName.ToString() + TEXT("\n");
					}
				}
			}
		}
		FFileHelper::SaveStringToFile(Text, *(FPaths::ProjectSavedDir() / TEXT("AtlasProfiling/BlueprintGraphs.txt")));
	}
	struct FCapture
	{
		FAutomationTestBase* Test;
		int32 Jobs = 0;
		int32 WorkflowCount = 5;
		int32 Pass = 0;
		bool bOpened = false;
		double OpenMs = 0, Started = 0, LastTick = 0, MaxTickMs = 0, NextOpenAt = 0;
		int32 WidgetBefore = 0, TextureBefore = 0;
		uint64 MemoryBefore = 0, PeakObserved = 0;
		bool Tick()
		{
			if (!bOpened)
			{
				if (FPlatformTime::Seconds() < NextOpenAt) return false; // Allow the old Slate window to finish closing.
				WidgetBefore = CountObjects<UWidget>(); TextureBefore = CountObjects<UTexture2D>();
				MemoryBefore = FPlatformMemory::GetStats().UsedPhysical; PeakObserved = MemoryBefore;
				FAtlasLibraryProfiler::Reset();
				Started = FPlatformTime::Seconds();
				FAtlasLibraryBenchmarkAccess::Open();
				LastTick = FPlatformTime::Seconds();
				OpenMs = (LastTick - Started) * 1000; MaxTickMs = OpenMs;
				bOpened = true;
				return false;
			}
			const double Now = FPlatformTime::Seconds();
			MaxTickMs = FMath::Max(MaxTickMs, (Now - LastTick) * 1000); LastTick = Now;
			PeakObserved = FMath::Max(PeakObserved, FPlatformMemory::GetStats().UsedPhysical);
			if (Now - Started < 2.0) return false; // Fixed observation window, not a claim of usability.
			TMap<FString, double> Metrics;
			Metrics.Add(TEXT("jobs"), Jobs);
			Metrics.Add(TEXT("openCallMs"), OpenMs);
			Metrics.Add(TEXT("maxObservedTickGapMs"), MaxTickMs);
			Metrics.Add(TEXT("captureWindowMs"), (Now - Started) * 1000);
			Metrics.Add(TEXT("widgetObjectsBefore"), WidgetBefore);
			Metrics.Add(TEXT("widgetObjectsAfter"), CountObjects<UWidget>());
			Metrics.Add(TEXT("textureObjectsBefore"), TextureBefore);
			Metrics.Add(TEXT("textureObjectsAfter"), CountObjects<UTexture2D>());
			Metrics.Add(TEXT("physicalBytesBefore"), MemoryBefore);
			Metrics.Add(TEXT("peakObservedPhysicalBytes"), PeakObserved);
			Metrics.Add(TEXT("processPeakPhysicalBytes"), FPlatformMemory::GetStats().PeakUsedPhysical);
			UEditorUtilityWidgetBlueprint* Blueprint = FindObject<UEditorUtilityWidgetBlueprint>(nullptr, MainPath);
			auto* Subsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
			Test->TestNotNull(TEXT("Actual Atlas panel created"), Blueprint ? Subsystem->FindUtilityWidgetFromBlueprint(Blueprint) : nullptr);
			Test->TestTrue(TEXT("Actual Atlas tab is live"), Blueprint && Subsystem->DoesTabExist(FName(*(Blueprint->GetPathName() + TEXT("_ActiveTab")))));
			Test->TestTrue(TEXT("Saved panel capture"), FAtlasLibraryProfiler::Save(ReportPath(Pass == 0 ? TEXT("FirstOpen") : FString::Printf(TEXT("WarmOpen%d"), Pass)), Metrics));
			if (Blueprint) Test->TestTrue(TEXT("Closed panel before reopening"), Subsystem->CloseTabByID(FName(*(Blueprint->GetPathName() + TEXT("_ActiveTab")))));
			++Pass;
			if (Pass < 3) { bOpened = false; NextOpenAt = FPlatformTime::Seconds() + 0.5; return false; }
			ExportGraphs();
			// Independent backend capture with an empty manager cache. OS cache is not flushed.
			TStrongObjectPtr<UAtlasHistoryManager> History(NewObject<UAtlasHistoryManager>());
			for (int32 Sample = 0; Sample < 3; ++Sample)
			{
				FAtlasLibraryProfiler::Reset();
				FAtlasHistoryQuery Query; Query.Limit = 20;
				const auto Page = History->QueryHistory(Query);
				const int32 Count = History->GetHistoryCount(Query);
				const auto Workflows = History->GetWorkflowInfoWithHistory();
				Test->TestEqual(TEXT("Fixture count preserved"), Count, Jobs);
				Test->TestEqual(TEXT("Fixture page size"), Page.Num(), FMath::Min(20, Jobs));
				Test->TestEqual(TEXT("Fixture workflow count"), Workflows.Num(), WorkflowCount);
				Test->TestTrue(TEXT("Saved backend capture"), FAtlasLibraryProfiler::Save(ReportPath(FString::Printf(TEXT("Backend%d"), Sample)), {{TEXT("jobs"), double(Jobs)}}));
			}
			return true;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasLibraryOpeningBenchmark, "Atlas.Performance.LibraryOpening",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter)
bool FAtlasLibraryOpeningBenchmark::RunTest(const FString& Parameters)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("AtlasLibraryBenchmark"))
		|| !FPaths::FileExists(FPaths::ProjectDir() / TEXT("AtlasLibraryBenchmark.fixture")))
	{
		AddError(TEXT("Run only through Tests/RunLibraryBenchmark.ps1 in its isolated fixture project."));
		return false;
	}
	if (!TestTrue(TEXT("Profiling enabled"), FAtlasLibraryProfiler::IsEnabled())) return false;
	// Offscreen rendering emits this restoration warning; NullRHI does not.
	// Actual widget/tab existence is asserted in both modes.
	if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
		AddExpectedError(TEXT("TryInvokeTab failed with TabId: /AtlasWorkflow/Core/UI/Editor/EUW_AtlasMain.EUW_AtlasMain_ActiveTab"), EAutomationExpectedErrorFlags::Contains, 1);
	const FString ExpectedOutput = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Atlas/Output"));
	if (!TestTrue(TEXT("Output confined to disposable fixture"), FPaths::IsSamePath(UAtlasSDKSettings::Get()->GetOutputFolderPath(), ExpectedOutput))) return false;
	TestTrue(TEXT("Saved startup capture"), FAtlasLibraryProfiler::Save(ReportPath(TEXT("Startup"))));
	TSharedRef<FCapture> Capture = MakeShared<FCapture>(); Capture->Test = this;
	FParse::Value(FCommandLine::Get(), TEXT("AtlasLibraryJobs="), Capture->Jobs);
	Capture->WorkflowCount = Capture->Jobs == 0 ? 0 : 5;
	FParse::Value(FCommandLine::Get(), TEXT("AtlasLibraryWorkflows="), Capture->WorkflowCount);
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Capture] { return Capture->Tick(); }));
	return true;
}
#endif
