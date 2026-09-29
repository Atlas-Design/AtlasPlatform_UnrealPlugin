// Copyright Atlas Platform. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS

#include "AtlasTempStorageSafety.h"
#include "AtlasSDKSettings.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	struct FTempFixture
	{
		FString Root;
		AtlasTempStorage::FContext Context;
		FTempFixture()
		{
			// Intentionally retain small fixtures as test evidence; no recursive teardown against user paths.
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),
				TEXT("Automation/AtlasTempStorageSafety"), FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			Context.ProjectDirectory = FPaths::Combine(Root, TEXT("Project"));
			Context.SavedDirectory = FPaths::Combine(Context.ProjectDirectory, TEXT("Saved"));
			Context.EngineDirectory = FPaths::Combine(Root, TEXT("Engine"));
			Context.OutputDirectory = FPaths::Combine(Context.SavedDirectory, TEXT("Atlas/Output"));
		}

		FString Scratch() const { return FPaths::Combine(Context.SavedDirectory, TEXT("Atlas/TempExports")); }

		bool Write(const FString& Path, const FString& Value = TEXT("keep me"))
		{
			return IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*FPaths::GetPath(Path))
				&& FFileHelper::SaveStringToFile(Value, *Path);
		}
	};

	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasTempPathPolicyTest, "Atlas.TempStorage.PathPolicy", TestFlags)
