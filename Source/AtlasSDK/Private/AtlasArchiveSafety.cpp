#include "AtlasArchiveSafety.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#else
#include <cstdio>
#endif

namespace AtlasArchive
{
#if PLATFORM_WINDOWS
	static FString NativePath(const FString& Path)
	{
		FString Full = FPaths::ConvertRelativePathToFull(Path);
		if (Full.Len() == 2 && Full[1] == TEXT(':')) Full += TEXT("/");
		Full.ReplaceInline(TEXT("/"), TEXT("\\"));
		if (Full.StartsWith(TEXT("\\\\?\\"))) return Full;
		return Full.StartsWith(TEXT("\\\\")) ? TEXT("\\\\?\\UNC\\") + Full.Mid(2) : TEXT("\\\\?\\") + Full;
	}
#endif

	namespace { int32 ScopeDepth = 0; TMap<FString, bool> AncestorChecks; }
	FScopedPathChecks::FScopedPathChecks() { check(IsInGameThread()); if (ScopeDepth++ == 0) AncestorChecks.Reset(); }
	FScopedPathChecks::~FScopedPathChecks() { if (--ScopeDepth == 0) AncestorChecks.Reset(); }
	bool HasSafeAncestors(const FString& Path)
	{
		const bool bCache = IsInGameThread() && ScopeDepth > 0;
		TArray<FString> Checked;
		bool bSafe = true;
		IPlatformFile& Disk = IPlatformFile::GetPlatformPhysical();
		for (FString Current = Path; !Current.IsEmpty(); Current = FPaths::GetPath(Current))
		{
			if (bCache)
			{
				if (const bool* Cached = AncestorChecks.Find(Current)) { bSafe = *Cached; break; }
				Checked.Add(Current);
			}

#if PLATFORM_WINDOWS
			const DWORD Attributes = ::GetFileAttributesW(*NativePath(Current));
			if (Attributes == INVALID_FILE_ATTRIBUTES)
			{
				const DWORD Error = ::GetLastError();
				if (Error != ERROR_FILE_NOT_FOUND && Error != ERROR_PATH_NOT_FOUND) { bSafe = false; break; }
			}
			else if ((Attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) { bSafe = false; break; }
#else
			if ((Disk.FileExists(*Current) || Disk.DirectoryExists(*Current))
				&& Disk.IsSymlink(*Current) != ESymlinkResult::NonSymlink) { bSafe = false; break; }
#endif

			if (FPaths::GetPath(Current) == Current) break;
		}
		if (bCache) for (const FString& Current : Checked) AncestorChecks.Add(Current, bSafe);
		return bSafe;
	}

	bool WriteBytes(const FString& Path, const uint8* Data, int64 Size)
	{
		IPlatformFile& Disk = IPlatformFile::GetPlatformPhysical();
		if (!IsContained(FPaths::GetPath(Path), Path) || !Disk.CreateDirectoryTree(*FPaths::GetPath(Path))) return false;
		const FString Temp = Path + TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
		TUniquePtr<IFileHandle> File(Disk.OpenWrite(*Temp));
		if (!File) return false;
		const bool bWritten = File->Write(Data, Size) && File->Flush(true);
		File.Reset();
		bool bReplaced = false;
		if (bWritten)
		{
#if PLATFORM_WINDOWS
			bReplaced = ::MoveFileExW(*NativePath(Temp), *NativePath(Path), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
			bReplaced = std::rename(TCHAR_TO_UTF8(*Temp), TCHAR_TO_UTF8(*Path)) == 0;
#endif
		}
		if (!bReplaced) Disk.DeleteFile(*Temp);
		return bReplaced;
	}

}
