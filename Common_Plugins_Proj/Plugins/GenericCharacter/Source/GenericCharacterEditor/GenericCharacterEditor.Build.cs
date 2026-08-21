// Copyright © 2026 Ethan Dawkins. All rights reserved.

using UnrealBuildTool;

public class GenericCharacterEditor : ModuleRules
{
    public GenericCharacterEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
				// ... add other public dependencies that you statically link with here ...
			}
        );


        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "UnrealEd",
                "PropertyEditor",
                "Slate",
                "SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
        );
    }
}