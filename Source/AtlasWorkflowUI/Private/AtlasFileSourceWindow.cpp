#include "AtlasFileWidgets.h"
#include "AtlasUIHelpers.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundWave.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/StrongObjectPtr.h"

void UAtlasFilePanel::OpenSource(const FGuid& Id)
{
    if (!IsEditable()) return;
    const int64 Revision=Selection->Revision;
    struct FDraft {
        EAtlasFileSource Source=EAtlasFileSource::File; FString Path,Id,Error;
        TStrongObjectPtr<UObject> Asset;
        FAtlasFileChoice Choice() const { FAtlasFileChoice C; C.Source=Source; C.Asset=Asset.Get(); C.File.FilePath=Path; C.File.FileId=Id; return C; }
    };
    auto Draft=MakeShared<FDraft>();
    const int32 Index=Selection->Find(Id);
    if (Index!=INDEX_NONE) { const auto& Current=Selection->Items[Index]; Draft->Source=Current.Source; Draft->Path=Current.File.FilePath; Draft->Id=Current.File.FileId; Draft->Asset.Reset(Current.Asset); }
    auto Window=SNew(SWindow).Title(FText::FromString(TEXT("Choose Atlas input source"))).ClientSize(FVector2D(520,250)).SupportsMaximize(false).SupportsMinimize(false);
    TWeakPtr<SWindow> WeakWindow=Window; TWeakObjectPtr<UAtlasFilePanel> Weak(this);
    auto SelectSource=[Draft](EAtlasFileSource Source) { if (Draft->Source!=Source) { Draft->Source=Source; Draft->Path.Empty(); Draft->Id.Empty(); Draft->Asset.Reset(); Draft->Error.Empty(); } return FReply::Handled(); };
    auto Validate=[Weak,Draft,Revision]() -> FString {
        if (!Weak.IsValid() || !Weak->IsEditable() || Weak->Selection->Revision!=Revision) return TEXT("Input changed; cancel and reopen this dialog");
        FString Error; Weak->Selection->ValidateChoice(Draft->Choice(),Error); return Draft->Error.IsEmpty() ? Error : Draft->Error;
    };
    UClass* AssetClass=Selection->Definition.Type==EAtlasValueType::Image ? UTexture2D::StaticClass() : Selection->Definition.Type==EAtlasValueType::Audio ? USoundWave::StaticClass() : UStaticMesh::StaticClass();
    const bool EngineAllowed=UAtlasFileSelection::CanSelectEngineAsset(Selection->Definition);
    Window->SetContent(SNew(SBorder).Padding(12)[SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)[SNew(SButton).Text(FText::FromString(TEXT("File"))).OnClicked_Lambda([SelectSource]{return SelectSource(EAtlasFileSource::File);})]
            +SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)[SNew(SButton).Text(FText::FromString(TEXT("Unreal asset"))).IsEnabled(EngineAllowed).OnClicked_Lambda([SelectSource]{return SelectSource(EAtlasFileSource::UnrealAsset);})]
            +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("Atlas asset ID"))).OnClicked_Lambda([SelectSource]{return SelectSource(EAtlasFileSource::AtlasId);})]]
        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SNew(SHorizontalBox).Visibility_Lambda([Draft]{return Draft->Source==EAtlasFileSource::File ? EVisibility::Visible : EVisibility::Collapsed;})
            +SHorizontalBox::Slot().FillWidth(1)[SNew(SEditableTextBox).Text_Lambda([Draft]{return FText::FromString(Draft->Path);}).HintText(FText::FromString(TEXT("Enter a file path"))).OnTextChanged_Lambda([Draft](const FText& T){Draft->Path=T.ToString(); Draft->Error.Empty();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(6,0)[SNew(SButton).Text(FText::FromString(TEXT("Browse..."))).OnClicked_Lambda([Weak,Draft]{
                if (Weak.IsValid() && Weak->IsEditable()) { FString Path; if (UAtlasUIHelpers::OpenFileDialogForType(Weak->Selection->Definition.Type,Weak->Selection->Definition.GetSelectionExtensions(),Path)) Draft->Path=Path; }
                return FReply::Handled();
            })]]
        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SNew(SBox).Visibility_Lambda([Draft]{return Draft->Source==EAtlasFileSource::UnrealAsset ? EVisibility::Visible : EVisibility::Collapsed;})[
            SNew(SObjectPropertyEntryBox).AllowedClass(AssetClass).ObjectPath_Lambda([Draft]{return Draft->Asset.IsValid() ? Draft->Asset->GetPathName() : FString();})
                .OnObjectChanged_Lambda([Draft](const FAssetData& Asset){Draft->Asset.Reset(Asset.GetAsset()); Draft->Error.Empty();})]]
        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SNew(SEditableTextBox).Visibility_Lambda([Draft]{return Draft->Source==EAtlasFileSource::AtlasId ? EVisibility::Visible : EVisibility::Collapsed;})
            .HintText(FText::FromString(TEXT("Atlas asset UUID"))).Text_Lambda([Draft]{return FText::FromString(Draft->Id);}).OnTextChanged_Lambda([Draft](const FText& T){Draft->Id=T.ToString(); Draft->Error.Empty();})]
        +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).AutoWrapText(true).Text_Lambda([Weak,Draft]{
            if (!Weak.IsValid() || !Weak->Selection) return FText();
            return FText::FromString(Draft->Source==EAtlasFileSource::AtlasId ? TEXT("Reused without upload. Workspace access is checked when the workflow runs.") :
                Draft->Source==EAtlasFileSource::UnrealAsset ? Weak->Selection->SourceDescription(Draft->Choice())+TEXT(" · Prepared only when you run") : TEXT("Accepted: ")+FString::Join(Weak->Selection->Definition.GetSelectionExtensions(),TEXT(", ")));
        })]
        +SVerticalBox::Slot().FillHeight(1).Padding(0,8)[SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FLinearColor(.94f,.64f,.52f)).Text_Lambda([Validate]{return FText::FromString(Validate());})]
        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[SNew(SButton).Text(FText::FromString(TEXT("Cancel"))).OnClicked_Lambda([WeakWindow]{if (auto W=WeakWindow.Pin()) W->RequestDestroyWindow(); return FReply::Handled();})]
            +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("Use selection"))).IsEnabled_Lambda([Validate]{return Validate().IsEmpty();}).OnClicked_Lambda([Weak,WeakWindow,Draft,Id,Revision]{
                if (Weak.IsValid() && Weak->IsEditable() && Weak->Selection->Apply({Draft->Choice()},Id,Revision)) { if (auto W=WeakWindow.Pin()) W->RequestDestroyWindow(); }
                else Draft->Error=TEXT("Input changed; cancel and reopen the selector");
                return FReply::Handled();
            })]]
    ]);
    FSlateApplication::Get().AddWindow(Window);
}
