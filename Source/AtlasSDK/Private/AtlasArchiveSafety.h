// Copyright Atlas Platform. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

namespace AtlasArchive
{
	// Memoize repeated ancestor checks only during one read-only index rebuild.
	struct FScopedPathChecks { FScopedPathChecks(); ~FScopedPathChecks(); };
	bool HasSafeAncestors(const FString& Path);
	inline bool IsContained(const FString& Root, const FString& Path)
	{
		if (Root.IsEmpty() || Path.IsEmpty()) return false;
		FString Base = FPaths::ConvertRelativePathToFull(Root);
		FString Target = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeDirectoryName(Base);
		FPaths::NormalizeFilename(Target);
		if (!FPaths::CollapseRelativeDirectories(Base) || !FPaths::CollapseRelativeDirectories(Target)) return false;
		#if PLATFORM_WINDOWS
		constexpr ESearchCase::Type PathCase = ESearchCase::IgnoreCase;
#else
		constexpr ESearchCase::Type PathCase = ESearchCase::CaseSensitive;
#endif
		if (!Target.StartsWith(Base + TEXT("/"), PathCase)) return false;
		// Reject Windows alternate streams and links, including links in existing ancestors.
		if (Target.Mid(2).Contains(TEXT(":"))) return false;
		return HasSafeAncestors(Target);
	}

	inline FString Component(const FString& Name)
	{
		FString Readable;
		for (TCHAR Ch : Name.Left(40)) Readable += FChar::IsAlnum(Ch) ? Ch : TEXT('_');
		// The original, case-sensitive name participates in identity on case-insensitive disks.
		FTCHARToUTF8 Utf8(*Name);
		return TEXT("p_") + Readable + TEXT("_") + FMD5::HashBytes(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}

	bool WriteBytes(const FString& Path, const uint8* Data, int64 Size);

	inline bool WriteText(const FString& Path, const FString& Text)
	{
		FTCHARToUTF8 Bytes(*Text);
		return WriteBytes(Path, reinterpret_cast<const uint8*>(Bytes.Get()), Bytes.Length());
	}
}
