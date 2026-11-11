#include "AtlasFileWidgets.h"
#include "Components/PanelSlot.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SNullWidget.h"
#include "Styling/AppStyle.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Input/DragAndDrop.h"
#include "InputCoreTypes.h"

class FAtlasReorderOperation : public FDragDropOperation
{
public:
    DRAG_DROP_OPERATOR_TYPE(FAtlasReorderOperation,FDragDropOperation)
    TWeakObjectPtr<UAtlasFilePanel> Owner;
    FGuid Item;
    int64 Revision=0;
    void Initialize() { Construct(); }
};
class SAtlasFileDropTarget : public SBorder
{
public:
    SLATE_BEGIN_ARGS(SAtlasFileDropTarget) {} SLATE_ARGUMENT(TWeakObjectPtr<UAtlasFileDropSurface>,Bridge) SLATE_END_ARGS()
    void Construct(const FArguments& Args) {
        Bridge=Args._Bridge;
        SBorder::Construct(SBorder::FArguments().Padding(1).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.23f,.23f,.23f)));
    }
    TWeakObjectPtr<UAtlasFileDropSurface> Bridge;
    UAtlasFilePanel* Panel() const { return Bridge.IsValid() ? Bridge->OwnerPanel.Get() : nullptr; }
    bool Resolve(const FDragDropEvent& Event,bool Commit,FString& Hint) {
        auto* P=Panel(); if (!P || !P->IsEditable()) { Hint=TEXT("This input is read-only or uses the server default"); return false; }
        if (auto Reorder=Event.GetOperationAs<FAtlasReorderOperation>()) {
            if (Reorder->Owner!=P || !P->Selection || Reorder->Revision!=P->Selection->Revision) { Hint=TEXT("Reorder within the original unchanged input"); return false; }
            return P->Reorder(Reorder->Item,Bridge->TargetId,Commit,Hint);
        }
        TArray<FString> Paths; TArray<UObject*> Assets;
        if (auto External=Event.GetOperationAs<FExternalDragOperation>()) { if (External->HasFiles()) Paths=External->GetFiles(); }
        if (auto Content=Event.GetOperationAs<FAssetDragDropOp>()) for (const auto& Asset : Content->GetAssets()) Assets.Add(Asset.GetAsset());
        return P->Drop(Paths,Assets,Bridge->TargetId,Commit,Hint);
    }
    virtual FReply OnDragOver(const FGeometry&,const FDragDropEvent& Event) override {
        FString Hint; const bool Valid=Resolve(Event,false,Hint);
        SetBorderBackgroundColor(Valid ? FLinearColor(.40f,.66f,.91f) : FLinearColor(.84f,.47f,.39f)); SetToolTipText(FText::FromString(Hint));
        if (auto Op=Event.GetOperation()) Op->SetCursorOverride(Valid ? EMouseCursor::GrabHandClosed : EMouseCursor::SlashedCircle);
        return FReply::Handled();
    }
    virtual void OnDragLeave(const FDragDropEvent& Event) override { SetBorderBackgroundColor(FLinearColor(.23f,.23f,.23f)); SetToolTipText(FText()); if (auto Op=Event.GetOperation()) Op->SetCursorOverride(TOptional<EMouseCursor::Type>()); }
    virtual FReply OnDrop(const FGeometry&,const FDragDropEvent& Event) override {
        FString Hint; if (!Resolve(Event,true,Hint)) if (auto* P=Panel()) P->ShowError(Hint);
        OnDragLeave(Event); return FReply::Handled(); // Never bubble a rejected replacement into parent append.
    }
    virtual bool SupportsKeyboardFocus() const override { return Bridge.IsValid() && Bridge->bReorderHandle; }
    virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent& Event) override {
        if (Bridge.IsValid() && Bridge->bReorderHandle && Panel() && Panel()->IsEditable() && Event.GetEffectingButton()==EKeys::LeftMouseButton)
            return FReply::Handled().SetUserFocus(SharedThis(this)).DetectDrag(SharedThis(this),EKeys::LeftMouseButton);
        return FReply::Unhandled();
    }
    virtual FReply OnDragDetected(const FGeometry&,const FPointerEvent&) override {
        auto* P=Panel(); if (!P || !P->IsEditable() || !Bridge->bReorderHandle) return FReply::Unhandled();
        auto Op=MakeShared<FAtlasReorderOperation>(); Op->Owner=P; Op->Item=Bridge->TargetId; Op->Revision=P->Selection->Revision; Op->Initialize();
        return FReply::Handled().BeginDragDrop(Op);
    }
    virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent& Event) override {
        if (Panel() && Bridge->bReorderHandle && Event.IsAltDown() && (Event.GetKey()==EKeys::Up || Event.GetKey()==EKeys::Down)) {
            Panel()->Move(Bridge->TargetId,Event.GetKey()==EKeys::Up ? -1 : 1); return FReply::Handled();
        }
        return FReply::Unhandled();
    }
};
TSharedRef<SWidget> UAtlasFileDropSurface::RebuildWidget() {
    Surface=SNew(SAtlasFileDropTarget).Bridge(this);
    Surface->SetContent(GetContent() ? GetContent()->TakeWidget() : SNullWidget::NullWidget); return Surface.ToSharedRef();
}
void UAtlasFileDropSurface::OnSlotAdded(UPanelSlot*) { if (Surface) Surface->SetContent(GetContent()->TakeWidget()); }
void UAtlasFileDropSurface::OnSlotRemoved(UPanelSlot*) { if (Surface) Surface->SetContent(SNullWidget::NullWidget); }
void UAtlasFileDropSurface::ReleaseSlateResources(bool Children) { Super::ReleaseSlateResources(Children); Surface.Reset(); }
