// Copyright Atlas Platform. All Rights Reserved.
#include "AtlasTempStorageSafety.h"

#include "HAL/PlatformFile.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace AtlasTempStorage
{
	namespace
	{
		const TCHAR* MarkerName = TEXT(".atlas-temp-storage");

		FString Normalize(const FString& Path)
		{
			// A trailing separator makes Unreal collapse a final '/.' component as well.
			FString Result = FPaths::ConvertRelativePathToFull(Path + TEXT("/"));
			FPaths::RemoveDuplicateSlashes(Result);
			FPaths::CollapseRelativeDirectories(Result);
			FPaths::NormalizeDirectoryName(Result);
			return Result;
		}

		bool IsWithin(const FString& Path, const FString& Parent)
		{
			return Path.Equals(Parent, ESearchCase::IgnoreCase)
				|| Path.StartsWith(Parent + TEXT("/"), ESearchCase::IgnoreCase);
		}

		bool Overlaps(const FString& A, const FString& B)
		{
			return IsWithin(A, B) || IsWithin(B, A);
		}

		FString ResolvePhysicalPath(const FString& Path)
		{
			IPlatformFile& Physical = IPlatformFile::GetPlatformPhysical();
			FString Ancestor = Normalize(Path);
			TArray<FString> Suffix;
			while (!Physical.DirectoryExists(*Ancestor) && !Physical.FileExists(*Ancestor))
			{
				const FString Parent = FPaths::GetPath(Ancestor);
				if (Parent.IsEmpty() || Parent == Ancestor) break;
				Suffix.Add(FPaths::GetCleanFilename(Ancestor));
				Ancestor = Parent;
			}
			FString Result = Physical.GetFilenameOnDisk(*Ancestor);
			for (int32 Index = Suffix.Num() - 1; Index >= 0; --Index)
			{
				Result = FPaths::Combine(Result, Suffix[Index]);
			}
			return Normalize(Result);
		}

		bool HasSafeAncestors(const FString& Path, FString& OutError)
		{
			// Use the physical filesystem: wrappers may not implement link detection.
			IPlatformFile& Physical = IPlatformFile::GetPlatformPhysical();
			FString Current = Path;
			while (!Current.IsEmpty())
			{
				if (Physical.IsSymlink(*Current) != ESymlinkResult::NonSymlink)
				{
					OutError = FString::Printf(TEXT("Linked paths or unavailable link detection are not permitted: %s"), *Current);
					return false;
				}
				const FString Parent = FPaths::GetPath(Current);
				if (Parent == Current)
				{
					break;
				}
				Current = Parent;
			}
			return true;
		}
	}

	bool ValidateDirectory(const FString& Directory, const FContext& Context, FString& OutDirectory, FString& OutError)
	{
		OutDirectory.Empty();
		OutError.Empty();
		if (Directory.IsEmpty() || Context.ProjectDirectory.IsEmpty() || Context.SavedDirectory.IsEmpty()
			|| Context.EngineDirectory.IsEmpty() || Context.OutputDirectory.IsEmpty())
		{
			OutError = TEXT("Cleanup requires a non-empty directory and complete storage context.");
			return false;
		}

		FString Input = Directory;
		Input.ReplaceInline(TEXT("\\"), TEXT("/"));
		// Disallow UNC/device paths and ambiguous Windows aliases, including alternate data streams.
		if (Input.StartsWith(TEXT("//")) || Input.Contains(TEXT("?")) || Input.Contains(TEXT("*")))
		{
			OutError = TEXT("Cleanup supports ordinary local filesystem paths only.");
			return false;
		}
		TArray<FString> Components;
		Input.ParseIntoArray(Components, TEXT("/"), true);
		for (int32 Index = 0; Index < Components.Num(); ++Index)
		{
			const FString& Component = Components[Index];
			const bool bDrive = Index == 0 && Component.Len() == 2
				&& FChar::IsAlpha(Component[0]) && Component[1] == TEXT(':');
			if ((!bDrive && Component.Contains(TEXT(":")))
				|| Component.EndsWith(TEXT(" "))
				|| (Component != TEXT(".") && Component != TEXT("..") && Component.EndsWith(TEXT("."))))
			{
				OutError = TEXT("Cleanup refuses ambiguous path components.");
				return false;
			}
		}

		const FString Project = Normalize(Context.ProjectDirectory);
		const FString Candidate = Normalize(FPaths::IsRelative(Input) ? FPaths::Combine(Project, Input) : Input);
		const FString Parent = FPaths::GetPath(Candidate);
		if (Candidate.IsEmpty() || Parent.IsEmpty() || Parent == Candidate
			|| (Candidate.Len() <= 3 && Candidate.Len() >= 2 && Candidate[1] == TEXT(':'))
			|| IsWithin(Project, Candidate))
		{
			OutError = TEXT("Cleanup cannot target a filesystem root, project root, or project ancestor.");
			return false;
		}

		TArray<FString> Protected;
		for (const TCHAR* Folder : { TEXT("Content"), TEXT("Config"), TEXT("Source"), TEXT("Plugins"),
			TEXT("Binaries"), TEXT("Build"), TEXT("Intermediate"), TEXT(".git"), TEXT(".p4") })
		{
			Protected.Add(FPaths::Combine(Project, Folder));
		}
		Protected.Add(Normalize(Context.EngineDirectory));
		Protected.Add(Normalize(Context.OutputDirectory));
		for (const TCHAR* Folder : { TEXT("History"), TEXT("Batches"), TEXT("Output") })
		{
			Protected.Add(FPaths::Combine(Normalize(Context.SavedDirectory), TEXT("Atlas"), Folder));
		}
		for (const FString& Path : Protected)
		{
			if (Overlaps(Candidate, Path))
			{
				OutError = FString::Printf(TEXT("Cleanup overlaps a protected directory: %s"), *Path);
				return false;
			}
		}
		OutDirectory = Candidate;
		return true;
	}

	int32 CleanupDirectory(const FString& Directory, const FContext& Context, FString& OutError)
	{
		FString Root;
		if (!ValidateDirectory(Directory, Context, Root, OutError) || !HasSafeAncestors(Root, OutError))
		{
			return 0;
		}
		// Resolve existing ancestors as well: a protected output folder may use a different
		// spelling, short-name alias, or link to the same physical scratch location.
		const FContext PhysicalContext{
			ResolvePhysicalPath(Context.ProjectDirectory), ResolvePhysicalPath(Context.SavedDirectory),
			ResolvePhysicalPath(Context.EngineDirectory), ResolvePhysicalPath(Context.OutputDirectory) };
		const FString PhysicalRoot = ResolvePhysicalPath(Root);
		if (!ValidateDirectory(PhysicalRoot, PhysicalContext, Root, OutError) || !HasSafeAncestors(Root, OutError))
		{
			return 0;
		}
		IPlatformFile& Filesystem = IPlatformFile::GetPlatformPhysical();
		if (!Filesystem.CreateDirectoryTree(*Root) || !HasSafeAncestors(Root, OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Could not create the scratch directory.");
			}
			return 0;
		}

		const FString Marker = FPaths::Combine(Root, MarkerName);
		const FString ExpectedMarker = TEXT("AtlasWorkflow temp storage v1\n") + Root;
		if (!HasSafeAncestors(Marker, OutError))
		{
			return 0;
		}
		if (!Filesystem.FileExists(*Marker))
		{
			bool bEmpty = true;
			const bool bEnumerated = Filesystem.IterateDirectory(*Root, [&bEmpty](const TCHAR*, bool)
			{
				bEmpty = false;
				return true;
			});
			if (!bEnumerated || !bEmpty)
			{
				OutError = TEXT("Unmarked non-empty directory preserved. Choose a new empty scratch folder; review existing files manually.");
				return 0;
			}
			if (!FFileHelper::SaveStringToFile(ExpectedMarker, *Marker, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				OutError = TEXT("Could not establish scratch directory ownership.");
			}
			return 0;
		}
		FString MarkerContents;
		if (Filesystem.FileSize(*Marker) > 16384 || !FFileHelper::LoadFileToString(MarkerContents, *Marker)
			|| MarkerContents != ExpectedMarker)
		{
			OutError = TEXT("Invalid scratch ownership marker; directory preserved.");
			return 0;
		}

		// Validate the entire tree before deleting any files. Never recurse through a link.
		TArray<FString> Pending{ Root };
		TArray<FString> Files;
		TArray<FString> Directories;
		while (!Pending.IsEmpty())
		{
			const FString Current = Pending.Pop();
			if (!HasSafeAncestors(Current, OutError))
			{
				return 0;
			}
			const bool bEnumerated = Filesystem.IterateDirectory(*Current,
				[&](const TCHAR* Path, bool bDirectory)
				{
					const FString Entry = Normalize(Path);
					if (!IsWithin(Entry, Root) || !HasSafeAncestors(Entry, OutError))
					{
						if (OutError.IsEmpty()) OutError = TEXT("Directory entry escapes scratch storage.");
						return false;
					}
					if (bDirectory)
					{
						Directories.Add(Entry);
						Pending.Add(Entry);
					}
					else if (!Entry.Equals(Marker, ESearchCase::IgnoreCase))
					{
						Files.Add(Entry);
					}
					return true;
				});
			if (!bEnumerated || !OutError.IsEmpty())
			{
				if (OutError.IsEmpty()) OutError = TEXT("Could not inspect all scratch directory entries.");
				return 0;
			}
		}

		int32 Deleted = 0;
		for (const FString& File : Files)
		{
			if (!HasSafeAncestors(File, OutError)) return Deleted;
			if (Filesystem.DeleteFile(*File))
			{
				++Deleted;
			}
			else
			{
				OutError = TEXT("Some scratch files could not be deleted; they were retained.");
			}
		}
		Directories.Sort([](const FString& A, const FString& B) { return A.Len() > B.Len(); });
		for (const FString& Child : Directories)
		{
			if (!HasSafeAncestors(Child, OutError)) return Deleted;
			Filesystem.DeleteDirectory(*Child); // Non-recursive: never remove newly added contents.
		}
		return Deleted;
	}
}
