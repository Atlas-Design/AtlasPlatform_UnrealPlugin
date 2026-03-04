// Copyright Atlas Platform. All Rights Reserved.

#include "AtlasUIHelpers.h"
#include "AtlasEditorSubsystem.h"
#include "AtlasWorkflowAsset.h"
#include "Types/AtlasSchemaTypes.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/FileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"

// ==================== Editor Subsystem Access ====================

UAtlasEditorSubsystem* UAtlasUIHelpers::GetEditorSubsystem(UObject* WorldContextObject)
{
	if (GEditor)
	{
		return GEditor->GetEditorSubsystem<UAtlasEditorSubsystem>();
	}
	return nullptr;
}

// ==================== Workflow Import ====================

bool UAtlasUIHelpers::OpenWorkflowFileDialog(FString& OutFilePath)
{
	return OpenFileDialog(
		TEXT("Select Workflow JSON File"),
		FPaths::ProjectDir(),
		TEXT("JSON Files (*.json)|*.json"),
		OutFilePath
	);
}

bool UAtlasUIHelpers::ImportWorkflowWithDialog(UAtlasWorkflowAsset*& OutAsset, FString& OutError)
{
	FString FilePath;
	if (!OpenWorkflowFileDialog(FilePath))
	{
		OutError = TEXT("File selection cancelled");
		return false;
	}

	return ImportWorkflowFromPath(FilePath, OutAsset, OutError);
}

bool UAtlasUIHelpers::ImportWorkflowFromPath(const FString& FilePath, UAtlasWorkflowAsset*& OutAsset, FString& OutError)
{
	// Generate asset name from filename
	FString AssetName = FPaths::GetBaseFilename(FilePath);
	AssetName = AssetName.Replace(TEXT(" "), TEXT("_"));
	AssetName = AssetName.Replace(TEXT("-"), TEXT("_"));

	// Default package path
	FString PackagePath = TEXT("/Game/AtlasWorkflows/");

	// Import using the workflow asset's static method
	OutAsset = UAtlasWorkflowAsset::ImportFromFile(FilePath, PackagePath, AssetName, OutError);

	if (OutAsset)
	{
		ShowNotification(FString::Printf(TEXT("Imported workflow: %s"), *OutAsset->GetWorkflowName()), true);
		return true;
	}

	return false;
}

// ==================== Workflow Query ====================

TArray<UAtlasWorkflowAsset*> UAtlasUIHelpers::GetAllWorkflowAssets()
{
	TArray<UAtlasWorkflowAsset*> Results;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByClass(UAtlasWorkflowAsset::StaticClass()->GetClassPathName(), AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		if (UAtlasWorkflowAsset* Asset = Cast<UAtlasWorkflowAsset>(AssetData.GetAsset()))
		{
			Results.Add(Asset);
		}
	}

	return Results;
}

void UAtlasUIHelpers::GetWorkflowDisplayInfo(UAtlasWorkflowAsset* Asset, FString& OutName, FString& OutApiId, bool& OutIsValid)
{
	if (!Asset)
	{
		OutName = TEXT("(None)");
		OutApiId = TEXT("");
		OutIsValid = false;
		return;
	}

	OutName = Asset->GetWorkflowName();
	OutApiId = Asset->GetApiId();
	OutIsValid = Asset->IsValid();

	if (OutName.IsEmpty())
	{
		OutName = Asset->GetName();
	}
}

// ==================== File Dialogs ====================

void* UAtlasUIHelpers::GetParentWindowHandle()
{
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
	{
		return ParentWindow->GetNativeWindow()->GetOSWindowHandle();
	}
	return nullptr;
}

bool UAtlasUIHelpers::OpenFileDialog(const FString& Title, const FString& DefaultPath, const FString& FileFilter, FString& OutFilePath)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return false;
	}

	TArray<FString> OutFiles;
	bool bOpened = DesktopPlatform->OpenFileDialog(
		GetParentWindowHandle(),
		Title,
		DefaultPath,
		TEXT(""),
		FileFilter,
		EFileDialogFlags::None,
		OutFiles
	);

	if (bOpened && OutFiles.Num() > 0)
	{
		OutFilePath = OutFiles[0];
		return true;
	}

	return false;
}

