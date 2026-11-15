/**
 * @file SecretLeakageTests.cpp
 * @brief US-028 / F-008, tester pass: does a key leak by a route other than the asset?
 *
 * C-llm asserts "APIKey is never logged" and C-rest-tts asserts the same of Headers and Key.
 * Nobody had checked. Three independent checks, and the scanner's control:
 *   - Leak.OpenAILog / Leak.RESTTTSLog: drive the real request path with a fake key against
 *     a closed loopback port, capture every line written to the global log, and assert the
 *     key is absent while a positive control line proves the capture works. The only network
 *     traffic is a refused connection to 127.0.0.1:1.
 *   - Leak.SourceScan: scan the plugin's own source for a log statement that names a secret
 *     identifier, and for any write to the config (SaveConfig and friends), so a key typed in
 *     the editor can never be written into DefaultGame.ini behind the user's back. Every
 *     offender is an error. Leak.ScannerControl checks the scanner against known-bad and
 *     known-good snippets, so a clean scan cannot mean a blind one.
 *   - Coverage.SecretNamedProperties: a reflection sweep for secret-looking editable
 *     properties in FANTASIA classes that do not carry Config. The coders' table
 *     (GetSecretProperties) is hand-maintained; this finds the property that is not in it.
 *     Config is the criterion because it is what the engine exposes; it does not keep a value
 *     typed in the Details panel out of the level or Blueprint asset (F-031), and this sweep
 *     does not assert that it does.
 *
 * Crash.RESTTTSPostWithoutContentType documents F-030 and runs only with -QualityRunCrashRepro.
 *
 * Fake keys only. No file is written, and no config entry is flushed.
 */

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "OpenAICompatibleComponent.h"
#include "RESTTTSComponent.h"
#include "Quality/SecretTestSupport.h"

using namespace FantasiaQuality;

