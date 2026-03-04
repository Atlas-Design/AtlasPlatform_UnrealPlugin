// Copyright Atlas Games. All Rights Reserved.

#include "AtlasHttpTests.h"
#include "AtlasJsonObject.h"
#include "AtlasJsonValue.h"
#include "AtlasHttpLibrary.h"
#include "AtlasHttpAsyncAction.h"
#include "AtlasHTTP.h"

int32 UAtlasHttpTests::TestsPassed = 0;
int32 UAtlasHttpTests::TestsFailed = 0;

void UAtlasHttpTests::LogTestResult(const FString& TestName, bool bPassed, const FString& Details)
{
	if (bPassed)
	{
		TestsPassed++;
		UE_LOG(LogAtlasHTTP, Log, TEXT("[PASS] %s"), *TestName);
	}
	else
	{
		TestsFailed++;
		UE_LOG(LogAtlasHTTP, Error, TEXT("[FAIL] %s - %s"), *TestName, *Details);
	}
}

bool UAtlasHttpTests::RunAllTests()
{
	UE_LOG(LogAtlasHTTP, Log, TEXT(""));
	UE_LOG(LogAtlasHTTP, Log, TEXT("========================================"));
	UE_LOG(LogAtlasHTTP, Log, TEXT("  AtlasHTTP Plugin Tests"));
	UE_LOG(LogAtlasHTTP, Log, TEXT("========================================"));
	UE_LOG(LogAtlasHTTP, Log, TEXT(""));

	// Reset counters
	TestsPassed = 0;
	TestsFailed = 0;

	// Run all test suites
	TestJsonObject();
	TestJsonValue();
	TestBase64();
	TestUrlEncoding();
	TestHashing();

	// Print summary
	UE_LOG(LogAtlasHTTP, Log, TEXT(""));
	UE_LOG(LogAtlasHTTP, Log, TEXT("========================================"));
	UE_LOG(LogAtlasHTTP, Log, TEXT("  Test Summary"));
	UE_LOG(LogAtlasHTTP, Log, TEXT("========================================"));
	UE_LOG(LogAtlasHTTP, Log, TEXT("  Passed: %d"), TestsPassed);
	UE_LOG(LogAtlasHTTP, Log, TEXT("  Failed: %d"), TestsFailed);
	UE_LOG(LogAtlasHTTP, Log, TEXT("  Total:  %d"), TestsPassed + TestsFailed);
	UE_LOG(LogAtlasHTTP, Log, TEXT("========================================"));
	
	if (TestsFailed == 0)
	{
		UE_LOG(LogAtlasHTTP, Log, TEXT("  ALL TESTS PASSED!"));
	}
	else
	{
		UE_LOG(LogAtlasHTTP, Error, TEXT("  SOME TESTS FAILED!"));
	}
	UE_LOG(LogAtlasHTTP, Log, TEXT("========================================"));
	UE_LOG(LogAtlasHTTP, Log, TEXT(""));
	UE_LOG(LogAtlasHTTP, Log, TEXT("Note: HTTP tests require network and must be run separately."));
	UE_LOG(LogAtlasHTTP, Log, TEXT("Call TestHttpGet() or TestHttpPost() to test HTTP functionality."));
	UE_LOG(LogAtlasHTTP, Log, TEXT(""));

	return TestsFailed == 0;
}

