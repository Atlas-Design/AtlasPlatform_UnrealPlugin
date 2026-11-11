#include "AtlasFileWidgets.h"
#include "AtlasNativeInputWidgets.h"
#include "AtlasNativeWorkflowWidgets.h"
#include "EditorUtilityWidgetBlueprintFactory.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CheckBox.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"

namespace {
template<typename T> T* Make(UWidgetTree* Tree,const TCHAR* Name) { auto* W=Tree->ConstructWidget<T>(T::StaticClass(),Name); W->SetFlags(RF_Transactional); W->bIsVariable=true; return W; }
UTextBlock* Text(UWidgetTree* Tree,const TCHAR* Name,const TCHAR* Caption,int Size=10) {
    auto* W=Make<UTextBlock>(Tree,Name); W->SetText(FText::FromString(Caption)); auto Font=W->GetFont(); Font.Size=Size; W->SetFont(Font); W->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); return W;
}
UButton* Button(UWidgetTree* Tree,const TCHAR* Name,const TCHAR* LabelName,const TCHAR* Caption) {
    auto* W=Make<UButton>(Tree,Name); auto* Slot=Cast<UButtonSlot>(W->AddChild(Text(Tree,LabelName,Caption))); Slot->SetPadding(FMargin(6,3)); return W;
}
void Fill(UHorizontalBox* Row,UWidget* Child) { auto* S=Row->AddChildToHorizontalBox(Child); S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0,0,10,0)); }
void Auto(UHorizontalBox* Row,UWidget* Child,FMargin Padding=FMargin(0)) { auto* S=Row->AddChildToHorizontalBox(Child); S->SetVerticalAlignment(VAlign_Center); S->SetPadding(Padding); }
UWidgetBlueprint* Create(const TCHAR* Name,UClass* Parent,bool& Created) {
    const FString PackageName=FString(TEXT("/AtlasWorkflow/Core/UI/Editor/Utilities/"))+Name;
    if (auto* Existing=LoadObject<UWidgetBlueprint>(nullptr,*(PackageName+TEXT(".")+Name),nullptr,LOAD_NoWarn)) { Created=false; return Existing; }
    auto* Factory=NewObject<UEditorUtilityWidgetBlueprintFactory>(); Factory->ParentClass=Parent;
    auto* BP=Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(UEditorUtilityWidgetBlueprint::StaticClass(),CreatePackage(*PackageName),Name,RF_Public|RF_Standalone|RF_Transactional,nullptr,GWarn));
    if (BP) FAssetRegistryModule::AssetCreated(BP); Created=true; return BP;
}
void CardTree(UWidgetBlueprint* BP) {
    auto* T=BP->WidgetTree.Get();
    auto* Drop=Make<UAtlasFileDropSurface>(T,TEXT("DropTarget")); T->RootWidget=Drop;
    auto* Bounds=Make<USizeBox>(T,TEXT("CardBounds")); Bounds->SetHeightOverride(84); Drop->AddChild(Bounds);
    auto* Back=Make<UBorder>(T,TEXT("CardBackground")); Back->SetBrushColor(FLinearColor(.045f,.045f,.045f)); Back->SetPadding(FMargin(8)); Bounds->AddChild(Back);
    auto* Row=Make<UHorizontalBox>(T,TEXT("CardRow")); Back->AddChild(Row);
    auto* Handle=Make<UAtlasFileDropSurface>(T,TEXT("ReorderHandle")); Handle->bReorderHandle=true; Handle->SetToolTipText(FText::FromString(TEXT("Drag to reorder, or Alt + Up / Down")));
    auto* HandleBounds=Make<USizeBox>(T,TEXT("HandleBounds")); HandleBounds->SetWidthOverride(16); HandleBounds->SetHeightOverride(40); Handle->AddChild(HandleBounds);
    auto* Grip=Text(T,TEXT("HandleGlyph"),TEXT("="),12); Grip->SetJustification(ETextJustify::Center); HandleBounds->AddChild(Grip); Auto(Row,Handle,FMargin(0,0,6,0));
    auto* Preview=Button(T,TEXT("PreviewButton"),TEXT("UnusedPreviewLabel"),TEXT("")); Preview->ClearChildren();
    auto* ThumbBounds=Make<USizeBox>(T,TEXT("ThumbnailBounds")); ThumbBounds->SetWidthOverride(64); ThumbBounds->SetHeightOverride(64);
    auto* PreviewSlot=Cast<UButtonSlot>(Preview->AddChild(ThumbBounds)); PreviewSlot->SetPadding(FMargin(0));
    auto* Overlay=Make<UOverlay>(T,TEXT("ThumbnailOverlay")); ThumbBounds->AddChild(Overlay);
    auto* Scale=Make<UScaleBox>(T,TEXT("ThumbnailScale")); Scale->SetStretch(EStretch::ScaleToFit); Scale->SetStretchDirection(EStretchDirection::DownOnly);
    auto* ScaleSlot=Overlay->AddChildToOverlay(Scale); ScaleSlot->SetHorizontalAlignment(HAlign_Fill); ScaleSlot->SetVerticalAlignment(VAlign_Fill);
    Scale->AddChild(Make<UImage>(T,TEXT("Thumbnail")));
    auto* Icon=Text(T,TEXT("TypeIcon"),TEXT("FILE"),10); auto* IconSlot=Overlay->AddChildToOverlay(Icon); IconSlot->SetHorizontalAlignment(HAlign_Center); IconSlot->SetVerticalAlignment(VAlign_Center); Auto(Row,Preview,FMargin(0,0,10,0));
    auto* Info=Make<UVerticalBox>(T,TEXT("FileInformation")); Info->SetClipping(EWidgetClipping::ClipToBoundsAlways); Fill(Row,Info);
    Info->AddChildToVerticalBox(Text(T,TEXT("Filename"),TEXT("Selected file"),11));
    auto* Meta=Text(T,TEXT("Metadata"),TEXT("Format and size")); Meta->SetColorAndOpacity(FLinearColor(.65f,.65f,.65f)); Info->AddChildToVerticalBox(Meta)->SetPadding(FMargin(0,3,0,2));
    Info->AddChildToVerticalBox(Text(T,TEXT("SourceStatus"),TEXT("File")));
    auto* Actions=Make<UVerticalBox>(T,TEXT("Actions")); Auto(Row,Actions);
    auto* Primary=Button(T,TEXT("PrimaryButton"),TEXT("PrimaryLabel"),TEXT("Replace")); Actions->AddChildToVerticalBox(Primary)->SetHorizontalAlignment(HAlign_Right);
    auto* Secondary=Make<UHorizontalBox>(T,TEXT("SecondaryActions")); Actions->AddChildToVerticalBox(Secondary)->SetHorizontalAlignment(HAlign_Right);
    Auto(Secondary,Button(T,TEXT("RemoveButton"),TEXT("RemoveLabel"),TEXT("Clear")),FMargin(0,4,4,0));
    Auto(Secondary,Button(T,TEXT("MoreButton"),TEXT("MoreLabel"),TEXT("...")),FMargin(0,4,0,0));
}
void PanelTree(UWidgetBlueprint* BP) {
    auto* T=BP->WidgetTree.Get(); auto* Root=Make<UVerticalBox>(T,TEXT("FilePanelRoot")); T->RootWidget=Root;
    auto* Header=Make<UHorizontalBox>(T,TEXT("Header")); Root->AddChildToVerticalBox(Header)->SetPadding(FMargin(0,0,0,4));
    Fill(Header,Button(T,TEXT("CollapseButton"),TEXT("HeaderLabel"),TEXT("0 items")));
    Auto(Header,Button(T,TEXT("ImportAllButton"),TEXT("ImportAllLabel"),TEXT("Import all")));
    Root->AddChildToVerticalBox(Make<UVerticalBox>(T,TEXT("Cards")));
    auto* Drop=Make<UAtlasFileDropSurface>(T,TEXT("AppendTarget")); Root->AddChildToVerticalBox(Drop);
    auto* Bounds=Make<USizeBox>(T,TEXT("EmptyBounds")); Bounds->SetMinDesiredHeight(76); Drop->AddChild(Bounds);
    auto* Back=Make<UBorder>(T,TEXT("EmptyBackground")); Back->SetBrushColor(FLinearColor(.045f,.045f,.045f)); Back->SetPadding(FMargin(8)); Bounds->AddChild(Back);
    auto* Empty=Make<UVerticalBox>(T,TEXT("EmptyContent")); Back->AddChild(Empty);
    auto* Hint=Text(T,TEXT("EmptyHint"),TEXT("Drop a compatible file or Unreal asset")); Hint->SetAutoWrapText(true); Empty->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0,0,0,8));
    auto* Buttons=Make<UHorizontalBox>(T,TEXT("SourceButtons")); Empty->AddChildToVerticalBox(Buttons);
    Auto(Buttons,Button(T,TEXT("BrowseButton"),TEXT("BrowseLabel"),TEXT("Browse...")),FMargin(0,0,6,0));
    Auto(Buttons,Button(T,TEXT("OtherSourcesButton"),TEXT("OtherSourcesLabel"),TEXT("Other sources...")));
    auto* Error=Text(T,TEXT("InlineError"),TEXT("")); Error->SetAutoWrapText(true); Error->SetColorAndOpacity(FLinearColor(.94f,.64f,.52f)); Root->AddChildToVerticalBox(Error)->SetPadding(FMargin(0,4,0,0));
}
bool Compile(UWidgetBlueprint* BP) {
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FBlueprintEditorUtils::UpdateTransactionalFlags(BP); FKismetEditorUtilities::CompileBlueprint(BP); return BP->Status!=BS_Error;
}
}