namespace
{
// -- Source scanner ------------------------------------------------------------------

/** Removes // and block comments, then blanks the inside of string literals (keeps the quotes). */
FString StripCommentsAndStrings(const FString& Source)
{
	FString Out;
	Out.Reserve(Source.Len());
	const int32 N = Source.Len();
	for (int32 i = 0; i < N;)
	{
		const TCHAR C = Source[i];
		if (C == TEXT('/') && i + 1 < N && Source[i + 1] == TEXT('/'))
		{
			while (i < N && Source[i] != TEXT('\n')) { ++i; }
		}
		else if (C == TEXT('/') && i + 1 < N && Source[i + 1] == TEXT('*'))
		{
			i += 2;
			while (i + 1 < N && !(Source[i] == TEXT('*') && Source[i + 1] == TEXT('/'))) { ++i; }
			i += 2;
		}
		else if (C == TEXT('"'))
		{
			Out.AppendChar(TEXT('"'));
			++i;
			while (i < N && Source[i] != TEXT('"'))
			{
				i += (Source[i] == TEXT('\\')) ? 2 : 1;
			}
			Out.AppendChar(TEXT('"'));
			++i;
		}
		else
		{
			Out.AppendChar(C);
			++i;
		}
	}
	return Out;
}

bool IsIdentChar(const TCHAR C)
{
	return FChar::IsAlnum(C) || C == TEXT('_');
}

/** True when Word occurs as a whole identifier. When bNotMember, a preceding '.' or '>' disqualifies it. */
bool ContainsWord(const FString& Text, const TCHAR* Word, const bool bNotMember = false)
{
	const int32 Len = FCString::Strlen(Word);
	int32 From = 0;
	while (true)
	{
		const int32 At = Text.Find(Word, ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
		if (At == INDEX_NONE)
		{
			return false;
		}
		const bool bLeftOk = At == 0 || !IsIdentChar(Text[At - 1]);
		const bool bRightOk = At + Len >= Text.Len() || !IsIdentChar(Text[At + Len]);
		const bool bMember = At > 0 && (Text[At - 1] == TEXT('.') || Text[At - 1] == TEXT('>'));
		if (bLeftOk && bRightOk && !(bNotMember && bMember))
		{
			return true;
		}
		From = At + Len;
	}
}

/** True when a statement names a secret identifier. A bare Key counts; a member such as Pair.Key does not. */
bool NamesSecret(const FString& Statement)
{
	static const TCHAR* SecretWords[] = {TEXT("APIKey"), TEXT("LLMAPIKey"), TEXT("password"), TEXT("Neo4jPassword"),
		TEXT("ElevenLabsKey"), TEXT("Authorization"), TEXT("Bearer"), TEXT("Headers"), TEXT("BodyTemplate"), TEXT("ExportText")};

	if (ContainsWord(Statement, TEXT("Key"), /*bNotMember=*/true))
	{
		return true;
	}
	for (const TCHAR* Word : SecretWords)
	{
		if (ContainsWord(Statement, Word))
		{
			return true;
		}
	}
	return false;
}

/** Each logging statement (UE_LOG, UE_LOGFMT, Logf, on-screen messages) that names a secret. Takes stripped source. */
TArray<FString> FindLoggedSecrets(const FString& Source)
{
	static const TCHAR* Markers[] = {TEXT("UE_LOG"), TEXT("Logf("), TEXT("AddOnScreenDebugMessage(")};

	TArray<FString> Hits;
	for (const TCHAR* Marker : Markers)
	{
		int32 From = 0;
		while (true)
		{
			const int32 At = Source.Find(Marker, ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
			if (At == INDEX_NONE)
			{
				break;
			}
			const int32 End = Source.Find(TEXT(";"), ESearchCase::CaseSensitive, ESearchDir::FromStart, At);
			const FString Statement = Source.Mid(At, End == INDEX_NONE ? MAX_int32 : End - At);
			if (NamesSecret(Statement))
			{
				Hits.Add(Statement.Left(120));
			}
			From = At + 1;
		}
	}
	return Hits;
}

/** Each call that writes configuration back to an ini. Takes stripped source. */
TArray<FString> FindConfigWrites(const FString& Source)
{
	static const TCHAR* Writers[] = {TEXT("SaveConfig("), TEXT("UpdateDefaultConfigFile("), TEXT("TryUpdateDefaultConfigFile("),
		TEXT("UpdateSinglePropertyInConfigFile("), TEXT("GConfig->Set"), TEXT("GConfig->Flush"), TEXT("GConfig->Add"), TEXT("GConfig->Remove")};
	TArray<FString> Hits;
	for (const TCHAR* Writer : Writers)
	{
		if (Source.Contains(Writer, ESearchCase::CaseSensitive))
		{
			Hits.Add(Writer);
		}
	}
	return Hits;
}

/** Both findings for one file, with its comments and string contents stripped once. */
struct FSourceFindings
{
	TArray<FString> LoggedSecrets;
	TArray<FString> ConfigWrites;
};

FSourceFindings ScanSource(const FString& RawSource)
{
	const FString Source = StripCommentsAndStrings(RawSource);
	return {FindLoggedSecrets(Source), FindConfigWrites(Source)};
}

// -- Reflection sweep ----------------------------------------------------------------

bool LooksSecret(const FString& Name)
{
	const FString L = Name.ToLower();
	static const TCHAR* Words[] = {TEXT("apikey"), TEXT("api_key"), TEXT("password"), TEXT("passwd"), TEXT("secret"),
		TEXT("token"), TEXT("credential"), TEXT("bearer"), TEXT("authorization"), TEXT("header")};
	for (const TCHAR* W : Words)
	{
		if (L.Contains(W))
		{
			return true;
		}
	}
	return L.EndsWith(TEXT("key"));
}

/** Text-carrying property types; a map counts because Headers is one. */
bool IsStringLike(const FProperty* P)
{
	return CastField<FStrProperty>(P) || CastField<FNameProperty>(P) || CastField<FTextProperty>(P) || CastField<FMapProperty>(P);
}

/** True when the property, or a string field of the struct it holds (one level deep), is named like a secret. */
bool HoldsSecret(const FProperty* P)
{
	if (const FStructProperty* S = CastField<FStructProperty>(P))
	{
		for (TFieldIterator<FProperty> It(S->Struct); It; ++It)
		{
			if (IsStringLike(*It) && LooksSecret(It->GetName()))
			{
				return true;
			}
		}
		return false;
	}
	return IsStringLike(P) && LooksSecret(P->GetName());
}

/** The sweep's offence: an editable, secret-named property with no Config flag, so no ini source for its value. */
bool IsEditableSecretWithoutConfig(const FProperty* P)
{
	return P->HasAnyPropertyFlags(CPF_Edit) && !P->HasAnyPropertyFlags(CPF_Config) && HoldsSecret(P);
}
} // namespace

// -- Scanner control: it must catch what it claims to catch -----------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretScannerControl, "FANTASIA.Quality.Secrets.Leak.ScannerControl", FantasiaQuality::TestFlags)
bool FSecretScannerControl::RunTest(const FString& Parameters)
{
	const auto LoggedSecrets = [](const TCHAR* Snippet) { return ScanSource(Snippet).LoggedSecrets.Num(); };
	const auto ConfigWrites = [](const TCHAR* Snippet) { return ScanSource(Snippet).ConfigWrites.Num(); };

	TestEqual(TEXT("flags a logged APIKey"),
		LoggedSecrets(TEXT("UE_LOG(LogTemp, Log, TEXT(\"k=%s\"), *Config.APIKey);")), 1);
	TestEqual(TEXT("flags a logged header map"),
		LoggedSecrets(TEXT("UE_LOG(LogTemp, Log, TEXT(\"h\"),\n   *Headers.FindRef(TEXT(\"x\")));")), 1);
	TestEqual(TEXT("flags a logged Key member"),
		LoggedSecrets(TEXT("UE_LOG(LogTemp, Log, TEXT(\"%s\"), *Key);")), 1);
	TestEqual(TEXT("flags GEngine on-screen output"),
		LoggedSecrets(TEXT("GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, password);")), 1);
	TestEqual(TEXT("ignores a secret word inside the format string"),
		LoggedSecrets(TEXT("UE_LOG(LogTemp, Error, TEXT(\"API key is required but empty. password APIKey\"));")), 0);
	TestEqual(TEXT("ignores a map pair's .Key"),
		LoggedSecrets(TEXT("UE_LOG(LogTemp, Log, TEXT(\"%s\"), *Pair.Key);")), 0);
	TestEqual(TEXT("ignores a commented-out log"),
		LoggedSecrets(TEXT("// UE_LOG(LogTemp, Log, TEXT(\"%s\"), *Config.APIKey);\nint x;")), 0);
	TestEqual(TEXT("ignores an unrelated log"),
		LoggedSecrets(TEXT("UE_LOG(LogTemp, Log, TEXT(\"hello %d\"), 1);")), 0);

	TestEqual(TEXT("flags SaveConfig"), ConfigWrites(TEXT("SaveConfig();")), 1);
	TestEqual(TEXT("flags UpdateDefaultConfigFile"), ConfigWrites(TEXT("UpdateDefaultConfigFile();")), 1);
	TestEqual(TEXT("flags GConfig->SetString"), ConfigWrites(TEXT("GConfig->SetString(a,b,c,d);")), 1);
	TestEqual(TEXT("ignores a comment that mentions SaveConfig"), ConfigWrites(TEXT("// never SaveConfig()\nint x;")), 0);
	TestEqual(TEXT("ignores GConfig reads"), ConfigWrites(TEXT("GConfig->GetString(a,b,c,d);")), 0);
	return true;
}

// -- Source scan over the real plugin source -------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretSourceScan, "FANTASIA.Quality.Secrets.Leak.SourceScan", FantasiaQuality::TestFlags)
bool FSecretSourceScan::RunTest(const FString& Parameters)
{
	// The tests module has no dependency on Projects, so the plugin is located from this file's
	// own path: .../Source/FANTASIATests/Private/Quality/ -> .../Source/FANTASIA and the sibling plugin.
	const FString Here = FPaths::GetPath(UTF8_TO_TCHAR(__FILE__));
	TArray<FString> Roots;
	Roots.Add(FPaths::ConvertRelativePathToFull(FPaths::Combine(Here, TEXT("../../../FANTASIA"))));
	Roots.Add(FPaths::ConvertRelativePathToFull(FPaths::Combine(Here, TEXT("../../../../../FANTASIAACE/Source/FANTASIAACE"))));

	TArray<FString> Files;
	for (const FString& Root : Roots)
	{
		if (!IFileManager::Get().DirectoryExists(*Root))
		{
			AddInfo(FString::Printf(TEXT("Source root not reachable from this build, not scanned: %s"), *Root));
			continue;
		}
		TArray<FString> Found;
		IFileManager::Get().FindFilesRecursive(Found, *Root, TEXT("*.cpp"), true, false);
		IFileManager::Get().FindFilesRecursive(Found, *Root, TEXT("*.h"), true, false, /*bClearFileNames=*/false);
		Files.Append(Found);
	}

	if (Files.Num() < 10)
	{
		AddWarning(FString::Printf(TEXT("Plugin source not found beside the binaries (%d files); the source scan did not run."), Files.Num()));
		return true;
	}

	int32 LogStatementsSeen = 0;
	for (const FString& File : Files)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *File))
		{
			AddError(FString::Printf(TEXT("Could not read %s"), *File));
			continue;
		}
		const FString Name = FPaths::GetCleanFilename(File);
		LogStatementsSeen += Text.Contains(TEXT("UE_LOG")) ? 1 : 0;

