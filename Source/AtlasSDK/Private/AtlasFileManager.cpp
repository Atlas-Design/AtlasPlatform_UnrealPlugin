// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasFileManager.h"
#include "AtlasFileWork.h"
#include "AtlasSDKSettings.h"
#include "HAL/PlatformTime.h"
#include "UObject/StrongObjectPtr.h"
#include "AtlasHttpRequest.h"
#include "AtlasJsonObject.h"
#include "AtlasMultipartForm.h"
#include "AtlasPlatformAuth.h"
#include "AtlasWorkflowAsset.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "Async/Async.h"

UAtlasFileManager::UAtlasFileManager()
{
}

// ==================== File Upload (Simple API) ====================

FGuid UAtlasFileManager::UploadFile(UAtlasWorkflowAsset* WorkflowAsset, const FString& FilePath)
{
	return UploadFileWithOperationId(WorkflowAsset, FilePath, FGuid::NewGuid());
}

FGuid UAtlasFileManager::UploadFileWithOperationId(UAtlasWorkflowAsset* WorkflowAsset, const FString& FilePath, const FGuid& OperationId)
{
	if (!WorkflowAsset || !WorkflowAsset->IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasFileManager: Cannot upload without a valid WorkflowAsset"));
		return FGuid();
	}

	return UploadFileToUrlWithOperationId(WorkflowAsset->GetUploadUrl(), FilePath, OperationId);
}

FGuid UAtlasFileManager::UploadFileData(UAtlasWorkflowAsset* WorkflowAsset, const TArray<uint8>& FileData, const FString& FileName)
{
	if (!WorkflowAsset || !WorkflowAsset->IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasFileManager: Cannot upload without a valid WorkflowAsset"));
		return FGuid();
	}

	return UploadFileDataToUrl(WorkflowAsset->GetUploadUrl(), FileData, FileName);
}

// ==================== File Upload (URL API - Advanced) ====================

FGuid UAtlasFileManager::UploadFileToUrl(const FString& UploadUrl, const FString& FilePath)
{
	return UploadFileToUrlWithOperationId(UploadUrl, FilePath, FGuid::NewGuid());
}

FGuid UAtlasFileManager::UploadFileToUrlWithOperationId(const FString& UploadUrl, const FString& FilePath, const FGuid& OperationId)
{
	if (!OperationId.IsValid() || IsUploadInProgress(OperationId)) return FGuid();
	FAtlasPendingUpload Pending;
	Pending.OperationId = OperationId;
	Pending.UploadUrl = UploadUrl;
	PendingUploads.Add(Pending);
	QueuedUploads.Add({OperationId, UploadUrl, FilePath, CacheKeyFor(UploadUrl, TEXT(""))});
	PrepareNextUpload();
	return OperationId;
}

FGuid UAtlasFileManager::UploadFileDataToUrl(const FString& UploadUrl, const TArray<uint8>& FileData, const FString& FileName)
{
	return UploadFileDataToUrlWithOperationId(UploadUrl, FileData, FileName, FGuid::NewGuid());
}

FGuid UAtlasFileManager::UploadFileDataToUrlWithOperationId(const FString& UploadUrl, const TArray<uint8>& FileData, const FString& FileName, const FGuid& OperationId, const FString& PreparedHash)
{
	if (!OperationId.IsValid() || IsUploadInProgress(OperationId))
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasFileManager: Upload requires a fresh, valid operation ID"));
		return FGuid();
	}
	if (UploadUrl.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasFileManager: Cannot upload without UploadUrl"));
		return FGuid();
	}

	if (FileData.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasFileManager: Cannot upload empty file"));
		return FGuid();
	}

	// Calculate content hash
	FString ContentHash = PreparedHash.IsEmpty() ? CalculateContentHash(FileData) : PreparedHash;

	LastUploadUrl = UploadUrl;
	PruneCaches();
	const FString CacheKey = CacheKeyFor(UploadUrl, ContentHash);
	// Check cache first
	if (FString* CachedFileId = UploadCache.Find(CacheKey))
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Cache hit for %s (hash: %s), FileId: %s"), 
			*FileName, *ContentHash, **CachedFileId);

		// Fire completion event asynchronously (next tick) to maintain consistent async behavior
		// Even though we have a cache hit, callers expect async response
		TWeakObjectPtr<UAtlasFileManager> WeakThis(this);
		FString CachedId = *CachedFileId;
		FAtlasPendingUpload Pending;
		Pending.OperationId = OperationId;
		Pending.ContentHash = ContentHash;
		PendingUploads.Add(Pending);
		
		AsyncTask(ENamedThreads::GameThread, [WeakThis, OperationId, CachedId, ContentHash]()
		{
			if (UAtlasFileManager* This = WeakThis.Get())
			{
				TStrongObjectPtr<UAtlasFileManager> KeepAlive(This);
				if (!This->IsUploadInProgress(OperationId)) return;
				This->RemovePendingUpload(OperationId);
				This->OnUploadComplete.Broadcast(OperationId, CachedId, ContentHash);
			}
		});

		return OperationId;
	}

	// No cache hit - perform actual upload
	ExecuteUpload(OperationId, UploadUrl, FileData, FileName, ContentHash);

	return OperationId;
}