bool UAtlasUIHelpers::OpenFileDialogForType(EAtlasValueType Type, const TArray<FString>& AllowedExtensions, FString& OutFilePath)
{
	FString Title;
	switch (Type)
	{
	case EAtlasValueType::Image:
		Title = TEXT("Select Image File");
		break;
	case EAtlasValueType::Mesh:
		Title = TEXT("Select Mesh File");
		break;
	default:
		Title = TEXT("Select File");
		break;
	}

	FString Filter = GetFileFilterForType(Type, AllowedExtensions);
	return OpenFileDialog(Title, FPaths::ProjectDir(), Filter, OutFilePath);
}

bool UAtlasUIHelpers::OpenImageFileDialog(FString& OutFilePath)
{
	return OpenFileDialogForType(EAtlasValueType::Image, TArray<FString>(), OutFilePath);
}

bool UAtlasUIHelpers::OpenMeshFileDialog(FString& OutFilePath)
{
	return OpenFileDialogForType(EAtlasValueType::Mesh, TArray<FString>(), OutFilePath);
}

// ==================== Asset Pickers ====================

bool UAtlasUIHelpers::OpenAssetPickerForType(EAtlasValueType Type, UObject*& OutAsset)
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	UClass* AssetClass = nullptr;
	FString Title;

	switch (Type)
	{
	case EAtlasValueType::Image:
		AssetClass = UTexture2D::StaticClass();
		Title = TEXT("Select Texture");
		break;
	case EAtlasValueType::Mesh:
		AssetClass = UStaticMesh::StaticClass();
		Title = TEXT("Select Mesh");
		break;
	default:
		AssetClass = UObject::StaticClass();
		Title = TEXT("Select Asset");
		break;
	}

	FOpenAssetDialogConfig Config;
	Config.DialogTitleOverride = FText::FromString(Title);
	Config.bAllowMultipleSelection = false;

	if (AssetClass && AssetClass != UObject::StaticClass())
	{
		Config.AssetClassNames.Add(AssetClass->GetClassPathName());
	}

	TArray<FAssetData> SelectedAssets = ContentBrowserModule.Get().CreateModalOpenAssetDialog(Config);

	if (SelectedAssets.Num() > 0)
	{
		OutAsset = SelectedAssets[0].GetAsset();
		return OutAsset != nullptr;
	}

	return false;
}

bool UAtlasUIHelpers::OpenTextureAssetPicker(UTexture2D*& OutTexture)
{
	UObject* Asset = nullptr;
	if (OpenAssetPickerForType(EAtlasValueType::Image, Asset))
	{
		OutTexture = Cast<UTexture2D>(Asset);
		return OutTexture != nullptr;
	}
	return false;
}

bool UAtlasUIHelpers::OpenMeshAssetPicker(UStaticMesh*& OutMesh)
{
	UObject* Asset = nullptr;
	if (OpenAssetPickerForType(EAtlasValueType::Mesh, Asset))
	{
		OutMesh = Cast<UStaticMesh>(Asset);
		return OutMesh != nullptr;
	}
	return false;
}

// ==================== File Utilities ====================

void UAtlasUIHelpers::GetFileInfo(const FString& FilePath, bool& bOutExists, int64& OutFileSize, FString& OutFileName)
{
	bOutExists = FPaths::FileExists(FilePath);
	OutFileName = FPaths::GetCleanFilename(FilePath);
	
	if (bOutExists)
	{
		OutFileSize = IFileManager::Get().FileSize(*FilePath);
	}
	else
	{
		OutFileSize = 0;
	}
}

