// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AtlasFileManager.generated.h"

class UAtlasHttpRequest;
class UAtlasWorkflowAsset;

// ==================== Delegate Declarations ====================

/** Delegate fired when file upload completes successfully */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFileUploadComplete, const FGuid&, OperationId, const FString&, FileId, const FString&, ContentHash);

/** Delegate fired when file upload fails */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFileUploadFailed, const FGuid&, OperationId, const FString&, ErrorMessage, int32, HttpStatusCode);

/** Delegate fired during file upload progress */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFileUploadProgress, const FGuid&, OperationId, float, Progress, int32, BytesSent);

/** Delegate fired when file download completes successfully */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFileDownloadComplete, const FGuid&, OperationId, const FString&, FileId, const TArray<uint8>&, FileData);

/** Delegate fired when file download fails */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFileDownloadFailed, const FGuid&, OperationId, const FString&, ErrorMessage, int32, HttpStatusCode);

/** Delegate fired during file download progress */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFileDownloadProgress, const FGuid&, OperationId, float, Progress, int32, BytesReceived);

/**
 * Pending upload operation tracking
 */
USTRUCT()
struct FAtlasPendingUpload
{
	GENERATED_BODY()

	/** Operation identifier */
	FGuid OperationId;

	/** Upload URL */
	FString UploadUrl;

	/** Content hash of the file being uploaded */
	FString ContentHash;

	/** Original filename */
	FString FileName;

	/** File size in bytes */
	int32 FileSize = 0;

	/** The HTTP request performing the upload */
	UPROPERTY()
	TObjectPtr<UAtlasHttpRequest> Request;

	/** Callback for completion */
	TFunction<void(const FString& FileId)> OnComplete;

	/** Callback for failure */
	TFunction<void(const FString& Error, int32 StatusCode)> OnFailed;
};

/**
 * Pending download operation tracking
 */
USTRUCT()
struct FAtlasPendingDownload
{
	GENERATED_BODY()

	/** Operation identifier */
	FGuid OperationId;

	/** FileId being downloaded */
	FString FileId;

	/** Download URL */
	FString DownloadUrl;

	/** The HTTP request performing the download */
	UPROPERTY()
	TObjectPtr<UAtlasHttpRequest> Request;

	/** Callback for completion */
	TFunction<void(const TArray<uint8>& Data)> OnComplete;

	/** Callback for failure */
	TFunction<void(const FString& Error, int32 StatusCode)> OnFailed;
};

/**
 * Manages file upload and download operations for Atlas workflows.
 * 
 * Features:
 * - Async file upload with progress tracking
 * - Content-based caching (avoids re-uploading identical files)
 * - MD5 hash calculation for content identification
 * - Multiple concurrent operations support
 * 
 * The FileManager is typically owned by UAtlasEditorSubsystem and shared
 * across all jobs. It maintains an upload cache to avoid redundant uploads
 * when the same file content is used in multiple jobs.
 */
UCLASS(BlueprintType)
class ATLASSDK_API UAtlasFileManager : public UObject
{
	GENERATED_BODY()

public:
	UAtlasFileManager();

	// ==================== File Upload (Simple API) ====================

