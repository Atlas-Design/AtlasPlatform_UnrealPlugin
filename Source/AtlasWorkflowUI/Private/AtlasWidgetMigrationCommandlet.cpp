#include "AtlasWidgetMigrationCommandlet.h"
#include "AtlasNativeInputWidgets.h"
#include "AtlasNativeWorkflowWidgets.h"
#include "AtlasHistoryFilterWidget.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "WidgetBlueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"


namespace AtlasWidgetMigration
{
static bool Wire(UEdGraph* Graph, UEdGraphPin* A, UEdGraphPin* B)
{
    return A && B && Graph->GetSchema()->TryCreateConnection(A, B);
}
static UK2Node_CallFunction* AddCall(UEdGraph* Graph, UClass* Class, FName Function)
{
    auto* Node = NewObject<UK2Node_CallFunction>(Graph, NAME_None, RF_Transactional);
    Graph->AddNode(Node, false, false); Node->CreateNewGuid();
    Node->SetFromFunction(Class->FindFunctionByName(Function)); Node->AllocateDefaultPins();
    Node->NodePosX = 300;
    return Node;
}
// Keep the original graph/function identity and pin names so existing callers need no rewiring.
static bool WrapFunction(UBlueprint* BP, FName GraphName, UClass* Class, FName Function, const TMap<FName,FName>& Outputs)
{
    UEdGraph* Graph = nullptr;
    for (UEdGraph* G : BP->FunctionGraphs) if (G->GetFName() == GraphName) Graph = G;
    if (!Graph) return false;
    UK2Node_FunctionEntry* Entry = nullptr; UK2Node_FunctionResult* Result = nullptr;
    for (UEdGraphNode* N : Graph->Nodes)
    {
        if (auto* E = Cast<UK2Node_FunctionEntry>(N)) Entry = E;
        if (!Result) Result = Cast<UK2Node_FunctionResult>(N);
    }
    if (!Entry || !Result) return false;
    const auto Nodes = Graph->Nodes;
    for (UEdGraphNode* N : Nodes)
    {
        N->BreakAllNodeLinks();
        if (N != Entry && N != Result) Graph->RemoveNode(N);
    }
    Entry->LocalVariables.Reset();
    auto* Call = AddCall(Graph, Class, Function);
    if (!Wire(Graph, Entry->FindPin(TEXT("then")), Call->GetExecPin())
        || !Wire(Graph, Call->GetThenPin(), Result->FindPin(TEXT("execute")))) return false;
    for (const auto& Pair : Outputs)
        if (!Wire(Graph, Call->FindPin(Pair.Key), Result->FindPin(Pair.Value))) return false;
    Result->NodePosX = 600;
    return true;
}
static bool WrapEvents(UBlueprint* BP, UClass* Class, const TMap<FName,FName>& Names, const TMap<FName,FName>& Inputs)
{
    TSet<FName> Found;
    for (UEdGraph* Graph : BP->UbergraphPages)
    {
        const auto Nodes = Graph->Nodes;
        for (UEdGraphNode* N : Nodes)
        {
            N->BreakAllNodeLinks();
            auto* Event = Cast<UK2Node_CustomEvent>(N);
            const FName* Function = Event ? Names.Find(Event->CustomFunctionName) : nullptr;
            if (!Function) { Graph->RemoveNode(N); continue; }
            Found.Add(Event->CustomFunctionName);
            auto* Call = AddCall(Graph, Class, *Function);
            Call->NodePosX = Event->NodePosX + 300; Call->NodePosY = Event->NodePosY;
            if (!Wire(Graph, Event->FindPin(TEXT("then")), Call->GetExecPin())) return false;
            for (const auto& Pair : Inputs)
                if (UEdGraphPin* Pin = Event->FindPin(Pair.Key))
                    if (!Wire(Graph, Pin, Call->FindPin(Pair.Value))) return false;
        }
    }
    return Found.Num() == Names.Num();
}
static TMap<FName,FString> Capture(UObject* Object, UClass* NativeClass)
{
    TMap<FName,FString> Values;
    for (TFieldIterator<FProperty> It(NativeClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        if (It->HasMetaData(TEXT("BindWidget")) || It->HasAnyPropertyFlags(CPF_BlueprintAssignable)) continue;
        if (FProperty* Old = FindFProperty<FProperty>(Object->GetClass(), It->GetFName()))
        {
            FString Text; Old->ExportText_InContainer(0, Text, Object, Object, Object, PPF_None);
            Values.Add(It->GetFName(), Text);
        }
    }
    return Values;
}
static bool Restore(UObject* Object, const TMap<FName,FString>& Values)
{
    for (const auto& Pair : Values)
    {
        FProperty* Prop = FindFProperty<FProperty>(Object->GetClass(), Pair.Key);
        if (!Prop || !Prop->ImportText_Direct(*Pair.Value, Prop->ContainerPtrToValuePtr<void>(Object), Object, PPF_None)) return false;
    }
    return true;
}
static int32 Migrate(const TArray<UBlueprint*>& Blueprints)
{
    const FString PickerPath = TEXT("/AtlasWorkflow/Core/UI/Editor/Utilities/EUW_FilePicker.EUW_FilePicker");
    const FString InputPath = TEXT("/AtlasWorkflow/Core/UI/Editor/Utilities/EUW_InputField.EUW_InputField");
    auto* Picker = LoadObject<UWidgetBlueprint>(nullptr, *PickerPath);
    auto* Input = LoadObject<UWidgetBlueprint>(nullptr, *InputPath);
    if (!Picker || !Input) return 10;
    if (Picker->ParentClass == UAtlasFilePickerWidget::StaticClass() && Input->ParentClass == UAtlasInputFieldWidget::StaticClass())
    { UE_LOG(LogTemp, Display, TEXT("Native input migration already applied; no assets changed.")); return 0; }
    if (Picker->ParentClass != UEditorUtilityWidget::StaticClass() || Input->ParentClass != UEditorUtilityWidget::StaticClass()) return 11;
    // Back up both assets before the first mutation. Originals are never overwritten by reruns.
    const FString BackupRoot = FPaths::ProjectSavedDir() / TEXT("NativeWidgetBackups") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
    IFileManager::Get().MakeDirectory(*BackupRoot, true);
    for (UWidgetBlueprint* BP : {Picker, Input})
    {
        const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        if (IFileManager::Get().Copy(*(BackupRoot / FPaths::GetCleanFilename(File)), *File) != COPY_OK) return 12;
    }
    UE_LOG(LogTemp, Display, TEXT("Original widget backups: %s"), *BackupRoot);
    const auto PickerDefaults = Capture(Picker->GeneratedClass->GetDefaultObject(), UAtlasFilePickerWidget::StaticClass());
    const auto InputDefaults = Capture(Input->GeneratedClass->GetDefaultObject(), UAtlasInputFieldWidget::StaticClass());
    TMap<FName,TMap<FName,FString>> ChildDefaults;
    TArray<UWidget*> Widgets; Input->WidgetTree->GetAllWidgets(Widgets);
    for (UWidget* W : Widgets)
        if (W->GetClass() == Picker->GeneratedClass) ChildDefaults.Add(W->GetFName(), Capture(W, UAtlasFilePickerWidget::StaticClass()));

    if (!WrapFunction(Picker, TEXT("GetImageFilePath"), UAtlasFilePickerWidget::StaticClass(), TEXT("ResolveImagePath"), {{TEXT("ReturnValue"),TEXT("FilePickerPath")}})
        || !WrapFunction(Picker, TEXT("GetMeshFilePath"), UAtlasFilePickerWidget::StaticClass(), TEXT("ResolveMeshPath"), {{TEXT("ReturnValue"),TEXT("FilePickerPath")}})
        || !WrapEvents(Picker, UAtlasFilePickerWidget::StaticClass(), {{TEXT("SetWidgetType"),TEXT("SetPickerMode")}}, {{TEXT("currentType"),TEXT("Mode")}})) return 13;
    if (!WrapFunction(Input, TEXT("GetValue"), UAtlasInputFieldWidget::StaticClass(), TEXT("CollectValue"), {{TEXT("OutName"),TEXT("name")},{TEXT("OutValue"),TEXT("value")}})
        || !WrapEvents(Input, UAtlasInputFieldWidget::StaticClass(), {{TEXT("InitializeInput"),TEXT("ConfigureInput")},{TEXT("SetDefaultValue"),TEXT("ApplyDefaultValue")}}, {{TEXT("currentDefParameter"),TEXT("Definition")}})) return 14;
    for (UWidgetBlueprint* BP : {Picker, Input})
    {
        UClass* Parent = BP == Picker ? UAtlasFilePickerWidget::StaticClass() : UAtlasInputFieldWidget::StaticClass();
        BP->NewVariables.RemoveAll([Parent](const FBPVariableDescription& V) { return FindFProperty<FProperty>(Parent, V.VarName) != nullptr; });
        const auto Delegates = BP->DelegateSignatureGraphs;
        for (UEdGraph* G : Delegates) if (FindFProperty<FMulticastDelegateProperty>(Parent, G->GetFName()))
            FBlueprintEditorUtils::RemoveGraph(BP, G);
        const auto Macros = BP->MacroGraphs;
        for (UEdGraph* G : Macros) FBlueprintEditorUtils::RemoveGraph(BP, G);
        // The native input sets label/value state on events. Layout, animation and styling stay in the tree.
        BP->Bindings.RemoveAll([BP, Input](const FDelegateEditorBinding& Binding) { return BP == Input && Binding.ObjectName == TEXT("InputName") && Binding.PropertyName == TEXT("Text"); });
        BP->ParentClass = Parent;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FBlueprintEditorUtils::RefreshAllNodes(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        if (BP->Status == BS_Error) return 15;
        if (!Restore(BP->GeneratedClass->GetDefaultObject(), BP == Picker ? PickerDefaults : InputDefaults)) return 16;
        if (BP == Picker)
        {
            for (const auto& Pair : ChildDefaults)
                if (UWidget* W = Input->WidgetTree->FindWidget(Pair.Key)) { if (!Restore(W, Pair.Value)) return 17; }
        }
    }
    // Verify consumers as well as migrated assets. No caller package is saved unless explicitly migrated.
    for (UBlueprint* BP : Blueprints)
    {
        FBlueprintEditorUtils::RefreshAllNodes(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        if (BP->Status == BS_Error) { UE_LOG(LogTemp, Error, TEXT("Consumer failed compilation: %s"), *BP->GetPathName()); return 18; }
    }
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    for (UWidgetBlueprint* BP : {Picker, Input})
    {
        const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        if (!UPackage::SavePackage(BP->GetOutermost(), BP, *File, Args)) return 19;
    }
    UE_LOG(LogTemp, Display, TEXT("Native input migration saved two assets; consumers compiled: %d"), Blueprints.Num());
    return 0;
}
static int32 MigrateWorkflow(const TArray<UBlueprint*>& Blueprints)
{
    auto* Row = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/AtlasWorkflow/Core/UI/Editor/Utilities/EUW_ParameterRow.EUW_ParameterRow"));
    auto* Workflow = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/AtlasWorkflow/Core/UI/Editor/SingleWorkflow/EUW_CurrentWorkflow.EUW_CurrentWorkflow"));
    UClass* InputClass = LoadClass<UAtlasInputFieldWidget>(nullptr, TEXT("/AtlasWorkflow/Core/UI/Editor/Utilities/EUW_InputField.EUW_InputField_C"));
    if (!Row || !Workflow || !InputClass) return 30;
    if (Row->ParentClass == UAtlasParameterRowWidget::StaticClass() && Workflow->ParentClass == UAtlasCurrentWorkflowWidget::StaticClass())
    { UE_LOG(LogTemp, Display, TEXT("Native workflow migration already applied.")); return 0; }
    if (Row->ParentClass != UEditorUtilityWidget::StaticClass() || Workflow->ParentClass != UEditorUtilityWidget::StaticClass()) return 31;
    const FString BackupRoot = FPaths::ProjectSavedDir() / TEXT("NativeWidgetBackups") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
    IFileManager::Get().MakeDirectory(*BackupRoot, true);
    TMap<FString,FString> Backups;
    for (UWidgetBlueprint* BP : {Row, Workflow})
    {
        FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        FString Backup = BackupRoot / FPaths::GetCleanFilename(File);
        if (IFileManager::Get().Copy(*Backup, *File) != COPY_OK) return 32;
        Backups.Add(File, Backup);
    }
    UE_LOG(LogTemp, Display, TEXT("Original workflow widget backups: %s"), *BackupRoot);
    if (!WrapEvents(Row, UAtlasParameterRowWidget::StaticClass(),
        {{TEXT("SetParam"),TEXT("ConfigureParameter")},{TEXT("ParamEnabled"),TEXT("RefreshEditability")},
         {TEXT("SetImageProperty"),TEXT("PresentImage")},{TEXT("SetMeshProperty"),TEXT("PresentMesh")},
         {TEXT("SetAudioProperty"),TEXT("PresentAudio")},{TEXT("SetProperty"),TEXT("PresentProperty")}},
        {{TEXT("isEditable"),TEXT("InEditable")},{TEXT("ParamName"),TEXT("InName")},{TEXT("atlasValue"),TEXT("InValue")},
         {TEXT("isOutput"),TEXT("InOutput")},{TEXT("jobRecord"),TEXT("InRecord")},{TEXT("object"),TEXT("InObject")},
         {TEXT("propertyWidget"),TEXT("InPropertyWidget")},{TEXT("NewPropertyName"),TEXT("InPropertyName")}})) return 33;
    if (!WrapEvents(Workflow, UAtlasCurrentWorkflowWidget::StaticClass(),
        {{TEXT("SetWorkflow"),TEXT("SelectWorkflow")},{TEXT("GenerateInputFields"),TEXT("RebuildInputs")},
         {TEXT("GenerateOutputFields"),TEXT("RebuildOutputs")},{TEXT("ClearPrevious"),TEXT("ClearFields")},{TEXT("InputsChanged"),TEXT("RefreshInputs")}},
        {{TEXT("CurrentWorkflow"),TEXT("InWorkflow")},{TEXT("TargetArray"),TEXT("Definitions")}})) return 34;
    for (UWidgetBlueprint* BP : {Row, Workflow})
    {
        const auto Defaults = Capture(BP->GeneratedClass->GetDefaultObject(), BP == Row ? UAtlasParameterRowWidget::StaticClass() : UAtlasCurrentWorkflowWidget::StaticClass());
        // Graph-local state has no callers; external calls continue through the retained custom events.
        BP->NewVariables.Reset();
        const auto Macros = BP->MacroGraphs;
        for (UEdGraph* G : Macros) FBlueprintEditorUtils::RemoveGraph(BP, G);
        BP->ParentClass = BP == Row ? UAtlasParameterRowWidget::StaticClass() : UAtlasCurrentWorkflowWidget::StaticClass();
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FBlueprintEditorUtils::RefreshAllNodes(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        if (BP->Status == BS_Error) return 35;
        if (!Restore(BP->GeneratedClass->GetDefaultObject(), Defaults)) return 36;
        if (BP == Workflow)
        {
            auto* CDO = CastChecked<UAtlasCurrentWorkflowWidget>(BP->GeneratedClass->GetDefaultObject());
            CDO->InputWidgetClass = InputClass;
            CDO->OutputRowClass = Row->GeneratedClass;
        }
    }
    for (UBlueprint* BP : Blueprints)
    {
        FBlueprintEditorUtils::RefreshAllNodes(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        if (BP->Status == BS_Error) { UE_LOG(LogTemp, Error, TEXT("Consumer failed compilation: %s"), *BP->GetPathName()); return 37; }
    }
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    for (UWidgetBlueprint* BP : {Row, Workflow})
    {
        const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        if (!UPackage::SavePackage(BP->GetOutermost(), BP, *File, Args))
        {
            for (const auto& Pair : Backups)
                if (IFileManager::Get().Copy(*Pair.Key, *Pair.Value, true) != COPY_OK)
                    UE_LOG(LogTemp, Error, TEXT("Restore failed: %s; original is %s"), *Pair.Key, *Pair.Value);
            return 38;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Native workflow migration saved two assets; consumers compiled: %d"), Blueprints.Num());
    return 0;
}

static int32 MigrateHistoryFilter(const TArray<UBlueprint*>& Blueprints)
{
    auto* BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/AtlasWorkflow/Core/UI/Editor/JobHistory/EUW_HistoryFilterBar.EUW_HistoryFilterBar"));
    if (!BP) return 50;
    UClass* Parent = UAtlasHistoryFilterWidget::StaticClass();
    if (BP->ParentClass == Parent) { UE_LOG(LogTemp, Display, TEXT("History filter already migrated.")); return 0; }
    if (BP->ParentClass != UEditorUtilityWidget::StaticClass()) return 51;
    const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    const FString Backup = FPaths::ProjectSavedDir() / TEXT("NativeWidgetBackups") / FGuid::NewGuid().ToString(EGuidFormats::Digits) / FPaths::GetCleanFilename(File);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
    if (IFileManager::Get().Copy(*Backup, *File) != COPY_OK) return 52;
    UE_LOG(LogTemp, Display, TEXT("History filter backup: %s"), *Backup);
    if (!WrapEvents(BP, Parent, {{TEXT("InitFilters"),TEXT("InitializeFilters")},{TEXT("RefreshHistoryList"),TEXT("BroadcastQuery")}}, {})) return 53;
    BP->NewVariables.Reset();
    const auto Delegates = BP->DelegateSignatureGraphs;
    for (UEdGraph* G : Delegates) FBlueprintEditorUtils::RemoveGraph(BP, G);
    BP->ParentClass = Parent;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FBlueprintEditorUtils::RefreshAllNodes(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    if (BP->Status == BS_Error) return 54;
    for (UBlueprint* Consumer : Blueprints)
    {
        FBlueprintEditorUtils::RefreshAllNodes(Consumer);
        FKismetEditorUtilities::CompileBlueprint(Consumer);
        if (Consumer->Status == BS_Error) { UE_LOG(LogTemp, Error, TEXT("Filter consumer failed: %s"), *Consumer->GetPathName()); return 55; }
    }
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(BP->GetOutermost(), BP, *File, Args))
    {
        if (IFileManager::Get().Copy(*File, *Backup, true) != COPY_OK) UE_LOG(LogTemp, Error, TEXT("Restore original from %s"), *Backup);
        return 56;
    }
    UE_LOG(LogTemp, Display, TEXT("History filter migrated; all %d consumers compiled."), Blueprints.Num());
    return 0;
}

static int32 RepairEditorMetadata(const TArray<UBlueprint*>& Blueprints, bool bVerifyOnly)
{
    TArray<UBlueprint*> Repairs;
    for (UBlueprint* BP : Blueprints)
    {
        TArray<UK2Node*> Nodes;
        FBlueprintEditorUtils::GetAllNodesOfClass(BP, Nodes);
        int32 Missing = 0;
        for (UK2Node* Node : Nodes) if (!Node->HasAnyFlags(RF_Transactional)) ++Missing;
        if (Missing)
        {
            UE_LOG(LogTemp, Display, TEXT("Editor metadata repair: %s has %d nodes missing RF_Transactional"), *BP->GetPathName(), Missing);
            Repairs.Add(BP);
        }
    }
    if (bVerifyOnly) return Repairs.IsEmpty() ? 0 : 40;
    if (Repairs.IsEmpty()) { UE_LOG(LogTemp, Display, TEXT("Editor metadata already valid; no assets changed.")); return 0; }
    const FString BackupRoot = FPaths::ProjectSavedDir() / TEXT("NativeWidgetBackups") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
    IFileManager::Get().MakeDirectory(*BackupRoot, true);
    TMap<FString,FString> Backups;
    for (UBlueprint* BP : Repairs)
    {
        const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        const FString Backup = BackupRoot / FPaths::GetCleanFilename(File);
        if (IFileManager::Get().Copy(*Backup, *File) != COPY_OK) return 41;
        Backups.Add(File, Backup);
    }
    UE_LOG(LogTemp, Display, TEXT("Editor metadata backups: %s"), *BackupRoot);
    for (UBlueprint* BP : Repairs)
    {
        FBlueprintEditorUtils::UpdateTransactionalFlags(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        if (BP->Status == BS_Error) return 42;
    }
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    for (UBlueprint* BP : Repairs)
    {
        const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        if (!UPackage::SavePackage(BP->GetOutermost(), BP, *File, Args))
        {
            for (const auto& Pair : Backups)
                if (IFileManager::Get().Copy(*Pair.Key, *Pair.Value, true) != COPY_OK)
                    UE_LOG(LogTemp, Error, TEXT("Restore failed: %s; original is %s"), *Pair.Key, *Pair.Value);
            return 43;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Editor metadata repaired and saved: %d assets"), Repairs.Num());
    return 0;
}

}

int32 UAtlasWidgetMigrationCommandlet::Main(const FString& Params)
{
	FString Output;
	const bool bFilter = FParse::Param(*Params, TEXT("MigrateHistoryFilter"));
	const bool bRepair = FParse::Param(*Params, TEXT("RepairEditorMetadata"));
	const bool bVerify = FParse::Param(*Params, TEXT("VerifyEditorMetadata"));
	const bool bMigrate = FParse::Param(*Params, TEXT("Migrate"));
	const bool bMigrateWorkflow = FParse::Param(*Params, TEXT("MigrateWorkflow"));
	if (!FParse::Value(*Params, TEXT("Output="), Output) && !bMigrate && !bMigrateWorkflow && !bRepair && !bVerify && !bFilter) return 1;
	FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	Registry.Get().ScanPathsSynchronous({TEXT("/AtlasWorkflow/Core/UI")}, true);
	TArray<FAssetData> Assets;
	Registry.Get().GetAssetsByPath(TEXT("/AtlasWorkflow/Core/UI"), Assets, true);
	TArray<UBlueprint*> LoadedBlueprints;
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const FAssetData& Asset : Assets)
	{
		UBlueprint* BP = Cast<UBlueprint>(Asset.GetAsset());
		if (!BP) continue;
		LoadedBlueprints.Add(BP);
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("path"), BP->GetPathName());
		Entry->SetStringField(TEXT("parent"), GetPathNameSafe(BP->ParentClass));
		TArray<TSharedPtr<FJsonValue>> Variables;
		for (const FBPVariableDescription& Variable : BP->NewVariables)
		{
			auto V = MakeShared<FJsonObject>();
			V->SetStringField(TEXT("name"), Variable.VarName.ToString());
			V->SetStringField(TEXT("type"), Variable.VarType.PinCategory.ToString());
			V->SetStringField(TEXT("object"), GetPathNameSafe(Variable.VarType.PinSubCategoryObject.Get()));
			V->SetStringField(TEXT("default"), Variable.DefaultValue);
			Variables.Add(MakeShared<FJsonValueObject>(V));
		}
		Entry->SetArrayField(TEXT("variables"), Variables);
		TArray<TSharedPtr<FJsonValue>> Widgets;
		if (UWidgetBlueprint* WBP = Cast<UWidgetBlueprint>(BP))
		{
			TArray<UWidget*> Children;
			if (WBP->WidgetTree) WBP->WidgetTree->GetAllWidgets(Children);
			for (UWidget* Widget : Children)
			{
				auto W = MakeShared<FJsonObject>();
				W->SetStringField(TEXT("name"), Widget->GetName());
				W->SetStringField(TEXT("class"), Widget->GetClass()->GetPathName());
				Widgets.Add(MakeShared<FJsonValueObject>(W));
			}
		}
		Entry->SetArrayField(TEXT("widgets"), Widgets);
		TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
		TArray<TSharedPtr<FJsonValue>> JsonGraphs;
		for (UEdGraph* Graph : Graphs)
		{
			auto G = MakeShared<FJsonObject>(); G->SetStringField(TEXT("name"), Graph->GetName());
			TArray<TSharedPtr<FJsonValue>> Nodes;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				auto N = MakeShared<FJsonObject>();
				N->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
				N->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
				N->SetStringField(TEXT("class"), Node->GetClass()->GetName());
				if (auto* Call = Cast<UK2Node_CallFunction>(Node)) N->SetStringField(TEXT("function"), Call->GetFunctionName().ToString());
				if (auto* Var = Cast<UK2Node_Variable>(Node)) N->SetStringField(TEXT("variable"), Var->GetVarName().ToString());
				TArray<TSharedPtr<FJsonValue>> Pins;
				for (UEdGraphPin* Pin : Node->Pins)
				{
					auto P = MakeShared<FJsonObject>();
					P->SetStringField(TEXT("name"), Pin->PinName.ToString());
					P->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("in") : TEXT("out"));
					P->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
					P->SetStringField(TEXT("object"), GetPathNameSafe(Pin->PinType.PinSubCategoryObject.Get()));
					P->SetStringField(TEXT("default"), Pin->DefaultValue);
					P->SetStringField(TEXT("defaultObject"), GetPathNameSafe(Pin->DefaultObject));
					TArray<TSharedPtr<FJsonValue>> Links;
					for (UEdGraphPin* Link : Pin->LinkedTo) Links.Add(MakeShared<FJsonValueString>(Link->GetOwningNode()->NodeGuid.ToString() + TEXT(":") + Link->PinName.ToString()));
					P->SetArrayField(TEXT("links"), Links); Pins.Add(MakeShared<FJsonValueObject>(P));
				}
				N->SetArrayField(TEXT("pins"), Pins); Nodes.Add(MakeShared<FJsonValueObject>(N));
			}
			G->SetArrayField(TEXT("nodes"), Nodes); JsonGraphs.Add(MakeShared<FJsonValueObject>(G));
		}
		Entry->SetArrayField(TEXT("graphs"), JsonGraphs); Result.Add(MakeShared<FJsonValueObject>(Entry));
	}
	if (bFilter) return AtlasWidgetMigration::MigrateHistoryFilter(LoadedBlueprints);
	if (bRepair || bVerify) return AtlasWidgetMigration::RepairEditorMetadata(LoadedBlueprints, bVerify);
	if (bMigrate) return AtlasWidgetMigration::Migrate(LoadedBlueprints);
	if (bMigrateWorkflow) return AtlasWidgetMigration::MigrateWorkflow(LoadedBlueprints);
	FString Json;
	FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Json));
	return FFileHelper::SaveStringToFile(Json, *Output) ? 0 : 2;
}