// ==================== Cache Helpers ====================

bool UAtlasFileManager::GetCachedFileId(const FString& ContentHash, FString& OutFileId) const
{
	const auto* Settings = UAtlasSDKSettings::Get();
	const double* Time = UploadCacheTimes.Find(CacheKeyFor(LastUploadUrl, ContentHash));
	if (!Settings->bEnableUploadCache || !Time || FPlatformTime::Seconds() - *Time >= Settings->CacheMaxAgeHours * 3600.0) return false;
	if (const FString* CachedId = UploadCache.Find(CacheKeyFor(LastUploadUrl, ContentHash)))
	{
		OutFileId = *CachedId;
		return true;
	}
	return false;
}

FString UAtlasFileManager::CalculateContentHash(const TArray<uint8>& FileData)
{
	// Use MD5 for content hashing
	FMD5 Md5;
	Md5.Update(FileData.GetData(), FileData.Num());

	uint8 Digest[16];
	Md5.Final(Digest);

	// Convert to hex string
	FString HashString;
	for (int32 i = 0; i < 16; i++)
	{
		HashString += FString::Printf(TEXT("%02x"), Digest[i]);
	}

	return HashString;
}

// ==================== Internal Upload Execution ====================

void UAtlasFileManager::ExecuteUpload(const FGuid& OperationId, const FString& UploadUrl, const TArray<uint8>& FileData, const FString& FileName, const FString& ContentHash)
{
	// Create HTTP request
	UAtlasHttpRequest* Request = UAtlasHttpRequest::CreateRequest();
	if (!Request)
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasFileManager: Failed to create HTTP request"));
		OnUploadFailed.Broadcast(OperationId, TEXT("Failed to create HTTP request"), 0);
		return;
	}

	// Keep request alive
	Request->AddToRoot();

	// Create pending upload record
	FAtlasPendingUpload PendingUpload;
	PendingUpload.OperationId = OperationId;
	PendingUpload.UploadUrl = UploadUrl;
	PendingUpload.ContentHash = ContentHash;
	PendingUpload.CacheKey = CacheKeyFor(UploadUrl, ContentHash);
	PendingUpload.FileName = FileName;
	PendingUpload.FileSize = FileData.Num();
	PendingUpload.Request = Request;
	PendingUploads.Add(PendingUpload);

	// Configure request
	Request->SetURL(UploadUrl);
	Request->SetVerb(EAtlasHttpVerb::POST);

	FString TrimmedUploadUrl = UploadUrl;
	TrimmedUploadUrl.TrimEndInline();
	const bool bUseMultipartUpload = TrimmedUploadUrl.EndsWith(TEXT("/upload"), ESearchCase::IgnoreCase);

	if (bUseMultipartUpload)
	{
		// API v0.2+: multipart form field "file" (matches platform Python export)
		FAtlasMultipartForm::FMultipartBody MultipartBody;
		if (!FAtlasMultipartForm::BuildSingleFileField(TEXT("file"), FileName, FileData, MultipartBody))
		{
			UE_LOG(LogTemp, Error, TEXT("AtlasFileManager: Failed to build multipart body for '%s'"), *FileName);
			OnUploadFailed.Broadcast(OperationId, TEXT("Failed to build multipart upload body"), 0);
			RemovePendingUpload(OperationId);
			Request->RemoveFromRoot();
			return;
		}

		Request->SetBinaryContentType(MultipartBody.ContentType);
		Request->SetBinaryRequestContent(MultipartBody.Content);
	}
	else
	{
		// Legacy v0.1: raw binary body with custom headers
		Request->SetBinaryContentType(TEXT("application/octet-stream"));
		Request->SetBinaryRequestContent(FileData);
		Request->SetHeader(TEXT("X-Filename"), FileName);
		Request->SetHeader(TEXT("X-Content-Hash"), ContentHash);
	}

	FAtlasPlatformAuth::ApplyPlatformRequestSettings(Request);

	// Bind callbacks
	Request->OnTransferProgress.AddDynamic(this, &UAtlasFileManager::HandleTransferProgress);
	Request->OnRequestComplete.AddDynamic(this, &UAtlasFileManager::OnUploadRequestComplete);
	Request->OnRequestFailed.AddDynamic(this, &UAtlasFileManager::OnUploadRequestFailed);

	UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Starting upload %s to %s (%s, %d bytes, hash: %s, multipart=%s)"),
		*OperationId.ToString(EGuidFormats::DigitsWithHyphens), TEXT("[details omitted]"), *FileName, FileData.Num(), *ContentHash,
		bUseMultipartUpload ? TEXT("true") : TEXT("false"));

	// Execute request