		const FSourceFindings Findings = ScanSource(Text);
		for (const FString& Hit : Findings.LoggedSecrets)
		{
			AddError(FString::Printf(TEXT("%s logs a secret-named value: %s"), *Name, *Hit));
		}
		for (const FString& Hit : Findings.ConfigWrites)
		{
			AddError(FString::Printf(TEXT("%s writes configuration back to an ini (%s): a key typed in the editor could land in DefaultGame.ini"), *Name, *Hit));
		}
	}
	// Vacuity guard: a scan that saw no logging at all did not look at the real tree.
	TestTrue(TEXT("the scan reached files that log"), LogStatementsSeen > 5);
	AddInfo(FString::Printf(TEXT("Scanned %d source files."), Files.Num()));
	return true;
}

// -- Runtime log capture: OpenAI-compatible request path -------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretLeakOpenAILog, "FANTASIA.Quality.Secrets.Leak.OpenAILog", FantasiaQuality::TestFlags)
bool FSecretLeakOpenAILog::RunTest(const FString& Parameters)
{
	AddExpectedErrorPlain(TEXT("[OpenAICompatible]"), EAutomationExpectedErrorFlags::Contains, -1);

	const TSharedRef<FLogCapture> Capture = MakeShared<FLogCapture>();
	const TSharedRef<TStrongObjectPtr<UOpenAICompatibleComponent>> Component =
		MakeShared<TStrongObjectPtr<UOpenAICompatibleComponent>>(NewTransient<UOpenAICompatibleComponent>());

	UOpenAICompatibleComponent* C = Component->Get();
	C->Config.URL = TEXT("http://127.0.0.1:1/v1/chat/completions");
	C->Config.APIKey = FakeKey();
	C->Config.bRequiresAuth = true;
	C->Config.TimeoutSeconds = 5;

	FChatTurn Turn;
	Turn.role = GPTRoleType::USER;
	Turn.content = TEXT("hello");
	const TArray<FChatTurn> Messages = {Turn};

	// Two requests, one per code path: non-streaming and streaming. Each ends in a logged failure.
	C->GetChatCompletion(Messages, false, FString());
	C->GetChatCompletion(Messages, true, FString());

	const FString Completion = TEXT("[OpenAICompatible] Request failed");
	AddCommand(new FPollLatentCommand([Capture, Completion]() { return Capture->CountContaining(Completion) >= 2; }, 30.0));
	AddCommand(new FRunLatentCommand([this, Capture, Component, Completion]()
	{
		TestTrue(TEXT("positive control: both requests ended in a captured failure line"), Capture->CountContaining(Completion) >= 2);
		TestFalse(TEXT("the API key appears in no log line"), Capture->Contains(FakeKey()));
		TestFalse(TEXT("the Bearer header appears in no log line"), Capture->Contains(TEXT("Bearer")));
		Component->Reset();
	}));
	return true;
}

