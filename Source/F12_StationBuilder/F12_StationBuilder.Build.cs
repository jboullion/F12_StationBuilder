// F12_StationBuilder.Build.cs
// Build configuration for the F12 Station Builder project
// Updated with Space Environment dependencies

using UnrealBuildTool;

public class F12_StationBuilder : ModuleRules
{
    public F12_StationBuilder(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
    
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", 
            "CoreUObject", 
            "Engine", 
            "InputCore", 
            "EnhancedInput",
            
            // Procedural mesh for F12 modules
            "ProceduralMeshComponent",
            
            // UI/HUD
            "UMG",
            "Slate",
            "SlateCore",
            
            // Rendering for post-process and materials
            "RenderCore",
            "RHI"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });
    }
}