#if WITH_DEV_AUTOMATION_TESTS
	if (UploadRequestDispatcherForTests)
	{
		UploadRequestDispatcherForTests(Request);
		return;
	}
#endif
	Request->ProcessRequest();
}

void UAtlasFileManager::OnUploadRequestComplete(UAtlasHttpRequest* Request, UAtlasJsonObject* ResponseJson, int32 StatusCode)
{
	TStrongObjectPtr<UAtlasFileManager> KeepAlive(this);
	TStrongObjectPtr<UAtlasHttpRequest> KeepRequest(Request);
	// Find the pending upload
	FAtlasPendingUpload* PendingUpload = FindUploadByRequest(Request);
	if (!PendingUpload)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasFileManager: Received completion for unknown upload request"));
		Request->RemoveFromRoot();
		return;
	}

	FGuid OperationId = PendingUpload->OperationId;
	FString ContentHash = PendingUpload->ContentHash;
	FString FileName = PendingUpload->FileName;

	if (StatusCode < 200 || StatusCode >= 300)
	{
		const FString FailureMessage = FAtlasPlatformAuth::GetHttpFailureMessage(
			StatusCode,
			FString::Printf(TEXT("Upload failed with HTTP %d"), StatusCode));

		UE_LOG(LogTemp, Error, TEXT("AtlasFileManager: Upload %s failed: %s"),
			*OperationId.ToString(EGuidFormats::DigitsWithHyphens), TEXT("[details omitted]"));

		OnUploadFailed.Broadcast(OperationId, FailureMessage, StatusCode);
		RemovePendingUpload(OperationId);
		Request->RemoveFromRoot();
		return;
	}

	// Parse response to get FileId
	const FString CacheKey = PendingUpload->CacheKey;
	FString FileId;
	if (ResponseJson)
	{
		// Try common response field names
		FileId = ResponseJson->GetStringField(TEXT("file_id"));
		if (FileId.IsEmpty())
		{
			FileId = ResponseJson->GetStringField(TEXT("fileId"));
		}
		if (FileId.IsEmpty())
		{
			FileId = ResponseJson->GetStringField(TEXT("id"));
		}
	}

	if (FileId.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasFileManager: Upload %s succeeded but no FileId in response"),
			*OperationId.ToString(EGuidFormats::DigitsWithHyphens));
		OnUploadFailed.Broadcast(OperationId, TEXT("Server response missing file_id"), StatusCode);
	}
	else
	{
		// Cache the result
		UploadCache.Add(CacheKey, FileId);
		UploadCacheTimes.Add(CacheKey, FPlatformTime::Seconds());
		PruneCaches();

		UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Upload %s completed. FileId: %s"),
			*OperationId.ToString(EGuidFormats::DigitsWithHyphens), *FileId);

		OnUploadComplete.Broadcast(OperationId, FileId, ContentHash);
	}

	// Cleanup
	RemovePendingUpload(OperationId);
	Request->RemoveFromRoot();
}

void UAtlasFileManager::OnUploadRequestFailed(UAtlasHttpRequest* Request, const FString& ErrorMessage, int32 StatusCode)
{
	TStrongObjectPtr<UAtlasFileManager> KeepAlive(this);
	TStrongObjectPtr<UAtlasHttpRequest> KeepRequest(Request);
	// Find the pending upload
	FAtlasPendingUpload* PendingUpload = FindUploadByRequest(Request);
	if (!PendingUpload)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasFileManager: Received failure for unknown upload request"));
		Request->RemoveFromRoot();
		return;
	}

	FGuid OperationId = PendingUpload->OperationId;

	const FString FailureMessage = FAtlasPlatformAuth::GetHttpFailureMessage(
		StatusCode,
		ErrorMessage.IsEmpty() ? FString::Printf(TEXT("Upload failed with HTTP %d"), StatusCode) : ErrorMessage);

	UE_LOG(LogTemp, Error, TEXT("AtlasFileManager: Upload %s failed: %s (HTTP %d)"),
		*OperationId.ToString(EGuidFormats::DigitsWithHyphens), TEXT("[details omitted]"), StatusCode);

	OnUploadFailed.Broadcast(OperationId, FailureMessage, StatusCode);

	// Cleanup
	RemovePendingUpload(OperationId);
	Request->RemoveFromRoot();
}

