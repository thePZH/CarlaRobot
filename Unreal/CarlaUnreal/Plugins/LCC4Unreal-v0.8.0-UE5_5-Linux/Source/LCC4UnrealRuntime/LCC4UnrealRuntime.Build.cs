// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Linq;
using EpicGames.Core;
using UnrealBuildTool;

public class LCC4UnrealRuntime : ModuleRules
{
    public LCC4UnrealRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        bUsePrecompiled = true;
        bEnableExceptions = true;
        PrecompileForTargets = PrecompileTargetsType.Any;  // 允许为所有目标类型预编译，解决打包时的路径问题
        
        PublicDefinitions.Add("WITH_LCC=1");
        
        PublicIncludePaths.AddRange(
            new string[]
            {
                // ... add public include paths required here ...
            }
        );

        PrivateIncludePaths.AddRange(
            new string[]
            {
                "LCC4UnrealRuntime/Private",
                Path.Combine(GetModuleDirectory("Renderer"), "Private")
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                // ... add other public dependencies that you statically link with here ...
                "CoreUObject",
                "DeveloperSettings",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Engine",
                "Slate",
                "SlateCore",
                "Core",
                "Engine",
                "RenderCore",
                "Renderer",
                "Projects",
                "RHI",
                "InputCore",
                "Json",
                "JsonUtilities",
                "HTTP",
                "SSL",
                "EngineSettings",
                "ApplicationCore",
                "OpenSSL",
                "GeoReferencing",
                "LCC4UnrealCesium"
            }
        );

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "EditorFramework",
                    "UnrealEd",
                    "PropertyEditor",
                    "ContentBrowser",
                    "AssetRegistry",
                    "DesktopPlatform"
                }
            );
        }

        var ResourcePath = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../Resources/"));
        RuntimeDependencies.Add(Path.Combine(ResourcePath, "public_key.pem"));
    }
}
