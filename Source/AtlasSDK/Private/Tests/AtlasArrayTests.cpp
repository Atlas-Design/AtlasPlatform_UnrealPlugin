#if WITH_DEV_AUTOMATION_TESTS
#include "AtlasJob.h"
#include "AtlasFileManager.h"
#include "AtlasHttpRequest.h"
#include "AtlasHistoryManager.h"
#include "AtlasOutputManager.h"
#include "AtlasWorkflowAsset.h"
#include "AtlasJsonObject.h"
#include "AtlasSDKSettings.h"
#include "AtlasFileWork.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

struct FAtlasArrayTestAccess
{
    static void Archive(UAtlasJob* J) { J->ArchiveInputFilesToRunFolder(); }
    static UAtlasJsonObject* Payload(UAtlasJob* J) { return J->BuildExecutePayload(); }
    static bool Parse(UAtlasJob* J,UAtlasJsonObject* R) { return J->ParseStatusResponse(R); }
    static void Start(UAtlasJob* J) { J->StartDownloadPhase(); }
    static void Complete(UAtlasJob* J,const FGuid& Id,const FString& Fid) { J->HandleDownloadComplete(Id,Fid,{1,2,3,4}); }
    static void Fail(UAtlasJob* J,const FGuid& Id) { J->HandleDownloadFailed(Id,TEXT("fixture missing file"),404); }
    static int32 Active(UAtlasJob* J) { return J->DownloadOperations.Num()+J->PendingArrayWrites; }
    static void Download(UAtlasJob* J,TFunction<FGuid(const FString&,const FGuid&)> F) { J->DownloadDispatcherForTests=MoveTemp(F); }
    static void Upload(UAtlasFileManager* M,TFunction<void(UAtlasHttpRequest*)> F) { M->UploadRequestDispatcherForTests=MoveTemp(F); }
    static void Execute(UAtlasJob* J,TFunction<void(UAtlasHttpRequest*)> F) { J->ExecuteRequestDispatcherForTests=MoveTemp(F); }
    static void History(UAtlasHistoryManager* H,const FString& Path) { H->TestHistoryDirectory=Path; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasOrderedFiles,"Atlas.IO.OrderedFiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAtlasOrderedFiles::RunTest(const FString&)
{
    auto* Settings=GetMutableDefault<UAtlasSDKSettings>(); const auto OldFolder=Settings->OutputFolder;
    const bool OldSave=Settings->bAutoSaveOutputFiles, OldCache=Settings->bEnableUploadCache;
    const int32 OldLimit=Settings->MaxConcurrentOutputDownloads, OldMemory=Settings->MaxRetainedArrayOutputMB;
    ON_SCOPE_EXIT { Settings->OutputFolder=OldFolder; Settings->bAutoSaveOutputFiles=OldSave; Settings->bEnableUploadCache=OldCache;
        Settings->MaxConcurrentOutputDownloads=OldLimit; Settings->MaxRetainedArrayOutputMB=OldMemory; };
    const FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Automation/Arrays")/FGuid::NewGuid().ToString(EGuidFormats::Digits));
    Settings->OutputFolder.Path=Root/TEXT("Output"); Settings->bAutoSaveOutputFiles=true; Settings->MaxConcurrentOutputDownloads=2; Settings->bEnableUploadCache=false;
    IFileManager::Get().MakeDirectory(*Root,true);
    const FString Local=Root/TEXT("source.png"); TestTrue(TEXT("Local fixture"),FFileHelper::SaveArrayToFile(TArray<uint8>{7,8,9},*Local));
    TStrongObjectPtr<UAtlasWorkflowAsset> W(NewObject<UAtlasWorkflowAsset>());
    W->Schema.ApiId=TEXT("array-fixture"); W->Schema.BaseUrl=TEXT("https://array-test.invalid"); W->Schema.Version=TEXT("0.2"); W->bIsValid=true;
    FAtlasParameterDef Def; Def.Name=TEXT("images"); Def.Type=EAtlasValueType::Image; Def.Format=TEXT("png"); Def.bIsArray=true;
    W->Schema.Inputs={Def}; W->Schema.Outputs={Def};
    FAtlasParameterDef Optional=Def; Optional.Name=TEXT("optional"); Optional.bHasRequiredMetadata=true; Optional.bRequired=false; W->Schema.Inputs.Add(Optional);
    FAtlasParameterDef Mesh=Def; Mesh.Name=TEXT("meshes"); Mesh.Type=EAtlasValueType::Mesh; Mesh.Format=TEXT("glb"); W->Schema.Outputs.Add(Mesh);
    FAtlasParameterDef Collision; Collision.Name=TEXT("@AtlasList/6:images/0"); Collision.Type=EAtlasValueType::String; W->Schema.Outputs.Add(Collision);
    FAtlasFileReference Existing, File; Existing.FileId=TEXT("a439f5b9-9b37-4a52-8d9d-441c142291c5"); File.FilePath=Local;
    FAtlasWorkflowInputs Inputs; Inputs.SetFileList(TEXT("images"),EAtlasValueType::Image,{Existing,File,Existing,File});
    TArray<FString> Errors; TestTrue(TEXT("Mixed file/ID list validates"),W->ValidateInputs(Inputs,Errors));
    TStrongObjectPtr<UAtlasFileManager> Files(NewObject<UAtlasFileManager>());
    TStrongObjectPtr<UAtlasJob> Job(NewObject<UAtlasJob>()); Job->Initialize(W->Schema.ApiId,TEXT("Array fixture"),Inputs); Job->SetWorkflowAsset(W.Get()); Job->SetFileManager(Files.Get());
    int32 Uploads=0, Executes=0;
    FAtlasArrayTestAccess::Upload(Files.Get(),[&](UAtlasHttpRequest* Request) {
        ++Uploads; auto* Reply=NewObject<UAtlasJsonObject>(); Reply->SetStringField(TEXT("file_id"),TEXT("uploaded-id"));
        Request->OnRequestComplete.Broadcast(Request,Reply,200);
    });
    FAtlasArrayTestAccess::Execute(Job.Get(),[&](UAtlasHttpRequest*) { ++Executes; });
    ON_SCOPE_EXIT { Job->Cancel(); Files->CancelAllUploads(); Files->CancelAllDownloads(); };
    FAtlasArrayTestAccess::Archive(Job.Get()); AtlasFileWork::Drain();
    TestEqual(TEXT("Only two local items uploaded"),Uploads,2); TestEqual(TEXT("Execute once after all uploads"),Executes,1);
    const auto& Archived=Job->Inputs.Values.FindChecked(TEXT("images")).FileItems;
    TestNotEqual(TEXT("Duplicate local files have distinct archive paths"),Archived[1].FilePath,Archived[3].FilePath);
    TestEqual(TEXT("Uploaded ID retained for rerun"),Archived[1].FileId,FString(TEXT("uploaded-id")));
    auto* Payload=FAtlasArrayTestAccess::Payload(Job.Get());
    if (!TestNotNull(TEXT("Array request payload"),Payload)) return false;
    const auto& Sent=Payload->GetJsonObject()->GetArrayField(TEXT("images"));
    TestEqual(TEXT("Request count"),Sent.Num(),4);
    TestEqual(TEXT("Request keeps order"),Sent[1]->AsString(),FString(TEXT("uploaded-id")));
    TestEqual(TEXT("Request keeps duplicate IDs"),Sent[0]->AsString(),Sent[2]->AsString());
    TStrongObjectPtr<UAtlasJsonObject> Reply(NewObject<UAtlasJsonObject>());
    Reply->DecodeJson(TEXT(R"({"result":{"outputs":{"images":["same","second","same","failed","fifth"],"meshes":["mesh"],"@AtlasList/6:images/0":"scalar"}}})"));
    TestTrue(TEXT("Array response including duplicates parses"),FAtlasArrayTestAccess::Parse(Job.Get(),Reply.Get()));
    Reply->DecodeJson(TEXT(R"({"result":{"outputs":{"images":[null],"meshes":[],"@AtlasList/6:images/0":"scalar"}}})"));
    TestFalse(TEXT("Null item is rejected without truncating existing outputs"),FAtlasArrayTestAccess::Parse(Job.Get(),Reply.Get()));
    TestEqual(TEXT("Parse failure leaves previous array intact"),Job->Outputs.Values.FindChecked(TEXT("images")).FileItems.Num(),5);
    TArray<TPair<FString,FGuid>> Started;
    FAtlasArrayTestAccess::Download(Job.Get(),[&](const FString& Id,const FGuid& Op) { Started.Emplace(Id,Op); return Op; });
    FAtlasArrayTestAccess::Start(Job.Get()); TestEqual(TEXT("Download limit enforced"),Started.Num(),2);
    auto Deliver=[&](int32 I) { const auto Pair=Started[I]; if (Pair.Key==TEXT("failed")) FAtlasArrayTestAccess::Fail(Job.Get(),Pair.Value); else FAtlasArrayTestAccess::Complete(Job.Get(),Pair.Value,Pair.Key); };
    Deliver(1); Deliver(0); AtlasFileWork::Drain();
    for (int32 I=2; I<Started.Num(); ++I)
    {
        TestTrue(TEXT("Transfers plus writes remain bounded"),FAtlasArrayTestAccess::Active(Job.Get())<=2);
        Deliver(I); AtlasFileWork::Drain();
    }
    TestTrue(TEXT("Partial download failure still completes job"),Job->State==EAtlasJobState::Completed);
    const auto& Out=Job->Outputs.Values.FindChecked(TEXT("images")).FileItems;
    TestEqual(TEXT("Output order survives reverse completions"),Out[1].FileId,FString(TEXT("second")));
    TestEqual(TEXT("Failure keeps ID"),Out[3].FileId,FString(TEXT("failed")));
    TestFalse(TEXT("Failure warning belongs to item"),Out[3].Warning.IsEmpty());
    TestTrue(TEXT("Failure does not borrow another item's path"),Out[3].FilePath.IsEmpty());
    TestNotEqual(TEXT("Duplicate output IDs cannot overwrite one another"),Out[0].FilePath,Out[2].FilePath);
    TestEqual(TEXT("Synthetic-looking parameter is still a scalar"),Job->Outputs.Values.FindChecked(Collision.Name).StringValue,FString(TEXT("scalar")));
    for (int32 I : {0,1,2,4})
    {
        TArray<uint8> Bytes; TestTrue(TEXT("Archived output exists"),FFileHelper::LoadFileToArray(Bytes,*Out[I].FilePath));
        TestTrue(TEXT("Archived bytes unchanged"),Bytes==TArray<uint8>({1,2,3,4})); TestTrue(TEXT("Saved array bytes released"),Out[I].FileData.IsEmpty());
    }
    TStrongObjectPtr<UAtlasHistoryManager> H(NewObject<UAtlasHistoryManager>()); FAtlasArrayTestAccess::History(H.Get(),Root/TEXT("History"));
    TestTrue(TEXT("Save array job history"),H->SaveJobToHistory(Job.Get()));
    H->RefreshHistory(); FAtlasJobHistoryRecord Record;
    if (!TestTrue(TEXT("Reload array job history"),H->FindRecord(Job->JobId,Record))) return false;
    TestTrue(TEXT("Omitted optional input recorded distinctly"),Record.Inputs.Values.Contains(TEXT("optional")) && Record.Inputs.Values.FindChecked(TEXT("optional")).bOmitted);
    TestEqual(TEXT("History preserves item positions"),Record.Outputs.Values.FindChecked(TEXT("images")).FileItems.Num(),5);
    TestEqual(TEXT("History preserves item warnings"),Record.Outputs.Values.FindChecked(TEXT("images")).FileItems[3].Warning,Out[3].Warning);
    TestEqual(TEXT("History preserves uploaded ID"),Record.Inputs.Values.FindChecked(TEXT("images")).FileItems[1].FileId,FString(TEXT("uploaded-id")));
    TStrongObjectPtr<UAtlasOutputManager> OutputManager(NewObject<UAtlasOutputManager>());
    TestEqual(TEXT("All file listing includes nested array outputs"),OutputManager->GetAllOutputFilesForJobFromHistory(Record).Num(),5);
    // Cancellation must stop queued items even while an archive write is in flight.
    TStrongObjectPtr<UAtlasJob> Cancelled(NewObject<UAtlasJob>()); Cancelled->Initialize(W->Schema.ApiId,TEXT("Cancelled array"),Inputs); Cancelled->SetWorkflowAsset(W.Get()); Cancelled->SetFileManager(Files.Get());
    Reply->DecodeJson(TEXT(R"({"result":{"outputs":{"images":["one","two","three"],"meshes":[],"@AtlasList/6:images/0":"scalar"}}})"));
    TestTrue(TEXT("Empty mesh list accepted"),FAtlasArrayTestAccess::Parse(Cancelled.Get(),Reply.Get()));
    TArray<TPair<FString,FGuid>> CancelOps;
    FAtlasArrayTestAccess::Download(Cancelled.Get(),[&](const FString& Id,const FGuid& Op) { CancelOps.Emplace(Id,Op); return Op; });
    FAtlasArrayTestAccess::Start(Cancelled.Get());
    FAtlasArrayTestAccess::Complete(Cancelled.Get(),CancelOps[0].Value,CancelOps[0].Key); Cancelled->Cancel();
    FAtlasArrayTestAccess::Complete(Cancelled.Get(),CancelOps[1].Value,CancelOps[1].Key); AtlasFileWork::Drain();
    TestTrue(TEXT("Late array writes cannot revive cancellation"),Cancelled->State==EAtlasJobState::Cancelled);
    TestEqual(TEXT("Queued item never starts after cancellation"),CancelOps.Num(),2);
    // Memory-only mode retains references when the configured byte budget is exhausted.
    Settings->bAutoSaveOutputFiles=false; Settings->MaxRetainedArrayOutputMB=0;
    TStrongObjectPtr<UAtlasJob> Limited(NewObject<UAtlasJob>()); Limited->SetWorkflowAsset(W.Get()); Limited->SetFileManager(Files.Get());
    FAtlasArrayTestAccess::Parse(Limited.Get(),Reply.Get());
    FAtlasArrayTestAccess::Download(Limited.Get(),[&](const FString& Id,const FGuid& Op) { FAtlasArrayTestAccess::Complete(Limited.Get(),Op,Id); return Op; });
    FAtlasArrayTestAccess::Start(Limited.Get());
    TestTrue(TEXT("Synchronous completions drain safely"),Limited->State==EAtlasJobState::Completed);
    for (const auto& Item : Limited->Outputs.Values.FindChecked(TEXT("images")).FileItems)
        TestTrue(TEXT("Memory limit retains ID and warning, not bytes"),!Item.FileId.IsEmpty() && Item.FileData.IsEmpty() && !Item.Warning.IsEmpty());
    return true;
}
#endif
