// Copyright Atlas Games. All Rights Reserved.

#include "AtlasHttpLibrary.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"

FString UAtlasHttpLibrary::PercentEncode(const FString& Source)
{
	return FGenericPlatformHttp::UrlEncode(Source);
}

FString UAtlasHttpLibrary::PercentDecode(const FString& Source)
{
	return FGenericPlatformHttp::UrlDecode(Source);
}

FString UAtlasHttpLibrary::Base64Encode(const FString& Source)
{
	return FBase64::Encode(Source);
}

FString UAtlasHttpLibrary::Base64Decode(const FString& Source)
{
	FString Decoded;
	if (FBase64::Decode(Source, Decoded))
	{
		return Decoded;
	}
	return FString();
}

FString UAtlasHttpLibrary::Base64EncodeData(const TArray<uint8>& Data)
{
	return FBase64::Encode(Data);
}

TArray<uint8> UAtlasHttpLibrary::Base64DecodeData(const FString& Source)
{
	TArray<uint8> Decoded;
	FBase64::Decode(Source, Decoded);
	return Decoded;
}

FString UAtlasHttpLibrary::StringToMd5(const FString& Source)
{
	return FMD5::HashAnsiString(*Source).ToLower();
}

FString UAtlasHttpLibrary::StringToSha1(const FString& Source)
{
	FTCHARToUTF8 Converter(*Source);
	FSHAHash Hash;
	FSHA1::HashBuffer((const uint8*)Converter.Get(), Converter.Length(), Hash.Hash);
	return Hash.ToString().ToLower();
}

FString UAtlasHttpLibrary::BuildURLWithParams(const FString& BaseURL, const TMap<FString, FString>& QueryParams)
{
	if (QueryParams.Num() == 0)
	{
		return BaseURL;
	}

	FString Result = BaseURL;
	bool bFirstParam = !BaseURL.Contains(TEXT("?"));

	for (const auto& Param : QueryParams)
	{
		if (bFirstParam)
		{
			Result += TEXT("?");
			bFirstParam = false;
		}
		else
		{
			Result += TEXT("&");
		}

		Result += PercentEncode(Param.Key);
		Result += TEXT("=");
		Result += PercentEncode(Param.Value);
	}

	return Result;
}