// -- Runtime log capture: generic REST TTS request path --------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretLeakRESTTTSLog, "FANTASIA.Quality.Secrets.Leak.RESTTTSLog", FantasiaQuality::TestFlags)
bool FSecretLeakRESTTTSLog::RunTest(const FString& Parameters)
{
	AddExpectedErrorPlain(TEXT("TTS request"), EAutomationExpectedErrorFlags::Contains, -1);

	const TSharedRef<FLogCapture> Capture = MakeShared<FLogCapture>();
	const TSharedRef<TStrongObjectPtr<URESTTTSComponent>> Component =
		MakeShared<TStrongObjectPtr<URESTTTSComponent>>(NewTransient<URESTTTSComponent>());

	URESTTTSComponent* C = Component->Get();
	C->URL = TEXT("http://127.0.0.1:1/tts");
	C->BodyTemplate = TEXT("{\"text\":\"{text}\"}");
	// Content-Type is supplied on purpose: without it the engine's curl layer asserts on a POST
	// body (see Crash.RESTTTSPostWithoutContentType), which would end this test's process.
	C->Headers.Add(TEXT("Content-Type"), TEXT("application/json"));
	C->Headers.Add(TEXT("xi-api-key"), FakeKey());
	C->Headers.Add(TEXT("Authorization"), TEXT("Bearer ") + FakeKey());

	C->TTSSynthesize(TEXT("hello"), TEXT("quality-1"), false);

	const FString Completion = TEXT("failed: connection to the endpoint failed");
	AddCommand(new FPollLatentCommand([Capture, Completion]() { return Capture->Contains(Completion); }, 30.0));
	AddCommand(new FRunLatentCommand([this, Capture, Component, Completion]()
	{
		TestTrue(TEXT("positive control: the request ended in a captured failure line"), Capture->Contains(Completion));
		TestFalse(TEXT("the xi-api-key value appears in no log line"), Capture->Contains(FakeKey()));
		Component->Reset();
	}));
	return true;
}

