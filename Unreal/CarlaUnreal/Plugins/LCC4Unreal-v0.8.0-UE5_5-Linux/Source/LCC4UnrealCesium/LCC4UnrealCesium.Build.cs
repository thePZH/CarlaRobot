// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using UnrealBuildTool;

public class LCC4UnrealCesium : ModuleRules
{
    public LCC4UnrealCesium(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        
        bool bCeisumEnable = ConfigurePlugins(this,Target,"CesiumForUnreal");
        
        if(bCeisumEnable)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "CesiumRuntime",
                }
            );
            PublicDefinitions.Add("LCC_WITH_CESIUM=1");
        }
        else
        {
            PublicDefinitions.Add("LCC_WITH_CESIUM=0");
        }

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Core",
                "Engine",
                "EngineSettings",
                "ApplicationCore"
            }
        );
    }
    
    public static bool ConfigurePlugins(ModuleRules Rules, ReadOnlyTargetRules Target,string PluginName)
    {
        if (Target.ProjectFile == null || !JsonObject.TryRead(Target.ProjectFile, out var rawObject)) return false;
        if (!rawObject.TryGetObjectArrayField("Plugins", out var pluginObjects)) return false;
        foreach (var pluginObject in pluginObjects)
        {
            pluginObject.TryGetStringField("Name", out var pluginName);
            pluginObject.TryGetBoolField("Enabled", out var pluginEnabled);
            if(pluginName == PluginName && pluginEnabled)
            {
                return true;
            }
        }
        return false;
    } 
}