	/**
	 * Upload a file from a path on disk for a workflow.
	 * URL is automatically constructed from the workflow asset's schema.
	 * If the file content has been uploaded before (same hash), returns cached FileId immediately.
	 * @param WorkflowAsset The workflow to upload for (provides upload URL)
	 * @param FilePath Path to the file on disk
	 * @return Operation GUID for tracking, or invalid GUID if file couldn't be read
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|FileManager", meta = (DisplayName = "Upload File"))
	FGuid UploadFile(UAtlasWorkflowAsset* WorkflowAsset, const FString& FilePath);

	/**
	 * Upload file data directly (bytes + filename) for a workflow.
	 * URL is automatically constructed from the workflow asset's schema.
	 * If the content has been uploaded before (same hash), returns cached FileId immediately.
	 * @param WorkflowAsset The workflow to upload for (provides upload URL)
	 * @param FileData The file content as bytes
	 * @param FileName The filename (used for content type detection)
	 * @return Operation GUID for tracking
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|FileManager", meta = (DisplayName = "Upload File Data"))
	FGuid UploadFileData(UAtlasWorkflowAsset* WorkflowAsset, const TArray<uint8>& FileData, const FString& FileName);

	// ==================== File Upload (URL API - Advanced) ====================

	/**
	 * Upload a file from a path on disk to a specific URL.
	 * Use this for custom endpoints or when you need direct URL control.
	 * If the file content has been uploaded before (same hash), returns cached FileId immediately.
	 * @param UploadUrl The complete upload URL
	 * @param FilePath Path to the file on disk
	 * @return Operation GUID for tracking, or invalid GUID if file couldn't be read
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|FileManager", meta = (DisplayName = "Upload File To URL"))
	FGuid UploadFileToUrl(const FString& UploadUrl, const FString& FilePath);

	/**
	 * Upload file data directly (bytes + filename) to a specific URL.
	 * Use this for custom endpoints or when you need direct URL control.
	 * If the content has been uploaded before (same hash), returns cached FileId immediately.
	 * @param UploadUrl The complete upload URL
	 * @param FileData The file content as bytes
	 * @param FileName The filename (used for content type detection)
	 * @return Operation GUID for tracking
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|FileManager", meta = (DisplayName = "Upload File Data To URL"))
	FGuid UploadFileDataToUrl(const FString& UploadUrl, const TArray<uint8>& FileData, const FString& FileName);

	/**
	 * Check if content with this hash is already cached (already uploaded).
	 * @param ContentHash MD5 hash of the content
	 * @param OutFileId Output: cached FileId if found
	 * @return True if cache hit
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|FileManager")
	bool GetCachedFileId(const FString& ContentHash, FString& OutFileId) const;

	/**
	 * Calculate MD5 hash of file content.
	 * @param FileData The file content
	 * @return MD5 hash as hex string
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|FileManager")
	static FString CalculateContentHash(const TArray<uint8>& FileData);

	// ==================== File Download (Simple API) ====================

	/**
	 * Download a file by FileId using the workflow's download URL.
	 * URL is automatically constructed from the workflow asset's schema.
	 * If the file has been downloaded before, returns cached bytes immediately.
	 * @param WorkflowAsset The workflow to download from (provides base URL)
	 * @param FileId The server-side file identifier
	 * @return Operation GUID for tracking
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|FileManager", meta = (DisplayName = "Download File"))
	FGuid DownloadFile(UAtlasWorkflowAsset* WorkflowAsset, const FString& FileId);

	// ==================== File Download (URL API - Advanced) ====================

	/**
	 * Download a file from a specific URL.
	 * Use this for custom endpoints or when you need direct URL control.
	 * If the file has been downloaded before, returns cached bytes immediately.
	 * @param DownloadUrl The complete download URL
	 * @param FileId The file identifier (used for caching)
	 * @return Operation GUID for tracking
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|FileManager", meta = (DisplayName = "Download File From URL"))
	FGuid DownloadFileFromUrl(const FString& DownloadUrl, const FString& FileId);

	/**
	 * Check if file data is already cached (already downloaded).
	 * @param FileId The file identifier
	 * @param OutFileData Output: cached file data if found
	 * @return True if cache hit
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|FileManager")
	bool GetCachedFileData(const FString& FileId, TArray<uint8>& OutFileData) const;

	// ==================== Cache Management ====================

	/**
	 * Clear the upload cache.
	 * Use this if you need to force re-upload of files.
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|FileManager")
	void ClearUploadCache();

	/**
	 * Clear the download cache (Phase 6).
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|FileManager")
	void ClearDownloadCache();

	/**
	 * Get the number of cached upload entries.
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|FileManager")
	int32 GetUploadCacheSize() const { return UploadCache.Num(); }

	/**
	 * Get the number of cached download entries.
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|FileManager")
	int32 GetDownloadCacheSize() const { return DownloadCache.Num(); }

	/**
	 * Get the total size of the download cache in bytes.
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|FileManager")
	int64 GetDownloadCacheSizeBytes() const;

	// ==================== Operation Management ====================

	/**
	 * Cancel an upload operation.
	 * @param OperationId The operation to cancel
	 * @return True if operation was found and cancelled
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|FileManager")
	bool CancelUpload(const FGuid& OperationId);

	/**
	 * Cancel all pending uploads.
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|FileManager")
	void CancelAllUploads();

	/**
	 * Check if an upload operation is in progress.
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|FileManager")
	bool IsUploadInProgress(const FGuid& OperationId) const;

	/**
	 * Get the number of pending upload operations.
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|FileManager")
	int32 GetPendingUploadCount() const { return PendingUploads.Num(); }

	/**
	 * Cancel a download operation.
	 * @param OperationId The operation to cancel
	 * @return True if operation was found and cancelled
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|FileManager")
	bool CancelDownload(const FGuid& OperationId);

	/**
	 * Cancel all pending downloads.
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|FileManager")
	void CancelAllDownloads();

	/**
	 * Check if a download operation is in progress.
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|FileManager")
	bool IsDownloadInProgress(const FGuid& OperationId) const;

	/**
	 * Get the number of pending download operations.
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|FileManager")
	int32 GetPendingDownloadCount() const { return PendingDownloads.Num(); }

	// ==================== Events ====================

	/** Fired when any upload completes successfully */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|FileManager|Events")
	FOnFileUploadComplete OnUploadComplete;