int32 MigrateAtlasFileParity(const TArray<UBlueprint*>& Blueprints) {
    TArray<UWidgetBlueprint*> Targets; TMap<FString,FString> Backups; TArray<FString> NewFiles;
    const FString BackupRoot=FPaths::ProjectSavedDir()/TEXT("NativeWidgetBackups")/FGuid::NewGuid().ToString(EGuidFormats::Digits);
    IFileManager::Get().MakeDirectory(*BackupRoot,true);
    auto Backup=[&](UWidgetBlueprint* BP) {
        const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        if (!FPaths::FileExists(File)) { NewFiles.Add(File); return true; }
        const FString Dest=BackupRoot/FPaths::GetCleanFilename(File); if (IFileManager::Get().Copy(*Dest,*File)!=COPY_OK) return false; Backups.Add(File,Dest); return true;
    };
    bool NewCard=false,NewPanel=false;
    auto* Card=Create(TEXT("WBP_AtlasFileCard"),UAtlasFileCard::StaticClass(),NewCard);
    auto* Panel=Create(TEXT("WBP_AtlasFilePanel"),UAtlasFilePanel::StaticClass(),NewPanel);
    if (!Card || !Panel) return 70;
    if (NewCard) { if (!Backup(Card)) return 71; CardTree(Card); if (!Compile(Card)) return 72; Targets.Add(Card); }
    if (!NewCard) {
        if (auto* Info=Card->WidgetTree->FindWidget(TEXT("FileInformation")); Info && Info->GetClipping()!=EWidgetClipping::ClipToBoundsAlways) {
            if (!Backup(Card)) return 71; Info->SetClipping(EWidgetClipping::ClipToBoundsAlways); if (!Compile(Card)) return 72; Targets.Add(Card);
        }
    }
    if (!Card->WidgetTree->FindWidget(TEXT("CompactPreviewBounds"))) {
        if (!Targets.Contains(Card) && !Backup(Card)) return 71;
        for (const TCHAR* Name : {TEXT("PreviewButton"),TEXT("MoreButton")}) {
            auto* Button=Card->WidgetTree->FindWidget(Name); auto* Parent=Button ? Button->GetParent() : nullptr;
            if (!Parent) return 72;
            const bool Preview=Button->GetFName()==TEXT("PreviewButton");
            auto* Bounds=Make<USizeBox>(Card->WidgetTree,Preview ? TEXT("CompactPreviewBounds") : TEXT("CompactMoreBounds"));
            Bounds->SetWidthOverride(Preview ? 64 : 28); Bounds->SetHeightOverride(Preview ? 64 : 24);
            Parent->ReplaceChild(Button,Bounds); Bounds->AddChild(Button);
        }
        if (!Compile(Card)) return 72; Targets.AddUnique(Card);
    }
    // Compact hit targets must not inherit the editor button style's extra 12px padding.
    for (const TCHAR* Name : {TEXT("PreviewButton"),TEXT("MoreButton")}) if (auto* Button=Cast<UButton>(Card->WidgetTree->FindWidget(Name))) {
        auto Style=Button->GetStyle();
        if (Style.NormalPadding!=FMargin(0) || Style.PressedPadding!=FMargin(0)) {
            if (!Targets.Contains(Card) && !Backup(Card)) return 71;
            Style.NormalPadding=Style.PressedPadding=FMargin(0); Button->SetStyle(Style); Targets.AddUnique(Card);
        }
    }
    if (Targets.Contains(Card) && !Compile(Card)) return 72;
    if (NewPanel) {
        if (!Backup(Panel)) return 71; PanelTree(Panel); if (!Compile(Panel)) return 72;
        CastChecked<UAtlasFilePanel>(Panel->GeneratedClass->GetDefaultObject())->CardClass=Card->GeneratedClass; Targets.Add(Panel);
    }
    for (const TCHAR* Name : {TEXT("EUW_InputField"),TEXT("EUW_ParameterRow")}) {
        auto* BP=LoadObject<UWidgetBlueprint>(nullptr,*(FString(TEXT("/AtlasWorkflow/Core/UI/Editor/Utilities/"))+Name+TEXT(".")+Name));
        if (!BP || !BP->WidgetTree || !BP->WidgetTree->RootWidget) return 73;
        auto* T=BP->WidgetTree.Get(); const bool Input=BP->ParentClass->IsChildOf(UAtlasInputFieldWidget::StaticClass());
        if (auto* Existing=Cast<UAtlasFilePanel>(T->FindWidget(Input ? TEXT("FileSelector") : TEXT("FilePresentation")))) {
            const bool BoundText=Input && !T->FindWidget(TEXT("CompactTextBounds"));
            if (!Existing->CardClass || BoundText) { if (!Backup(BP)) return 71; Existing->CardClass=Card->GeneratedClass.Get(); Targets.AddUnique(BP); }
            if (BoundText) {
                auto* TextInput=T->FindWidget(TEXT("EditableText_string")); auto* Parent=TextInput ? TextInput->GetParent() : nullptr;
                if (!Parent) return 73;
                auto* Bounds=Make<USizeBox>(T,TEXT("CompactTextBounds")); Bounds->SetMinDesiredHeight(60); Bounds->SetMaxDesiredHeight(144);
                Parent->ReplaceChild(TextInput,Bounds); Bounds->AddChild(TextInput);
                if (!Compile(BP)) return 74;
            }
            continue;
        }
        if (!Backup(BP)) return 71;
        auto* Root=Make<UVerticalBox>(T,TEXT("FileParityRoot")); auto* Original=T->RootWidget.Get(); T->RootWidget=Root; Root->AddChildToVerticalBox(Original);
        if (Input) {
            auto* Options=Make<UHorizontalBox>(T,TEXT("OptionalControls")); Root->AddChildToVerticalBox(Options)->SetPadding(FMargin(0,4,0,4));
            Auto(Options,Make<UCheckBox>(T,TEXT("UseServerDefault")),FMargin(0,0,5,0)); Fill(Options,Text(T,TEXT("UseDefaultLabel"),TEXT("Use server default (omit this input)")));
        }
        auto* Files=T->ConstructWidget<UAtlasFilePanel>(Panel->GeneratedClass.Get(),Input ? TEXT("FileSelector") : TEXT("FilePresentation")); Files->SetFlags(RF_Transactional); Files->bIsVariable=true;
        Root->AddChildToVerticalBox(Files)->SetPadding(FMargin(0,4,0,0));
        if (Input) {
            Root->AddChildToVerticalBox(Text(T,TEXT("BooleanValueLabel"),TEXT("False")));
            Root->AddChildToVerticalBox(Button(T,TEXT("ExpandTextButton"),TEXT("ExpandTextLabel"),TEXT("Expand text...")))->SetHorizontalAlignment(HAlign_Right);
            auto* Error=Text(T,TEXT("ValidationMessage"),TEXT("")); Error->SetAutoWrapText(true); Error->SetColorAndOpacity(FLinearColor(.94f,.64f,.52f)); Root->AddChildToVerticalBox(Error);
        } else { auto* Scalar=Text(T,TEXT("RecordedScalar"),TEXT(""),11); Scalar->SetAutoWrapText(true); Root->AddChildToVerticalBox(Scalar); }
        if (Input && !T->FindWidget(TEXT("CompactTextBounds"))) {
            auto* TextInput=T->FindWidget(TEXT("EditableText_string")); auto* Parent=TextInput ? TextInput->GetParent() : nullptr;
            if (!Parent) return 73;
            auto* Bounds=Make<USizeBox>(T,TEXT("CompactTextBounds")); Bounds->SetMinDesiredHeight(60); Bounds->SetMaxDesiredHeight(144); Parent->ReplaceChild(TextInput,Bounds); Bounds->AddChild(TextInput);
        }
        if (!Compile(BP)) return 74; Targets.Add(BP);
    }
    for (auto* BP : Blueprints) { FKismetEditorUtilities::CompileBlueprint(BP); if (BP->Status==BS_Error) return 75; }
    // Apply native class defaults after consumer compilation, which can recreate CDOs.
    if (!Targets.Contains(Panel)) { if (!Backup(Panel)) return 71; Targets.Add(Panel); }
    for (TObjectIterator<UAtlasFilePanel> It; It; ++It) if (It->GetClass()==Panel->GeneratedClass.Get()) It->CardClass=Card->GeneratedClass.Get();
    FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
    for (auto* BP : Targets) {
        const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        if (!UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args)) {
            for (const auto& Pair : Backups) IFileManager::Get().Copy(*Pair.Key,*Pair.Value,true);
            for (const auto& New : NewFiles) IFileManager::Get().Delete(*New);
            return 76;
        }
    }
    UE_LOG(LogTemp,Display,TEXT("File parity: saved %d Designer assets; compiled %d consumers. Backups: %s"),Targets.Num(),Blueprints.Num(),*BackupRoot); return 0;
}
