// Copyright Atlas Platform. All Rights Reserved.

using UnrealBuildTool;

public class AtlasSDK : ModuleRules
{
	public AtlasSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
			}
		);
				
		PrivateIncludePaths.AddRange(
			new string[] {
			}
		);
			
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DeveloperSettings", // For UDeveloperSettings base class
				"Json",
				"JsonUtilities",
				"AtlasHTTP", // For HTTP requests and JSON utilities
			}
		);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"AssetRegistry",
				"HTTP", // For MD5 hashing
				"ImageWrapper", // For decoding PNG/JPEG to raw pixels (thumbnails)
			}
		);
		
		// Editor-only dependencies for import functionality
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"ContentBrowser",
				}
			);
		}
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}
