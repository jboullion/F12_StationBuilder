// F12StationBuilder.Build.cs
// Build configuration for the F12 Station Builder project

using UnrealBuildTool;

public class F12StationBuilder : ModuleRules
{
    public F12StationBuilder(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { 
            "Core", 
            "CoreUObject", 
            "Engine", 
            "InputCore",
            "ProceduralMeshComponent"  // Required for procedural mesh generation
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });
    }
}
