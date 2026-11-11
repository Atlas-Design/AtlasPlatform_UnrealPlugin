#include "AtlasFileSelection.h"
#include "AtlasPlatformAuth.h"
#include "AtlasSDKSettings.h"
#include "Utilities/AtlasAssetExporter.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundWave.h"
#include "EditorFramework/AssetImportData.h"
#include "Exporters/GLTFExporter.h"
#include "Options/GLTFExportOptions.h"
#include "HAL/FileManager.h"
#include "UObject/StrongObjectPtr.h"
namespace {
bool OriginalSource(UObject* Asset,FString& Path) {
    if (auto* Sound=Cast<USoundWave>(Asset)) { Path=Sound->AssetImportData ? Sound->AssetImportData->GetFirstFilename() : FString(); return FPaths::FileExists(Path); }
    return UAtlasAssetExporter::GetSourceFilePath(Asset,Path);
}
}
void UAtlasFileSelection::Configure(const FAtlasParameterDef& Def,const FAtlasValue& Value,const FString& Origin) {
    Definition=Def; ServiceOrigin=Origin; Items.Empty(); Error.Empty(); ++Revision;
    auto Add=[&](const FAtlasFileReference& Ref) { FAtlasFileChoice C; C.Identity=FGuid::NewGuid(); C.File=Ref; C.Source=Ref.FilePath.IsEmpty() && !Ref.FileId.IsEmpty() ? EAtlasFileSource::AtlasId : EAtlasFileSource::File; Items.Add(C); };
    if (Value.bIsArray) for (const auto& Ref : Value.FileItems) Add(Ref);
    else if (!Value.FilePath.IsEmpty() || !Value.FileId.IsEmpty()) {
        FAtlasFileReference R; R.FileId=Value.FileId; R.FilePath=Value.FilePath; R.FileName=Value.FileName; R.OriginalFileName=Value.OriginalFileName; R.SourceKind=Value.SourceKind; R.SourceAssetPath=Value.SourceAssetPath; R.SourceOrigin=Value.SourceOrigin; Add(R);
    }
    OnChanged.Broadcast();
}
void UAtlasFileSelection::Changed() { ++Revision; Error.Empty(); OnChanged.Broadcast(); }
void UAtlasFileSelection::SetMutationEnabled(bool Enabled) { if (bMutationEnabled!=Enabled) { bMutationEnabled=Enabled; ++Revision; OnChanged.Broadcast(); } }
int32 UAtlasFileSelection::Find(const FGuid& Id) const { return Items.IndexOfByPredicate([&](const auto& I){return I.Identity==Id;}); }
bool UAtlasFileSelection::UsesOriginalSource(const FAtlasParameterDef& Def) {
    const auto Ext=Def.GetSelectionExtensions();
    return Def.Type==EAtlasValueType::Audio || (Def.Type==EAtlasValueType::Image && ((!Def.OriginalType.IsEmpty() && Def.OriginalType!=TEXT("image")) || (!Ext.IsEmpty() && !Ext.Contains(TEXT("png")))));
}
bool UAtlasFileSelection::CanSelectEngineAsset(const FAtlasParameterDef& Def) {
    return Def.Type==EAtlasValueType::Image || Def.Type==EAtlasValueType::Audio || (Def.Type==EAtlasValueType::Mesh && Def.GetSelectionExtensions().Contains(TEXT("glb")));
}
bool UAtlasFileSelection::ValidateChoice(const FAtlasFileChoice& C,FString& E) const {
    if (!Definition.IsFileType()) { E=TEXT("Unsupported file type: ")+Definition.OriginalType; return false; }
    if (C.Source==EAtlasFileSource::AtlasId) {
        FGuid Id;
        if (!FGuid::ParseExact(C.File.FileId.TrimStartAndEnd(),EGuidFormats::DigitsWithHyphens,Id) || !Id.IsValid()) { E=TEXT("Enter a valid Atlas asset UUID"); return false; }
        if (!ServiceOrigin.IsEmpty() && !FAtlasPlatformAuth::IsTrustedDestination(ServiceOrigin)) { E=TEXT("The workflow service origin is not trusted"); return false; }
        if (!C.File.SourceOrigin.IsEmpty() && !ServiceOrigin.IsEmpty() && C.File.SourceOrigin!=ServiceOrigin) { E=TEXT("This asset ID belongs to a different recorded service origin"); return false; }
        return true;
    }
    if (C.Source==EAtlasFileSource::File) return Definition.ValidateFilePath(C.File.FilePath,E);
    if (!IsValid(C.Asset)) { E=TEXT("Select a compatible Unreal asset"); return false; }
    if (Definition.Type==EAtlasValueType::Image && !C.Asset->IsA<UTexture2D>()) { E=TEXT("Select a Texture2D asset"); return false; }
    if (Definition.Type==EAtlasValueType::Audio && !C.Asset->IsA<USoundWave>()) { E=TEXT("Select a SoundWave with an accessible original file"); return false; }
    if (UsesOriginalSource(Definition)) { FString P; if (!OriginalSource(C.Asset,P)) { E=TEXT("Original source file unavailable; use File or Atlas asset ID"); return false; } return Definition.ValidateFilePath(P,E); }
    if (Definition.Type==EAtlasValueType::Image) return true;
    if (Definition.Type==EAtlasValueType::Mesh) {
        auto* Mesh=Cast<UStaticMesh>(C.Asset);
        if (!Mesh || Mesh->GetNumLODs()==0 || Mesh->GetNumTriangles(0)==0) { E=TEXT("Choose a static mesh with exportable geometry"); return false; }
        if (!Definition.GetSelectionExtensions().Contains(TEXT("glb"))) { E=TEXT("Unreal mesh export produces GLB; this input requires another format"); return false; }
        return true;
    }
    E=TEXT("This type supports File or Atlas asset ID; no Unreal exporter is available"); return false;
}
bool UAtlasFileSelection::Validate(FString& E) const {
    if (!Definition.bIsArray && Items.Num()!=1) { E=TEXT("Select one file"); return false; }
    for (int32 I=0; I<Items.Num(); ++I) if (!ValidateChoice(Items[I],E)) { if (Definition.bIsArray) E=FString::Printf(TEXT("Item %d: %s"),I+1,*E); return false; }
    return true;
}
bool UAtlasFileSelection::ResolveDrop(const TArray<FString>& Paths,const TArray<UObject*>& Assets,bool bSingle,TArray<FAtlasFileChoice>& Out,FString& E) const {
    Out.Empty(); if (!bMutationEnabled) { E=TEXT("This input is read-only or uses the server default"); return false; }
    TArray<FString> Remaining=Paths;
    for (UObject* Asset : Assets) {
        FAtlasFileChoice C; C.Source=EAtlasFileSource::UnrealAsset; C.Asset=Asset;
        if (!ValidateChoice(C,E)) { Out.Empty(); return false; } Out.Add(C); FString Source;
        if (OriginalSource(Asset,Source)) { const int32 Match=Remaining.IndexOfByPredicate([&](const FString& P){return FPaths::IsSamePath(P,Source);}); if (Match!=INDEX_NONE) Remaining.RemoveAt(Match); }
    }
    for (const auto& P : Remaining) {
        FAtlasFileChoice C; C.File.FilePath=FPaths::ConvertRelativePathToFull(P);
        if (!ValidateChoice(C,E)) { Out.Empty(); return false; } Out.Add(C);
    }
    if (Out.IsEmpty() || (bSingle && Out.Num()!=1)) { E=TEXT("This target accepts exactly one file"); Out.Empty(); return false; }
    return true;
}
bool UAtlasFileSelection::Apply(const TArray<FAtlasFileChoice>& Choices,const FGuid& ReplaceId,int64 ExpectedRevision) {
    if (!bMutationEnabled || Revision!=ExpectedRevision) { Error=TEXT("Input changed while choosing a file; reopen the selector"); return false; }
    const int32 Index=ReplaceId.IsValid() ? Find(ReplaceId) : INDEX_NONE;
    if (ReplaceId.IsValid() && Index==INDEX_NONE) { Error=TEXT("The original item is no longer available"); return false; }
    if (Choices.IsEmpty() || ((!Definition.bIsArray || ReplaceId.IsValid()) && Choices.Num()!=1)) { Error=TEXT("This target accepts one item"); return false; }
    for (const auto& C : Choices) if (!ValidateChoice(C,Error)) return false;
    TArray<FAtlasFileChoice> Clean=Choices;
    for (auto& C : Clean) {
        C.File.FileData.Empty(); C.File.Warning.Empty(); C.File.SourceOrigin=ServiceOrigin;
        if (C.Source!=EAtlasFileSource::AtlasId) C.File.FileId.Empty();
        if (C.Source==EAtlasFileSource::File) { C.Asset=nullptr; C.File.SourceKind=TEXT("File"); C.File.SourceAssetPath.Empty(); C.File.OriginalFileName=FPaths::GetCleanFilename(C.File.FilePath); }
        else if (C.Source==EAtlasFileSource::AtlasId) { C.Asset=nullptr; C.File.FilePath.Empty(); C.File.FileName.Empty(); C.File.OriginalFileName.Empty(); C.File.SourceAssetPath.Empty(); C.File.SourceKind=TEXT("Atlas ID"); C.File.FileId=C.File.FileId.TrimStartAndEnd().ToLower(); }
        else { C.File.FilePath.Empty(); C.File.SourceKind=TEXT("Unreal asset"); C.File.SourceAssetPath=C.Asset->GetPathName(); C.File.OriginalFileName=C.Asset->GetName(); }
        C.Identity=FGuid::NewGuid();
    }
    if (Index!=INDEX_NONE) { Clean[0].Identity=Items[Index].Identity; Items[Index]=Clean[0]; }
    else if (Definition.bIsArray) Items.Append(Clean); else Items=Clean;
    Changed(); return true;
}
bool UAtlasFileSelection::Remove(const FGuid& Id) { const int32 I=Find(Id); if (!bMutationEnabled || I==INDEX_NONE) return false; Items.RemoveAt(I); Changed(); return true; }
bool UAtlasFileSelection::Move(const FGuid& Id,int32 To) {
    const int32 From=Find(Id); if (!bMutationEnabled || !Definition.bIsArray || From==INDEX_NONE || Items.IsEmpty()) return false;
    To=FMath::Clamp(To,0,Items.Num()-1); if (From==To) return true; auto Item=Items[From]; Items.RemoveAt(From); Items.Insert(Item,To); Changed(); return true;
}
FString UAtlasFileSelection::SourceDescription(const FAtlasFileChoice& C) const {
    if (C.Source==EAtlasFileSource::AtlasId) return TEXT("Atlas ID \u00b7 Reused without upload");
    if (C.Source==EAtlasFileSource::File) return TEXT("File \u00b7 Original bytes");
    if (UsesOriginalSource(Definition)) return TEXT("Unreal asset \u00b7 Original file");
    return Definition.Type==EAtlasValueType::Mesh ? TEXT("Unreal asset \u2192 GLB") : TEXT("Unreal asset \u2192 PNG");
}
FAtlasValue UAtlasFileSelection::GetReferenceValue() const {
    FAtlasValue R; R.Type=Definition.Type; R.bIsArray=Definition.bIsArray; R.OriginalType=Definition.OriginalType; R.FileFormat=Definition.Format;
    for (const auto& C : Items) R.FileItems.Add(C.File.WithoutFileData());
    if (!R.bIsArray) {
        if (R.FileItems.IsEmpty()) return FAtlasValue();
        const auto F=R.FileItems[0]; R.FileItems.Empty(); R.FilePath=F.FilePath; R.FileId=F.FileId; R.FileName=F.FileName; R.SourceKind=F.SourceKind; R.SourceAssetPath=F.SourceAssetPath; R.SourceOrigin=F.SourceOrigin; R.OriginalFileName=F.OriginalFileName;
    }
    return R;
}
bool UAtlasFileSelection::Prepare(FAtlasValue& Out,FString& E) const {
    if (!Validate(E)) return false;
    TArray<FAtlasFileReference> Prepared;
    for (const auto& C : Items) {
        auto Ref=C.File.WithoutFileData();
        if (C.Source==EAtlasFileSource::UnrealAsset) {
            Ref.FileId.Empty(); Ref.SourceKind=SourceDescription(C); Ref.SourceAssetPath=C.Asset->GetPathName();
            if (UsesOriginalSource(Definition)) OriginalSource(C.Asset,Ref.FilePath);
            else if (auto* Texture=Cast<UTexture2D>(C.Asset)) { const auto R=UAtlasAssetExporter::ExportTextureToPNG(Texture); if (!R.bSuccess) { E=R.ErrorMessage; return false; } Ref.FilePath=R.FilePath; }
            else {
                const FString Folder=UAtlasSDKSettings::Get()->GetTempExportFolderPath(); Ref.FilePath=Folder/(FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".glb"));
                TStrongObjectPtr<UGLTFExportOptions> Options(NewObject<UGLTFExportOptions>());
                if (!IFileManager::Get().MakeDirectory(*Folder,true) || !UGLTFExporter::ExportToGLTF(C.Asset,Ref.FilePath,Options.Get())) { E=TEXT("Static mesh GLB export failed"); return false; }
            }
            if (!Definition.ValidateFilePath(Ref.FilePath,E)) return false;
            Ref.OriginalFileName=UsesOriginalSource(Definition) ? FPaths::GetCleanFilename(Ref.FilePath) : C.Asset->GetName()+TEXT(".")+FPaths::GetExtension(Ref.FilePath);
        }
        if (!Ref.FilePath.IsEmpty()) Ref.FileName=FPaths::GetCleanFilename(Ref.FilePath); Prepared.Add(Ref);
    }
    Out=GetReferenceValue();
    if (Out.bIsArray) Out.FileItems=MoveTemp(Prepared);
    else if (!Prepared.IsEmpty()) { const auto& R=Prepared[0]; Out.FilePath=R.FilePath; Out.FileName=R.FileName; Out.FileId=R.FileId; Out.SourceKind=R.SourceKind; Out.SourceAssetPath=R.SourceAssetPath; Out.SourceOrigin=R.SourceOrigin; Out.OriginalFileName=R.OriginalFileName; }
    return true;
}
