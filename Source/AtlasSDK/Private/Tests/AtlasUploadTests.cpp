// Copyright Atlas Platform. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "AtlasUploadTestObserver.h"
#include "AtlasFileWork.h"
#include "AtlasFileManager.h"
#include "AtlasHttpRequest.h"
#include "AtlasJsonObject.h"
#include "AtlasWorkflowAsset.h"
#include "Async/TaskGraphInterfaces.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFile.h"
#include "UObject/StrongObjectPtr.h"

struct FAtlasUploadTestAccess
{
	static void AddFile(UAtlasJob* Job, const FString& Name, const FString& Path) { Job->FilesToUpload.Add(Name, Path); }
	static void Start(UAtlasJob* Job) { Job->StartUploadPhase(); }
	static int32 Pending(UAtlasJob* Job) { return Job->UploadOperations.Num(); }
	static int32 Completed(UAtlasJob* Job) { return Job->UploadsCompleted; }
	static FGuid Operation(UAtlasFileManager* Manager, UAtlasHttpRequest* Request)
	{
		const FAtlasPendingUpload* Upload = Manager->FindUploadByRequest(Request);
		return Upload ? Upload->OperationId : FGuid();
	}
	static void UploadDispatcher(UAtlasFileManager* Manager, TFunction<void(UAtlasHttpRequest*)> Dispatcher)
	{
		Manager->UploadRequestDispatcherForTests = MoveTemp(Dispatcher);
	}
	static void ExecuteDispatcher(UAtlasJob* Job, TFunction<void(UAtlasHttpRequest*)> Dispatcher)
	{
		Job->ExecuteRequestDispatcherForTests = MoveTemp(Dispatcher);
	}
	static void Cache(UAtlasFileManager* Manager, const TArray<uint8>& Bytes)
	{
		const FString Key = UAtlasFileManager::CacheKeyFor(TEXT("https://upload-test.invalid/0.2/upload"), UAtlasFileManager::CalculateContentHash(Bytes));
		Manager->UploadCache.Add(Key, TEXT("cached-file-id"));
		Manager->UploadCacheTimes.Add(Key, FPlatformTime::Seconds());
	}
};

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	struct FUploadFixture
	{
		TStrongObjectPtr<UAtlasFileManager> Manager{NewObject<UAtlasFileManager>()};
		TStrongObjectPtr<UAtlasWorkflowAsset> Workflow{NewObject<UAtlasWorkflowAsset>()};
		TStrongObjectPtr<UAtlasJob> Job{NewObject<UAtlasJob>()};
		TStrongObjectPtr<UAtlasUploadTestObserver> Observer{NewObject<UAtlasUploadTestObserver>()};
		FString Directory;
		TArray<UAtlasHttpRequest*> Requests;
		int32 ExecuteCount = 0;

		FUploadFixture()
		{
			Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),
				TEXT("Automation/Uploads"), FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*Directory);
			Workflow->Schema.ApiId = TEXT("upload-regression");
			Workflow->Schema.BaseUrl = TEXT("https://upload-test.invalid");
			Workflow->Schema.Version = TEXT("0.2");
			Workflow->bIsValid = true;
			Job->SetWorkflowAsset(Workflow.Get());
			Job->SetFileManager(Manager.Get());
			Job->State = EAtlasJobState::Running;
			Job->OnFailed.AddDynamic(Observer.Get(), &UAtlasUploadTestObserver::Failed);
			Job->OnComplete.AddDynamic(Observer.Get(), &UAtlasUploadTestObserver::Completed);
			Job->OnProgress.AddDynamic(Observer.Get(), &UAtlasUploadTestObserver::Progressed);
			FAtlasUploadTestAccess::UploadDispatcher(Manager.Get(), [this](UAtlasHttpRequest* Request) { Requests.Add(Request); });
			FAtlasUploadTestAccess::ExecuteDispatcher(Job.Get(), [this](UAtlasHttpRequest*) { ++ExecuteCount; });
		}
		~FUploadFixture()
		{
			Job->OnFailed.Clear();
			Job->OnProgress.Clear();
			Job->Cancel();
			Manager->CancelAllUploads();
			// Retain the tiny disposable input files for inspection, as in cleanup tests.
		}
		FString File(FAutomationTestBase& Test, const FString& Name, const TArray<uint8>& Bytes = {1, 2, 3})
		{
			const FString Path = FPaths::Combine(Directory, Name);
			Test.TestTrue(TEXT("Write upload fixture"), FFileHelper::SaveArrayToFile(Bytes, *Path));
			return Path;
		}
		void Add(const FString& Name, const FString& Path) { FAtlasUploadTestAccess::AddFile(Job.Get(), Name, Path); }
		void Start() { FAtlasUploadTestAccess::Start(Job.Get()); AtlasFileWork::Drain(); }
	};
	void ExpectUploadErrors(FAutomationTestBase& Test, bool bManagerFailure = true)
	{
		Test.AddExpectedError(TEXT("AtlasJob\\[.*upload failed"), EAutomationExpectedErrorFlags::Contains, 0);
		if (bManagerFailure) Test.AddExpectedError(TEXT("AtlasFileManager: Upload .* failed"), EAutomationExpectedErrorFlags::Contains, 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasUploadInvalidStartup, "Atlas.Upload.Startup.InvalidFiles", Flags)
bool FAtlasUploadInvalidStartup::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("AtlasFileManager: Cannot upload without a valid WorkflowAsset"), EAutomationExpectedErrorFlags::Contains, 1);
	ExpectUploadErrors(*this, false);
	for (int32 Case : {0, 1, 2})
	{
		FUploadFixture Fixture;
		const FString Path = Case == 0 ? FPaths::Combine(Fixture.Directory, TEXT("missing.bin"))
			: Fixture.File(*this, TEXT("input.bin"), Case == 1 ? TArray<uint8>{} : TArray<uint8>{1, 2, 3});
		if (Case == 2) Fixture.Workflow->bIsValid = false;
		Fixture.Add(TEXT("invalid-input"), Path);
		Fixture.Start();
		TestTrue(TEXT("Invalid upload immediately fails job"), Fixture.Job->State == EAtlasJobState::Failed);
		TestEqual(TEXT("Exactly one failure event"), Fixture.Observer->FailureCount, 1);
		TestTrue(TEXT("Input identified in failure"), Fixture.Job->Error.Message.Contains(TEXT("invalid-input")));
		TestTrue(TEXT("UploadFailed error"), Fixture.Job->Error.Code == EAtlasErrorCode::UploadFailed);
		TestEqual(TEXT("No execute request"), Fixture.ExecuteCount, 0);
		TestEqual(TEXT("No tracked upload remains"), FAtlasUploadTestAccess::Pending(Fixture.Job.Get()), 0);
		TestEqual(TEXT("No request sent"), Fixture.Requests.Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasUploadImmediateFailure, "Atlas.Upload.Startup.SynchronousFailure", Flags)
bool FAtlasUploadImmediateFailure::RunTest(const FString& Parameters)
{
	ExpectUploadErrors(*this);
	for (int32 Status : {0, 401})
	{
		FUploadFixture Fixture;
		int32 Starts = 0;
		FAtlasUploadTestAccess::UploadDispatcher(Fixture.Manager.Get(), [&Starts, Status](UAtlasHttpRequest* Request)
		{
			++Starts;
			Request->OnRequestFailed.Broadcast(Request, TEXT("injected immediate failure"), Status);
		});
		Fixture.Add(TEXT("first"), Fixture.File(*this, TEXT("first.bin")));
		Fixture.Add(TEXT("second"), Fixture.File(*this, TEXT("second.bin")));
		Fixture.Start();
		TestTrue(TEXT("Synchronous failure is terminal"), Fixture.Job->State == EAtlasJobState::Failed);
		TestEqual(TEXT("One failure event"), Fixture.Observer->FailureCount, 1);
		TestEqual(TEXT("No subsequent upload starts"), Starts, 1);
		TestEqual(TEXT("HTTP code preserved"), Fixture.Job->Error.HttpStatusCode, Status);
		TestTrue(TEXT("Correct error category"), Fixture.Job->Error.Code == (Status == 401 ? EAtlasErrorCode::Unauthorized : (Status == 500 ? EAtlasErrorCode::ServerError : EAtlasErrorCode::UploadFailed)));
		TestEqual(TEXT("No execute"), Fixture.ExecuteCount, 0);
		TestEqual(TEXT("Pending manager request removed"), Fixture.Manager->GetPendingUploadCount(), 0);
		TestEqual(TEXT("Pending job operation removed"), FAtlasUploadTestAccess::Pending(Fixture.Job.Get()), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasUploadFailureLifetime, "Atlas.Upload.FailureLifetime", Flags)
bool FAtlasUploadFailureLifetime::RunTest(const FString& Parameters)
{
	ExpectUploadErrors(*this);
	for (int32 Status : {500, 401})
	{
		FUploadFixture Fixture;
		const FString Name = TEXT("long-input-") + FString::ChrN(4096, TEXT('X'));
		Fixture.Add(Name, Fixture.File(*this, TEXT("long.bin")));
		Fixture.Start();
		TestEqual(TEXT("One deferred request"), Fixture.Requests.Num(), 1);
		if (Fixture.Requests.Num() != 1) return false;
		const FGuid OperationId = FAtlasUploadTestAccess::Operation(Fixture.Manager.Get(), Fixture.Requests[0]);
		Fixture.Manager->OnUploadFailed.Broadcast(FGuid::NewGuid(), TEXT("another job's upload"), 500);
		TestTrue(TEXT("Unrelated upload failure leaves job running"), Fixture.Job->State == EAtlasJobState::Running);
		TestEqual(TEXT("Unrelated upload produces no failure"), Fixture.Observer->FailureCount, 0);
		Fixture.Requests[0]->OnRequestFailed.Broadcast(Fixture.Requests[0], TEXT("injected deferred failure"), Status);
		TestTrue(TEXT("Input name survives map removal"), Fixture.Job->Error.Message.Contains(Name));
		TestEqual(TEXT("Exactly one terminal failure"), Fixture.Observer->FailureCount, 1);
		TestTrue(TEXT("Failed state"), Fixture.Job->State == EAtlasJobState::Failed);
		TestTrue(TEXT("Category retained"), Fixture.Job->Error.Code == (Status == 401 ? EAtlasErrorCode::Unauthorized : (Status == 500 ? EAtlasErrorCode::ServerError : EAtlasErrorCode::UploadFailed)));
		Fixture.Manager->OnUploadFailed.Broadcast(OperationId, TEXT("duplicate failure"), 500);
		TestEqual(TEXT("Late/unrelated event ignored"), Fixture.Observer->FailureCount, 1);
		TestEqual(TEXT("No success event"), Fixture.Observer->CompletionCount, 0);
		TestFalse(TEXT("Request released from root"), Fixture.Requests[0]->IsRooted());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasUploadMixedStartup, "Atlas.Upload.MixedStartup", Flags)
bool FAtlasUploadMixedStartup::RunTest(const FString& Parameters)
{
	ExpectUploadErrors(*this, false);
	FUploadFixture Fixture;
	Fixture.Add(TEXT("valid"), Fixture.File(*this, TEXT("valid.bin")));
	Fixture.Add(TEXT("missing"), FPaths::Combine(Fixture.Directory, TEXT("missing.bin")));
	Fixture.Add(TEXT("never-start"), Fixture.File(*this, TEXT("later.bin")));
	Fixture.Start();
	TestTrue(TEXT("Mixed startup fails immediately"), Fixture.Job->State == EAtlasJobState::Failed);
	TestEqual(TEXT("Only preceding valid upload started"), Fixture.Requests.Num(), 1);
	TestEqual(TEXT("One failure"), Fixture.Observer->FailureCount, 1);
	TestEqual(TEXT("No execute"), Fixture.ExecuteCount, 0);
	if (Fixture.Requests.Num() == 1)
	{
		UAtlasJsonObject* Response = NewObject<UAtlasJsonObject>();
		Response->SetStringField(TEXT("file_id"), TEXT("late-file-id"));
		Fixture.Requests[0]->OnRequestComplete.Broadcast(Fixture.Requests[0], Response, 200);
	}
	TestTrue(TEXT("Late completion cannot revive failed job"), Fixture.Job->State == EAtlasJobState::Failed);
	TestEqual(TEXT("Still one failure"), Fixture.Observer->FailureCount, 1);
	TestEqual(TEXT("Still no execute"), Fixture.ExecuteCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasUploadSuccessfulHandoff, "Atlas.Upload.SuccessfulHandoff", Flags)
bool FAtlasUploadSuccessfulHandoff::RunTest(const FString& Parameters)
{
	for (int32 Mode : {0, 1, 2}) // synchronous, cached, and mixed
	{
		FUploadFixture Fixture;
		int32 UploadCount = 0;
		FAtlasUploadTestAccess::UploadDispatcher(Fixture.Manager.Get(), [&UploadCount](UAtlasHttpRequest* Request)
		{
			++UploadCount;
			UAtlasJsonObject* Response = NewObject<UAtlasJsonObject>();
			Response->SetStringField(TEXT("file_id"), TEXT("uploaded-file-id"));
			Request->OnRequestComplete.Broadcast(Request, Response, 200);
		});
		if (Mode == 1) FAtlasUploadTestAccess::Cache(Fixture.Manager.Get(), {1, 2, 3});
		Fixture.Add(TEXT("first"), Fixture.File(*this, TEXT("first.bin")));
		Fixture.Add(TEXT("second"), Fixture.File(*this, TEXT("second.bin"), Mode == 0 ? TArray<uint8>{4, 5, 6} : TArray<uint8>{1, 2, 3}));
		Fixture.Start();
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		TestEqual(TEXT("Both uploads accounted for"), FAtlasUploadTestAccess::Completed(Fixture.Job.Get()), 2);
		TestEqual(TEXT("Exactly one execute handoff"), Fixture.ExecuteCount, 1);
		TestEqual(TEXT("No failed event"), Fixture.Observer->FailureCount, 0);
		TestEqual(TEXT("No remaining upload operations"), FAtlasUploadTestAccess::Pending(Fixture.Job.Get()), 0);
		TestEqual(TEXT("Cached inputs avoid transport"), UploadCount, Mode == 0 ? 2 : (Mode == 1 ? 0 : 1));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasUploadCancellationAtStart, "Atlas.Upload.CancelOnPhaseEntry", Flags)
bool FAtlasUploadCancellationAtStart::RunTest(const FString& Parameters)
{
	FUploadFixture Fixture;
	Fixture.Observer->bCancelOnUploading = true;
	Fixture.Add(TEXT("input"), Fixture.File(*this, TEXT("input.bin")));
	Fixture.Start();
	TestTrue(TEXT("Phase callback cancellation stays cancelled"), Fixture.Job->State == EAtlasJobState::Cancelled);
	TestEqual(TEXT("No upload dispatched after cancellation"), Fixture.Requests.Num(), 0);
	TestEqual(TEXT("One terminal notification"), Fixture.Observer->FailureCount, 1);
	TestEqual(TEXT("No execute"), Fixture.ExecuteCount, 0);
	return true;
}
#endif
