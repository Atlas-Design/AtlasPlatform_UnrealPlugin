// Fill out your copyright notice in the Description page of Project Settings.

#include "AtlasApiSettings.h"
#include "Misc/Paths.h"

FString UAtlasApiSettings::GetAtlasDefaultImportFolder()
{
	const UAtlasApiSettings* Settings = GetDefault<UAtlasApiSettings>();

	// Fallback in case something is weird
	const FString RelativePath = (Settings && !Settings->DefaultImportPath.IsEmpty())
		? Settings->DefaultImportPath
		: TEXT("Saved/Atlas/TempImports");

	return FPaths::Combine(FPaths::ProjectDir(), RelativePath);
}

FString UAtlasApiSettings::GetAtlasDefaultExportFolder()
{
	const UAtlasApiSettings* Settings = GetDefault<UAtlasApiSettings>();

	// Fallback in case something is weird
	const FString RelativePath = (Settings && !Settings->DefaultExportPath.IsEmpty())
		? Settings->DefaultExportPath
		: TEXT("Saved/Atlas/TempExports");

	return FPaths::Combine(FPaths::ProjectDir(), RelativePath);
}