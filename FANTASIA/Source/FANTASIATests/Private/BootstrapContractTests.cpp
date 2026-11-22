/**
 * @file BootstrapContractTests.cpp
 * @brief Conformance tests for the install surface: what bootstrap.bat and bootstrap.sh
 *        name, and that the two scripts agree where they share a fact (C-install, C-build).
 *
 * The scripts ship inside the plugin directory, so every test fails when its subject
 * cannot be read: nothing here passes vacuously. Files are read once into a line array and
 * matched by short anchored tokens; nothing parses batch syntax.
 */

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace FantasiaBootstrap
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** The lines of one install script, and the assignment syntax of its shell. */
	struct FScript
	{
		TArray<FString> Lines;
		bool bBatch = false;

		/** Value assigned to Name at the left margin (`set NAME=v` or `NAME="v"`), unquoted; empty if absent. */
		FString Value(const FString& Name) const
		{
			const FString Prefix = (bBatch ? TEXT("set ") : TEXT("")) + Name + TEXT("=");
			for (const FString& Line : Lines)
			{
				if (Line.StartsWith(Prefix, ESearchCase::CaseSensitive))
				{
					FString Assigned = Line.Mid(Prefix.Len()).TrimStartAndEnd();
					Assigned.TrimQuotesInline();
					return Assigned;
				}
			}
			return FString();
		}

		/** True when some line contains Token. */
		bool Contains(const FString& Token) const
		{
			return Lines.ContainsByPredicate([&Token](const FString& Line) { return Line.Contains(Token, ESearchCase::CaseSensitive); });
		}
	};

	/** Index of the first line at or after From that contains Token, or INDEX_NONE. */
	int32 FindLine(const TArray<FString>& Lines, const FString& Token, int32 From = 0)
	{
		for (int32 Index = From; Index < Lines.Num(); ++Index)
		{
			if (Lines[Index].Contains(Token, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** Index of the first line that is exactly Label once trimmed (":parse_args"), or INDEX_NONE. */
	int32 FindLabel(const TArray<FString>& Lines, const FString& Label)
	{
		return Lines.IndexOfByPredicate([&Label](const FString& Line) { return Line.TrimStartAndEnd() == Label; });
	}

	/** True for a line that is a batch comment, which no assertion reads. */
	bool IsBatchComment(const FString& Line)
	{
		return Line.TrimStart().StartsWith(TEXT("REM"), ESearchCase::IgnoreCase);
	}

	/** The documented opt-out and opt-in flags of C-install, minus --enable-plugins (Windows only). */
	const TCHAR* const SharedFlags[] = { TEXT("--no-model"), TEXT("--no-gpu"), TEXT("--no-deps"), TEXT("--minimal"), TEXT("--force"),
		TEXT("--no-ace"), TEXT("--no-mark"), TEXT("--no-james"), TEXT("--no-claire"), TEXT("--no-characters"), TEXT("--no-nvidia"),
		TEXT("--ace"), TEXT("--mark"), TEXT("--james"), TEXT("--claire"), TEXT("--characters"), TEXT("--nvidia") };

	/** The lines of bootstrap.bat's argument loop (between :parse_args and :args_done); an absent loop is an error. */
	bool ParseLoop(FAutomationTestBase& Test, const FScript& Bat, TArray<FString>& OutLoop)
	{
		const int32 Begin = FindLabel(Bat.Lines, TEXT(":parse_args"));
		const int32 End = FindLabel(Bat.Lines, TEXT(":args_done"));
		if (Begin == INDEX_NONE || End == INDEX_NONE || End < Begin)
		{
			Test.AddError(TEXT("bootstrap.bat has no :parse_args ... :args_done argument loop."));
			return false;
		}
		OutLoop.Append(&Bat.Lines[Begin + 1], End - Begin - 1);
		return true;
	}

	/** Plugin directory of the running project (<Project>/Plugins/FANTASIA), resolved at run time, never from the compiled-in source path. */
	FString PluginDir()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("FANTASIA")));
	}

	/** Reads a file of the plugin directory; an unreadable or empty file is an error, not a skip. */
	bool Load(FAutomationTestBase& Test, const TCHAR* RelPath, TArray<FString>& OutLines)
	{
		const FString Path = FPaths::Combine(PluginDir(), RelPath);
		if (!FFileHelper::LoadFileToStringArray(OutLines, *Path) || OutLines.IsEmpty())
		{
			Test.AddError(FString::Printf(TEXT("Cannot read %s: the install surface cannot be checked."), *Path));
			return false;
		}
		return true;
	}

	/** Loads bootstrap.bat and bootstrap.sh; reports every unreadable one. */
	bool LoadScripts(FAutomationTestBase& Test, FScript& Bat, FScript& Sh)
	{
		Bat.bBatch = true;
		const bool bBat = Load(Test, TEXT("bootstrap.bat"), Bat.Lines);
		const bool bSh = Load(Test, TEXT("bootstrap.sh"), Sh.Lines);
		return bBat && bSh;
	}

	/** The asset-name infix the engine line implies, derived from FANTASIA.uplugin ("5.8.0" -> "ue58"). */
	bool DeriveEngineInfix(FAutomationTestBase& Test, FString& OutInfix)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(PluginDir(), TEXT("FANTASIA.uplugin"))))
		{
			Test.AddError(TEXT("Cannot read FANTASIA.uplugin: the engine line cannot be derived."));
			return false;
		}
		TSharedPtr<FJsonObject> Descriptor;
		FString Version;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Descriptor) || !Descriptor.IsValid()
			|| !Descriptor->TryGetStringField(TEXT("EngineVersion"), Version))
		{
			Test.AddError(TEXT("FANTASIA.uplugin has no readable EngineVersion."));
			return false;
		}
		TArray<FString> Parts;
		Version.ParseIntoArray(Parts, TEXT("."));
		if (Parts.Num() < 2 || !Parts[0].IsNumeric() || !Parts[1].IsNumeric())
		{
			Test.AddError(FString::Printf(TEXT("EngineVersion '%s' is not major.minor[.patch]."), *Version));
			return false;
		}
		OutInfix = TEXT("ue") + Parts[0] + Parts[1];
		return true;
	}
}