FString UAtlasUIHelpers::FormatFileSize(int64 SizeInBytes)
{
	if (SizeInBytes <= 0)
	{
		return TEXT("0 B");
	}

	const int64 KB = 1024;
	const int64 MB = KB * 1024;
	const int64 GB = MB * 1024;

	if (SizeInBytes >= GB)
	{
		return FString::Printf(TEXT("%.2f GB"), (double)SizeInBytes / GB);
	}
	else if (SizeInBytes >= MB)
	{
		return FString::Printf(TEXT("%.2f MB"), (double)SizeInBytes / MB);
	}
	else if (SizeInBytes >= KB)
	{
		return FString::Printf(TEXT("%.2f KB"), (double)SizeInBytes / KB);
	}
	else
	{
		return FString::Printf(TEXT("%lld B"), SizeInBytes);
	}
}

FString UAtlasUIHelpers::GetFileFilterForType(EAtlasValueType Type, const TArray<FString>& AllowedExtensions)
{
	// If specific extensions are provided, use those
	if (AllowedExtensions.Num() > 0)
	{
		FString ExtFilter;
		for (const FString& Ext : AllowedExtensions)
		{
			if (!ExtFilter.IsEmpty()) ExtFilter += TEXT(";");
			ExtFilter += FString::Printf(TEXT("*.%s"), *Ext);
		}
		return FString::Printf(TEXT("Allowed Files (%s)|%s|All Files (*.*)|*.*"), *ExtFilter, *ExtFilter);
	}

	// Otherwise use defaults based on type
	switch (Type)
	{
	case EAtlasValueType::Image:
		return TEXT("Image Files (*.png;*.jpg;*.jpeg)|*.png;*.jpg;*.jpeg|All Files (*.*)|*.*");

	case EAtlasValueType::Mesh:
		return TEXT("Mesh Files (*.fbx;*.obj;*.glb;*.gltf)|*.fbx;*.obj;*.glb;*.gltf|All Files (*.*)|*.*");

	case EAtlasValueType::File:
	default:
		return TEXT("All Files (*.*)|*.*");
	}
}

bool UAtlasUIHelpers::ValidateFilePath(const FString& FilePath, EAtlasValueType ExpectedType, const TArray<FString>& AllowedExtensions, FString& OutError)
{
	if (FilePath.IsEmpty())
	{
		OutError = TEXT("No file selected");
		return false;
	}

	if (!FPaths::FileExists(FilePath))
	{
		OutError = FString::Printf(TEXT("File not found: %s"), *FilePath);
		return false;
	}

	// Check extension if we have allowed extensions
	if (AllowedExtensions.Num() > 0)
	{
		FString Extension = FPaths::GetExtension(FilePath).ToLower();
		bool bAllowed = false;
		for (const FString& AllowedExt : AllowedExtensions)
		{
			if (Extension == AllowedExt.ToLower())
			{
				bAllowed = true;
				break;
			}
		}

		if (!bAllowed)
		{
			OutError = FString::Printf(TEXT("Invalid file type. Allowed: %s"),
				*FString::Join(AllowedExtensions, TEXT(", ")));
			return false;
		}
	}
	else
	{
		// Use default extensions for the type
		FString Extension = FPaths::GetExtension(FilePath).ToLower();
		bool bAllowed = true;

		switch (ExpectedType)
		{
		case EAtlasValueType::Image:
			bAllowed = (Extension == TEXT("png") || Extension == TEXT("jpg") || Extension == TEXT("jpeg"));
			if (!bAllowed) OutError = TEXT("Invalid image format. Allowed: png, jpg, jpeg");
			break;

		case EAtlasValueType::Mesh:
			bAllowed = (Extension == TEXT("fbx") || Extension == TEXT("obj") || Extension == TEXT("glb") || Extension == TEXT("gltf"));
			if (!bAllowed) OutError = TEXT("Invalid mesh format. Allowed: fbx, obj, glb, gltf");
			break;

		default:
			// Generic file - any extension is fine
			break;
		}

		if (!bAllowed)
		{
			return false;
		}
	}

	return true;
}

bool UAtlasUIHelpers::FileExists(const FString& FilePath)
{
	return FPaths::FileExists(FilePath);
}

FString UAtlasUIHelpers::GetCleanFilename(const FString& FilePath)
{
	return FPaths::GetCleanFilename(FilePath);
}

