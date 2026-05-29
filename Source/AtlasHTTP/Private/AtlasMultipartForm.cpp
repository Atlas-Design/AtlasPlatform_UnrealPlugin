// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasMultipartForm.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace
{
	void AppendAscii(TArray<uint8>& Buffer, const FString& Text)
	{
		const FTCHARToUTF8 Utf8(*Text);
		Buffer.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}
}

bool FAtlasMultipartForm::BuildSingleFileField(
	const FString& FieldName,
	const FString& FileName,
	const TArray<uint8>& FileContent,
	FMultipartBody& OutBody)
{
	OutBody = FMultipartBody();

	if (FieldName.IsEmpty() || FileName.IsEmpty() || FileContent.Num() == 0)
	{
		return false;
	}

	const FString Boundary = FString::Printf(TEXT("AtlasFormBoundary%s"), *FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
	const FString MimeType = GuessMimeTypeFromFileName(FileName);

	AppendAscii(OutBody.Content, FString::Printf(TEXT("--%s\r\n"), *Boundary));
	AppendAscii(
		OutBody.Content,
		FString::Printf(
			TEXT("Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"),
			*FieldName,
			*FileName));
	AppendAscii(OutBody.Content, FString::Printf(TEXT("Content-Type: %s\r\n\r\n"), *MimeType));
	OutBody.Content.Append(FileContent);
	AppendAscii(OutBody.Content, FString::Printf(TEXT("\r\n--%s--\r\n"), *Boundary));

	OutBody.ContentType = FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary);
	return OutBody.Content.Num() > 0;
}

FString FAtlasMultipartForm::GuessMimeTypeFromFileName(const FString& FileName)
{
	const FString Extension = FPaths::GetExtension(FileName, false).ToLower();

	if (Extension == TEXT("png"))
	{
		return TEXT("image/png");
	}
	if (Extension == TEXT("jpg") || Extension == TEXT("jpeg"))
	{
		return TEXT("image/jpeg");
	}
	if (Extension == TEXT("gif"))
	{
		return TEXT("image/gif");
	}
	if (Extension == TEXT("webp"))
	{
		return TEXT("image/webp");
	}
	if (Extension == TEXT("bmp"))
	{
		return TEXT("image/bmp");
	}
	if (Extension == TEXT("glb"))
	{
		return TEXT("model/gltf-binary");
	}
	if (Extension == TEXT("gltf"))
	{
		return TEXT("model/gltf+json");
	}
	if (Extension == TEXT("fbx"))
	{
		return TEXT("application/octet-stream");
	}
	if (Extension == TEXT("wav"))
	{
		return TEXT("audio/wav");
	}
	if (Extension == TEXT("mp3"))
	{
		return TEXT("audio/mpeg");
	}
	if (Extension == TEXT("ogg"))
	{
		return TEXT("audio/ogg");
	}

	return TEXT("application/octet-stream");
}