// ============================================================================
// C-install: one named release, identical in both scripts (ADR-0007)
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootstrapReleaseTag, "FANTASIA.Install.Bootstrap.ReleaseTagParity", FantasiaBootstrap::Flags)
bool FBootstrapReleaseTag::RunTest(const FString& Parameters)
{
	FantasiaBootstrap::FScript Bat, Sh;
	if (!FantasiaBootstrap::LoadScripts(*this, Bat, Sh))
	{
		return false;
	}
	const FString BatTag = Bat.Value(TEXT("RELEASE_TAG"));
	TestFalse(TEXT("bootstrap.bat sets RELEASE_TAG"), BatTag.IsEmpty());
	TestEqual(TEXT("bootstrap.sh fetches from the same release as bootstrap.bat"), Sh.Value(TEXT("RELEASE_TAG")), BatTag);
	return true;
}

// ============================================================================
// C-install: the assets both scripts share are named identically
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootstrapSharedAssets, "FANTASIA.Install.Bootstrap.SharedAssetParity", FantasiaBootstrap::Flags)
bool FBootstrapSharedAssets::RunTest(const FString& Parameters)
{
	FantasiaBootstrap::FScript Bat, Sh;
	if (!FantasiaBootstrap::LoadScripts(*this, Bat, Sh))
	{
		return false;
	}
	for (const FString& Name : { FString(TEXT("MODEL_ASSET")), FString(TEXT("DEPS_ASSET")) })
	{
		const FString BatAsset = Bat.Value(Name);
		TestFalse(*FString::Printf(TEXT("bootstrap.bat names %s"), *Name), BatAsset.IsEmpty());
		TestEqual(*FString::Printf(TEXT("%s is the same asset in both scripts"), *Name), Sh.Value(Name), BatAsset);
	}

	// The GPU stack is built per platform, so the two names differ by design. F-015 (open):
	// the Linux asset bootstrap.sh names has never been published. Recorded, not asserted.
	const FString BatGpu = Bat.Value(TEXT("GPU_ASSET"));
	const FString ShGpu = Sh.Value(TEXT("GPU_ASSET"));
	TestFalse(TEXT("bootstrap.bat names GPU_ASSET"), BatGpu.IsEmpty());
	TestFalse(TEXT("bootstrap.sh names GPU_ASSET"), ShGpu.IsEmpty());
	if (BatGpu != ShGpu)
	{
		AddInfo(FString::Printf(TEXT("F-015: GPU_ASSET differs by platform (bat '%s', sh '%s'); the Linux asset is unpublished."), *BatGpu, *ShGpu));
	}
	return true;
}

