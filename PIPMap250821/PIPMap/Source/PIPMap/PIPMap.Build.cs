// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class PIPMap : ModuleRules
{
	public PIPMap(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Json" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "UnrealEd",           // 에디터 기능 전반 (GEditor 등)
                    "EditorSubsystem"     // 에디터 서브시스템
                }
            );
        }
        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
