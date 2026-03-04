// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasFileManager.h"
#include "AtlasHttpRequest.h"
#include "AtlasJsonObject.h"
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
	if (!WorkflowAsset || !WorkflowAsset->IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasFileManager: Cannot upload without a valid WorkflowAsset"));
		return FGuid();
	}

	return UploadFileToUrl(WorkflowAsset->GetUploadUrl(), FilePath);
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
	// Read file from disk
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("AtlasFileManager: Failed to read file: %s"), *FilePath);
		return FGuid();
	}

	// Extract filename from path
	FString FileName = FPaths::GetCleanFilename(FilePath);

	return UploadFileDataToUrl(UploadUrl, FileData, FileName);
}

FGuid UAtlasFileManager::UploadFileDataToUrl(const FString& UploadUrl, const TArray<uint8>& FileData, const FString& FileName)
{
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
	FString ContentHash = CalculateContentHash(FileData);

	// Check cache first
	if (FString* CachedFileId = UploadCache.Find(ContentHash))
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Cache hit for %s (hash: %s), FileId: %s"), 
			*FileName, *ContentHash, **CachedFileId);

		// Create operation ID for consistency
		FGuid OperationId = FGuid::NewGuid();

		// Fire completion event asynchronously (next tick) to maintain consistent async behavior
		// Even though we have a cache hit, callers expect async response
		TWeakObjectPtr<UAtlasFileManager> WeakThis(this);
		FString CachedId = *CachedFileId;
		
		AsyncTask(ENamedThreads::GameThread, [WeakThis, OperationId, CachedId, ContentHash]()
		{
			if (UAtlasFileManager* This = WeakThis.Get())
			{
				This->OnUploadComplete.Broadcast(OperationId, CachedId, ContentHash);
			}
		});

		return OperationId;
	}

	// No cache hit - perform actual upload
	FGuid OperationId = FGuid::NewGuid();
	ExecuteUpload(OperationId, UploadUrl, FileData, FileName, ContentHash);

	return OperationId;
}

// ==================== Cache Helpers ====================

bool UAtlasFileManager::GetCachedFileId(const FString& ContentHash, FString& OutFileId) const
{
	if (const FString* CachedId = UploadCache.Find(ContentHash))
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
	PendingUpload.FileName = FileName;
	PendingUpload.FileSize = FileData.Num();
	PendingUpload.Request = Request;
	PendingUploads.Add(PendingUpload);

	// Configure request
	Request->SetURL(UploadUrl);
	Request->SetVerb(EAtlasHttpVerb::POST);

	// Set binary content
	Request->SetBinaryContentType(TEXT("application/octet-stream"));
	Request->SetBinaryRequestContent(FileData);

	// Add filename header (server uses this for content type detection)
	Request->SetHeader(TEXT("X-Filename"), FileName);
	Request->SetHeader(TEXT("X-Content-Hash"), ContentHash);

	// Bind callbacks
	Request->OnRequestComplete.AddDynamic(this, &UAtlasFileManager::OnUploadRequestComplete);
	Request->OnRequestFailed.AddDynamic(this, &UAtlasFileManager::OnUploadRequestFailed);

	UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Starting upload %s to %s (%s, %d bytes, hash: %s)"),
		*OperationId.ToString(EGuidFormats::DigitsWithHyphens), *UploadUrl, *FileName, FileData.Num(), *ContentHash);

	// Execute request
	Request->ProcessRequest();
}

void UAtlasFileManager::OnUploadRequestComplete(UAtlasHttpRequest* Request, UAtlasJsonObject* ResponseJson, int32 StatusCode)
{
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

	// Parse response to get FileId
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
		UploadCache.Add(ContentHash, FileId);

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
	// Find the pending upload
	FAtlasPendingUpload* PendingUpload = FindUploadByRequest(Request);
	if (!PendingUpload)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasFileManager: Received failure for unknown upload request"));
		Request->RemoveFromRoot();
		return;
	}

	FGuid OperationId = PendingUpload->OperationId;

	UE_LOG(LogTemp, Error, TEXT("AtlasFileManager: Upload %s failed: %s (HTTP %d)"),
		*OperationId.ToString(EGuidFormats::DigitsWithHyphens), *ErrorMessage, StatusCode);

	OnUploadFailed.Broadcast(OperationId, ErrorMessage, StatusCode);

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

	return DownloadFileFromUrl(WorkflowAsset->GetDownloadUrl(FileId), FileId);
}

// ==================== File Download (URL API - Advanced) ====================

FGuid UAtlasFileManager::DownloadFileFromUrl(const FString& DownloadUrl, const FString& FileId)
{
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

	// Check cache first
	if (const TArray<uint8>* CachedData = DownloadCache.Find(FileId))
	{
		UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Cache hit for FileId: %s (%d bytes)"), 
			*FileId, CachedData->Num());

		// Create operation ID for consistency
		FGuid OperationId = FGuid::NewGuid();

		// Fire completion event asynchronously (next tick) to maintain consistent async behavior
		TWeakObjectPtr<UAtlasFileManager> WeakThis(this);
		TArray<uint8> CachedBytes = *CachedData;
		FString CachedFileId = FileId;
		
		AsyncTask(ENamedThreads::GameThread, [WeakThis, OperationId, CachedFileId, CachedBytes]()
		{
			if (UAtlasFileManager* This = WeakThis.Get())
			{
				This->OnDownloadComplete.Broadcast(OperationId, CachedFileId, CachedBytes);
			}
		});

		return OperationId;
	}

	// No cache hit - perform actual download
	FGuid OperationId = FGuid::NewGuid();
	ExecuteDownload(OperationId, DownloadUrl, FileId);

	return OperationId;
}

