/**
 * @file InstallProvenanceQualityTests.cpp
 * @brief Whether the install-contract tests judge the plugin the editor loaded, or a different tree.
 *
 * BootstrapContractTests.cpp finds the scripts it reads from the compile-time __FILE__ of its own
 * source. That path is baked into the DLL. In the gate's compiled mode it is the harness the DLL was
 * compiled through, which is also where the editor loads the DLL from, so the two agree. In the
 * prebuilt mode, and on any machine that did not compile the shipped DLL, they do not: the tests read
 * whatever tree the baked path names, or nothing at all, while the editor runs the DLL of another.
 * A green result then says nothing about the tree the run is about.
 *
 * This test compares the two trees by what matters (the content of bootstrap.bat) and reports a
 * difference as a warning that names the open finding, so the suite stays green while the finding
 * is open and the report still carries the evidence. It never reads or writes anything else.
 */

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace FantasiaQualityProvenance
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** The open finding this test reports on. */
	const TCHAR* const FindingId = TEXT("F-080");

	/**
	 * The plugin directory BootstrapContractTests.cpp reads: its own __FILE__ is .../Source/FANTASIATests/Private/, so
	 * three levels up. This file is one level deeper, in Private/Quality/, hence four.
	 */
	FString ContractTestsRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::GetPath(UTF8_TO_TCHAR(__FILE__)), TEXT("../../../..")));
	}

	/** The plugin directory this DLL was loaded from: <plugin>/Binaries/Win64/<dll>. */
	FString LoadedPluginRoot()
	{
		const FString Dll = FModuleManager::Get().GetModuleFilename(FName(TEXT("FANTASIATests")));
		return Dll.IsEmpty() ? FString() : FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::GetPath(Dll), TEXT("../..")));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualityContractTestsTree, "FANTASIA.Quality.Install.ContractTestsReadTheLoadedPlugin", FantasiaQualityProvenance::Flags)
bool FQualityContractTestsTree::RunTest(const FString& Parameters)
{
	const FString Loaded = FantasiaQualityProvenance::LoadedPluginRoot();
	const FString Baked = FantasiaQualityProvenance::ContractTestsRoot();
	TestFalse(TEXT("the editor knows where it loaded FANTASIATests from"), Loaded.IsEmpty());
	if (Loaded.IsEmpty())
	{
		return true;
	}

	FString LoadedScript, BakedScript;
	const bool bLoadedReadable = FFileHelper::LoadFileToString(LoadedScript, *FPaths::Combine(Loaded, TEXT("bootstrap.bat")));
	const bool bBakedReadable = FFileHelper::LoadFileToString(BakedScript, *FPaths::Combine(Baked, TEXT("bootstrap.bat")));
	TestTrue(*FString::Printf(TEXT("bootstrap.bat is readable beside the loaded module (%s)"), *Loaded), bLoadedReadable);

	if (bLoadedReadable && bBakedReadable && LoadedScript == BakedScript)
	{
		return true;
	}
	AddWarning(FString::Printf(
		TEXT("%s: the install-contract tests read '%s' (%s), but the editor loaded FANTASIATests from '%s'. Their verdict is about the former."),
		FantasiaQualityProvenance::FindingId, *Baked,
		bBakedReadable ? TEXT("a different bootstrap.bat") : TEXT("unreadable, so they fail"), *Loaded));
	return true;
}