FString UAtlasUIHelpers::GetFileExtension(const FString& FilePath)
{
	return FPaths::GetExtension(FilePath);
}

// ==================== Image Loading ====================

UTexture2D* UAtlasUIHelpers::LoadImageFromDisk(const FString& FilePath)
{
	if (FilePath.IsEmpty() || !FPaths::FileExists(FilePath))
	{
		return nullptr;
	}

	FString Extension = FPaths::GetExtension(FilePath).ToLower();
	if (Extension != TEXT("png") && Extension != TEXT("jpg") && Extension != TEXT("jpeg") && Extension != TEXT("bmp"))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AtlasUIHelpers] Unsupported image format: %s"), *Extension);
		return nullptr;
	}

	UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(FilePath);
	
	if (Texture)
	{
		UE_LOG(LogTemp, Log, TEXT("[AtlasUIHelpers] Loaded image: %s (%dx%d)"),
			*FilePath, Texture->GetSizeX(), Texture->GetSizeY());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[AtlasUIHelpers] Failed to load image: %s"), *FilePath);
	}

	return Texture;
}

void UAtlasUIHelpers::GetTextureDimensions(UTexture2D* Texture, int32& OutWidth, int32& OutHeight)
{
	if (Texture)
	{
		OutWidth = Texture->GetSizeX();
		OutHeight = Texture->GetSizeY();
	}
	else
	{
		OutWidth = 0;
		OutHeight = 0;
	}
}

void UAtlasUIHelpers::CalculateAspectRatioSize(int32 OriginalWidth, int32 OriginalHeight, float MaxHeight, float& OutWidth, float& OutHeight)
{
	if (OriginalWidth <= 0 || OriginalHeight <= 0 || MaxHeight <= 0)
	{
		OutWidth = 0;
		OutHeight = 0;
		return;
	}

	float AspectRatio = (float)OriginalWidth / (float)OriginalHeight;
	OutHeight = MaxHeight;
	OutWidth = MaxHeight * AspectRatio;
}

// ==================== Type Colors ====================

FLinearColor UAtlasUIHelpers::GetColorForValueType(EAtlasValueType Type)
{
	switch (Type)
	{
	case EAtlasValueType::Boolean:
		return GetBooleanColor();
	case EAtlasValueType::Number:
		return GetNumberColor();
	case EAtlasValueType::Integer:
		return GetIntegerColor();
	case EAtlasValueType::String:
		return GetStringColor();
	case EAtlasValueType::Image:
		return GetImageColor();
	case EAtlasValueType::Mesh:
		return GetMeshColor();
	case EAtlasValueType::File:
	case EAtlasValueType::FileId:
		return GetFileColor();
	default:
		return FLinearColor::White;
	}
}

FLinearColor UAtlasUIHelpers::GetBooleanColor()
{
	return FLinearColor(0.906f, 0.298f, 0.235f); // #E74C3C - Red
}

FLinearColor UAtlasUIHelpers::GetNumberColor()
{
	return FLinearColor(0.902f, 0.494f, 0.133f); // #E67E22 - Orange
}

FLinearColor UAtlasUIHelpers::GetStringColor()
{
	return FLinearColor(0.204f, 0.596f, 0.859f); // #3498DB - Blue
}

FLinearColor UAtlasUIHelpers::GetImageColor()
{
	return FLinearColor(0.945f, 0.769f, 0.059f); // #F1C40F - Yellow
}

FLinearColor UAtlasUIHelpers::GetMeshColor()
{
	return FLinearColor(0.608f, 0.349f, 0.714f); // #9B59B6 - Purple
}

FLinearColor UAtlasUIHelpers::GetFileColor()
{
	return FLinearColor(0.584f, 0.647f, 0.651f); // #95A5A6 - Gray
}

FLinearColor UAtlasUIHelpers::GetIntegerColor()
{
	return FLinearColor(0.902f, 0.494f, 0.133f); // #E67E22 - Orange (same as Number)
}

