// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LCC4UnrealEditor : ModuleRules
{
    public LCC4UnrealEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        bUsePrecompiled = true;
        PublicIncludePaths.AddRange(
            new string[] {
                // ... add public include paths required here ...
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
                "LCC4UnrealRuntime"
            }
            );
            
        
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "EditorFramework",
                "UnrealEd",
                "MainFrame",
                "GameProjectGeneration",
                "EditorStyle",
                "Projects",
                "PropertyEditor",
                "InputCore",
                "ToolWidgets",
                "EngineSettings",
                "ApplicationCore",
                "ToolMenus",
                "DesktopPlatform",
                "PlyConverter"
            }
            );
        
        
        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
                // ... add any modules that your module loads dynamically here ...
            }
            );
    }
}