FAtlasPendingUpload* UAtlasFileManager::FindUploadByRequest(UAtlasHttpRequest* Request)
{
	for (FAtlasPendingUpload& Upload : PendingUploads)
	{
		if (Upload.Request == Request)
		{
			return &Upload;
		}
	}
	return nullptr;
}

void UAtlasFileManager::RemovePendingUpload(const FGuid& OperationId)
{
	PendingUploads.RemoveAll([&OperationId](const FAtlasPendingUpload& Upload)
	{
		return Upload.OperationId == OperationId;
	});
}

// ==================== File Download (Simple API) ====================

FGuid UAtlasFileManager::DownloadFile(UAtlasWorkflowAsset* WorkflowAsset, const FString& FileId)
{
	return DownloadFileWithOperationId(WorkflowAsset, FileId, FGuid::NewGuid());
}

FGuid UAtlasFileManager::DownloadFileWithOperationId(UAtlasWorkflowAsset* WorkflowAsset, const FString& FileId, const FGuid& OperationId)
{
	if (!WorkflowAsset || !WorkflowAsset->IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasFileManager: Cannot download without a valid WorkflowAsset"));
		return FGuid();
	}

	if (FileId.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasFileManager: Cannot download without FileId"));
		return FGuid();
	}

	return DownloadFileFromUrlWithOperationId(WorkflowAsset->GetDownloadUrl(FileId), FileId, OperationId);
}

// ==================== File Download (URL API - Advanced) ====================

FGuid UAtlasFileManager::DownloadFileFromUrl(const FString& DownloadUrl, const FString& FileId)
{
	return DownloadFileFromUrlWithOperationId(DownloadUrl, FileId, FGuid::NewGuid());
}

FGuid UAtlasFileManager::DownloadFileFromUrlWithOperationId(const FString& DownloadUrl, const FString& FileId, const FGuid& OperationId)
{
	if (!OperationId.IsValid()) return FGuid();
	if (DownloadUrl.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasFileManager: Cannot download without DownloadUrl"));
		return FGuid();
	}

	if (FileId.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasFileManager: Cannot download without FileId"));
		return FGuid();
	}

	LastDownloadUrl = DownloadUrl;
	PruneCaches();
	// Check cache first
	if (const TArray<uint8>* CachedData = DownloadCache.Find(CacheKeyFor(DownloadUrl, FileId)))
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Cache hit for FileId: %s (%d bytes)"), 
			*FileId, CachedData->Num());

		// Cached completions are still cancellable while queued on the game thread.
		FAtlasPendingDownload Pending;
		Pending.OperationId = OperationId;
		Pending.FileId = FileId;
		Pending.CacheKey = CacheKeyFor(DownloadUrl, FileId);
		PendingDownloads.Add(Pending);

		// Fire completion event asynchronously (next tick) to maintain consistent async behavior
		TWeakObjectPtr<UAtlasFileManager> WeakThis(this);
		TArray<uint8> CachedBytes = *CachedData;
		FString CachedFileId = FileId;
		
		AsyncTask(ENamedThreads::GameThread, [WeakThis, OperationId, CachedFileId, CachedBytes = MoveTemp(CachedBytes)]()
		{
			if (UAtlasFileManager* This = WeakThis.Get())
			{
				TStrongObjectPtr<UAtlasFileManager> KeepAlive(This);
				if (!This->IsDownloadInProgress(OperationId)) return;
				This->OnDownloadComplete.Broadcast(OperationId, CachedFileId, CachedBytes);
				This->RemovePendingDownload(OperationId);
			}
		});

		return OperationId;
	}

	// No cache hit - perform actual download
	ExecuteDownload(OperationId, DownloadUrl, FileId);

	return OperationId;
}