bool UAtlasFileManager::GetCachedFileData(const FString& FileId, TArray<uint8>& OutFileData) const
{
	if (const TArray<uint8>* CachedData = DownloadCache.Find(FileId))
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
	PendingDownload.Request = Request;
	PendingDownloads.Add(PendingDownload);

	// Configure request
	Request->SetURL(DownloadUrl);
	Request->SetVerb(EAtlasHttpVerb::GET);

	// Bind callbacks
	Request->OnRequestComplete.AddDynamic(this, &UAtlasFileManager::OnDownloadRequestComplete);
	Request->OnRequestFailed.AddDynamic(this, &UAtlasFileManager::OnDownloadRequestFailed);

	UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Starting download %s from %s (FileId: %s)"),
		*OperationId.ToString(EGuidFormats::DigitsWithHyphens), *DownloadUrl, *FileId);

	// Execute request
	Request->ProcessRequest();
}

void UAtlasFileManager::OnDownloadRequestComplete(UAtlasHttpRequest* Request, UAtlasJsonObject* ResponseJson, int32 StatusCode)
{
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
		DownloadCache.Add(FileId, ResponseData);

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
	// Find the pending download
	FAtlasPendingDownload* PendingDownload = FindDownloadByRequest(Request);
	if (!PendingDownload)
	{
		UE_LOG(LogTemp, Warning, TEXT("AtlasFileManager: Received failure for unknown download request"));
		Request->RemoveFromRoot();
		return;
	}

	FGuid OperationId = PendingDownload->OperationId;

	UE_LOG(LogTemp, Error, TEXT("AtlasFileManager: Download %s failed: %s (HTTP %d)"),
		*OperationId.ToString(EGuidFormats::DigitsWithHyphens), *ErrorMessage, StatusCode);

	OnDownloadFailed.Broadcast(OperationId, ErrorMessage, StatusCode);

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
	UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Cleared %d upload cache entries"), ClearedCount);
}

void UAtlasFileManager::ClearDownloadCache()
{
	int32 ClearedCount = DownloadCache.Num();
	DownloadCache.Empty();
	UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Cleared %d download cache entries"), ClearedCount);
}

// ==================== Operation Management ====================

bool UAtlasFileManager::CancelUpload(const FGuid& OperationId)
{
	for (int32 i = 0; i < PendingUploads.Num(); i++)
	{
		if (PendingUploads[i].OperationId == OperationId)
		{
			if (PendingUploads[i].Request)
			{
				PendingUploads[i].Request->CancelRequest();
				PendingUploads[i].Request->RemoveFromRoot();
			}
			PendingUploads.RemoveAt(i);
			
			UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Cancelled upload %s"),
				*OperationId.ToString(EGuidFormats::DigitsWithHyphens));
			
			OnUploadFailed.Broadcast(OperationId, TEXT("Upload cancelled"), 0);
			return true;
		}
	}
	return false;
}

void UAtlasFileManager::CancelAllUploads()
{
	for (FAtlasPendingUpload& Upload : PendingUploads)
	{
		if (Upload.Request)
		{
			Upload.Request->CancelRequest();
			Upload.Request->RemoveFromRoot();
		}
		OnUploadFailed.Broadcast(Upload.OperationId, TEXT("Upload cancelled"), 0);
	}

	int32 CancelledCount = PendingUploads.Num();
	PendingUploads.Empty();
	
	UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Cancelled %d uploads"), CancelledCount);
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
	for (int32 i = 0; i < PendingDownloads.Num(); i++)
	{
		if (PendingDownloads[i].OperationId == OperationId)
		{
			if (PendingDownloads[i].Request)
			{
				PendingDownloads[i].Request->CancelRequest();
				PendingDownloads[i].Request->RemoveFromRoot();
			}
			PendingDownloads.RemoveAt(i);
			
			UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Cancelled download %s"),
				*OperationId.ToString(EGuidFormats::DigitsWithHyphens));
			
			OnDownloadFailed.Broadcast(OperationId, TEXT("Download cancelled"), 0);
			return true;
		}
	}
	return false;
}

void UAtlasFileManager::CancelAllDownloads()
{
	for (FAtlasPendingDownload& Download : PendingDownloads)
	{
		if (Download.Request)
		{
			Download.Request->CancelRequest();
			Download.Request->RemoveFromRoot();
		}
		OnDownloadFailed.Broadcast(Download.OperationId, TEXT("Download cancelled"), 0);
	}

	int32 CancelledCount = PendingDownloads.Num();
	PendingDownloads.Empty();
	
	UE_LOG(LogTemp, Log, TEXT("AtlasFileManager: Cancelled %d downloads"), CancelledCount);
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