// ============================================================================
// C-install / C-build: NVIDIA asset names carry the engine line of FANTASIA.uplugin
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootstrapNvidiaInfix, "FANTASIA.Install.Bootstrap.NvidiaAssetsMatchEngineLine", FantasiaBootstrap::Flags)
bool FBootstrapNvidiaInfix::RunTest(const FString& Parameters)
{
	FantasiaBootstrap::FScript Bat, Sh;
	FString Infix;
	if (!FantasiaBootstrap::LoadScripts(*this, Bat, Sh) || !FantasiaBootstrap::DeriveEngineInfix(*this, Infix))
	{
		return false;
	}
	TestEqual(TEXT("ACE runtime asset"), Bat.Value(TEXT("ACE_ASSET")), FString::Printf(TEXT("nv-ace-reference-%s-full.tar.gz"), *Infix));
	TestEqual(TEXT("Mark asset"), Bat.Value(TEXT("MARK_ASSET")), FString::Printf(TEXT("nv-audio2face-mark-%s.tar.gz"), *Infix));
	TestEqual(TEXT("James asset"), Bat.Value(TEXT("JAMES_ASSET")), FString::Printf(TEXT("nv-audio2face-james-%s.tar.gz"), *Infix));
	TestEqual(TEXT("Claire asset"), Bat.Value(TEXT("CLAIRE_ASSET")), FString::Printf(TEXT("nv-audio2face-claire-%s.tar.gz"), *Infix));
	return true;
}

// ============================================================================
// C-install: NVIDIA availability, and the degrade branch that honours it
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootstrapNvidiaAvailability, "FANTASIA.Install.Bootstrap.NvidiaAvailability", FantasiaBootstrap::Flags)
bool FBootstrapNvidiaAvailability::RunTest(const FString& Parameters)
{
	FantasiaBootstrap::FScript Bat, Sh;
	if (!FantasiaBootstrap::LoadScripts(*this, Bat, Sh))
	{
		return false;
	}

	// bootstrap.bat declares whether the NVIDIA bundles are published for the line, and keeps
	// the branch that drops them (default run) or refuses a named flag (explicit run) when not.
	const FString Available = Bat.Value(TEXT("NVIDIA_AVAILABLE"));
	TestTrue(TEXT("bootstrap.bat declares NVIDIA_AVAILABLE as 0 or 1"), Available == TEXT("0") || Available == TEXT("1"));
	TestTrue(TEXT("bootstrap.bat tests NVIDIA_AVAILABLE before fetching"), Bat.Contains(TEXT("if \"%NVIDIA_AVAILABLE%\"==\"1\" goto :nvidia_ok")));
	TestTrue(TEXT("bootstrap.bat refuses a named NVIDIA flag when unavailable"), Bat.Contains(TEXT(":nvidia_refuse")));
	for (const TCHAR* Fetch : { TEXT("FETCH_ACE"), TEXT("FETCH_MARK"), TEXT("FETCH_JAMES"), TEXT("FETCH_CLAIRE") })
	{
		TestTrue(*FString::Printf(TEXT("bootstrap.bat drops %s when unavailable"), Fetch), Bat.Contains(FString::Printf(TEXT("set \"%s=0\""), Fetch)));
	}

	// bootstrap.sh has no NVIDIA_AVAILABLE: the bundles are Win64-only, so it names none and
	// refuses the flags. The two scripts agree as long as sh never names an NVIDIA asset.
	TestFalse(TEXT("bootstrap.sh names no NVIDIA ACE asset"), Sh.Contains(TEXT("nv-ace-")));
	TestFalse(TEXT("bootstrap.sh names no NVIDIA Audio2Face asset"), Sh.Contains(TEXT("nv-audio2face-")));
	TestTrue(TEXT("bootstrap.sh refuses a named NVIDIA flag"), Sh.Contains(TEXT("WANT_NVIDIA")));
	return true;
}