FLinearColor UAtlasUIHelpers::GetSuccessColor()
{
	return FLinearColor(0.180f, 0.800f, 0.443f); // #2ECC71 - Green
}

FLinearColor UAtlasUIHelpers::GetFailedColor()
{
	return FLinearColor(0.906f, 0.298f, 0.235f); // #E74C3C - Red
}

FLinearColor UAtlasUIHelpers::GetRunningColor()
{
	return FLinearColor(0.945f, 0.769f, 0.059f); // #F1C40F - Yellow
}

FLinearColor UAtlasUIHelpers::GetPendingColor()
{
	return FLinearColor(0.584f, 0.647f, 0.651f); // #95A5A6 - Gray
}

// ==================== AtlasValue Helpers ====================

FAtlasValue UAtlasUIHelpers::MakeStringValue(const FString& Value)
{
	return FAtlasValue::MakeString(Value);
}

FAtlasValue UAtlasUIHelpers::MakeNumberValue(float Value)
{
	return FAtlasValue::MakeNumber(Value);
}

FAtlasValue UAtlasUIHelpers::MakeIntegerValue(int32 Value)
{
	return FAtlasValue::MakeInteger(Value);
}

FAtlasValue UAtlasUIHelpers::MakeBoolValue(bool Value)
{
	return FAtlasValue::MakeBool(Value);
}

FAtlasValue UAtlasUIHelpers::MakeFileValue(const FString& FilePath, EAtlasValueType FileType)
{
	switch (FileType)
	{
	case EAtlasValueType::Image:
		return FAtlasValue::MakeImage(FilePath);
	case EAtlasValueType::Mesh:
		return FAtlasValue::MakeMesh(FilePath);
	case EAtlasValueType::File:
	default:
		return FAtlasValue::MakeFile(FilePath);
	}
}

FAtlasValue UAtlasUIHelpers::MakeEmptyValue()
{
	return FAtlasValue();
}

bool UAtlasUIHelpers::ValidateValue(const FAtlasValue& Value, const FAtlasParameterDef& ParameterDef, FString& OutError)
{
	return ParameterDef.ValidateValue(Value, OutError);
}

bool UAtlasUIHelpers::IsValueEmpty(const FAtlasValue& Value)
{
	return Value.Type == EAtlasValueType::None;
}

FString UAtlasUIHelpers::ValueToString(const FAtlasValue& Value)
{
	return Value.ToString();
}

// ==================== Schema Helpers ====================

FAtlasParameterDef UAtlasUIHelpers::MakeParameterDefFromValue(const FAtlasValue& Value, const FString& Name, const FString& DisplayName)
{
	return FAtlasParameterDef::FromValue(Value, Name, DisplayName);
}

FString UAtlasUIHelpers::GetParameterDisplayName(const FAtlasParameterDef& ParameterDef)
{
	return ParameterDef.GetDisplayName();
}

// ==================== UI Utilities ====================

FString UAtlasUIHelpers::FormatRelativeTime(const FDateTime& DateTime)
{
	FTimespan TimeSince = FDateTime::Now() - DateTime;  // Use local time for consistency

	if (TimeSince.GetTotalMinutes() < 1)
	{
		return TEXT("Just now");
	}
	else if (TimeSince.GetTotalMinutes() < 60)
	{
		int32 Minutes = FMath::RoundToInt(TimeSince.GetTotalMinutes());
		return FString::Printf(TEXT("%dm ago"), Minutes);
	}
	else if (TimeSince.GetTotalHours() < 24)
	{
		int32 Hours = FMath::RoundToInt(TimeSince.GetTotalHours());
		return FString::Printf(TEXT("%dh ago"), Hours);
	}
	else if (TimeSince.GetTotalDays() < 2)
	{
		return TEXT("Yesterday");
	}
	else if (TimeSince.GetTotalDays() < 7)
	{
		int32 Days = FMath::RoundToInt(TimeSince.GetTotalDays());
		return FString::Printf(TEXT("%dd ago"), Days);
	}
	else
	{
		return DateTime.ToString(TEXT("%b %d, %Y"));
	}
}

