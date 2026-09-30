#include "AtlasLibraryMigrationCommandlet.h"
#include "AtlasUIHelpers.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"

int32 UAtlasLibraryMigrationCommandlet::Main(const FString& Params)
{
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, TEXT("/AtlasWorkflow/Core/UI/Editor/Utilities/EUW_WorkflowLibrary.EUW_WorkflowLibrary"));
	if (!Blueprint) return 1;
	UPackage* Package = Blueprint->GetOutermost();
	const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	const FString Backup = FPaths::ProjectSavedDir() / TEXT("AtlasMigrationBackup") / (FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT("_EUW_WorkflowLibrary.uasset"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
	if (IFileManager::Get().Copy(*Backup, *Filename) != COPY_OK) return 2;
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	int32 DiscoveryCount = 0, SelectionCount = 0;
	for (UEdGraph* Graph : Graphs)
	{
		const TArray<UEdGraphNode*> Nodes = Graph->Nodes;
		for (UEdGraphNode* Node : Nodes)
		{
			if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
			{
				if (Call->GetFunctionName() == TEXT("GetAllWorkflowAssets"))
				{
					Call->SetFromFunction(UAtlasUIHelpers::StaticClass()->FindFunctionByName(TEXT("GetWorkflowDisplayAssets")));
					Call->ReconstructNode();
					++DiscoveryCount;
				}
				else if (Call->GetFunctionName() == TEXT("GetWorkflowDisplayAssets")) ++DiscoveryCount;
			}
			UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Node);
			if (!Set || Set->GetVarName() != TEXT("selectedWorkflow")) continue;
			UEdGraphPin* Input = Set->FindPin(TEXT("selectedWorkflow"));
			if (!Input || Input->LinkedTo.Num() != 1) return 3;
			UEdGraphPin* Source = Input->LinkedTo[0];
			if (UK2Node_CallFunction* Existing = Cast<UK2Node_CallFunction>(Source->GetOwningNode()))
				if (Existing->GetFunctionName() == TEXT("ResolveWorkflowDisplayAsset")) { ++SelectionCount; continue; }
			UK2Node_CallFunction* Resolve = NewObject<UK2Node_CallFunction>(Graph, NAME_None, RF_Transactional);
			Graph->AddNode(Resolve, false, false);
			Resolve->CreateNewGuid();
			Resolve->SetFromFunction(UAtlasUIHelpers::StaticClass()->FindFunctionByName(TEXT("ResolveWorkflowDisplayAsset")));
			Resolve->AllocateDefaultPins();
			Resolve->NodePosX = Set->NodePosX - 300;
			Resolve->NodePosY = Set->NodePosY + 100;
			Input->BreakAllPinLinks();
			const UEdGraphSchema* Schema = Graph->GetSchema();
			if (!Schema->TryCreateConnection(Source, Resolve->FindPin(TEXT("Asset")))
				|| !Schema->TryCreateConnection(Resolve->GetReturnValuePin(), Input)) return 4;
			++SelectionCount;
		}
	}
	if (DiscoveryCount != 1 || SelectionCount != 1) return 5;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	if (Blueprint->Status == BS_Error) return 6;
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, Blueprint, *Filename, Args)) return 7;
	UE_LOG(LogTemp, Display, TEXT("Atlas library migration saved; backup: %s"), *Backup);
	UBlueprint* Row = LoadObject<UBlueprint>(nullptr, TEXT("/AtlasWorkflow/Core/UI/Editor/Utilities/EUW_ParameterRow.EUW_ParameterRow"));
	if (!Row) return 8;
	const FString RowFile = FPackageName::LongPackageNameToFilename(Row->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
	const FString RowBackup = FPaths::GetPath(Backup) / (FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT("_EUW_ParameterRow.uasset"));
	if (IFileManager::Get().Copy(*RowBackup, *RowFile) != COPY_OK) return 9;
	Graphs.Reset(); Row->GetAllGraphs(Graphs);
	int32 PreviewCount = 0;
	for (UEdGraph* Graph : Graphs)
	{
		const TArray<UEdGraphNode*> Nodes = Graph->Nodes;
		for (UEdGraphNode* Node : Nodes)
		{
			UK2Node_CallFunction* Load = Cast<UK2Node_CallFunction>(Node);
			if (!Load) continue;
			if (Load->GetFunctionName() == TEXT("LoadPreviewIntoImage")) { ++PreviewCount; continue; }
			if (Load->GetFunctionName() != TEXT("LoadTextureFromFile")) continue;
			UEdGraphPin* Texture = Load->GetReturnValuePin();
			if (!Texture || Texture->LinkedTo.Num() != 1) return 10;
			UK2Node_CallFunction* Brush = Cast<UK2Node_CallFunction>(Texture->LinkedTo[0]->GetOwningNode());
			if (!Brush || Brush->GetFunctionName() != TEXT("SetBrushFromTexture")) return 11;
			UK2Node_CallFunction* Async = NewObject<UK2Node_CallFunction>(Graph, NAME_None, RF_Transactional);
			Graph->AddNode(Async, false, false); Async->CreateNewGuid();
			Async->SetFromFunction(UAtlasUIHelpers::StaticClass()->FindFunctionByName(TEXT("LoadPreviewIntoImage")));
			Async->AllocateDefaultPins(); Async->NodePosX = Load->NodePosX; Async->NodePosY = Load->NodePosY;
			auto Transfer = [Graph](UEdGraphPin* From, UEdGraphPin* To)
			{
				if (!From || !To) return false;
				const auto Links = From->LinkedTo;
				To->DefaultValue = From->DefaultValue; To->DefaultObject = From->DefaultObject;
				From->BreakAllPinLinks();
				for (UEdGraphPin* Link : Links) if (!Graph->GetSchema()->TryCreateConnection(Link, To)) return false;
				return true;
			};
			if (!Transfer(Load->GetExecPin(), Async->GetExecPin())
				|| !Transfer(Load->FindPin(TEXT("FilePath")), Async->FindPin(TEXT("FilePath")))
				|| !Transfer(Load->FindPin(UEdGraphSchema_K2::PN_Self), Async->FindPin(TEXT("Manager")))
				|| !Transfer(Brush->FindPin(UEdGraphSchema_K2::PN_Self), Async->FindPin(TEXT("Image")))
				|| !Transfer(Brush->GetThenPin(), Async->GetThenPin())) return 12;
			Graph->RemoveNode(Load); Graph->RemoveNode(Brush);
			++PreviewCount;
		}
	}
	if (PreviewCount != 1) return 13;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Row);
	FKismetEditorUtilities::CompileBlueprint(Row);
	if (Row->Status == BS_Error) return 14;
	if (!UPackage::SavePackage(Row->GetOutermost(), Row, *RowFile, Args)) return 15;
	UE_LOG(LogTemp, Display, TEXT("Atlas preview migration saved; backup: %s"), *RowBackup);
	return 0;
}
