// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Builds multipart/form-data bodies for Atlas Platform file uploads (API v0.2+).
 */
class ATLASHTTP_API FAtlasMultipartForm
{
public:
	struct FMultipartBody
	{
		TArray<uint8> Content;
		FString ContentType;
	};

	/** Guess MIME type from filename extension for multipart file parts. */
	static FString GuessMimeTypeFromFileName(const FString& FileName);

	/**
	 * Build a single-file multipart body (matches platform Python: files={"file": (...)}).
	 * @param FieldName Form field name (e.g. "file")
	 * @param FileName Filename sent in Content-Disposition
	 * @param FileContent Raw file bytes
	 */
	static bool BuildSingleFileField(
		const FString& FieldName,
		const FString& FileName,
		const TArray<uint8>& FileContent,
		FMultipartBody& OutBody);
};
