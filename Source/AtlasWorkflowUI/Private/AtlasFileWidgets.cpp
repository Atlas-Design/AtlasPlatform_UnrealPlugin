#include "AtlasFileWidgets.h"
#include "AtlasOutputManager.h"
#include "AtlasEditorSubsystem.h"
#include "AtlasUIHelpers.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Factories/TextureFactory.h"
#include "Factories/SoundFactory.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "EditorReimportHandler.h"
#include "ScopedTransaction.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/SWindow.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SBox.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "UObject/StrongObjectPtr.h"
#include "Containers/Ticker.h"

namespace {
void Visible(UWidget* Widget,bool Value) { if (Widget) Widget->SetVisibility(Value ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
bool ImageType(const FAtlasParameterDef& Def) { return Def.Type==EAtlasValueType::Image; }
FString SizeText(const FString& Path) { const int64 Size=IFileManager::Get().FileSize(*Path); return Size>=0 ? FText::AsMemory(Size).ToString() : FString(); }
FAtlasFileChoice* FindChoice(UAtlasFilePanel* Panel,const FGuid& Id) { if (!Panel || !Panel->Selection) return nullptr; const int32 I=Panel->Selection->Find(Id); return I==INDEX_NONE ? nullptr : &Panel->Selection->Items[I]; }
void ShowInBrowser(UObject* Object) { if (Object) FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().SyncBrowserToAssets(TArray<UObject*>{Object}); }
}
void UAtlasFilePanel::Configure(const FAtlasParameterDef& Def,const FAtlasValue& Value,EAtlasFilePresentation InMode,const FAtlasJobHistoryRecord& InRecord,const FString& Origin) {
    if (!Selection) Selection=NewObject<UAtlasFileSelection>(this);
    if (!OutputManager) { auto* Subsystem=UAtlasUIHelpers::GetEditorSubsystem(this); OutputManager=Subsystem ? Subsystem->GetOutputManager() : NewObject<UAtlasOutputManager>(this); }
    bConfigured=true; bOmitted=Value.bOmitted; Mode=InMode; Record=InRecord; ParameterName=Def.Name; Importing.Empty(); ImportErrors.Empty(); bCollapsed=false;
    Selection->bMutationEnabled=Mode==EAtlasFilePresentation::Editable;
    Selection->Configure(Def,Value,Origin); RebuildCards();
}
bool UAtlasFilePanel::IsEditable() const { return bConfigured && Mode==EAtlasFilePresentation::Editable && Selection && Selection->bMutationEnabled && GetIsEnabled(); }
void UAtlasFilePanel::SetMutationEnabled(bool Enabled) { if (Selection) Selection->SetMutationEnabled(Enabled && Mode==EAtlasFilePresentation::Editable); }
void UAtlasFilePanel::Invalidate() { if (Selection) { ++Selection->Revision; Selection->bMutationEnabled=false; } }
void UAtlasFilePanel::NativeConstruct() {
    Super::NativeConstruct(); bReady=true;
    if (!Selection) Selection=NewObject<UAtlasFileSelection>(this);
    if (!OutputManager) { auto* Subsystem=UAtlasUIHelpers::GetEditorSubsystem(this); OutputManager=Subsystem ? Subsystem->GetOutputManager() : NewObject<UAtlasOutputManager>(this); }
    Selection->OnChanged.AddUniqueDynamic(this,&ThisClass::Changed);
    if (BrowseButton) BrowseButton->OnClicked.AddUniqueDynamic(this,&ThisClass::Browse);
    if (OtherSourcesButton) OtherSourcesButton->OnClicked.AddUniqueDynamic(this,&ThisClass::OtherSources);
    if (CollapseButton) CollapseButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ToggleCollapsed);
    if (ImportAllButton) ImportAllButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ImportAll);
    if (AppendTarget) { AppendTarget->OwnerPanel=this; AppendTarget->TargetId.Invalidate(); }
    RebuildCards();
}
void UAtlasFilePanel::NativeDestruct() {
    bReady=false; Invalidate();
    if (IsValid(Selection)) Selection->OnChanged.RemoveDynamic(this,&ThisClass::Changed);
    if (IsValid(BrowseButton)) BrowseButton->OnClicked.RemoveDynamic(this,&ThisClass::Browse);
    if (IsValid(OtherSourcesButton)) OtherSourcesButton->OnClicked.RemoveDynamic(this,&ThisClass::OtherSources);
    if (IsValid(CollapseButton)) CollapseButton->OnClicked.RemoveDynamic(this,&ThisClass::ToggleCollapsed);
    if (IsValid(ImportAllButton)) ImportAllButton->OnClicked.RemoveDynamic(this,&ThisClass::ImportAll);
    Super::NativeDestruct();
}
void UAtlasFilePanel::Changed() { RebuildCards(); OnSelectionChanged.Broadcast(); }
void UAtlasFilePanel::ShowError(const FString& Error) { if (Selection) Selection->Error=Error; if (InlineError) { InlineError->SetText(FText::FromString(Error)); Visible(InlineError,!Error.IsEmpty()); } }
void UAtlasFilePanel::RebuildCards() {
    if (!bReady || !Cards || !Selection || !CardClass) return;
    Cards->ClearChildren();
    const bool Array=Selection->Definition.bIsArray, Editable=Mode==EAtlasFilePresentation::Editable;
    Visible(Header,Array); Visible(ImportAllButton,Array && Mode==EAtlasFilePresentation::Output);
    if (ImportAllButton) ImportAllButton->SetIsEnabled(Importing.IsEmpty());
    if (HeaderLabel) HeaderLabel->SetText(FText::FromString(FString::Printf(TEXT("%s %d items"),bCollapsed ? TEXT("+") : TEXT("-"),Selection->Items.Num())));
    Visible(Cards,!bCollapsed);
    if (!bCollapsed) for (int32 I=0; I<Selection->Items.Num(); ++I) {
        auto* Card=CreateWidget<UAtlasFileCard>(this,CardClass); if (!Card) continue;
        Card->Configure(this,Selection->Items[I],Array ? I : INDEX_NONE);
        Cards->AddChildToVerticalBox(Card)->SetPadding(FMargin(0,0,0,5));
        Card->TakeWidget(); if (FocusAfterRebuild==Selection->Items[I].Identity) Card->FocusHandle();
    }
    FocusAfterRebuild.Invalidate();
    Visible(AppendTarget,!bCollapsed && (Editable ? Array || Selection->Items.IsEmpty() : Selection->Items.IsEmpty()));
    if (AppendTarget) AppendTarget->SetIsEnabled(!Editable || IsEditable());
    Visible(BrowseButton,Editable); Visible(OtherSourcesButton,Editable);
    if (BrowseLabel) BrowseLabel->SetText(FText::FromString(Array ? TEXT("Add files...") : TEXT("Browse...")));
    if (EmptyHint) EmptyHint->SetText(FText::FromString(Editable ? (Array ? TEXT("Drop files or Unreal assets here to append") : TEXT("Drop a compatible file or Unreal asset")) : Array ? (Mode==EAtlasFilePresentation::Output ? TEXT("Empty output list") : TEXT("Empty input list")) : TEXT("File unavailable")));
    if (bOmitted && EmptyHint) EmptyHint->SetText(FText::FromString(TEXT("Omitted - Uses server default")));
    ShowError(Selection->Error);
}
void UAtlasFilePanel::ToggleCollapsed() { bCollapsed=!bCollapsed; RebuildCards(); }
bool UAtlasFilePanel::Drop(const TArray<FString>& Paths,const TArray<UObject*>& Assets,const FGuid& Target,bool Commit,FString& Hint) {
    if (!IsEditable()) { Hint=TEXT("This input is read-only or uses the server default"); return false; }
    const bool Single=!Selection->Definition.bIsArray || Target.IsValid(); TArray<FAtlasFileChoice> Choices;
    if (!Selection->ResolveDrop(Paths,Assets,Single,Choices,Hint)) return false;
    const int32 Index=Target.IsValid() ? Selection->Find(Target) : INDEX_NONE;
    if (Target.IsValid() && Index==INDEX_NONE) { Hint=TEXT("Original item is no longer available"); return false; }
    Hint=Target.IsValid() ? FString::Printf(TEXT("Replace item %d"),Index+1) : Single ? TEXT("Replace input") : TEXT("Append items");
    return !Commit || Selection->Apply(Choices,Target,Selection->Revision);
}
bool UAtlasFilePanel::Reorder(const FGuid& Id,const FGuid& Target,bool Commit,FString& Hint) {
    if (!IsEditable() || !Selection->Definition.bIsArray || Selection->Find(Id)==INDEX_NONE) { Hint=TEXT("Reorder within the same editable input"); return false; }
    const int32 To=Target.IsValid() ? Selection->Find(Target) : Selection->Items.Num()-1;
    if (To<0) { Hint=TEXT("Target item no longer exists"); return false; }
    Hint=FString::Printf(TEXT("Move to position %d"),To+1);
    if (!Commit) return true; FocusAfterRebuild=Id; return Selection->Move(Id,To);
}
void UAtlasFilePanel::Move(const FGuid& Id,int32 Delta) { if (IsEditable()) { FocusAfterRebuild=Id; Selection->Move(Id,Selection->Find(Id)+Delta); } }
void UAtlasFilePanel::Browse() {
    if (!IsEditable()) return; auto* Desktop=FDesktopPlatformModule::Get(); if (!Desktop) return;
    const int64 Revision=Selection->Revision; TArray<FString> Paths;
    const FString Filter=UAtlasUIHelpers::GetFileFilterForType(Selection->Definition.Type,Selection->Definition.GetSelectionExtensions());
    if (!Desktop->OpenFileDialog(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),TEXT("Select Atlas input files"),FString(),FString(),Filter,
        Selection->Definition.bIsArray ? EFileDialogFlags::Multiple : 0,Paths)) return;
    if (!IsEditable() || Selection->Revision!=Revision) { ShowError(TEXT("Input changed while choosing files; reopen the picker")); return; }
    // Preserve native picker order. Users can reorder explicitly after adding.
    TArray<FAtlasFileChoice> Choices; FString Error;
    if (!Selection->ResolveDrop(Paths,{},!Selection->Definition.bIsArray,Choices,Error) || !Selection->Apply(Choices,FGuid(),Revision)) ShowError(Error.IsEmpty() ? Selection->Error : Error);
}
void UAtlasFilePanel::OtherSources() { OpenSource(); }
FString UAtlasFilePanel::LocalPath(const FAtlasFileChoice& Choice,int32 Index) const {
    if (Mode==EAtlasFilePresentation::Output) return OutputManager ? OutputManager->GetOutputItemPath(Record,ParameterName,Index) : FString();
    return FPaths::FileExists(Choice.File.FilePath) ? Choice.File.FilePath : FString();
}
bool UAtlasFilePanel::CanImport(const FAtlasFileChoice& Choice) const {
    if (Mode!=EAtlasFilePresentation::Output || !Selection) return false;
    const auto& Def=Selection->Definition; const FString Ext=FPaths::GetExtension(Choice.File.FileName).ToLower();
    if (Def.Type==EAtlasValueType::Image) {
        for (const auto& Format : GetDefault<UTextureFactory>()->Formats) if (Format.StartsWith(Ext+TEXT(";"),ESearchCase::IgnoreCase)) return true;
    }
    if (Def.Type==EAtlasValueType::Audio) for (const auto& Format : GetDefault<USoundFactory>()->Formats) if (Format.StartsWith(Ext+TEXT(";"),ESearchCase::IgnoreCase)) return true;
    return Def.Type==EAtlasValueType::Mesh && Def.OriginalType!=TEXT("rig_animation") && (Ext==TEXT("glb") || Ext==TEXT("gltf") || Ext==TEXT("fbx") || Ext==TEXT("obj"));
}
UObject* UAtlasFilePanel::FindImported(int32 Index) const { return Mode==EAtlasFilePresentation::Output && OutputManager ? OutputManager->FindImportedOutputItem(Record,ParameterName,Index).ImportedAsset.Get() : nullptr; }
bool UAtlasFilePanel::HasImported(int32 Index) const { return Mode==EAtlasFilePresentation::Output && OutputManager && OutputManager->FindImportedOutputItem(Record,ParameterName,Index,false).bSuccess; }
void UAtlasFilePanel::Import(const FGuid& Id,bool AddToLevel) {
    const auto* Choice=FindChoice(this,Id); if (!Choice || !CanImport(*Choice) || Importing.Contains(Id)) return;
    const int32 Index=Selection->Definition.bIsArray ? Selection->Find(Id) : INDEX_NONE;
    const auto Snapshot=Record; const FString Name=ParameterName; const int64 Revision=Selection->Revision;
    Importing.Add(Id); ImportErrors.Remove(Id); RebuildCards();
    TWeakObjectPtr<UAtlasFilePanel> Weak(this);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,Id,Index,Snapshot,Name,Revision,AddToLevel](float) {
        auto* P=Weak.Get(); if (!P || !P->bReady || !P->Selection || P->Selection->Revision!=Revision) return false;
        TStrongObjectPtr<UAtlasOutputManager> Manager(P->OutputManager);
        auto Result=Manager->ImportOutputItem(Snapshot,Name,Index);
        if (AddToLevel && Result.bSuccess) if (auto* Mesh=Cast<UStaticMesh>(Result.ImportedAsset)) {
            UWorld* World=GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
            if (World && World->GetCurrentLevel()) { const FScopedTransaction Transaction(NSLOCTEXT("Atlas","AddFileToLevel","Add Atlas mesh to level"));
                if (auto* Actor=Cast<AStaticMeshActor>(GEditor->AddActor(World->GetCurrentLevel(),AStaticMeshActor::StaticClass(),FTransform::Identity,true,RF_Transactional))) {
                    Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh); Actor->SetActorLabel(Mesh->GetName()); Actor->MarkPackageDirty();
                }
            }
        }
        P=Weak.Get(); if (P && P->Selection && P->Selection->Revision==Revision) { P->Importing.Remove(Id); if (!Result.bSuccess) P->ImportErrors.Add(Id,Result.ErrorMessage); P->RebuildCards(); }
        return false;
    }));
}
void UAtlasFilePanel::ImportAll() {
    if (Mode!=EAtlasFilePresentation::Output || !Selection) return;
    const auto Items=Selection->Items;
    for (int32 I=0; I<Items.Num(); ++I) if (CanImport(Items[I]) && !HasImported(I) && !LocalPath(Items[I],I).IsEmpty()) Import(Items[I].Identity);
}
void UAtlasFilePanel::OpenMenu(const FGuid& Id) {
    auto* C=FindChoice(this,Id); if (!C || Mode==EAtlasFilePresentation::RecordedInput) return;
    const int32 Index=Selection->Definition.bIsArray ? Selection->Find(Id) : INDEX_NONE;
    FMenuBuilder Menu(true,nullptr); TWeakObjectPtr<UAtlasFilePanel> Weak(this);
    if (IsEditable()) {
        if (Selection->Definition.bIsArray) {
            Menu.AddMenuEntry(FText::FromString(TEXT("Move up")),FText(),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([Weak,Id]{if (Weak.IsValid()) Weak->Move(Id,-1);} )));
            Menu.AddMenuEntry(FText::FromString(TEXT("Move down")),FText(),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([Weak,Id]{if (Weak.IsValid()) Weak->Move(Id,1);} )));
        }
    } else {
        const FString Path=LocalPath(*C,Index), Fid=C->File.FileId;
        if (!Path.IsEmpty()) {
            Menu.AddMenuEntry(FText::FromString(TEXT("Reveal downloaded file")),FText(),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([Path]{FPlatformProcess::ExploreFolder(*FPaths::GetPath(Path));})));
            Menu.AddMenuEntry(FText::FromString(TEXT("Save as...")),FText(),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([Path]{
                auto* D=FDesktopPlatformModule::Get(); TArray<FString> Dest;
                if (D && D->SaveFileDialog(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),TEXT("Save Atlas output"),FPaths::GetPath(Path),FPaths::GetCleanFilename(Path),TEXT("All files|*.*"),0,Dest) && !Dest.IsEmpty()) {
                    if (FPaths::IsSamePath(Path,Dest[0])) return;
                    // The native Save dialog obtains overwrite confirmation.
                    if (IFileManager::Get().Copy(*Dest[0],*Path,true)!=COPY_OK) UAtlasUIHelpers::ShowNotification(TEXT("Could not save output copy"),false);
                }
            })));
        }
        if (!Fid.IsEmpty()) Menu.AddMenuEntry(FText::FromString(TEXT("Copy Atlas asset ID")),FText(),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([Fid]{UAtlasUIHelpers::CopyToClipboard(Fid);})));
        if (auto* Imported=FindImported(Index); Imported && !Path.IsEmpty()) {
            TWeakObjectPtr<UObject> Asset(Imported);
            Menu.AddMenuEntry(FText::FromString(TEXT("Reimport")),FText(),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([Asset]{if (Asset.IsValid()) FReimportManager::Instance()->Reimport(Asset.Get(),true,true);} )));
        }
        if (Selection->Definition.Type==EAtlasValueType::Mesh && CanImport(*C)) Menu.AddMenuEntry(FText::FromString(FindImported(Index) ? TEXT("Add to Level") : TEXT("Import & Add to Level")),FText(),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([Weak,Id]{if (Weak.IsValid()) Weak->Import(Id,true);} )));
    }
    FSlateApplication::Get().PushMenu(TakeWidget(),FWidgetPath(),Menu.MakeWidget(),FSlateApplication::Get().GetCursorPos(),FPopupTransitionEffect::ContextMenu);
}
void UAtlasFilePanel::Preview(const FGuid& Id) {
    const auto* Choice=FindChoice(this,Id); if (!Choice) return;
    if (Choice->Asset && Choice->Asset->IsA<UStaticMesh>()) { if (GEditor) GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Choice->Asset); return; }
    const int32 Index=Selection->Definition.bIsArray ? Selection->Find(Id) : INDEX_NONE;
    const FString Path=LocalPath(*Choice,Index);
    if ((Selection->Definition.Type==EAtlasValueType::Audio || Selection->Definition.OriginalType==TEXT("video")) && !Path.IsEmpty()) { FPlatformProcess::LaunchFileInDefaultExternalApplication(*Path); return; }
    auto Open=[](UTexture2D* Texture) {
        if (!Texture) return;
        struct FPreview { TStrongObjectPtr<UTexture2D> Texture; FSlateBrush Brush; FPreview(UTexture2D* T):Texture(T) { Brush.SetResourceObject(T); Brush.ImageSize=FVector2D(T->GetSizeX(),T->GetSizeY()); } };
        auto State=MakeShared<FPreview>(Texture);
        auto Window=SNew(SWindow).Title(FText::FromString(TEXT("Atlas image preview"))).ClientSize(FVector2D(760,620))
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&State->Brush)]];
        Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([State](const TSharedRef<SWindow>&){}));
        FSlateApplication::Get().AddWindow(Window);
    };
    if (auto* Texture=Cast<UTexture2D>(Choice->Asset)) { Open(Texture); return; }
    if (ImageType(Selection->Definition) && !Path.IsEmpty()) {
        const int64 Revision=Selection->Revision; TWeakObjectPtr<UAtlasFilePanel> Weak(this);
        OutputManager->LoadPreviewAsync(Path,[Weak,Revision,Open](UTexture2D* Texture){if (Weak.IsValid() && Weak->bReady && Weak->Selection->Revision==Revision) Open(Texture);});
    }
}
void UAtlasFileCard::Configure(UAtlasFilePanel* Panel,const FAtlasFileChoice& InChoice,int32 Index) { Owner=Panel; Choice=InChoice; ItemIndex=Index; Refresh(); }
void UAtlasFileCard::NativeConstruct() {
    Super::NativeConstruct(); bReady=true;
    if (PrimaryButton) PrimaryButton->OnClicked.AddUniqueDynamic(this,&ThisClass::Primary);
    if (RemoveButton) RemoveButton->OnClicked.AddUniqueDynamic(this,&ThisClass::Remove);
    if (MoreButton) MoreButton->OnClicked.AddUniqueDynamic(this,&ThisClass::More);
    if (PreviewButton) PreviewButton->OnClicked.AddUniqueDynamic(this,&ThisClass::Preview);
    Refresh();
}
void UAtlasFileCard::NativeDestruct() {
    bReady=false; ++PreviewRevision; PreviewTexture=nullptr;
    if (IsValid(PrimaryButton)) PrimaryButton->OnClicked.RemoveDynamic(this,&ThisClass::Primary);
    if (IsValid(RemoveButton)) RemoveButton->OnClicked.RemoveDynamic(this,&ThisClass::Remove);
    if (IsValid(MoreButton)) MoreButton->OnClicked.RemoveDynamic(this,&ThisClass::More);
    if (IsValid(PreviewButton)) PreviewButton->OnClicked.RemoveDynamic(this,&ThisClass::Preview);
    Super::NativeDestruct();
}
void UAtlasFileCard::Refresh() {
    auto* P=Owner.Get(); if (!bReady || !P || !P->Selection || !Filename) return;
    ++PreviewRevision; bPreviewRequested=false; PreviewTexture=nullptr;
    DropTarget->OwnerPanel=P; DropTarget->TargetId=Choice.Identity; ReorderHandle->OwnerPanel=P; ReorderHandle->TargetId=Choice.Identity;
    const bool Edit=P->Mode==EAtlasFilePresentation::Editable, Output=P->Mode==EAtlasFilePresentation::Output, Busy=P->IsImporting(Choice.Identity);
    const FString Path=P->LocalPath(Choice,ItemIndex); const bool Imported=Output && P->HasImported(ItemIndex);
    FString Name=Choice.Asset ? Choice.Asset->GetName() : !Choice.File.OriginalFileName.IsEmpty() ? Choice.File.OriginalFileName : !Choice.File.FileName.IsEmpty() ? Choice.File.FileName : FPaths::GetCleanFilename(Choice.File.FilePath);
    if (Name.IsEmpty()) Name=Choice.File.FileId.IsEmpty() ? TEXT("File unavailable") : TEXT("Atlas asset ID");
    Filename->SetText(FText::FromString((ItemIndex==INDEX_NONE ? FString() : FString::Printf(TEXT("%02d  "),ItemIndex+1))+Name));
    SetToolTipText(FText::FromString(Choice.File.FilePath+TEXT("\n")+Choice.File.FileId));
    FString Format=FPaths::GetExtension(Name).ToUpper(); if (Format.IsEmpty()) Format=P->Selection->Definition.OriginalType.ToUpper();
    const FString Size=SizeText(Path); Metadata->SetText(FText::FromString(Format+(Size.IsEmpty() ? FString() : TEXT(" · ")+Size)));
    FString Status=Edit ? P->Selection->SourceDescription(Choice) : Path.IsEmpty() ? Choice.File.FilePath.IsEmpty() && !Choice.File.FileId.IsEmpty() ? TEXT("Atlas asset ID · No local preview") : TEXT("Input file unavailable") : TEXT("Recorded file · Original bytes");
    FString Error; if (Edit && !P->Selection->ValidateChoice(Choice,Error)) Status=Error;
    if (Output) Status=Busy ? TEXT("Importing...") : Imported ? TEXT("Imported") : !Choice.File.Warning.IsEmpty() ? Choice.File.Warning : Path.IsEmpty() ? TEXT("Downloaded file unavailable · ID retained") : TEXT("Downloaded original");
    if (!P->ImportError(Choice.Identity).IsEmpty()) Status=TEXT("Import failed: ")+P->ImportError(Choice.Identity);
    SourceStatus->SetText(FText::FromString(Status)); SourceStatus->SetToolTipText(FText::FromString(Status));
    SourceStatus->SetColorAndOpacity(FSlateColor(!Error.IsEmpty() || !Choice.File.Warning.IsEmpty() || !P->ImportError(Choice.Identity).IsEmpty() ? FLinearColor(.94f,.64f,.52f) : Imported ? FLinearColor(.56f,.77f,.62f) : FLinearColor(.65f,.65f,.65f)));
    Visible(ReorderHandle,Edit && ItemIndex!=INDEX_NONE); Visible(RemoveButton,Edit); Visible(MoreButton,(Edit && ItemIndex!=INDEX_NONE) || Output);
    if (auto* Bounds=Cast<USizeBox>(MoreButton->GetParent())) Visible(Bounds,(Edit && ItemIndex!=INDEX_NONE) || Output);
    MoreButton->SetToolTipText(FText::FromString(TEXT("More file actions")));
    RemoveButton->SetToolTipText(FText::FromString(ItemIndex==INDEX_NONE ? TEXT("Clear this input") : TEXT("Remove this item")));
    Visible(PrimaryButton,Edit || (Output && (Imported || P->CanImport(Choice))));
    PrimaryLabel->SetText(FText::FromString(Edit ? TEXT("Replace") : Busy ? TEXT("Importing...") : Imported ? TEXT("Show in Browser") : !P->ImportError(Choice.Identity).IsEmpty() ? TEXT("Retry") : TEXT("Import")));
    PrimaryButton->SetToolTipText(FText::FromString(Imported ? TEXT("Show imported asset in Content Browser") : TEXT("")));
    PrimaryButton->SetIsEnabled(Edit ? P->IsEditable() : !Busy && (Imported || !Path.IsEmpty()));
    RemoveLabel->SetText(FText::FromString(ItemIndex==INDEX_NONE ? TEXT("Clear") : TEXT("×"))); RemoveButton->SetIsEnabled(P->IsEditable()); MoreButton->SetIsEnabled(!Busy && (!Edit || P->IsEditable()));
    TypeIcon->SetText(FText::FromString(P->Selection->Definition.Type==EAtlasValueType::Image ? TEXT("IMG") : P->Selection->Definition.Type==EAtlasValueType::Mesh ? TEXT("3D") : P->Selection->Definition.Type==EAtlasValueType::Audio ? TEXT("AUDIO") : TEXT("FILE")));
    Thumbnail->SetBrushFromTexture(nullptr); Visible(Thumbnail,false); Visible(TypeIcon,true);
    PreviewButton->SetIsEnabled(Choice.Asset || (!Path.IsEmpty() && (ImageType(P->Selection->Definition) || P->Selection->Definition.Type==EAtlasValueType::Audio || P->Selection->Definition.OriginalType==TEXT("video"))));
}
int32 UAtlasFileCard::NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool Enabled) const {
    if (!bPreviewRequested && bReady) {
        auto* Self=const_cast<UAtlasFileCard*>(this); Self->bPreviewRequested=true; TWeakObjectPtr<UAtlasFileCard> Weak(Self);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float){if (Weak.IsValid() && Weak->bReady) Weak->RequestPreview(); return false;}));
    }
    return Super::NativePaint(Args,Geometry,Culling,Elements,Layer,Style,Enabled);
}
void UAtlasFileCard::RequestPreview() {
    auto* P=Owner.Get(); if (!P || !P->Selection || !ImageType(P->Selection->Definition)) return;
    const int64 Revision=PreviewRevision; TWeakObjectPtr<UAtlasFileCard> Weak(this);
    auto Apply=[Weak,Revision](UTexture2D* Texture) { if (Weak.IsValid() && Weak->bReady && Weak->PreviewRevision==Revision && Texture) { Weak->PreviewTexture=Texture; Weak->Thumbnail->SetBrushFromTexture(Texture,true); Visible(Weak->Thumbnail,true); Visible(Weak->TypeIcon,false); } };
    if (auto* Texture=Cast<UTexture2D>(Choice.Asset)) Apply(Texture);
    else { const FString Path=P->LocalPath(Choice,ItemIndex); if (!Path.IsEmpty()) P->OutputManager->LoadPreviewAsync(Path,Apply); }
}
void UAtlasFileCard::FocusHandle() { if (ReorderHandle) ReorderHandle->SetKeyboardFocus(); }
void UAtlasFileCard::Primary() { if (auto* P=Owner.Get()) { if (P->IsEditable()) P->OpenSource(Choice.Identity); else if (P->Mode==EAtlasFilePresentation::Output) { if (auto* Asset=P->FindImported(ItemIndex)) ShowInBrowser(Asset); else P->Import(Choice.Identity); } } }
void UAtlasFileCard::Remove() { if (auto* P=Owner.Get(); P && P->IsEditable()) P->Selection->Remove(Choice.Identity); }
void UAtlasFileCard::More() { if (Owner.IsValid()) Owner->OpenMenu(Choice.Identity); }
void UAtlasFileCard::Preview() { if (Owner.IsValid()) Owner->Preview(Choice.Identity); }