// ============================================================================
// C-install: every documented flag is handled; an unknown argument exits non-zero with usage
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootstrapFlagsHandled, "FANTASIA.Install.Bootstrap.DocumentedFlagsHandled", FantasiaBootstrap::Flags)
bool FBootstrapFlagsHandled::RunTest(const FString& Parameters)
{
	FantasiaBootstrap::FScript Bat, Sh;
	TArray<FString> Loop;
	if (!FantasiaBootstrap::LoadScripts(*this, Bat, Sh) || !FantasiaBootstrap::ParseLoop(*this, Bat, Loop))
	{
		return false;
	}
	for (const TCHAR* Flag : FantasiaBootstrap::SharedFlags)
	{
		const FString BatTest = FString::Printf(TEXT("\"%%~1\"==\"%s\""), Flag);
		TestTrue(*FString::Printf(TEXT("bootstrap.bat handles %s in its argument loop"), Flag), FantasiaBootstrap::FindLine(Loop, BatTest) != INDEX_NONE);
		const bool bShHandles = Sh.Contains(FString(Flag) + TEXT(")")) || Sh.Contains(FString(Flag) + TEXT("|"));
		TestTrue(*FString::Printf(TEXT("bootstrap.sh handles %s"), Flag), bShHandles);
	}

	// --enable-plugins edits a Windows .uproject through enable-plugins.ps1: bat only.
	TestTrue(TEXT("bootstrap.bat handles --enable-plugins in its argument loop"), FantasiaBootstrap::FindLine(Loop, TEXT("\"%~1\"==\"--enable-plugins\"")) != INDEX_NONE);
	if (!Sh.Contains(TEXT("--enable-plugins)")))
	{
		AddInfo(TEXT("bootstrap.sh does not handle --enable-plugins: it is Windows-only (enable-plugins.ps1)."));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootstrapUnknownArgument, "FANTASIA.Install.Bootstrap.UnknownArgumentExitsWithUsage", FantasiaBootstrap::Flags)
bool FBootstrapUnknownArgument::RunTest(const FString& Parameters)
{
	FantasiaBootstrap::FScript Bat, Sh;
	TArray<FString> Loop;
	if (!FantasiaBootstrap::LoadScripts(*this, Bat, Sh) || !FantasiaBootstrap::ParseLoop(*this, Bat, Loop))
	{
		return false;
	}
	// Reached only when no flag test matched: after the last one come the message, the usage, the exit.
	const int32 LastFlag = Loop.FindLastByPredicate([](const FString& Line) { return Line.Contains(TEXT("if /I \"%~1\"==")); });
	const int32 Unknown = FantasiaBootstrap::FindLine(Loop, TEXT("Unknown argument"), LastFlag + 1);
	const int32 Usage = FantasiaBootstrap::FindLine(Loop, TEXT("Usage:"), LastFlag + 1);
	const int32 Exit = FantasiaBootstrap::FindLine(Loop, TEXT("exit /b 1"), LastFlag + 1);
	TestTrue(TEXT("bootstrap.bat reports an unknown argument after the last flag test"), LastFlag != INDEX_NONE && Unknown != INDEX_NONE);
	TestTrue(TEXT("bootstrap.bat prints its usage"), Usage != INDEX_NONE);
	TestTrue(TEXT("bootstrap.bat exits non-zero for an unknown argument"), Exit != INDEX_NONE);

	const int32 ShDefault = FantasiaBootstrap::FindLine(Sh.Lines, TEXT("*) echo \"Unknown argument"));
	TestTrue(TEXT("bootstrap.sh's default case reports the argument, prints usage and exits 1"),
		ShDefault != INDEX_NONE && Sh.Lines[ShDefault].Contains(TEXT("Usage:")) && Sh.Lines[ShDefault].Contains(TEXT("exit 1")));
	return true;
}

// ============================================================================
// C-install: bootstrap.bat hygiene that reached users as defects
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootstrapScriptDir, "FANTASIA.Install.Bootstrap.ScriptDirCapturedBeforeArgLoop", FantasiaBootstrap::Flags)
bool FBootstrapScriptDir::RunTest(const FString& Parameters)
{
	FantasiaBootstrap::FScript Bat, Sh;
	if (!FantasiaBootstrap::LoadScripts(*this, Bat, Sh))
	{
		return false;
	}
	const int32 Loop = FantasiaBootstrap::FindLabel(Bat.Lines, TEXT(":parse_args"));
	const int32 Capture = FantasiaBootstrap::FindLine(Bat.Lines, TEXT("set SCRIPT_DIR=%~dp0"));
	TestTrue(TEXT("bootstrap.bat has an argument loop and captures SCRIPT_DIR from %~dp0"), Loop != INDEX_NONE && Capture != INDEX_NONE);
	TestTrue(TEXT("SCRIPT_DIR is captured before :parse_args, because shift shifts %0"), Capture < Loop);

	// After the loop %~dp0 is the working directory: nothing may read it there.
	int32 LateUse = INDEX_NONE;
	for (int32 Index = Loop + 1; Loop != INDEX_NONE && Index < Bat.Lines.Num() && LateUse == INDEX_NONE; ++Index)
	{
		if (!FantasiaBootstrap::IsBatchComment(Bat.Lines[Index]) && Bat.Lines[Index].Contains(TEXT("%~dp0")))
		{
			LateUse = Index;
		}
	}
	TestEqual(TEXT("no %~dp0 is read after the argument loop begins"), LateUse, static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootstrapParseLoopSets, "FANTASIA.Install.Bootstrap.ParseLoopSetsCarryNoTrailingSpace", FantasiaBootstrap::Flags)
bool FBootstrapParseLoopSets::RunTest(const FString& Parameters)
{
	FantasiaBootstrap::FScript Bat, Sh;
	TArray<FString> Loop;
	if (!FantasiaBootstrap::LoadScripts(*this, Bat, Sh) || !FantasiaBootstrap::ParseLoop(*this, Bat, Loop))
	{
		return false;
	}
	// The quoted form `set "NAME=value"` cannot capture the space before `&`; an unquoted one can.
	int32 Sets = 0;
	for (const FString& Line : Loop)
	{
		if (FantasiaBootstrap::IsBatchComment(Line))
		{
			continue;
		}
		TArray<FString> Tokens;
		Line.ParseIntoArray(Tokens, TEXT(" "));
		for (int32 Index = 0; Index < Tokens.Num(); ++Index)
		{
			if (Tokens[Index].Equals(TEXT("set"), ESearchCase::IgnoreCase))
			{
				++Sets;
				const bool bQuoted = Tokens.IsValidIndex(Index + 1) && Tokens[Index + 1].Len() >= 3
					&& Tokens[Index + 1].StartsWith(TEXT("\"")) && Tokens[Index + 1].EndsWith(TEXT("\""));
				TestTrue(*FString::Printf(TEXT("set in the argument loop is quoted: %s"), *Line.TrimStartAndEnd().Left(60)), bQuoted);
			}
		}
	}
	TestTrue(TEXT("the argument loop contains set assignments to check"), Sets > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootstrapTarPinned, "FANTASIA.Install.Bootstrap.TarPinnedToBundledBsdtar", FantasiaBootstrap::Flags)
bool FBootstrapTarPinned::RunTest(const FString& Parameters)
{
	FantasiaBootstrap::FScript Bat, Sh;
	if (!FantasiaBootstrap::LoadScripts(*this, Bat, Sh))
	{
		return false;
	}
	TestEqual(TEXT("TAR is the bsdtar bundled with Windows"), Bat.Value(TEXT("TAR")), FString(TEXT("%SystemRoot%\\System32\\tar.exe")));
	int32 Extracts = 0;
	for (const FString& Line : Bat.Lines)
	{
		if (FantasiaBootstrap::IsBatchComment(Line) || !Line.Contains(TEXT(" -xf ")))
		{
			continue;
		}
		++Extracts;
		TestTrue(*FString::Printf(TEXT("extraction goes through %%TAR%%, not PATH: %s"), *Line.TrimStartAndEnd().Left(60)), Line.TrimStart().StartsWith(TEXT("\"%TAR%\"")));
	}
	TestTrue(TEXT("bootstrap.bat extracts at least one archive"), Extracts > 0);
	return true;
}

// ============================================================================
// C-install: re-running is safe -- every fetch is guarded by a presence test and honours --force
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootstrapFetchGuards, "FANTASIA.Install.Bootstrap.EveryFetchGuardedAndHonoursForce", FantasiaBootstrap::Flags)
bool FBootstrapFetchGuards::RunTest(const FString& Parameters)
{
	FantasiaBootstrap::FScript Bat, Sh;
	if (!FantasiaBootstrap::LoadScripts(*this, Bat, Sh))
	{
		return false;
	}

	// bootstrap.bat: each curl sits in a :do_ section opened by `if "%FORCE%"=="0" if exist` or by
	// :nv_prepare, which sets NV_SKIP only when FORCE is 0 and the staged plugin is present.
	const int32 NvPrepare = FantasiaBootstrap::FindLabel(Bat.Lines, TEXT(":nv_prepare"));
	TestTrue(TEXT(":nv_prepare skips only when --force is absent"),
		NvPrepare != INDEX_NONE && FantasiaBootstrap::FindLine(Bat.Lines, TEXT("if \"%FORCE%\"==\"0\" set \"NV_SKIP=1\""), NvPrepare) != INDEX_NONE);
	int32 BatCurls = 0, BatAssets = 0;
	for (int32 Index = 0; Index < Bat.Lines.Num(); ++Index)
	{
		const FString Line = Bat.Lines[Index].TrimStart();
		BatAssets += (Line.StartsWith(TEXT("set ")) && Line.Contains(TEXT("_ASSET="))) ? 1 : 0;
		if (!Line.StartsWith(TEXT("curl ")))
		{
			continue;
		}
		++BatCurls;
		int32 Section = Index;
		while (Section > 0 && !Bat.Lines[Section].TrimStart().StartsWith(TEXT(":do_")))
		{
			--Section;
		}
		const TArray<FString> Guard(&Bat.Lines[Section], Index - Section);
		const bool bGuarded = FantasiaBootstrap::FindLine(Guard, TEXT("if \"%FORCE%\"==\"0\" if exist")) != INDEX_NONE
			|| (FantasiaBootstrap::FindLine(Guard, TEXT("call :nv_prepare")) != INDEX_NONE && FantasiaBootstrap::FindLine(Guard, TEXT("\"%NV_SKIP%\"==\"1\"")) != INDEX_NONE);
		TestTrue(*FString::Printf(TEXT("bootstrap.bat line %d: fetch is guarded by a presence test and --force"), Index + 1), bGuarded);
	}
	TestTrue(TEXT("every asset bootstrap.bat names has a fetch"), BatCurls > 0 && BatCurls == BatAssets);

	// bootstrap.sh: each curl sits in an `if [ "${FETCH_...}" = "1" ]` block that tests FORCE first.
	int32 ShCurls = 0, ShAssets = 0;
	for (int32 Index = 0; Index < Sh.Lines.Num(); ++Index)
	{
		const FString Line = Sh.Lines[Index].TrimStart();
		ShAssets += (Line.StartsWith(TEXT("MODEL_ASSET=")) || Line.StartsWith(TEXT("GPU_ASSET=")) || Line.StartsWith(TEXT("DEPS_ASSET="))) ? 1 : 0;
		if (!Line.Contains(TEXT("curl -L")))
		{
			continue;
		}
		++ShCurls;
		int32 Block = Index;
		while (Block > 0 && !Sh.Lines[Block].Contains(TEXT("if [ \"${FETCH_")))
		{
			--Block;
		}
		const TArray<FString> Guard(&Sh.Lines[Block], Index - Block);
		TestTrue(*FString::Printf(TEXT("bootstrap.sh line %d: fetch is guarded by a presence test and --force"), Index + 1),
			FantasiaBootstrap::FindLine(Guard, TEXT("\"${FORCE}\" = \"0\"")) != INDEX_NONE);
	}
	TestTrue(TEXT("every asset bootstrap.sh names has a fetch"), ShCurls > 0 && ShCurls == ShAssets);
	return true;
}

// ============================================================================
// C-install: the run ends in a summary
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootstrapSummary, "FANTASIA.Install.Bootstrap.RunEndsInSummary", FantasiaBootstrap::Flags)
bool FBootstrapSummary::RunTest(const FString& Parameters)
{
	FantasiaBootstrap::FScript Bat, Sh;
	if (!FantasiaBootstrap::LoadScripts(*this, Bat, Sh))
	{
		return false;
	}
	// bootstrap.bat: the :done section prints and then leaves with success, before any other label.
	const int32 Done = FantasiaBootstrap::FindLabel(Bat.Lines, TEXT(":done"));
	const int32 Exit = Done == INDEX_NONE ? INDEX_NONE : FantasiaBootstrap::FindLine(Bat.Lines, TEXT("exit /b 0"), Done);
	bool bPrints = false, bOtherLabel = false;
	for (int32 Index = Done + 1; Done != INDEX_NONE && Exit != INDEX_NONE && Index < Exit; ++Index)
	{
		const FString Line = Bat.Lines[Index].TrimStart();
		bPrints |= Line.StartsWith(TEXT("echo"));
		bOtherLabel |= Line.StartsWith(TEXT(":")) && !FantasiaBootstrap::IsBatchComment(Line);
	}
	TestTrue(TEXT("bootstrap.bat has a :done section that prints and exits 0"), Done != INDEX_NONE && Exit != INDEX_NONE && bPrints && !bOtherLabel);

	// bootstrap.sh: the last thing the script does is print.
	const int32 Last = Sh.Lines.FindLastByPredicate([](const FString& Line) { return !Line.TrimStartAndEnd().IsEmpty(); });
	TestTrue(TEXT("bootstrap.sh ends by printing"), Last != INDEX_NONE && Sh.Lines[Last].TrimStart().StartsWith(TEXT("echo")));
	return true;
}

