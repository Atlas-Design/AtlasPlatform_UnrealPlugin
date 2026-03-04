// AtlasWorkflowEditor.Build.cs

using UnrealBuildTool;

public class AtlasWorkflowEditor : ModuleRules
{
    public AtlasWorkflowEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        //bBuildInEditor = true;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "UnrealEd"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "Projects", 
                "AtlasWorkflow",
                "AtlasSDK",
                "DeveloperSettings",
                "AssetRegistry", 
                "ImageWrapper",
                "AssetTools",
                "EditorSubsystem",
                "RenderCore",
                "RHI",
                "ImageWrapper",
                "ImageCore",
                "DesktopPlatform",
                "ToolMenus",
                "LevelEditor",
                "Blutility",
                "UMG",
                "UMGEditor"
            }
        );
    }
}