bool UAtlasFileManager::GetCachedFileData(const FString& FileId, TArray<uint8>& OutFileData) const
{
	const double* Time = DownloadCacheTimes.Find(CacheKeyFor(LastDownloadUrl, FileId));
	if (UAtlasSDKSettings::Get()->MaxDownloadCacheMB <= 0 || !Time || FPlatformTime::Seconds() - *Time >= UAtlasSDKSettings::Get()->CacheMaxAgeHours * 3600.0) return false;
	if (const TArray<uint8>* CachedData = DownloadCache.Find(CacheKeyFor(LastDownloadUrl, FileId)))
	{
		OutFileData = *CachedData;
		return true;
	}
	return false;
}

int64 UAtlasFileManager::GetDownloadCacheSizeBytes() const
{
	int64 TotalBytes = 0;
	for (const auto& Entry : DownloadCache)
	{
		TotalBytes += Entry.Value.Num();
	}
	return TotalBytes;
}

// ==================== Internal Download Execution ====================

void UAtlasFileManager::ExecuteDownload(const FGuid& OperationId, const FString& DownloadUrl, const FString& FileId)
{
	// Create HTTP request
	UAtlasHttpRequest* Request = UAtlasHttpRequest::CreateRequest();
	if (!Request)
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasFileManager: Failed to create HTTP request for download"));
		OnDownloadFailed.Broadcast(OperationId, TEXT("Failed to create HTTP request"), 0);
		return;
	}

	// Keep request alive
	Request->AddToRoot();

	// Create pending download record
	FAtlasPendingDownload PendingDownload;
	PendingDownload.OperationId = OperationId;
	PendingDownload.FileId = FileId;
	PendingDownload.DownloadUrl = DownloadUrl;
	PendingDownload.CacheKey = CacheKeyFor(DownloadUrl, FileId);
	PendingDownload.Request = Request;
	PendingDownloads.Add(PendingDownload);

	// Configure request
	Request->SetURL(DownloadUrl);
	Request->SetVerb(EAtlasHttpVerb::GET);
	Request->SetBinaryResponse(true);

	FAtlasPlatformAuth::ApplyPlatformRequestSettings(Request);

	// Bind callbacks
	Request->OnTransferProgress.AddDynamic(this, &UAtlasFileManager::HandleTransferProgress);
	Request->OnRequestComplete.AddDynamic(this, &UAtlasFileManager::OnDownloadRequestComplete);
	Request->OnRequestFailed.AddDynamic(this, &UAtlasFileManager::OnDownloadRequestFailed);

	UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Starting download %s from %s (FileId: %s)"),
		*OperationId.ToString(EGuidFormats::DigitsWithHyphens), TEXT("[details omitted]"), *FileId);

	// Execute request
	Request->ProcessRequest();
}

void UAtlasFileManager::OnDownloadRequestComplete(UAtlasHttpRequest* Request, UAtlasJsonObject* ResponseJson, int32 StatusCode)
{
	TStrongObjectPtr<UAtlasFileManager> KeepAlive(this);
	TStrongObjectPtr<UAtlasHttpRequest> KeepRequest(Request);
	// Find the pending download
	FAtlasPendingDownload* PendingDownload = FindDownloadByRequest(Request);
	if (!PendingDownload)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasFileManager: Received completion for unknown download request"));
		Request->RemoveFromRoot();
		return;
	}

	FGuid OperationId = PendingDownload->OperationId;
	FString FileId = PendingDownload->FileId;

	if (StatusCode < 200 || StatusCode >= 300)
	{
		const FString FailureMessage = FAtlasPlatformAuth::GetHttpFailureMessage(
			StatusCode,
			FString::Printf(TEXT("Download failed with HTTP %d"), StatusCode));

		UE_LOG(LogTemp, Error, TEXT("AtlasFileManager: Download %s failed: %s"),
			*OperationId.ToString(EGuidFormats::DigitsWithHyphens), TEXT("[details omitted]"));

		OnDownloadFailed.Broadcast(OperationId, FailureMessage, StatusCode);
		RemovePendingDownload(OperationId);
		Request->RemoveFromRoot();
		return;
	}

	// Get the response content as binary data
	const TArray<uint8>& ResponseData = Request->GetResponseContentBinary();

	if (ResponseData.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasFileManager: Download %s completed but response is empty"),
			*OperationId.ToString(EGuidFormats::DigitsWithHyphens));
		OnDownloadFailed.Broadcast(OperationId, TEXT("Server returned empty response"), StatusCode);
	}
	else
	{
		// Cache the result
		const FString CacheKey = PendingDownload->CacheKey;
		if (ResponseData.Num() <= int64(UAtlasSDKSettings::Get()->MaxDownloadCacheMB) * 1024 * 1024)
		{
			DownloadCache.Add(CacheKey, ResponseData);
			DownloadCacheTimes.Add(CacheKey, FPlatformTime::Seconds());
			DownloadCacheExtensions.Add(CacheKey, GetDownloadExtension(OperationId));
		}
		PruneCaches();

		UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Download %s completed. FileId: %s, Size: %d bytes"),
			*OperationId.ToString(EGuidFormats::DigitsWithHyphens), *FileId, ResponseData.Num());

		OnDownloadComplete.Broadcast(OperationId, FileId, ResponseData);
	}

	// Cleanup
	RemovePendingDownload(OperationId);
	Request->RemoveFromRoot();
}

