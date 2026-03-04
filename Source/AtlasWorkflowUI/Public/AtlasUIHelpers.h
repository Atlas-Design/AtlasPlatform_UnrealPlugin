// Copyright Atlas Platform. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Types/AtlasValueTypes.h"
#include "Types/AtlasSchemaTypes.h"
#include "AtlasUIHelpers.generated.h"

class UAtlasWorkflowAsset;
class UAtlasEditorSubsystem;
class UTexture2D;
class UStaticMesh;

/**
 * Blueprint function library providing helper functions for Atlas UI widgets.
 * 
 * These functions wrap common operations needed by the UI widgets,
 * providing Blueprint-friendly interfaces to editor functionality.
 * 
 * Categories:
 * - Editor Subsystem Access
 * - Workflow Import/Query
 * - File Dialogs (OS native file pickers)
 * - Asset Pickers (Content Browser)
 * - File Utilities (info, validation, formatting)
 * - Image Loading (for previews)
 * - Type Colors (for visual type indicators)
 * - AtlasValue Helpers (create/validate values)
 * - UI Utilities (notifications, clipboard, formatting)
 */
UCLASS()
class ATLASWORKFLOWUI_API UAtlasUIHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ==================== Editor Subsystem Access ====================

	/**
	 * Get the Atlas Editor Subsystem.
	 * This is the main entry point for all Atlas SDK functionality in the editor.
	 * @return The editor subsystem (may be null if not in editor)
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI", meta = (WorldContext = "WorldContextObject"))
	static UAtlasEditorSubsystem* GetEditorSubsystem(UObject* WorldContextObject);

	// ==================== Workflow Import ====================

	/**
	 * Open a file dialog to select a workflow JSON file.
	 * @param OutFilePath The selected file path (if user didn't cancel)
	 * @return True if user selected a file, false if cancelled
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|UI|Workflow")
	static bool OpenWorkflowFileDialog(FString& OutFilePath);

	/**
	 * Import a workflow from a JSON file and create an asset.
	 * Opens file dialog, reads the file, creates the asset in Content Browser.
	 * @param OutAsset The created workflow asset (if successful)
	 * @param OutError Error message if import failed
	 * @return True if import was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|UI|Workflow")
	static bool ImportWorkflowWithDialog(UAtlasWorkflowAsset*& OutAsset, FString& OutError);

	/**
	 * Import a workflow from a specific JSON file path.
	 * @param FilePath Path to the JSON file
	 * @param OutAsset The created workflow asset (if successful)
	 * @param OutError Error message if import failed
	 * @return True if import was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|UI|Workflow")
	static bool ImportWorkflowFromPath(const FString& FilePath, UAtlasWorkflowAsset*& OutAsset, FString& OutError);

	// ==================== Workflow Query ====================

	/**
	 * Get all workflow assets in the project.
	 * @return Array of all workflow assets
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|UI|Workflow")
	static TArray<UAtlasWorkflowAsset*> GetAllWorkflowAssets();

	/**
	 * Get display info for a workflow asset (for dropdown/list display).
	 * @param Asset The workflow asset
	 * @param OutName The display name
	 * @param OutApiId The API ID
	 * @param OutIsValid Whether the asset is valid
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Workflow")
	static void GetWorkflowDisplayInfo(UAtlasWorkflowAsset* Asset, FString& OutName, FString& OutApiId, bool& OutIsValid);

	// ==================== File Dialogs ====================

	/**
	 * Open a native OS file dialog with custom settings.
	 * @param Title Dialog title
	 * @param DefaultPath Starting directory
	 * @param FileFilter Filter string (e.g., "Image Files (*.png;*.jpg)|*.png;*.jpg")
	 * @param OutFilePath The selected file path
	 * @return True if user selected a file
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|UI|Files")
	static bool OpenFileDialog(const FString& Title, const FString& DefaultPath, const FString& FileFilter, FString& OutFilePath);

	/**
	 * Open a file dialog configured for the given Atlas value type.
	 * Automatically sets appropriate file filters based on type.
	 * @param Type The Atlas value type (Image, Mesh, File)
	 * @param AllowedExtensions Optional specific extensions to allow (overrides defaults)
	 * @param OutFilePath The selected file path
	 * @return True if user selected a file
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|UI|Files")
	static bool OpenFileDialogForType(EAtlasValueType Type, const TArray<FString>& AllowedExtensions, FString& OutFilePath);

	/**
	 * Open a file dialog specifically for image files.
	 * @param OutFilePath The selected file path
	 * @return True if user selected a file
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|UI|Files")
	static bool OpenImageFileDialog(FString& OutFilePath);

	/**
	 * Open a file dialog specifically for mesh files.
	 * @param OutFilePath The selected file path
	 * @return True if user selected a file
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|UI|Files")
	static bool OpenMeshFileDialog(FString& OutFilePath);

	// ==================== Asset Pickers ====================

	/**
	 * Open the Content Browser asset picker for the given Atlas value type.
	 * @param Type The Atlas value type (Image -> UTexture2D, Mesh -> UStaticMesh)
	 * @param OutAsset The selected asset
	 * @return True if user selected an asset
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|UI|Assets")
	static bool OpenAssetPickerForType(EAtlasValueType Type, UObject*& OutAsset);

	/**
	 * Open the Content Browser asset picker for textures.
	 * @param OutTexture The selected texture
	 * @return True if user selected a texture
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|UI|Assets")
	static bool OpenTextureAssetPicker(UTexture2D*& OutTexture);

	/**
	 * Open the Content Browser asset picker for static meshes.
	 * @param OutMesh The selected mesh
	 * @return True if user selected a mesh
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|UI|Assets")
	static bool OpenMeshAssetPicker(UStaticMesh*& OutMesh);

	// ==================== File Utilities ====================

	/**
	 * Get information about a file.
	 * @param FilePath Path to the file
	 * @param bOutExists Whether the file exists
	 * @param OutFileSize Size in bytes
	 * @param OutFileName Just the filename (without path)
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Files")
	static void GetFileInfo(const FString& FilePath, bool& bOutExists, int64& OutFileSize, FString& OutFileName);

	/**
	 * Format a file size in bytes to human-readable string.
	 * @param SizeInBytes The size in bytes
	 * @return Formatted string (e.g., "1.5 MB")
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Files")
	static FString FormatFileSize(int64 SizeInBytes);

	/**
	 * Get the file filter string for a given Atlas value type.
	 * @param Type The value type
	 * @param AllowedExtensions Optional specific extensions
	 * @return Filter string for file dialog
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Files")
	static FString GetFileFilterForType(EAtlasValueType Type, const TArray<FString>& AllowedExtensions);

	/**
	 * Validate a file path for a given type.
	 * Checks existence and extension.
	 * @param FilePath The file path to validate
	 * @param ExpectedType The expected Atlas value type
	 * @param AllowedExtensions Specific allowed extensions (empty = use defaults)
	 * @param OutError Error message if invalid
	 * @return True if valid
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Files")
	static bool ValidateFilePath(const FString& FilePath, EAtlasValueType ExpectedType, const TArray<FString>& AllowedExtensions, FString& OutError);

	/**
	 * Check if a file exists.
	 * @param FilePath The file path
	 * @return True if the file exists
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Files")
	static bool FileExists(const FString& FilePath);

	/**
	 * Get just the filename from a path.
	 * @param FilePath Full file path
	 * @return Just the filename with extension
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Files")
	static FString GetCleanFilename(const FString& FilePath);

	/**
	 * Get the file extension from a path (without the dot).
	 * @param FilePath Full file path
	 * @return The extension (e.g., "png")
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Files")
	static FString GetFileExtension(const FString& FilePath);

	// ==================== Image Loading ====================

	/**
	 * Load an image file from disk as a UTexture2D.
	 * Supports PNG, JPG, JPEG, BMP formats.
	 * @param FilePath Path to the image file
	 * @return The loaded texture, or nullptr if failed
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|UI|Images")
	static UTexture2D* LoadImageFromDisk(const FString& FilePath);

	/**
	 * Get the dimensions of a texture.
	 * @param Texture The texture
	 * @param OutWidth Width in pixels
	 * @param OutHeight Height in pixels
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Images")
	static void GetTextureDimensions(UTexture2D* Texture, int32& OutWidth, int32& OutHeight);

	/**
	 * Calculate display size maintaining aspect ratio.
	 * @param OriginalWidth Original width
	 * @param OriginalHeight Original height
	 * @param MaxHeight Maximum height constraint
	 * @param OutWidth Calculated display width
	 * @param OutHeight Calculated display height
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Images")
	static void CalculateAspectRatioSize(int32 OriginalWidth, int32 OriginalHeight, float MaxHeight, float& OutWidth, float& OutHeight);

	// ==================== Type Colors ====================

	/**
	 * Get the color associated with an Atlas value type.
	 * @param Type The value type
	 * @return The color for that type
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Colors")
	static FLinearColor GetColorForValueType(EAtlasValueType Type);

	/** Get the color for Boolean type (Red) */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Colors")
	static FLinearColor GetBooleanColor();

	/** Get the color for Number type (Orange) */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Colors")
	static FLinearColor GetNumberColor();

	/** Get the color for String type (Blue) */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Colors")
	static FLinearColor GetStringColor();

	/** Get the color for Image type (Yellow) */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Colors")
	static FLinearColor GetImageColor();

	/** Get the color for Mesh type (Purple) */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Colors")
	static FLinearColor GetMeshColor();

	/** Get the color for File type (Gray) */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Colors")
	static FLinearColor GetFileColor();

	/** Get the color for Integer type (Orange, same as Number) */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Colors")
	static FLinearColor GetIntegerColor();

	/** Get success status color (Green) */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Colors")
	static FLinearColor GetSuccessColor();

	/** Get failed status color (Red) */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Colors")
	static FLinearColor GetFailedColor();

	/** Get running status color (Yellow) */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Colors")
	static FLinearColor GetRunningColor();

	/** Get pending status color (Gray) */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Colors")
	static FLinearColor GetPendingColor();

	// ==================== AtlasValue Helpers ====================

	/**
	 * Create an AtlasValue containing a string.
	 * @param Value The string value
	 * @return AtlasValue of type String
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Values")
	static FAtlasValue MakeStringValue(const FString& Value);

	/**
	 * Create an AtlasValue containing a number (float).
	 * @param Value The number value
	 * @return AtlasValue of type Number
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Values")
	static FAtlasValue MakeNumberValue(float Value);

	/**
	 * Create an AtlasValue containing an integer.
	 * @param Value The integer value
	 * @return AtlasValue of type Integer
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Values")
	static FAtlasValue MakeIntegerValue(int32 Value);

	/**
	 * Create an AtlasValue containing a boolean.
	 * @param Value The boolean value
	 * @return AtlasValue of type Boolean
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Values")
	static FAtlasValue MakeBoolValue(bool Value);

	/**
	 * Create an AtlasValue for a file path.
	 * @param FilePath Path to the file
	 * @param FileType The specific file type (File, Image, or Mesh)
	 * @return AtlasValue configured for file upload
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Values")
	static FAtlasValue MakeFileValue(const FString& FilePath, EAtlasValueType FileType);

	/**
	 * Create an empty/none AtlasValue.
	 * @return AtlasValue of type None
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Values")
	static FAtlasValue MakeEmptyValue();

	/**
	 * Validate an AtlasValue against a parameter definition.
	 * @param Value The value to validate
	 * @param ParameterDef The parameter definition to validate against
	 * @param OutError Error message if validation fails
	 * @return True if valid
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Values")
	static bool ValidateValue(const FAtlasValue& Value, const FAtlasParameterDef& ParameterDef, FString& OutError);

	/**
	 * Check if an AtlasValue is empty/none.
	 * @param Value The value to check
	 * @return True if the value is empty
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Values")
	static bool IsValueEmpty(const FAtlasValue& Value);

	/**
	 * Get a human-readable string representation of an AtlasValue.
	 * @param Value The value
	 * @return String representation
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Values")
	static FString ValueToString(const FAtlasValue& Value);

	// ==================== Schema Helpers ====================

	/**
	 * Create a minimal ParameterDef from an existing value.
	 * Useful for displaying output values or dynamic content where schema isn't available.
	 * @param Value The value to infer type from
	 * @param Name The parameter name
	 * @param DisplayName Optional display name (uses Name if empty)
	 * @return A ParameterDef with type matching the value
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Schema")
	static FAtlasParameterDef MakeParameterDefFromValue(const FAtlasValue& Value, const FString& Name, const FString& DisplayName = TEXT(""));

	/**
	 * Get the display name from a parameter definition.
	 * Falls back to Name if DisplayName is empty.
	 * @param ParameterDef The parameter definition
	 * @return The display name
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Schema")
	static FString GetParameterDisplayName(const FAtlasParameterDef& ParameterDef);

	// ==================== UI Utilities ====================

	/**
	 * Format a relative time string (e.g., "2 hours ago").
	 * @param DateTime The time to format
	 * @return Formatted relative time string
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Utilities")
	static FString FormatRelativeTime(const FDateTime& DateTime);

	/**
	 * Get the current local time as a formatted string (HH:MM:SS).
	 * @return Current time string in 24-hour format
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Utilities")
	static FString GetCurrentTimeString();

	/**
	 * Format a DateTime as time string (HH:MM:SS).
	 * @param DateTime The datetime to format
	 * @return Time string in 24-hour format
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Utilities")
	static FString FormatTimeString(const FDateTime& DateTime);

	/**
	 * Format a DateTime as date and time string (e.g., "Jan 29, 19:12").
	 * @param DateTime The datetime to format
	 * @return Formatted date and time string
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Utilities")
	static FString FormatDateTimeString(const FDateTime& DateTime);

	/**
	 * Format a DateTime as full date string (e.g., "Jan 29, 2026").
	 * @param DateTime The datetime to format
	 * @return Formatted date string
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Utilities")
	static FString FormatDateString(const FDateTime& DateTime);

	/**
	 * Get the date category for grouping history items.
	 * Returns "Today", "Last 7 Days", "Last 30 Days", or "Older".
	 * @param DateTime The datetime to categorize
	 * @return Category string for grouping
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Utilities")
	static FString GetHistoryDateCategory(const FDateTime& DateTime);

	/**
	 * Format a duration in seconds to MM:SS format.
	 * @param DurationSeconds Duration in seconds
	 * @return Formatted duration string (e.g., "01:30")
	 */
	UFUNCTION(BlueprintPure, Category = "Atlas|UI|Utilities")
	static FString FormatDuration(float DurationSeconds);

	/**
	 * Copy text to the system clipboard.
	 * @param Text The text to copy
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|UI|Utilities")
	static void CopyToClipboard(const FString& Text);

	/**
	 * Show a notification toast in the editor.
	 * @param Message The message to display
	 * @param bSuccess True for success (green), false for error (red)
	 */
	UFUNCTION(BlueprintCallable, Category = "Atlas|UI|Utilities")
	static void ShowNotification(const FString& Message, bool bSuccess = true);

private:
	/** Get parent window handle for dialogs */
	static void* GetParentWindowHandle();
};