bool UAtlasHttpTests::TestJsonObject()
{
	UE_LOG(LogAtlasHTTP, Log, TEXT("--- JSON Object Tests ---"));
	
	int32 LocalPassed = 0;
	int32 LocalFailed = 0;

	// Test 1: Create and set string field
	{
		UAtlasJsonObject* Obj = UAtlasJsonObject::MakeJsonObject();
		Obj->SetStringField(TEXT("name"), TEXT("Atlas"));
		FString Value = Obj->GetStringField(TEXT("name"));
		bool bPassed = Value == TEXT("Atlas");
		LogTestResult(TEXT("JSON SetStringField/GetStringField"), bPassed, 
			bPassed ? TEXT("") : FString::Printf(TEXT("Expected 'Atlas', got '%s'"), *Value));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	// Test 2: Set number field
	{
		UAtlasJsonObject* Obj = UAtlasJsonObject::MakeJsonObject();
		Obj->SetNumberField(TEXT("value"), 42.5f);
		float Value = Obj->GetNumberField(TEXT("value"));
		bool bPassed = FMath::IsNearlyEqual(Value, 42.5f);
		LogTestResult(TEXT("JSON SetNumberField/GetNumberField"), bPassed,
			bPassed ? TEXT("") : FString::Printf(TEXT("Expected 42.5, got %f"), Value));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	// Test 3: Set integer field
	{
		UAtlasJsonObject* Obj = UAtlasJsonObject::MakeJsonObject();
		Obj->SetIntegerField(TEXT("count"), 100);
		int32 Value = Obj->GetIntegerField(TEXT("count"));
		bool bPassed = Value == 100;
		LogTestResult(TEXT("JSON SetIntegerField/GetIntegerField"), bPassed,
			bPassed ? TEXT("") : FString::Printf(TEXT("Expected 100, got %d"), Value));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	// Test 4: Set bool field
	{
		UAtlasJsonObject* Obj = UAtlasJsonObject::MakeJsonObject();
		Obj->SetBoolField(TEXT("enabled"), true);
		bool Value = Obj->GetBoolField(TEXT("enabled"));
		bool bPassed = Value == true;
		LogTestResult(TEXT("JSON SetBoolField/GetBoolField"), bPassed,
			bPassed ? TEXT("") : TEXT("Expected true, got false"));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	// Test 5: HasField
	{
		UAtlasJsonObject* Obj = UAtlasJsonObject::MakeJsonObject();
		Obj->SetStringField(TEXT("test"), TEXT("value"));
		bool bHas = Obj->HasField(TEXT("test"));
		bool bNotHas = !Obj->HasField(TEXT("nonexistent"));
		bool bPassed = bHas && bNotHas;
		LogTestResult(TEXT("JSON HasField"), bPassed,
			bPassed ? TEXT("") : TEXT("HasField returned incorrect result"));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	// Test 6: EncodeJson
	{
		UAtlasJsonObject* Obj = UAtlasJsonObject::MakeJsonObject();
		Obj->SetStringField(TEXT("name"), TEXT("test"));
		Obj->SetNumberField(TEXT("value"), 123);
		FString Json = Obj->EncodeJson();
		bool bPassed = Json.Contains(TEXT("name")) && Json.Contains(TEXT("test")) && Json.Contains(TEXT("123"));
		LogTestResult(TEXT("JSON EncodeJson"), bPassed,
			bPassed ? TEXT("") : FString::Printf(TEXT("Unexpected JSON: %s"), *Json));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	// Test 7: DecodeJson / FromJsonString
	{
		FString JsonStr = TEXT("{\"name\":\"decoded\",\"count\":42}");
		UAtlasJsonObject* Obj = UAtlasJsonObject::FromJsonString(JsonStr);
		bool bPassed = Obj != nullptr && 
			Obj->GetStringField(TEXT("name")) == TEXT("decoded") &&
			Obj->GetIntegerField(TEXT("count")) == 42;
		LogTestResult(TEXT("JSON FromJsonString"), bPassed,
			bPassed ? TEXT("") : TEXT("Failed to decode JSON string"));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	// Test 8: Nested object
	{
		UAtlasJsonObject* Inner = UAtlasJsonObject::MakeJsonObject();
		Inner->SetStringField(TEXT("inner"), TEXT("value"));
		
		UAtlasJsonObject* Outer = UAtlasJsonObject::MakeJsonObject();
		Outer->SetObjectField(TEXT("nested"), Inner);
		
		UAtlasJsonObject* Retrieved = Outer->GetObjectField(TEXT("nested"));
		bool bPassed = Retrieved != nullptr && Retrieved->GetStringField(TEXT("inner")) == TEXT("value");
		LogTestResult(TEXT("JSON Nested Object"), bPassed,
			bPassed ? TEXT("") : TEXT("Failed to get nested object"));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	return LocalFailed == 0;
}

bool UAtlasHttpTests::TestJsonValue()
{
	UE_LOG(LogAtlasHTTP, Log, TEXT("--- JSON Value Tests ---"));

	int32 LocalPassed = 0;
	int32 LocalFailed = 0;

	// Test 1: String value
	{
		UAtlasJsonValue* Val = UAtlasJsonValue::MakeString(TEXT("hello"));
		bool bPassed = Val->AsString() == TEXT("hello") && Val->GetType() == EAtlasJsonValueType::String;
		LogTestResult(TEXT("JSON Value String"), bPassed,
			bPassed ? TEXT("") : TEXT("String value mismatch"));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	// Test 2: Number value
	{
		UAtlasJsonValue* Val = UAtlasJsonValue::MakeNumber(3.14f);
		bool bPassed = FMath::IsNearlyEqual(Val->AsNumber(), 3.14f, 0.01f) && Val->GetType() == EAtlasJsonValueType::Number;
		LogTestResult(TEXT("JSON Value Number"), bPassed,
			bPassed ? TEXT("") : FString::Printf(TEXT("Expected 3.14, got %f"), Val->AsNumber()));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	// Test 3: Bool value
	{
		UAtlasJsonValue* Val = UAtlasJsonValue::MakeBool(true);
		bool bPassed = Val->AsBool() == true && Val->GetType() == EAtlasJsonValueType::Boolean;
		LogTestResult(TEXT("JSON Value Bool"), bPassed,
			bPassed ? TEXT("") : TEXT("Bool value mismatch"));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	// Test 4: Null value
	{
		UAtlasJsonValue* Val = UAtlasJsonValue::MakeNull();
		bool bPassed = Val->IsNull() && Val->GetType() == EAtlasJsonValueType::Null;
		LogTestResult(TEXT("JSON Value Null"), bPassed,
			bPassed ? TEXT("") : TEXT("Null value check failed"));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	return LocalFailed == 0;
}

bool UAtlasHttpTests::TestBase64()
{
	UE_LOG(LogAtlasHTTP, Log, TEXT("--- Base64 Tests ---"));

	int32 LocalPassed = 0;
	int32 LocalFailed = 0;

	// Test 1: Encode string
	{
		FString Encoded = UAtlasHttpLibrary::Base64Encode(TEXT("Hello World"));
		bool bPassed = Encoded == TEXT("SGVsbG8gV29ybGQ=");
		LogTestResult(TEXT("Base64 Encode String"), bPassed,
			bPassed ? TEXT("") : FString::Printf(TEXT("Expected 'SGVsbG8gV29ybGQ=', got '%s'"), *Encoded));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	// Test 2: Decode string
	{
		FString Decoded = UAtlasHttpLibrary::Base64Decode(TEXT("SGVsbG8gV29ybGQ="));
		bool bPassed = Decoded == TEXT("Hello World");
		LogTestResult(TEXT("Base64 Decode String"), bPassed,
			bPassed ? TEXT("") : FString::Printf(TEXT("Expected 'Hello World', got '%s'"), *Decoded));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	// Test 3: Round-trip
	{
		FString Original = TEXT("AtlasHTTP Plugin Test!");
		FString Encoded = UAtlasHttpLibrary::Base64Encode(Original);
		FString Decoded = UAtlasHttpLibrary::Base64Decode(Encoded);
		bool bPassed = Original == Decoded;
		LogTestResult(TEXT("Base64 Round-trip"), bPassed,
			bPassed ? TEXT("") : FString::Printf(TEXT("Round-trip failed: '%s' != '%s'"), *Original, *Decoded));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	return LocalFailed == 0;
}

bool UAtlasHttpTests::TestUrlEncoding()
{
	UE_LOG(LogAtlasHTTP, Log, TEXT("--- URL Encoding Tests ---"));

	int32 LocalPassed = 0;
	int32 LocalFailed = 0;

	// Test 1: Encode spaces
	{
		FString Encoded = UAtlasHttpLibrary::PercentEncode(TEXT("hello world"));
		bool bPassed = Encoded == TEXT("hello%20world");
		LogTestResult(TEXT("URL Encode Spaces"), bPassed,
			bPassed ? TEXT("") : FString::Printf(TEXT("Expected 'hello%%20world', got '%s'"), *Encoded));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	// Test 2: Encode special characters
	{
		FString Encoded = UAtlasHttpLibrary::PercentEncode(TEXT("a=b&c=d"));
		bool bPassed = Encoded.Contains(TEXT("%3D")) && Encoded.Contains(TEXT("%26"));
		LogTestResult(TEXT("URL Encode Special Chars"), bPassed,
			bPassed ? TEXT("") : FString::Printf(TEXT("Special chars not encoded: '%s'"), *Encoded));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	// Test 3: Decode
	{
		FString Decoded = UAtlasHttpLibrary::PercentDecode(TEXT("hello%20world"));
		bool bPassed = Decoded == TEXT("hello world");
		LogTestResult(TEXT("URL Decode"), bPassed,
			bPassed ? TEXT("") : FString::Printf(TEXT("Expected 'hello world', got '%s'"), *Decoded));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	// Test 4: BuildURLWithParams
	{
		TMap<FString, FString> Params;
		Params.Add(TEXT("key"), TEXT("value"));
		Params.Add(TEXT("name"), TEXT("test"));
		FString URL = UAtlasHttpLibrary::BuildURLWithParams(TEXT("https://example.com/api"), Params);
		bool bPassed = URL.Contains(TEXT("?")) && URL.Contains(TEXT("key=value")) && URL.Contains(TEXT("name=test"));
		LogTestResult(TEXT("BuildURLWithParams"), bPassed,
			bPassed ? TEXT("") : FString::Printf(TEXT("URL format incorrect: '%s'"), *URL));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	return LocalFailed == 0;
}

bool UAtlasHttpTests::TestHashing()
{
	UE_LOG(LogAtlasHTTP, Log, TEXT("--- Hashing Tests ---"));

	int32 LocalPassed = 0;
	int32 LocalFailed = 0;

	// Test 1: MD5
	{
		FString Hash = UAtlasHttpLibrary::StringToMd5(TEXT("test"));
		// Known MD5 of "test" is 098f6bcd4621d373cade4e832627b4f6
		bool bPassed = Hash == TEXT("098f6bcd4621d373cade4e832627b4f6");
		LogTestResult(TEXT("MD5 Hash"), bPassed,
			bPassed ? TEXT("") : FString::Printf(TEXT("Expected '098f6bcd4621d373cade4e832627b4f6', got '%s'"), *Hash));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	// Test 2: SHA1 (just verify it produces output)
	{
		FString Hash = UAtlasHttpLibrary::StringToSha1(TEXT("test"));
		bool bPassed = Hash.Len() == 40; // SHA1 is 40 hex characters
		LogTestResult(TEXT("SHA1 Hash Length"), bPassed,
			bPassed ? TEXT("") : FString::Printf(TEXT("Expected 40 chars, got %d: '%s'"), Hash.Len(), *Hash));
		bPassed ? LocalPassed++ : LocalFailed++;
	}

	return LocalFailed == 0;
}

void UAtlasHttpTests::TestHttpGet(UObject* WorldContextObject)
{
	UE_LOG(LogAtlasHTTP, Log, TEXT(""));
	UE_LOG(LogAtlasHTTP, Log, TEXT("--- HTTP GET Test (Async) ---"));
	UE_LOG(LogAtlasHTTP, Log, TEXT("Sending GET request to httpbin.org/get..."));

	UAtlasHttpAsyncAction* Action = UAtlasHttpAsyncAction::AsyncHttpGet(
		WorldContextObject,
		TEXT("https://httpbin.org/get"),
		TMap<FString, FString>()
	);

	// The AtlasHttpAsyncAction already logs success/failure
	Action->Activate();
	
	UE_LOG(LogAtlasHTTP, Log, TEXT("Request sent. Check log for 'Async request succeeded' message."));
}

void UAtlasHttpTests::TestHttpPost(UObject* WorldContextObject)
{
	UE_LOG(LogAtlasHTTP, Log, TEXT(""));
	UE_LOG(LogAtlasHTTP, Log, TEXT("--- HTTP POST Test (Async) ---"));
	UE_LOG(LogAtlasHTTP, Log, TEXT("Sending POST request to httpbin.org/post..."));

	UAtlasJsonObject* Body = UAtlasJsonObject::MakeJsonObject();
	Body->SetStringField(TEXT("test"), TEXT("AtlasHTTP"));
	Body->SetNumberField(TEXT("version"), 1.0f);

	UAtlasHttpAsyncAction* Action = UAtlasHttpAsyncAction::AsyncHttpPost(
		WorldContextObject,
		TEXT("https://httpbin.org/post"),
		Body,
		TMap<FString, FString>()
	);

	Action->Activate();
	
	UE_LOG(LogAtlasHTTP, Log, TEXT("Request sent. Check log for 'Async request succeeded' message."));
}