void UAtlasFileManager::OnDownloadRequestFailed(UAtlasHttpRequest* Request, const FString& ErrorMessage, int32 StatusCode)
{
	TStrongObjectPtr<UAtlasFileManager> KeepAlive(this);
	TStrongObjectPtr<UAtlasHttpRequest> KeepRequest(Request);
	// Find the pending download
	FAtlasPendingDownload* PendingDownload = FindDownloadByRequest(Request);
	if (!PendingDownload)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasFileManager: Received failure for unknown download request"));
		Request->RemoveFromRoot();
		return;
	}

	FGuid OperationId = PendingDownload->OperationId;

	const FString FailureMessage = FAtlasPlatformAuth::GetHttpFailureMessage(
		StatusCode,
		ErrorMessage.IsEmpty() ? FString::Printf(TEXT("Download failed with HTTP %d"), StatusCode) : ErrorMessage);

	UE_LOG(LogTemp, Error, TEXT("AtlasFileManager: Download %s failed: %s (HTTP %d)"),
		*OperationId.ToString(EGuidFormats::DigitsWithHyphens), TEXT("[details omitted]"), StatusCode);

	OnDownloadFailed.Broadcast(OperationId, FailureMessage, StatusCode);

	// Cleanup
	RemovePendingDownload(OperationId);
	Request->RemoveFromRoot();
}

FAtlasPendingDownload* UAtlasFileManager::FindDownloadByRequest(UAtlasHttpRequest* Request)
{
	for (FAtlasPendingDownload& Download : PendingDownloads)
	{
		if (Download.Request == Request)
		{
			return &Download;
		}
	}
	return nullptr;
}

void UAtlasFileManager::RemovePendingDownload(const FGuid& OperationId)
{
	PendingDownloads.RemoveAll([&OperationId](const FAtlasPendingDownload& Download)
	{
		return Download.OperationId == OperationId;
	});
}

// ==================== Cache Management ====================

void UAtlasFileManager::ClearUploadCache()
{
	int32 ClearedCount = UploadCache.Num();
	UploadCache.Empty();
	UploadCacheTimes.Empty();
	UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Cleared %d upload cache entries"), ClearedCount);
}

void UAtlasFileManager::ClearDownloadCache()
{
	int32 ClearedCount = DownloadCache.Num();
	DownloadCache.Empty();
	DownloadCacheExtensions.Empty();
	DownloadCacheTimes.Empty();
	UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Cleared %d download cache entries"), ClearedCount);
}

// ==================== Operation Management ====================

bool UAtlasFileManager::CancelUpload(const FGuid& OperationId)
{
	TStrongObjectPtr<UAtlasFileManager> KeepAlive(this);
	// The caller may have passed a reference into the pending array.
	const FGuid CancelledId = OperationId;
	for (int32 i = 0; i < PendingUploads.Num(); ++i)
	{
		if (PendingUploads[i].OperationId == CancelledId)
		{
			TStrongObjectPtr<UAtlasHttpRequest> Request(PendingUploads[i].Request);
			PendingUploads.RemoveAt(i);
			if (Request.IsValid())
			{
				Request->OnRequestComplete.RemoveDynamic(this, &UAtlasFileManager::OnUploadRequestComplete);
				Request->OnRequestFailed.RemoveDynamic(this, &UAtlasFileManager::OnUploadRequestFailed);
				Request->CancelRequest();
				Request->RemoveFromRoot();
			}
			OnUploadFailed.Broadcast(CancelledId, TEXT("Upload cancelled"), 0);
			return true;
		}
	}
	return false;
}

void UAtlasFileManager::CancelAllUploads()
{
	TStrongObjectPtr<UAtlasFileManager> KeepAlive(this);
	TArray<FGuid> OperationIds;
	for (const auto& Pending : PendingUploads) OperationIds.Add(Pending.OperationId);
	for (const FGuid& Id : OperationIds) CancelUpload(Id);
}