// ============================================================================
// C-install: --enable-plugins writes .uproject.bak before touching the .uproject, always
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootstrapEnablePluginsBackup, "FANTASIA.Install.Bootstrap.EnablePluginsBacksUpFirst", FantasiaBootstrap::Flags)
bool FBootstrapEnablePluginsBackup::RunTest(const FString& Parameters)
{
	FantasiaBootstrap::FScript Bat, Sh;
	TArray<FString> Enable;
	if (!FantasiaBootstrap::LoadScripts(*this, Bat, Sh) || !FantasiaBootstrap::Load(*this, TEXT("enable-plugins.ps1"), Enable))
	{
		return false;
	}
	TestTrue(TEXT("bootstrap.bat hands --enable-plugins to enable-plugins.ps1"), Bat.Contains(TEXT("enable-plugins.ps1")));

	int32 Backup = INDEX_NONE;
	for (int32 Index = 0; Index < Enable.Num() && Backup == INDEX_NONE; ++Index)
	{
		Backup = (Enable[Index].Contains(TEXT("Copy-Item")) && Enable[Index].Contains(TEXT("$bak"))) ? Index : INDEX_NONE;
	}
	const int32 Write = FantasiaBootstrap::FindLine(Enable, TEXT("WriteAllText"));
	TestTrue(TEXT("enable-plugins.ps1 copies the .uproject to its .bak"), Backup != INDEX_NONE);
	TestTrue(TEXT("enable-plugins.ps1 writes the .uproject"), Write != INDEX_NONE);
	TestTrue(TEXT("the backup is written before the .uproject is"), Backup != INDEX_NONE && Backup < Write);
	return true;
}