FString UAtlasUIHelpers::GetCurrentTimeString()
{
	return FormatTimeString(FDateTime::Now());
}

FString UAtlasUIHelpers::FormatTimeString(const FDateTime& DateTime)
{
	return FString::Printf(TEXT("%02d:%02d:%02d"), 
		DateTime.GetHour(), 
		DateTime.GetMinute(), 
		DateTime.GetSecond());
}

FString UAtlasUIHelpers::FormatDateTimeString(const FDateTime& DateTime)
{
	// Format: "Jan 29, 19:12"
	static const TCHAR* MonthNames[] = {
		TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"),
		TEXT("May"), TEXT("Jun"), TEXT("Jul"), TEXT("Aug"),
		TEXT("Sep"), TEXT("Oct"), TEXT("Nov"), TEXT("Dec")
	};
	
	int32 MonthIndex = DateTime.GetMonth() - 1;
	if (MonthIndex < 0 || MonthIndex > 11)
	{
		MonthIndex = 0;
	}
	
	return FString::Printf(TEXT("%s %d, %02d:%02d"),
		MonthNames[MonthIndex],
		DateTime.GetDay(),
		DateTime.GetHour(),
		DateTime.GetMinute());
}

FString UAtlasUIHelpers::FormatDateString(const FDateTime& DateTime)
{
	// Format: "Jan 29, 2026"
	static const TCHAR* MonthNames[] = {
		TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"),
		TEXT("May"), TEXT("Jun"), TEXT("Jul"), TEXT("Aug"),
		TEXT("Sep"), TEXT("Oct"), TEXT("Nov"), TEXT("Dec")
	};
	
	int32 MonthIndex = DateTime.GetMonth() - 1;
	if (MonthIndex < 0 || MonthIndex > 11)
	{
		MonthIndex = 0;
	}
	
	return FString::Printf(TEXT("%s %d, %d"),
		MonthNames[MonthIndex],
		DateTime.GetDay(),
		DateTime.GetYear());
}

FString UAtlasUIHelpers::GetHistoryDateCategory(const FDateTime& DateTime)
{
	FDateTime Now = FDateTime::Now();
	FDateTime Today = FDateTime(Now.GetYear(), Now.GetMonth(), Now.GetDay());
	
	// Calculate the date portion of the input
	FDateTime InputDate = FDateTime(DateTime.GetYear(), DateTime.GetMonth(), DateTime.GetDay());
	
	// Calculate days difference
	FTimespan Difference = Today - InputDate;
	int32 DaysAgo = FMath::FloorToInt(Difference.GetTotalDays());
	
	if (DaysAgo < 0)
	{
		// Future date (shouldn't happen, but handle it)
		return TEXT("Today");
	}
	else if (DaysAgo == 0)
	{
		return TEXT("Today");
	}
	else if (DaysAgo <= 7)
	{
		return TEXT("Last 7 Days");
	}
	else if (DaysAgo <= 30)
	{
		return TEXT("Last 30 Days");
	}
	else
	{
		return TEXT("Older");
	}
}

FString UAtlasUIHelpers::FormatDuration(float DurationSeconds)
{
	int32 TotalSeconds = FMath::RoundToInt(DurationSeconds);
	int32 Minutes = TotalSeconds / 60;
	int32 Seconds = TotalSeconds % 60;
	return FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
}

void UAtlasUIHelpers::CopyToClipboard(const FString& Text)
{
	FPlatformApplicationMisc::ClipboardCopy(*Text);
}

void UAtlasUIHelpers::ShowNotification(const FString& Message, bool bSuccess)
{
	FNotificationInfo Info(FText::FromString(Message));
	Info.ExpireDuration = 3.0f;
	Info.bUseLargeFont = false;
	Info.bUseSuccessFailIcons = true;
	Info.Image = bSuccess 
		? FCoreStyle::Get().GetBrush(TEXT("NotificationList.SuccessImage"))
		: FCoreStyle::Get().GetBrush(TEXT("NotificationList.FailImage"));

	FSlateNotificationManager::Get().AddNotification(Info);
}