bool UAtlasFileManager::IsUploadInProgress(const FGuid& OperationId) const
{
	for (const FAtlasPendingUpload& Upload : PendingUploads)
	{
		if (Upload.OperationId == OperationId)
		{
			return true;
		}
	}
	return false;
}

bool UAtlasFileManager::CancelDownload(const FGuid& OperationId)
{
	TStrongObjectPtr<UAtlasFileManager> KeepAlive(this);
	// The caller may have passed a reference into the pending array.
	const FGuid CancelledId = OperationId;
	for (int32 i = 0; i < PendingDownloads.Num(); ++i)
	{
		if (PendingDownloads[i].OperationId == CancelledId)
		{
			TStrongObjectPtr<UAtlasHttpRequest> Request(PendingDownloads[i].Request);
			PendingDownloads.RemoveAt(i);
			if (Request.IsValid())
			{
				Request->OnRequestComplete.RemoveDynamic(this, &UAtlasFileManager::OnDownloadRequestComplete);
				Request->OnRequestFailed.RemoveDynamic(this, &UAtlasFileManager::OnDownloadRequestFailed);
				Request->CancelRequest();
				Request->RemoveFromRoot();
			}
			OnDownloadFailed.Broadcast(CancelledId, TEXT("Download cancelled"), 0);
			return true;
		}
	}
	return false;
}

void UAtlasFileManager::CancelAllDownloads()
{
	TStrongObjectPtr<UAtlasFileManager> KeepAlive(this);
	TArray<FGuid> OperationIds;
	for (const auto& Pending : PendingDownloads) OperationIds.Add(Pending.OperationId);
	for (const FGuid& Id : OperationIds) CancelDownload(Id);
}

bool UAtlasFileManager::IsDownloadInProgress(const FGuid& OperationId) const
{
	for (const FAtlasPendingDownload& Download : PendingDownloads)
	{
		if (Download.OperationId == OperationId)
		{
			return true;
		}
	}
	return false;
}

FString UAtlasFileManager::CacheKeyFor(const FString& Url, const FString& Identity)
{
	// Credentials are fingerprinted only in memory; neither keys nor credentials are logged/persisted.
	FTCHARToUTF8 Scope(*(FAtlasPlatformAuth::GetResolvedApiKey() + TEXT("\n") + Url));
	return FMD5::HashBytes(reinterpret_cast<const uint8*>(Scope.Get()), Scope.Length()) + TEXT(":") + Identity;
}

void UAtlasFileManager::PruneCaches()
{
	const UAtlasSDKSettings* Settings = UAtlasSDKSettings::Get();
	const double Now = FPlatformTime::Seconds();
	const double MaxAge = FMath::Max(0, Settings->CacheMaxAgeHours) * 3600.0;
	for (auto It = UploadCache.CreateIterator(); It; ++It)
	{
		const double* Time = UploadCacheTimes.Find(It.Key());
		if (!Settings->bEnableUploadCache || !Time || Now - *Time >= MaxAge) { UploadCacheTimes.Remove(It.Key()); It.RemoveCurrent(); }
	}
	for (auto It = DownloadCache.CreateIterator(); It; ++It)
	{
		const double* Time = DownloadCacheTimes.Find(It.Key());
		if (Settings->MaxDownloadCacheMB <= 0 || !Time || Now - *Time >= MaxAge) { DownloadCacheTimes.Remove(It.Key()); DownloadCacheExtensions.Remove(It.Key()); It.RemoveCurrent(); }
	}
	auto Oldest = [](const TMap<FString, double>& Times)
	{
		FString Key;
		double OldestTime = TNumericLimits<double>::Max();
		for (const auto& Pair : Times) if (Pair.Value < OldestTime) { Key = Pair.Key; OldestTime = Pair.Value; }
		return Key;
	};
	while (UploadCache.Num() > FMath::Max(0, Settings->MaxUploadCacheEntries))
	{
		const FString Key = Oldest(UploadCacheTimes);
		UploadCache.Remove(Key); UploadCacheTimes.Remove(Key);
	}
	while (GetDownloadCacheSizeBytes() > int64(FMath::Max(0, Settings->MaxDownloadCacheMB)) * 1024 * 1024)
	{
		const FString Key = Oldest(DownloadCacheTimes);
		DownloadCache.Remove(Key); DownloadCacheTimes.Remove(Key); DownloadCacheExtensions.Remove(Key);
	}
}

