// Copyright Atlas Platform. All Rights Reserved.

using UnrealBuildTool;

public class AtlasWorkflowUI : ModuleRules
{
	public AtlasWorkflowUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UMG",
				"Slate",
				"SlateCore",
				"AtlasSDK",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"EditorStyle",
				"InputCore",
				"AssetTools",
				"ContentBrowser",
				"AtlasWorkflowEditor",
				"DesktopPlatform",
				"AssetRegistry",
				"ApplicationCore",
				"ImageCore",
				"RenderCore",
			}
		);
	}
}