bool FAtlasTempPathPolicyTest::RunTest(const FString& Parameters)
{
	FTempFixture Fixture;
	const FString Rejected[] = {
		TEXT(""), TEXT("."), TEXT(".."), TEXT("Content"), TEXT("Content/Child"), TEXT("Config"),
		TEXT("Source"), TEXT("Plugins/Other"), TEXT("Saved"), TEXT("Saved/Atlas"),
		TEXT("Saved/Atlas/History"), TEXT("Saved/Atlas/Batches/Runs"),
		TEXT("Saved/Atlas/Output"), TEXT("Saved/Atlas/TempExports/../../../Content"),
		TEXT("Saved/Atlas/Scratch."), TEXT("Saved/Atlas/Scratch "),
		TEXT("Saved/Atlas/Scratch:stream"), TEXT("C:relative"),
		TEXT("//server/share/scratch"), TEXT("//?/C:/scratch"),
		TEXT("C:/"), TEXT("/"), Fixture.Context.EngineDirectory,
		FPaths::Combine(Fixture.Context.EngineDirectory, TEXT("Scratch"))
	};
	for (const FString& Path : Rejected)
	{
		FString Resolved, Error;
		TestFalse(*FString::Printf(TEXT("Reject %s"), *Path),
			AtlasTempStorage::ValidateDirectory(Path, Fixture.Context, Resolved, Error));
		TestFalse(TEXT("Rejection has a reason"), Error.IsEmpty());
	}
	const FString Accepted[] = {
		TEXT("Saved/Atlas/TempExports"), TEXT("Saved/Atlas/TempImports"),
		TEXT("Saved/Atlas/Unused/../TempExports/"),
		TEXT("ContentBackup"), FPaths::Combine(Fixture.Root, TEXT("ExternalScratch"))
	};
	for (const FString& Path : Accepted)
	{
		FString Resolved, Error;
		TestTrue(*FString::Printf(TEXT("Accept dedicated scratch candidate %s"), *Path),
			AtlasTempStorage::ValidateDirectory(Path, Fixture.Context, Resolved, Error));
		TestTrue(TEXT("Absolute resolved path"), !FPaths::IsRelative(Resolved));
	}
	auto Overlapping = Fixture.Context;
	Overlapping.OutputDirectory = Fixture.Scratch();
	FString Resolved, Error;
	TestFalse(TEXT("Output root cannot be temp"), AtlasTempStorage::ValidateDirectory(Fixture.Scratch(), Overlapping, Resolved, Error));
	TestFalse(TEXT("Output ancestor cannot be temp"), AtlasTempStorage::ValidateDirectory(FPaths::GetPath(Fixture.Scratch()), Overlapping, Resolved, Error));
	TestFalse(TEXT("Output child cannot be temp"), AtlasTempStorage::ValidateDirectory(FPaths::Combine(Fixture.Scratch(), TEXT("Child")), Overlapping, Resolved, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasTempOwnershipTest, "Atlas.TempStorage.OwnershipAndCleanup", TestFlags)
bool FAtlasTempOwnershipTest::RunTest(const FString& Parameters)
{
	FTempFixture Fixture;
	IPlatformFile& Filesystem = IPlatformFile::GetPlatformPhysical();
	FString Error;
	const FString Scratch = Fixture.Scratch();
	const FString Marker = FPaths::Combine(Scratch, TEXT(".atlas-temp-storage"));
	const FString Outside = FPaths::Combine(Fixture.Context.ProjectDirectory, TEXT("Content/Important.txt"));
	TestTrue(TEXT("Create protected sentinel"), Fixture.Write(Outside));

	TestEqual(TEXT("Initialize empty scratch"), AtlasTempStorage::CleanupDirectory(Scratch, Fixture.Context, Error), 0);
	TestTrue(*Error, Error.IsEmpty());
	TestTrue(TEXT("Ownership marker exists"), Filesystem.FileExists(*Marker));
	FString OriginalMarker;
	TestTrue(TEXT("Read marker"), FFileHelper::LoadFileToString(OriginalMarker, *Marker));

	const FString FileA = FPaths::Combine(Scratch, TEXT("Export.png"));
	const FString FileB = FPaths::Combine(Scratch, TEXT("Nested/Download.glb"));
	TestTrue(TEXT("Create first temp file"), Fixture.Write(FileA));
	TestTrue(TEXT("Create nested temp file"), Fixture.Write(FileB));
	TestEqual(TEXT("Delete exactly two owned files"), AtlasTempStorage::CleanupDirectory(Scratch, Fixture.Context, Error), 2);
	TestTrue(*Error, Error.IsEmpty());
	TestFalse(TEXT("First temp file gone"), Filesystem.FileExists(*FileA));
	TestFalse(TEXT("Nested temp file gone"), Filesystem.FileExists(*FileB));
	TestFalse(TEXT("Empty nested folder gone"), Filesystem.DirectoryExists(*FPaths::GetPath(FileB)));
	TestTrue(TEXT("Root survives"), Filesystem.DirectoryExists(*Scratch));
	TestTrue(TEXT("Marker survives"), Filesystem.FileExists(*Marker));
	TestTrue(TEXT("Protected sentinel survives"), Filesystem.FileExists(*Outside));
	TestEqual(TEXT("Repeated cleanup is harmless"), AtlasTempStorage::CleanupDirectory(Scratch, Fixture.Context, Error), 0);

	const FString Legacy = FPaths::Combine(Fixture.Root, TEXT("ExternalLegacy"));
	const FString LegacyFile = FPaths::Combine(Legacy, TEXT("Unrelated.txt"));
	TestTrue(TEXT("Create unmarked external contents"), Fixture.Write(LegacyFile));
	TestEqual(TEXT("Unmarked folder untouched"), AtlasTempStorage::CleanupDirectory(Legacy, Fixture.Context, Error), 0);
	TestFalse(TEXT("Unmarked folder reports why"), Error.IsEmpty());
	TestTrue(TEXT("Unrelated file preserved"), Filesystem.FileExists(*LegacyFile));
	TestFalse(TEXT("Non-empty folder never silently adopted"), Filesystem.FileExists(*FPaths::Combine(Legacy, TEXT(".atlas-temp-storage"))));

	TestTrue(TEXT("Create file before corrupt marker check"), Fixture.Write(FileA));
	TestTrue(TEXT("Corrupt marker"), Fixture.Write(Marker, TEXT("wrong marker")));
	TestEqual(TEXT("Invalid marker prevents deletion"), AtlasTempStorage::CleanupDirectory(Scratch, Fixture.Context, Error), 0);
	TestTrue(TEXT("Data behind invalid marker survives"), Filesystem.FileExists(*FileA));

	const FString External = FPaths::Combine(Fixture.Root, TEXT("ExternalScratch"));
	TestEqual(TEXT("Initialize external scratch"), AtlasTempStorage::CleanupDirectory(External, Fixture.Context, Error), 0);
	TestTrue(*Error, Error.IsEmpty());
	TestTrue(TEXT("Write external scratch"), Fixture.Write(FPaths::Combine(External, TEXT("Temp.bin"))));
	TestEqual(TEXT("Owned external scratch works"), AtlasTempStorage::CleanupDirectory(External, Fixture.Context, Error), 1);
	TestTrue(TEXT("Copy marker from another root"), Fixture.Write(FPaths::Combine(External, TEXT(".atlas-temp-storage")), OriginalMarker));
	TestTrue(TEXT("Write data behind copied marker"), Fixture.Write(FPaths::Combine(External, TEXT("Keep.bin"))));
	TestEqual(TEXT("Copied marker is refused"), AtlasTempStorage::CleanupDirectory(External, Fixture.Context, Error), 0);
	TestFalse(TEXT("Copied marker reports reason"), Error.IsEmpty());
	TestTrue(TEXT("Data behind copied marker survives"), Filesystem.FileExists(*FPaths::Combine(External, TEXT("Keep.bin"))));

	TestEqual(TEXT("Protected folder never cleaned"), AtlasTempStorage::CleanupDirectory(FPaths::GetPath(Outside), Fixture.Context, Error), 0);
	TestTrue(TEXT("Protected contents still exist"), Filesystem.FileExists(*Outside));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasTempSettingsTest, "Atlas.TempStorage.SettingsResolution", TestFlags)
bool FAtlasTempSettingsTest::RunTest(const FString& Parameters)
{
	UAtlasSDKSettings* Settings = NewObject<UAtlasSDKSettings>();
	Settings->TempExportPath = TEXT("Saved/Atlas/TempExports");
	FString Expected = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Settings->TempExportPath);
	FPaths::NormalizeDirectoryName(Expected);
	TestEqual(TEXT("Relative export is project-relative"), Settings->GetTempExportFolderPath(), Expected);
	Settings->TempExportPath.Empty();
	TestEqual(TEXT("Empty export retains safe default"), Settings->GetTempExportFolderPath(), Expected);

	FTempFixture Fixture;
	Settings->TempImportPath = FPaths::Combine(Fixture.Root, TEXT("ExternalScratch"));
	TestEqual(TEXT("Absolute import remains absolute"), Settings->GetTempImportFolderPath(), Settings->TempImportPath);
	Settings->TempExportPath = Settings->TempImportPath;
	TestEqual(TEXT("Absolute export remains absolute"), Settings->GetTempExportFolderPath(), Settings->TempExportPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtlasTempJunctionTest, "Atlas.TempStorage.Junctions", TestFlags)
bool FAtlasTempJunctionTest::RunTest(const FString& Parameters)
{
	// The Windows integration runner supplies disposable real junctions. Never create links to user data.
	const FString Root = FPlatformMisc::GetEnvironmentVariable(TEXT("ATLAS_TEMP_TEST_LINK_ROOT"));
	if (Root.IsEmpty())
	{
		AddWarning(TEXT("Junction integration not exercised: set ATLAS_TEMP_TEST_LINK_ROOT to the prepared disposable fixture."));
		return true;
	}
	AtlasTempStorage::FContext Context{
		FPaths::Combine(Root, TEXT("Project")), FPaths::Combine(Root, TEXT("Project/Saved")),
		FPaths::Combine(Root, TEXT("Engine")), FPaths::Combine(Root, TEXT("Project/Saved/Atlas/Output")) };
	IPlatformFile& Filesystem = IPlatformFile::GetPlatformPhysical();
	FString Error;
	const FString Link = FPaths::Combine(Root, TEXT("Link"));
	TestTrue(TEXT("Fixture contains a real junction"), Filesystem.IsSymlink(*Link) == ESymlinkResult::Symlink);
	TestEqual(TEXT("Refuse junction root"), AtlasTempStorage::CleanupDirectory(Link, Context, Error), 0);
	TestFalse(TEXT("Junction rejection has reason"), Error.IsEmpty());
	TestEqual(TEXT("Refuse junction ancestor"), AtlasTempStorage::CleanupDirectory(FPaths::Combine(Link, TEXT("Child")), Context, Error), 0);
	TestFalse(TEXT("Junction ancestor rejection has reason"), Error.IsEmpty());

	const FString Scratch = FPaths::Combine(Root, TEXT("Scratch"));
	TestEqual(TEXT("Refuse nested junction before deleting ordinary files"), AtlasTempStorage::CleanupDirectory(Scratch, Context, Error), 0);
	TestFalse(TEXT("Nested junction rejection has reason"), Error.IsEmpty());
	TestTrue(TEXT("Scratch file survives preflight refusal"), Filesystem.FileExists(*FPaths::Combine(Scratch, TEXT("Keep.txt"))));
	TestTrue(TEXT("Junction target survives"), Filesystem.FileExists(*FPaths::Combine(Root, TEXT("Target/Important.txt"))));
	Context.OutputDirectory = Link;
	TestEqual(TEXT("Output alias target cannot be cleaned"), AtlasTempStorage::CleanupDirectory(FPaths::Combine(Root, TEXT("Target")), Context, Error), 0);
	TestTrue(TEXT("Physical output overlap detected"), Error.Contains(TEXT("protected directory")));
	return true;
}
#endif
