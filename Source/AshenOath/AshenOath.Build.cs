using UnrealBuildTool;

public class AshenOath : ModuleRules
{
	public AshenOath(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"EnhancedInput",
				"GameplayAbilities",
				"GameplayTags"
			}
		);

		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"GameplayTasks",
				"StateTreeModule",
				"GameplayStateTreeModule"
			}
		);

	}
}
