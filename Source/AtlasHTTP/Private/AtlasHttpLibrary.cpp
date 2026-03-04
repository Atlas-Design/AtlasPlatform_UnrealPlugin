// Copyright Atlas Games. All Rights Reserved.

#include "AtlasHttpLibrary.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"

FString UAtlasHttpLibrary::PercentEncode(const FString& Source)
{
	// Manual URL encoding
	FString Result;
	for (int32 i = 0; i < Source.Len(); i++)
	{
		TCHAR Char = Source[i];
		if ((Char >= 'A' && Char <= 'Z') ||
			(Char >= 'a' && Char <= 'z') ||
			(Char >= '0' && Char <= '9') ||
			Char == '-' || Char == '_' || Char == '.' || Char == '~')
		{
			Result.AppendChar(Char);
		}
		else
		{
			Result += FString::Printf(TEXT("%%%02X"), (uint8)Char);
		}
	}
	return Result;
}

FString UAtlasHttpLibrary::PercentDecode(const FString& Source)
{
	FString Result;
	for (int32 i = 0; i < Source.Len(); i++)
	{
		if (Source[i] == '%' && i + 2 < Source.Len())
		{
			FString HexStr = Source.Mid(i + 1, 2);
			int32 HexValue = FParse::HexNumber(*HexStr);
			Result.AppendChar((TCHAR)HexValue);
			i += 2;
		}
		else if (Source[i] == '+')
		{
			Result.AppendChar(' ');
		}
		else
		{
			Result.AppendChar(Source[i]);
		}
	}
	return Result;
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
