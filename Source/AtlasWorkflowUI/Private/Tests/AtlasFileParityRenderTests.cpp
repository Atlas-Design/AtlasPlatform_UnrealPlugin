#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AtlasFileWidgets.h"
#include "AtlasNativeInputWidgets.h"
#include "AtlasOutputManager.h"
#include "AtlasSDKSettings.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Editor.h"
#include "EditorFramework/AssetImportData.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Containers/Ticker.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "UObject/StrongObjectPtr.h"
#include "Async/TaskGraphInterfaces.h"

// Explicit opt-in: uses frozen external fixtures and an isolated, disposable host project.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasFileParityRender,"Atlas.Acceptance.FileParityRender",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAtlasFileParityRender::RunTest(const FString&) {
    const FString Fixtures=FPlatformMisc::GetEnvironmentVariable(TEXT("ATLAS_PARITY_FIXTURES"));
    if (!TestTrue(TEXT("Frozen fixture directory provided"),FPaths::DirectoryExists(Fixtures))) return false;
    const FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("FileParityAcceptance")); IFileManager::Get().MakeDirectory(*Root,true);
    auto* Settings=GetMutableDefault<UAtlasSDKSettings>(); const auto Previous=Settings->OutputFolder; Settings->OutputFolder.Path=Root;
    ON_SCOPE_EXIT { Settings->OutputFolder=Previous; };
    const FString Image=Root/TEXT("gnome_redcap_original_image_with_a_long_filename_v001.png"), Exr=Root/TEXT("gnome_lantern_hdr_v001.exr");
    if (!TestTrue(TEXT("Copy frozen PNG"),IFileManager::Get().Copy(*Image,*(Fixtures/TEXT("images/gnome_redcap_v001.png")))==COPY_OK)
        || !TestTrue(TEXT("Copy frozen EXR"),IFileManager::Get().Copy(*Exr,*(Fixtures/TEXT("exr/gnome_lantern_hdr_v001.exr")))==COPY_OK)) return false;
    UWorld* World=GEditor->GetEditorWorldContext().World();
    UClass* Class=LoadClass<UAtlasFilePanel>(nullptr,TEXT("/AtlasWorkflow/Core/UI/Editor/Utilities/WBP_AtlasFilePanel.WBP_AtlasFilePanel_C"));
    TStrongObjectPtr<UAtlasFilePanel> Panel(CreateWidget<UAtlasFilePanel>(World,Class)); TSharedPtr<SWidget> Slate=Panel->TakeWidget();
    FAtlasParameterDef Def; Def.Name=TEXT("images"); Def.Type=EAtlasValueType::Image; Def.OriginalType=TEXT("image");
    FAtlasValue Value; Value.Type=Def.Type; Value.OriginalType=Def.OriginalType; Value.FilePath=Image; Value.FileName=FPaths::GetCleanFilename(Image);
    FAtlasJobHistoryRecord Record; Record.JobId=FGuid::NewGuid(); Record.ApiId=TEXT("file-parity-acceptance"); Record.WorkflowName=TEXT("File parity acceptance"); Record.StartedAt=Record.CompletedAt=FDateTime::UtcNow();
    auto Draw=[&](const FString& State,int32 Width,float Scale=1.f) {
        auto RootWidget=SNew(SBorder).Padding(10).VAlign(VAlign_Top).BorderBackgroundColor(FLinearColor(.035f,.035f,.035f))[Slate.ToSharedRef()];
        FWidgetRenderer Renderer(false); const FVector2D Size(Width*Scale,450*Scale);
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size,TF_Bilinear,false));
        Renderer.DrawWidget(Target.Get(),RootWidget,Scale,Size,0.f); FTSTicker::GetCoreTicker().Tick(.016f); FlushRenderingCommands();
        Renderer.DrawWidget(Target.Get(),RootWidget,Scale,Size,0.f); FlushRenderingCommands();
        TArray<FColor> Pixels; if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels)) return false;
        TArray64<uint8> Bytes; FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Bytes);
        return FFileHelper::SaveArrayToFile(Bytes,*(Root/(State+FString::Printf(TEXT("_%d_%d.png"),Width,FMath::RoundToInt(Scale*100)))));
    };
    Panel->Configure(Def,FAtlasValue(),EAtlasFilePresentation::Editable);
    TestTrue(TEXT("Render empty scalar"),Draw(TEXT("empty"),380));
    Panel->Configure(Def,Value,EAtlasFilePresentation::Editable); auto PreviewReady=MakeShared<bool>(false); Panel->OutputManager->LoadPreviewAsync(Image,[PreviewReady](UTexture2D*){*PreviewReady=true;});
    const double Until=FPlatformTime::Seconds()+5.; while (!*PreviewReady && FPlatformTime::Seconds()<Until) { FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread); FPlatformProcess::Sleep(.005f); }
    TestTrue(TEXT("Async original-file preview completed"),*PreviewReady);
    TestTrue(TEXT("Render selected scalar"),Draw(TEXT("scalar"),380)); TestTrue(TEXT("Render wide scalar"),Draw(TEXT("scalar"),700));
    Def.bIsArray=true; FAtlasValue Array; Array.Type=Def.Type; Array.OriginalType=Def.OriginalType; Array.bIsArray=true;
    FAtlasFileReference Ref; Ref.FilePath=Image; Ref.FileName=Value.FileName; Ref.FileId=TEXT("8d6931f1-24e0-44c0-a626-ed47e9326e7a"); Array.FileItems={Ref,Ref,Ref};
    Panel->Configure(Def,Array,EAtlasFilePresentation::Editable); TestTrue(TEXT("Render ordered array"),Draw(TEXT("array"),380)); TestTrue(TEXT("Render array at DPI 125"),Draw(TEXT("array"),700,1.25f));
    Panel->Configure(Def,Array,EAtlasFilePresentation::RecordedInput); TestTrue(TEXT("Render recorded input"),Draw(TEXT("recorded"),380));
    Record.Outputs.Values.Add(Def.Name,Array);
    const auto First=Panel->OutputManager->ImportOutputItem(Record,Def.Name,0); const auto Second=Panel->OutputManager->ImportOutputItem(Record,Def.Name,1);
    TestTrue(TEXT("Array image imported"),First.bSuccess && Second.bSuccess); TestTrue(TEXT("Duplicate filenames have distinct import destinations"),First.ImportedAsset!=Second.ImportedAsset);
    TestEqual(TEXT("Import lookup restored"),Panel->OutputManager->FindImportedOutputItem(Record,Def.Name,0).ImportedAsset.Get(),First.ImportedAsset.Get());
    Panel->Configure(Def,Array,EAtlasFilePresentation::Output,Record); TestTrue(TEXT("Render imported outputs"),Draw(TEXT("output"),380)); TestTrue(TEXT("Render wide outputs"),Draw(TEXT("output"),700));
    FAtlasValue Hdr; Hdr.Type=EAtlasValueType::Image; Hdr.OriginalType=TEXT("exr"); Hdr.FileFormat=TEXT("exr"); Hdr.FilePath=Exr; Hdr.FileName=FPaths::GetCleanFilename(Exr); Record.Outputs.Values.Add(TEXT("hdr"),Hdr);
    auto ImportedHdr=Panel->OutputManager->ImportOutputItem(Record,TEXT("hdr"),INDEX_NONE); auto* Texture=Cast<UTexture2D>(ImportedHdr.ImportedAsset);
    if (TestNotNull(TEXT("EXR imports as Texture2D"),Texture)) {
        TestTrue(TEXT("HDR source remains floating point"),Texture->Source.GetFormat()==TSF_RGBA16F || Texture->Source.GetFormat()==TSF_RGBA32F || Texture->Source.GetFormat()==TSF_R16F || Texture->Source.GetFormat()==TSF_R32F);
        TestFalse(TEXT("HDR texture is linear"),Texture->SRGB);
    }
    Array.FileItems[0].FilePath=Root/TEXT("missing.png"); Array.FileItems[1].FilePath.Empty(); Array.FileItems[1].FileName.Empty(); Array.FileItems[2].Warning=TEXT("Download failed; ID retained");
    Panel->Configure(Def,Array,EAtlasFilePresentation::RecordedInput); TestTrue(TEXT("Render missing and ID-only states"),Draw(TEXT("missing-id"),380));
    UClass* InputClass=LoadClass<UAtlasInputFieldWidget>(nullptr,TEXT("/AtlasWorkflow/Core/UI/Editor/Utilities/EUW_InputField.EUW_InputField_C"));
    TStrongObjectPtr<UAtlasInputFieldWidget> Field(CreateWidget<UAtlasInputFieldWidget>(World,InputClass)); Field->currentDefParameter=Def; Field->currentDefParameter.bRequired=false; Field->currentDefParameter.bHasRequiredMetadata=true; Field->defaultValue=Array;
    auto PanelSlate=Slate; Slate=Field->TakeWidget(); TestTrue(TEXT("Render complete workflow input"),Draw(TEXT("workflow-array"),380));
    Slate.Reset(); Field->ReleaseSlateResources(true); Slate=PanelSlate; PanelSlate.Reset();
    Slate.Reset(); Panel->ReleaseSlateResources(true); AddInfo(TEXT("Rendered captures: ")+Root); return true;
}
#endif