	/** Fired when any upload fails */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|FileManager|Events")
	FOnFileUploadFailed OnUploadFailed;

	/** Fired during upload progress */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|FileManager|Events")
	FOnFileUploadProgress OnUploadProgress;

	/** Fired when any download completes (Phase 6) */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|FileManager|Events")
	FOnFileDownloadComplete OnDownloadComplete;

	/** Fired when any download fails (Phase 6) */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|FileManager|Events")
	FOnFileDownloadFailed OnDownloadFailed;

	/** Fired during download progress (Phase 6) */
	UPROPERTY(BlueprintAssignable, Category = "Atlas|FileManager|Events")
	FOnFileDownloadProgress OnDownloadProgress;

protected:
	// ==================== Upload Internal ====================

	/** Start the actual HTTP upload to a URL */
	void ExecuteUpload(const FGuid& OperationId, const FString& UploadUrl, const TArray<uint8>& FileData, const FString& FileName, const FString& ContentHash);

	/** Handle upload HTTP response */
	UFUNCTION()
	void OnUploadRequestComplete(UAtlasHttpRequest* Request, class UAtlasJsonObject* ResponseJson, int32 StatusCode);

	UFUNCTION()
	void OnUploadRequestFailed(UAtlasHttpRequest* Request, const FString& ErrorMessage, int32 StatusCode);

	/** Find pending upload by request */
	FAtlasPendingUpload* FindUploadByRequest(UAtlasHttpRequest* Request);

	/** Remove pending upload */
	void RemovePendingUpload(const FGuid& OperationId);

	// ==================== Download Internal ====================

	/** Start the actual HTTP download from a URL */
	void ExecuteDownload(const FGuid& OperationId, const FString& DownloadUrl, const FString& FileId);

	/** Handle download HTTP response (binary data) */
	UFUNCTION()
	void OnDownloadRequestComplete(UAtlasHttpRequest* Request, class UAtlasJsonObject* ResponseJson, int32 StatusCode);

	UFUNCTION()
	void OnDownloadRequestFailed(UAtlasHttpRequest* Request, const FString& ErrorMessage, int32 StatusCode);

	/** Find pending download by request */
	FAtlasPendingDownload* FindDownloadByRequest(UAtlasHttpRequest* Request);

	/** Remove pending download */
	void RemovePendingDownload(const FGuid& OperationId);

private:
	/** Upload cache: ContentHash → FileId */
	UPROPERTY()
	TMap<FString, FString> UploadCache;

	/** Download cache: FileId → FileData (Phase 6) */
	TMap<FString, TArray<uint8>> DownloadCache;

	/** Pending upload operations */
	UPROPERTY()
	TArray<FAtlasPendingUpload> PendingUploads;

	/** Pending download operations (Phase 6) */
	UPROPERTY()
	TArray<FAtlasPendingDownload> PendingDownloads;
};
