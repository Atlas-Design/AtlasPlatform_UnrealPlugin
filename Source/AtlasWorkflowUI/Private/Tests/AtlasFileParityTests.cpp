#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AtlasFileWidgets.h"
#include "AtlasNativeInputWidgets.h"
#include "AtlasOutputManager.h"
#include "AtlasSDKSettings.h"
#include "AtlasNativeUITestObserver.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "EditorFramework/AssetImportData.h"
#include "Editor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasFileParitySelection,"Atlas.NativeUI.FileParitySelection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAtlasFileParitySelection::RunTest(const FString&) {
    const FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Automation/FileParity")/FGuid::NewGuid().ToString(EGuidFormats::Digits));
    IFileManager::Get().MakeDirectory(*Root,true);
    const FString A=Root/TEXT("image.png"), B=Root/TEXT("second.JPEG"), Hdr=Root/TEXT("source.EXR");
    for (const auto& P : {A,B,Hdr}) FFileHelper::SaveStringToFile(TEXT("Selection-only fixture"),*P);
    TStrongObjectPtr<UAtlasFileSelection> Model(NewObject<UAtlasFileSelection>());
    FAtlasParameterDef Def; Def.Name=TEXT("image"); Def.Type=EAtlasValueType::Image; Def.OriginalType=TEXT("image");
    Model->Configure(Def,FAtlasValue(),FString()); TArray<FAtlasFileChoice> Choices; FString Error;
    TestTrue(TEXT("Drop path accepted"),Model->ResolveDrop({A},{},true,Choices,Error));
    TestTrue(TEXT("Scalar applies"),Model->Apply(Choices,FGuid(),Model->Revision)); const FGuid Id=Model->Items[0].Identity;
    TestFalse(TEXT("Scalar refuses multiple files"),Model->ResolveDrop({A,B},{},true,Choices,Error));
    TestEqual(TEXT("Rejected drop preserves scalar identity"),Model->Items[0].Identity,Id);
    TestFalse(TEXT("Folder mixed with valid file rejects entire payload"),Model->ResolveDrop({B,Root},{},false,Choices,Error)); TestTrue(TEXT("Rejected payload has no partial result"),Choices.IsEmpty());
    const int64 Stale=Model->Revision; Model->SetMutationEnabled(false);
    FAtlasFileChoice Choice; Choice.File.FilePath=B;
    TestFalse(TEXT("Disabled selector rejects draft"),Model->Apply({Choice},Id,Stale)); Model->SetMutationEnabled(true);
    TestFalse(TEXT("Re-enabled selector still rejects stale draft"),Model->Apply({Choice},Id,Stale));
    TestTrue(TEXT("Fresh replacement accepted"),Model->Apply({Choice},Id,Model->Revision));
    TestEqual(TEXT("Replacement retains item identity"),Model->Items[0].Identity,Id);
    Choice.Source=EAtlasFileSource::AtlasId; Choice.File.FileId=TEXT("8d6931f1-24e0-44c0-a626-ed47e9326e7a");
    TestTrue(TEXT("Valid ID replaces local source"),Model->Apply({Choice},Id,Model->Revision)); TestTrue(TEXT("ID clears stale path"),Model->Items[0].File.FilePath.IsEmpty());
    Choice.File.FileId=TEXT("bad"); TestFalse(TEXT("Invalid UUID rejected"),Model->Apply({Choice},Id,Model->Revision));
    Choice.Source=EAtlasFileSource::File; Choice.File.FilePath=A;
    TestTrue(TEXT("File replaces ID"),Model->Apply({Choice},Id,Model->Revision)); TestTrue(TEXT("File clears stale ID"),Model->Items[0].File.FileId.IsEmpty());
    Def.bIsArray=true; Model->Configure(Def,FAtlasValue(),FString());
    TestTrue(TEXT("Ordered duplicate files resolve"),Model->ResolveDrop({A,B,A},{},false,Choices,Error)); TestTrue(TEXT("Append list"),Model->Apply(Choices,FGuid(),Model->Revision));
    TestEqual(TEXT("Duplicates preserved"),Model->Items.Num(),3); const auto Last=Model->Items[2].Identity;
    TestTrue(TEXT("Move last to first"),Model->Move(Last,0)); TestEqual(TEXT("Stable moved identity"),Model->Items[0].Identity,Last);
    Model->Remove(Model->Items[1].Identity); TestEqual(TEXT("Remove one occurrence only"),Model->Items.Num(),2);
    TestEqual(TEXT("Remaining order retained"),Model->Items[1].File.FilePath,B);
    TStrongObjectPtr<UTexture2D> Texture(NewObject<UTexture2D>()); Texture->AssetImportData=NewObject<UAssetImportData>(Texture.Get()); Texture->AssetImportData->UpdateFilenameOnly(A);
    TestTrue(TEXT("Object plus its original source normalized"),Model->ResolveDrop({A,B},{Texture.Get()},false,Choices,Error)); TestEqual(TEXT("Unrelated file preserved"),Choices.Num(),2);
    TestTrue(TEXT("Engine source does not export during selection"),Choices[0].File.FilePath.IsEmpty());
    Def.OriginalType=TEXT("exr"); Def.bIsArray=false; Model->Configure(Def,FAtlasValue(),FString());
    Choice.Source=EAtlasFileSource::UnrealAsset; Choice.Asset=Texture.Get();
    TestFalse(TEXT("PNG source cannot satisfy EXR"),Model->ValidateChoice(Choice,Error));
    Texture->AssetImportData->UpdateFilenameOnly(Hdr);
    TestTrue(TEXT("Original EXR source accepted"),Model->Apply({Choice},FGuid(),Model->Revision));
    FAtlasValue Prepared; TestTrue(TEXT("Prepare uses original HDR source"),Model->Prepare(Prepared,Error)); TestEqual(TEXT("No HDR conversion"),Prepared.FilePath,Hdr);
    Def.Type=EAtlasValueType::Mesh; Def.OriginalType=TEXT("mesh"); Model->Configure(Def,FAtlasValue(),FString()); Choice.Asset=NewObject<UStaticMesh>();
    TestFalse(TEXT("Empty mesh rejected before export"),Model->ValidateChoice(Choice,Error));
    Def.Type=EAtlasValueType::Image; Def.OriginalType=TEXT("image"); Def.bHasExplicitFormat=true; Def.Format=TEXT("tiff"); Def.AllowedExtensions={TEXT("png")};
    TestTrue(TEXT("Explicit format and aliases authoritative"),Def.GetSelectionExtensions().Contains(TEXT("tif")) && !Def.GetSelectionExtensions().Contains(TEXT("png")));
    Def.bHasExplicitFormat=false; Def.AllowedExtensions.Empty(); Def.bHasRequiredMetadata=true; Def.bRequired=false; Def.bIsArray=true;
    UWorld* World=GEditor->GetEditorWorldContext().World();
    UClass* Class=LoadClass<UAtlasInputFieldWidget>(nullptr,TEXT("/AtlasWorkflow/Core/UI/Editor/Utilities/EUW_InputField.EUW_InputField_C"));
    TStrongObjectPtr<UAtlasInputFieldWidget> Field(CreateWidget<UAtlasInputFieldWidget>(World,Class)); Field->currentDefParameter=Def;
    TSharedPtr<SWidget> Slate=Field->TakeWidget();
    if (!TestNotNull(TEXT("Designer file panel bound"),Field->FileSelector.Get())) return false;
    if (!TestNotNull(TEXT("Designer card class assigned"),Field->FileSelector->CardClass.Get())) return false;
    auto* Panel=Field->FileSelector.Get();
    TestTrue(TEXT("UI append"),Panel->Drop({A,B,A},{},FGuid(),true,Error));
    auto* Cards=Cast<UVerticalBox>(Panel->GetWidgetFromName(TEXT("Cards"))); if (!TestEqual(TEXT("Three actual Designer cards"),Cards->GetChildrenCount(),3)) { AddError(TEXT("Panel state: editable=")+FString::FromInt(Panel->IsEditable())+TEXT(" items=")+FString::FromInt(Panel->Selection->Items.Num())+TEXT(" error=")+Error); return false; }
    auto* Card=Cast<UAtlasFileCard>(Cards->GetChildAt(0)); const auto First=Card->Choice.Identity;
    TestFalse(TEXT("Existing card rejects multi-file replacement"),Panel->Drop({A,B},{},First,true,Error)); TestEqual(TEXT("No parent append after rejection"),Panel->Selection->Items.Num(),3);
    TStrongObjectPtr<UAtlasNativeUITestObserver> Observer(NewObject<UAtlasNativeUITestObserver>()); Field->OnInputUpdated.AddDynamic(Observer.Get(),&UAtlasNativeUITestObserver::Changed);
    Field->UseServerDefault->SetIsChecked(true); Field->UseServerDefault->OnCheckStateChanged.Broadcast(true);
    TestTrue(TEXT("Omission is explicit and disables mutations"),Field->bUseServerDefault && !Panel->IsEditable());
    TestFalse(TEXT("Disabled drop rejected"),Panel->Drop({B},{},First,true,Error)); TestTrue(TEXT("Omitted input validates"),Field->ValidateSelection(Error));
    Field->UseServerDefault->SetIsChecked(false); Field->UseServerDefault->OnCheckStateChanged.Broadcast(false);
    TestEqual(TEXT("Toggle restores exact selection"),Panel->Selection->Items[0].Identity,First);
    const auto Snapshot=Panel->Selection->GetReferenceValue();
    Panel->Configure(Def,Snapshot,EAtlasFilePresentation::RecordedInput);
    Card=Cast<UAtlasFileCard>(Cards->GetChildAt(0));
    for (const TCHAR* N : {TEXT("PrimaryButton"),TEXT("RemoveButton"),TEXT("MoreButton"),TEXT("ReorderHandle")}) TestTrue(FString(TEXT("Recorded hides "))+N,Card->GetWidgetFromName(N)->GetVisibility()==ESlateVisibility::Collapsed);
    TestFalse(TEXT("Recorded input rejects mutation"),Panel->Drop({A},{},FGuid(),true,Error));
    FAtlasValue Omitted; Omitted.Type=Def.Type; Omitted.bIsArray=true; Omitted.bOmitted=true; Panel->Configure(Def,Omitted,EAtlasFilePresentation::RecordedInput);
    TestEqual(TEXT("Omitted history message"),Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("EmptyHint")))->GetText().ToString(),FString(TEXT("Omitted - Uses server default")));
    Field->OnInputUpdated.RemoveDynamic(Observer.Get(),&UAtlasNativeUITestObserver::Changed);
    Slate.Reset(); Field->ReleaseSlateResources(true); return true;
}
#endif
