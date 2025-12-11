// Copyright Epic Games, Inc. All Rights Reserved.

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
			"RHI",
			
			// Mesh generation and conversion (for instanced renderer)
			"MeshDescription",
			"StaticMeshDescription"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
