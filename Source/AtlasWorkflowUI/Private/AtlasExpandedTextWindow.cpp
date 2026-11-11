#include "AtlasNativeInputWidgets.h"
#include "Components/MultiLineEditableText.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Application/SlateApplication.h"

void UAtlasInputFieldWidget::ExpandText()
{
    if (bUseServerDefault || !EditableText_string) return;
    const int64 Revision=DefinitionRevision;
    TWeakObjectPtr<UAtlasInputFieldWidget> Weak(this);
    auto Draft=MakeShared<FString>(EditableText_string->GetText().ToString());
    auto Window=SNew(SWindow).Title(FText::FromString(currentDefParameter.Name)).ClientSize(FVector2D(640,420));
    TWeakPtr<SWindow> WeakWindow=Window;
    auto CanApply=[Weak,Revision]{return Weak.IsValid() && Weak->DefinitionRevision==Revision && !Weak->bUseServerDefault && IsValid(Weak->EditableText_string);};
    Window->SetContent(SNew(SBorder).Padding(12)[SNew(SVerticalBox)
        +SVerticalBox::Slot().FillHeight(1)[SNew(SMultiLineEditableTextBox).Text(FText::FromString(*Draft)).OnTextChanged_Lambda([Draft](const FText& T){*Draft=T.ToString();})]
        +SVerticalBox::Slot().AutoHeight().Padding(0,8).HAlign(HAlign_Right)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[SNew(SButton).Text(FText::FromString(TEXT("Cancel"))).OnClicked_Lambda([WeakWindow]{if(auto W=WeakWindow.Pin()) W->RequestDestroyWindow(); return FReply::Handled();})]
            +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("Use text"))).IsEnabled_Lambda(CanApply).OnClicked_Lambda([Weak,WeakWindow,Draft,CanApply]{
                if(CanApply()) { Weak->bUpdating=true; Weak->EditableText_string->SetText(FText::FromString(*Draft)); Weak->bUpdating=false; ++Weak->DefinitionRevision; Weak->NotifyChanged(); if(auto W=WeakWindow.Pin()) W->RequestDestroyWindow(); }
                return FReply::Handled();
            })]]]);
    FSlateApplication::Get().AddWindow(Window);
}