// -- Repro for a process-ending defect, run only on request ------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretCrashRESTTTSNoContentType, "FANTASIA.Quality.Secrets.Crash.RESTTTSPostWithoutContentType", FantasiaQuality::TestFlags)
bool FSecretCrashRESTTTSNoContentType::RunTest(const FString& Parameters)
{
	// F-030. The generic REST component sends a POST whenever BodyTemplate is set, and sets only the headers
	// the user configured. A user who follows the Headers tooltip and configures just the key
	// (`xi-api-key`) sends a POST body with no Content-Type, and the engine's curl layer asserts
	// ("!GetHeader(Content-Type).IsEmpty() || RequestPayload->IsURLEncoded()", CurlHttp.cpp) on the
	// HTTP thread, which ends the process in Development and Editor builds. Opt in with
	// -QualityRunCrashRepro on the editor command line; the gate never passes it.
	if (!FParse::Param(FCommandLine::Get(), TEXT("QualityRunCrashRepro")))
	{
		AddInfo(TEXT("Skipped: pass -QualityRunCrashRepro to reproduce the crash this test documents."));
		return true;
	}

	const TSharedRef<TStrongObjectPtr<URESTTTSComponent>> Component =
		MakeShared<TStrongObjectPtr<URESTTTSComponent>>(NewTransient<URESTTTSComponent>());
	URESTTTSComponent* C = Component->Get();
	C->URL = TEXT("http://127.0.0.1:1/tts");
	C->BodyTemplate = TEXT("{\"text\":\"{text}\"}");
	C->Headers.Add(TEXT("xi-api-key"), FakeKey());
	C->TTSSynthesize(TEXT("hello"), TEXT("quality-crash"), false);
	AddCommand(new FPollLatentCommand([]() { return false; }, 15.0));
	return true;
}

// -- A secret-named property that is not Config ---------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretCoverageSweep, "FANTASIA.Quality.Secrets.Coverage.SecretNamedProperties", FantasiaQuality::TestFlags)
bool FSecretCoverageSweep::RunTest(const FString& Parameters)
{
	// Covered: the properties each class declares itself (inherited ones are covered through their
	// declaring class), for every class of a module whose name begins FANTASIA (the plugin,
	// FANTASIAACE, and any other). Every offender is an error.
	TSet<FString> Offenders;
	int32 ClassesSeen = 0;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		const UClass* Class = *It;
		if (!Class->GetOutermost()->GetName().StartsWith(TEXT("/Script/FANTASIA")))
		{
			continue;
		}
		++ClassesSeen;
		for (TFieldIterator<FProperty> P(Class, EFieldIteratorFlags::ExcludeSuper); P; ++P)
		{
			if (IsEditableSecretWithoutConfig(*P))
			{
				Offenders.Add(FString::Printf(TEXT("%s.%s"), *Class->GetName(), *P->GetName()));
			}
		}
	}
	// Vacuity guard: a sweep that saw almost no classes did not look at the plugin.
	TestTrue(TEXT("the sweep reached FANTASIA classes"), ClassesSeen > 10);

	for (const FString& O : Offenders)
	{
		AddError(FString::Printf(TEXT("Secret-looking editable property without Config: %s"), *O));
	}
	return true;
}