FString UAtlasFileManager::GetDownloadExtension(const FGuid& OperationId) const
{
	for (const auto& Pending : PendingDownloads)
	{
		if (Pending.OperationId != OperationId) continue;
		if (!Pending.Request) return DownloadCacheExtensions.FindRef(Pending.CacheKey);
		const FString Type = Pending.Request->GetResponseHeader(TEXT("Content-Type")).ToLower();
		if (Type.Contains(TEXT("image/png"))) return TEXT("png");
		if (Type.Contains(TEXT("image/jpeg"))) return TEXT("jpg");
		if (Type.Contains(TEXT("image/webp"))) return TEXT("webp");
		if (Type.Contains(TEXT("audio/wav")) || Type.Contains(TEXT("audio/x-wav"))) return TEXT("wav");
		if (Type.Contains(TEXT("audio/ogg"))) return TEXT("ogg");
		if (Type.Contains(TEXT("audio/mpeg"))) return TEXT("mp3");
		if (Type.Contains(TEXT("model/gltf-binary"))) return TEXT("glb");
		if (Type.Contains(TEXT("model/gltf+json"))) return TEXT("gltf");
		const FString Disposition = Pending.Request->GetResponseHeader(TEXT("Content-Disposition"));
		FString Before, FileName;
		if (Disposition.Split(TEXT("filename="), &Before, &FileName, ESearchCase::IgnoreCase))
		{
			FileName = FileName.TrimStartAndEnd().TrimQuotes();
			FString Extension = FPaths::GetExtension(FileName).ToLower();
			bool bSafe = !Extension.IsEmpty() && Extension.Len() <= 12;
			for (TCHAR Ch : Extension) bSafe &= FChar::IsAlnum(Ch);
			if (bSafe) return Extension;
		}
	}
	return FString();
}

void UAtlasFileManager::PrepareNextUpload()
{
	if (bPreparingUpload) return;
	while (!QueuedUploads.IsEmpty())
	{
		const FQueuedUpload Work = QueuedUploads[0];
		QueuedUploads.RemoveAt(0);
		if (!IsUploadInProgress(Work.Id)) continue;
		bPreparingUpload = true;
		const TWeakObjectPtr<UAtlasFileManager> WeakThis(this);
		AtlasFileWork::Enqueue([WeakThis, Work]()
		{
			TArray<uint8> Bytes;
			const bool bRead = FFileHelper::LoadFileToArray(Bytes, *Work.Path) && !Bytes.IsEmpty();
			const FString Hash = bRead ? CalculateContentHash(Bytes) : FString();
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Work, bRead, Hash, Bytes = MoveTemp(Bytes)]()
			{
				TStrongObjectPtr<UAtlasFileManager> Manager(WeakThis.Get());
				if (!Manager.IsValid()) return;
				Manager->bPreparingUpload = false;
				if (Manager->IsUploadInProgress(Work.Id))
				{
					Manager->RemovePendingUpload(Work.Id);
					if (!bRead || Work.Scope != CacheKeyFor(Work.Url, TEXT("")))
						Manager->OnUploadFailed.Broadcast(Work.Id, TEXT("Input is empty/unreadable or workspace changed during preparation"), 0);
					else Manager->UploadFileDataToUrlWithOperationId(Work.Url, Bytes, FPaths::GetCleanFilename(Work.Path), Work.Id, Hash);
				}
				Manager->PrepareNextUpload();
			});
		});
		return;
	}
}

void UAtlasFileManager::HandleTransferProgress(UAtlasHttpRequest* Request, int64 Sent, int64 Received)
{
	TStrongObjectPtr<UAtlasFileManager> KeepAlive(this);
	if (const auto* Upload = FindUploadByRequest(Request))
	{
		const FGuid Id = Upload->OperationId;
		const float Progress = Upload->FileSize > 0 ? FMath::Clamp(float(double(Sent) / Upload->FileSize), 0.f, 1.f) : 0.f;
		OnUploadProgress.Broadcast(Id, Progress, int32(FMath::Min<int64>(Sent, MAX_int32)));
	}
	else if (const auto* Download = FindDownloadByRequest(Request))
	{
		const FGuid Id = Download->OperationId;
		// Total may be unknown for streamed responses; bytes received remain meaningful.
		OnDownloadProgress.Broadcast(Id, 0.f, int32(FMath::Min<int64>(Received, MAX_int32)));
	}
}

FString UAtlasFileManager::GetTransferResponse(const FGuid& OperationId) const
{
	for (const auto& Pending : PendingUploads) if (Pending.OperationId == OperationId && Pending.Request) return Pending.Request->GetResponseContent();
	for (const auto& Pending : PendingDownloads) if (Pending.OperationId == OperationId && Pending.Request) return Pending.Request->GetResponseContent();
	return FString();
}
