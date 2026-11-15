using UnrealBuildTool;
using System.IO;

public class FANTASIATests : ModuleRules
{
    public FANTASIATests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        CppStandard = CppStandardVersion.Cpp20;
        bEnableExceptions = true;
        bUseRTTI = false;
        bUseUnity = false;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Json",
            "WebSockets",
            "FANTASIA"
        });

        // OpenSSL, so that tests can assert SSL_CTX verification state directly (F-020).
        // Engine module, never the plugin's bundled 1.1.1t path (F-044).
        AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");

        // kdepp header-only library (needed for MathUtilities tests)
        string ThirdParty = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../ThirdParty/"));
        PublicIncludePaths.Add(Path.Combine(ThirdParty, "kdepp"));
    }
}
