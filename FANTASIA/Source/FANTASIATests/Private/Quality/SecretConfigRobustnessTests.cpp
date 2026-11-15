/**
 * @file SecretConfigRobustnessTests.cpp
 * @brief US-028 / F-008, tester pass: what the coders' SecretConfigTests do not establish.
 *
 * The coder's tests prove the Config flag is set on every property in their table and that two
 * of them round-trip. These tests go after the rest:
 *   - the other four properties actually load, not merely carry the flag;
 *   - every field of FOpenAICompatibleConfig loads, not only APIKey;
 *   - real ini text syntax (a hand-written DefaultGame.ini line), not only GConfig->SetString;
 *   - hostile and malformed values: embedded quotes, '=', ';', '#', truncation, 1 MiB;
 *   - which ini section a derived class reads (what the tooltip does not say);
 *   - what the Config flag does NOT do: stop a typed key being written into an asset;
 *   - ApplyPreset, which discards a key loaded from the ini.
 *
 * These are characterisation tests. Where UE or the plugin does something surprising the
 * test pins it as it is and names the finding, so a later change shows up as a failure.
 * Fake keys only; the in-memory ini is restored after each test and never flushed.
 */

#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/UnrealType.h"
#include "ElevenLabsTTSComponent.h"
#include "Neo4jComponent.h"
#include "OpenAICompatibleComponent.h"
#include "RESTTTSComponent.h"
#include "Quality/SecretTestSupport.h"

using namespace FantasiaQuality;

namespace
{
FString LLMSection()
{
	return UOpenAICompatibleComponent::StaticClass()->GetPathName();
}

/** The text SaveConfig would write for a property value: what a user's ini line holds. */
FString ExportForIni(const UObject* Owner, const FName PropertyName)
{
	const FProperty* Property = Owner->GetClass()->FindPropertyByName(PropertyName);
	FString Out;
	if (Property)
	{
		Property->ExportText_InContainer(0, Out, Owner, Owner, const_cast<UObject*>(Owner), PPF_SerializedAsImportText);
	}
	return Out;
}

/** RAII, here and below: lets a test reload a class default object's config, then restores what it held. */
struct FScopedLLMDefaults
{
	UOpenAICompatibleComponent* CDO = GetMutableDefault<UOpenAICompatibleComponent>();
	FOpenAICompatibleConfig Saved = CDO->Config;
	~FScopedLLMDefaults() { CDO->Config = Saved; }
};

struct FScopedRESTDefaults
{
	URESTTTSComponent* CDO = GetMutableDefault<URESTTTSComponent>();
	TMap<FString, FString> Saved = CDO->Headers;
	~FScopedRESTDefaults() { CDO->Headers = Saved; }
};
} // namespace

// -- The other four properties actually load ---------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretLoadRemainingFour, "FANTASIA.Quality.Secrets.Load.RemainingFourProperties", FantasiaQuality::TestFlags)
bool FSecretLoadRemainingFour::RunTest(const FString& Parameters)
{
	// ElevenLabs Key, FString.
	{
		FScopedGameConfigEntry E(UElevenLabsTTSComponent::StaticClass(), TEXT("Key"), FakeKey());
		UElevenLabsTTSComponent* C = NewTransient<UElevenLabsTTSComponent>();
		TestTrue(TEXT("Key starts without the fake key"), C->Key != FakeKey());
		C->LoadConfig();
		TestEqual(TEXT("UElevenLabsTTSComponent::Key loaded from DefaultGame.ini"), C->Key, FakeKey());
	}

	// Neo4j password, FString.
	{
		FScopedGameConfigEntry E(UNeo4jComponent::StaticClass(), TEXT("password"), FakeKey());
		UNeo4jComponent* C = NewTransient<UNeo4jComponent>();
		TestTrue(TEXT("password starts without the fake key"), C->password != FakeKey());
		C->LoadConfig();
		TestEqual(TEXT("UNeo4jComponent::password loaded from DefaultGame.ini"), C->password, FakeKey());
	}

	// REST URL and BodyTemplate, FString.
	{
		const FString WantURL = TEXT("http://127.0.0.1:1/tts?k=") + FakeKey();
		const FString WantBody = TEXT("{\"k\":\"") + FakeKey() + TEXT("\"}");
		FScopedGameConfigEntry EU(URESTTTSComponent::StaticClass(), TEXT("URL"), WantURL);
		FScopedGameConfigEntry EB(URESTTTSComponent::StaticClass(), TEXT("BodyTemplate"), WantBody);
		URESTTTSComponent* C = NewTransient<URESTTTSComponent>();
		C->LoadConfig();
		TestEqual(TEXT("URESTTTSComponent::URL loaded"), C->URL, WantURL);
		TestEqual(TEXT("URESTTTSComponent::BodyTemplate loaded"), C->BodyTemplate, WantBody);
	}
	return true;
}

// -- Every field of the struct loads, not only APIKey ------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretLoadEveryStructField, "FANTASIA.Quality.Secrets.Load.EveryStructField", FantasiaQuality::TestFlags)
bool FSecretLoadEveryStructField::RunTest(const FString& Parameters)
{
	{
		FScopedGameConfigEntry E(UOpenAICompatibleComponent::StaticClass(), TEXT("Config"),
			FString::Printf(TEXT("(URL=\"http://127.0.0.1:1/v1\",APIKey=\"%s\",DefaultModel=\"m-test\",TimeoutSeconds=7,bRequiresAuth=False)"), *FakeKey()));
		UOpenAICompatibleComponent* C = NewTransient<UOpenAICompatibleComponent>();
		C->LoadConfig();
		TestEqual(TEXT("URL"), C->Config.URL, FString(TEXT("http://127.0.0.1:1/v1")));
		TestEqual(TEXT("APIKey"), C->Config.APIKey, FakeKey());
		TestEqual(TEXT("DefaultModel"), C->Config.DefaultModel, FString(TEXT("m-test")));
		TestEqual(TEXT("TimeoutSeconds"), C->Config.TimeoutSeconds, 7);
		TestFalse(TEXT("bRequiresAuth"), C->Config.bRequiresAuth);
	}

	// A partial literal changes only the fields it names; the rest keep their defaults.
	{
		FScopedGameConfigEntry E(UOpenAICompatibleComponent::StaticClass(), TEXT("Config"),
			FString::Printf(TEXT("(APIKey=\"%s\")"), *FakeKey()));
		UOpenAICompatibleComponent* D = NewTransient<UOpenAICompatibleComponent>();
		const FOpenAICompatibleConfig Defaults = D->Config;
		D->LoadConfig();
		TestEqual(TEXT("partial literal sets APIKey"), D->Config.APIKey, FakeKey());
		TestEqual(TEXT("partial literal leaves URL"), D->Config.URL, Defaults.URL);
		TestEqual(TEXT("partial literal leaves DefaultModel"), D->Config.DefaultModel, Defaults.DefaultModel);
		TestEqual(TEXT("partial literal leaves TimeoutSeconds"), D->Config.TimeoutSeconds, Defaults.TimeoutSeconds);
		TestEqual(TEXT("partial literal leaves bRequiresAuth"), D->Config.bRequiresAuth, Defaults.bRequiresAuth);
	}
	return true;
}

// -- Real ini text, parsed by the engine's ini reader ------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretLoadIniTextSyntax, "FANTASIA.Quality.Secrets.Load.IniTextSyntax", FantasiaQuality::TestFlags)
bool FSecretLoadIniTextSyntax::RunTest(const FString& Parameters)
{
	struct FCase
	{
		const TCHAR* Name;
		const TCHAR* IniKeyLiteral; // what the user types between the quotes of APIKey="..."
		const TCHAR* Expected;      // the key the component should end up holding
	};
	const FCase Cases[] = {
		{TEXT("equals sign"), TEXT("sk-a=b"), TEXT("sk-a=b")},
		{TEXT("semicolon"), TEXT("sk;semi"), TEXT("sk;semi")},
		{TEXT("hash"), TEXT("sk#hash"), TEXT("sk#hash")},
		{TEXT("double slash"), TEXT("sk//slash"), TEXT("sk//slash")},
		{TEXT("comma and parens"), TEXT("sk,a)b(c"), TEXT("sk,a)b(c")},
		{TEXT("inner spaces"), TEXT("sk a  b"), TEXT("sk a  b")},
		{TEXT("escaped quote"), TEXT("sk\\\"q"), TEXT("sk\"q")},
		{TEXT("escaped backslash"), TEXT("sk\\\\b"), TEXT("sk\\b")},
	};

	for (const FCase& Case : Cases)
	{
		const FString Line = FString::Printf(TEXT("Config=(URL=\"http://127.0.0.1:1/v1\",APIKey=\"%s\")   "), Case.IniKeyLiteral);
		const FString Text = FString::Printf(TEXT("[%s]\r\n%s\r\n"), *LLMSection(), *Line);

		FConfigFile Parsed;
		Parsed.CombineFromBuffer(Text, TEXT("QualityTest.ini"));
		FString Value;
		if (!TestTrue(*FString::Printf(TEXT("%s: the ini reader found the Config key"), Case.Name),
			Parsed.GetString(*LLMSection(), TEXT("Config"), Value)))
		{
			continue;
		}

		FScopedGameConfigEntry E(UOpenAICompatibleComponent::StaticClass(), TEXT("Config"), Value);
		UOpenAICompatibleComponent* C = NewTransient<UOpenAICompatibleComponent>();
		C->LoadConfig();
		TestEqual(*FString::Printf(TEXT("%s: APIKey"), Case.Name), C->Config.APIKey, FString(Case.Expected));
		TestEqual(*FString::Printf(TEXT("%s: URL survives the same line"), Case.Name), C->Config.URL, FString(TEXT("http://127.0.0.1:1/v1")));
	}

	// A Headers map written as one ini line.
	{
		const FString Section = URESTTTSComponent::StaticClass()->GetPathName();
		const FString Text = FString::Printf(TEXT("[%s]\r\nHeaders=((\"xi-api-key\",\"%s\"),(\"Authorization\",\"Bearer a=b;c\"))\r\n"),
			*Section, *FakeKey());
		FConfigFile Parsed;
		Parsed.CombineFromBuffer(Text, TEXT("QualityTest.ini"));
		FString Value;
		if (TestTrue(TEXT("Headers: the ini reader found the key"), Parsed.GetString(*Section, TEXT("Headers"), Value)))
		{
			FScopedGameConfigEntry E(URESTTTSComponent::StaticClass(), TEXT("Headers"), Value);
			URESTTTSComponent* C = NewTransient<URESTTTSComponent>();
			C->LoadConfig();
			TestEqual(TEXT("Headers: two entries"), C->Headers.Num(), 2);
			const FString* Key = C->Headers.Find(TEXT("xi-api-key"));
			TestTrue(TEXT("Headers: xi-api-key present"), Key && *Key == FakeKey());
			const FString* Auth = C->Headers.Find(TEXT("Authorization"));
			TestTrue(TEXT("Headers: Authorization value keeps '=' and ';'"), Auth && *Auth == TEXT("Bearer a=b;c"));
		}
	}
	return true;
}

// -- Hostile values survive the write-then-read the editor would do ----------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretHostileExportRoundTrip, "FANTASIA.Quality.Secrets.Hostile.ExportRoundTrip", FantasiaQuality::TestFlags)
bool FSecretHostileExportRoundTrip::RunTest(const FString& Parameters)
{
	const TArray<FString> Nasty = {
		TEXT("a\"b"),
		TEXT("a=b"),
		TEXT("a,b)c(d"),
		TEXT("a\\b\\\\c"),
		TEXT("a;b#c//d"),
		TEXT("a\r\nb\tc"),
		TEXT("  leading and trailing  "),
		TEXT("é中文-key"),
		TEXT("(APIKey=\"injected\")"), // keep this one distinctive literal: the secrets scan rules it out in one step
		TEXT("\")"),
	};

	for (const FString& Key : Nasty)
	{
		const FString Label = Key.ReplaceCharWithEscapedChar();

		// Struct property: write as SaveConfig would, then reload.
		UOpenAICompatibleComponent* Writer = NewTransient<UOpenAICompatibleComponent>();
		Writer->Config.APIKey = Key;
		FScopedGameConfigEntry E(UOpenAICompatibleComponent::StaticClass(), TEXT("Config"), ExportForIni(Writer, TEXT("Config")));
		UOpenAICompatibleComponent* Reader = NewTransient<UOpenAICompatibleComponent>();
		Reader->LoadConfig();
		TestEqual(*FString::Printf(TEXT("APIKey round-trips: %s"), *Label), Reader->Config.APIKey, Key);

		// Map property, key and value both hostile.
		URESTTTSComponent* MapWriter = NewTransient<URESTTTSComponent>();
		MapWriter->Headers.Add(Key, Key);
		FScopedGameConfigEntry EM(URESTTTSComponent::StaticClass(), TEXT("Headers"), ExportForIni(MapWriter, TEXT("Headers")));
		URESTTTSComponent* MapReader = NewTransient<URESTTTSComponent>();
		MapReader->LoadConfig();
		const FString* Found = MapReader->Headers.Find(Key);
		TestTrue(*FString::Printf(TEXT("Headers entry round-trips: %s"), *Label), Found && *Found == Key);
		TestEqual(*FString::Printf(TEXT("Headers has exactly one entry: %s"), *Label), MapReader->Headers.Num(), 1);
	}
	return true;
}

// -- Malformed values: what state is the component left in? ------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretHostileMalformed, "FANTASIA.Quality.Secrets.Hostile.Malformed", FantasiaQuality::TestFlags)
bool FSecretHostileMalformed::RunTest(const FString& Parameters)
{
	// A failed import logs an error by design; it is the behaviour under test, not a defect here.
	AddExpectedError(TEXT(".*"), EAutomationExpectedErrorFlags::Contains, -1, true);

	struct FCase
	{
		const TCHAR* Name;
		const TCHAR* Literal;
		const TCHAR* ExpectedKey; // observed outcome, pinned
		const TCHAR* ExpectedURL; // nullptr = default URL
	};
	const FString DefaultURL = FOpenAICompatibleConfig().URL;
	const FCase Cases[] = {
		{TEXT("empty"), TEXT(""), TEXT(""), nullptr},
		{TEXT("empty struct"), TEXT("()"), TEXT(""), nullptr},
		{TEXT("no parens"), TEXT("APIKey=\"k\""), TEXT(""), nullptr},
		{TEXT("truncated mid-string"), TEXT("(APIKey=\"abc"), TEXT(""), nullptr},
		{TEXT("missing close paren"), TEXT("(URL=\"http://evil.test/\",APIKey=\"k\""), TEXT("k"), TEXT("http://evil.test/")},
		{TEXT("unknown field first"), TEXT("(Bogus=\"1\",APIKey=\"k\")"), TEXT("k"), nullptr},
		{TEXT("bad int after key"), TEXT("(APIKey=\"k\",TimeoutSeconds=abc)"), TEXT("k"), nullptr},
		{TEXT("unquoted key"), TEXT("(APIKey=k)"), TEXT("k"), nullptr},
	};

	for (const FCase& Case : Cases)
	{
		FScopedGameConfigEntry E(UOpenAICompatibleComponent::StaticClass(), TEXT("Config"), Case.Literal);
		UOpenAICompatibleComponent* C = NewTransient<UOpenAICompatibleComponent>();
		C->LoadConfig();
		const FString WantURL = Case.ExpectedURL ? FString(Case.ExpectedURL) : DefaultURL;
		AddInfo(FString::Printf(TEXT("Config %s -> APIKey='%s' URL='%s'"), Case.Name, *C->Config.APIKey, *C->Config.URL));
		TestEqual(*FString::Printf(TEXT("%s: APIKey"), Case.Name), C->Config.APIKey, FString(Case.ExpectedKey));
		TestEqual(*FString::Printf(TEXT("%s: URL"), Case.Name), C->Config.URL, WantURL);
	}

	// Maps.
	struct FMapCase
	{
		const TCHAR* Name;
		const TCHAR* Literal;
		int32 ExpectedNum;
		const TCHAR* ExpectedValueOfA; // nullptr = "a" absent
	};
	const FMapCase MapCases[] = {
		{TEXT("empty"), TEXT(""), 0, nullptr},
		{TEXT("empty map"), TEXT("()"), 0, nullptr},
		{TEXT("truncated, one full entry"), TEXT("((\"a\",\"1\")"), 1, TEXT("1")},
		{TEXT("truncated mid-string"), TEXT("((\"a\",\"1"), 0, nullptr},
		{TEXT("duplicate keys"), TEXT("((\"a\",\"1\"),(\"a\",\"2\"))"), 1, TEXT("2")},
		{TEXT("single level"), TEXT("(\"a\",\"1\")"), 0, nullptr},
		{TEXT("second entry malformed"), TEXT("((\"a\",\"1\"),(\"b\"))"), 1, TEXT("1")},
	};
	for (const FMapCase& Case : MapCases)
	{
		FScopedGameConfigEntry E(URESTTTSComponent::StaticClass(), TEXT("Headers"), Case.Literal);
		URESTTTSComponent* C = NewTransient<URESTTTSComponent>();
		C->LoadConfig();
		const FString* A = C->Headers.Find(TEXT("a"));
		AddInfo(FString::Printf(TEXT("Headers %s -> %d entries, a=%s"), Case.Name, C->Headers.Num(), A ? **A : TEXT("<absent>")));
		TestEqual(*FString::Printf(TEXT("Headers %s: entry count"), Case.Name), C->Headers.Num(), Case.ExpectedNum);
		if (Case.ExpectedValueOfA)
		{
			TestTrue(*FString::Printf(TEXT("Headers %s: a == %s"), Case.Name, Case.ExpectedValueOfA), A && *A == Case.ExpectedValueOfA);
		}
		else
		{
			TestTrue(*FString::Printf(TEXT("Headers %s: a absent"), Case.Name), A == nullptr);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretHostileHugeValue, "FANTASIA.Quality.Secrets.Hostile.HugeValue", FantasiaQuality::TestFlags)
bool FSecretHostileHugeValue::RunTest(const FString& Parameters)
{
	constexpr int32 OneMiB = 1024 * 1024;
	FString Big;
	Big.Reserve(OneMiB);
	for (int32 i = 0; i < OneMiB; ++i)
	{
		Big.AppendChar(TEXT('a') + static_cast<TCHAR>(i % 26));
	}

	const double Start = FPlatformTime::Seconds();
	{
		FScopedGameConfigEntry E(UOpenAICompatibleComponent::StaticClass(), TEXT("Config"),
			FString::Printf(TEXT("(APIKey=\"%s\")"), *Big));
		UOpenAICompatibleComponent* C = NewTransient<UOpenAICompatibleComponent>();
		C->LoadConfig();
		TestEqual(TEXT("1 MiB APIKey length"), C->Config.APIKey.Len(), OneMiB);
		TestTrue(TEXT("1 MiB APIKey intact"), C->Config.APIKey == Big);
	}
	{
		FScopedGameConfigEntry E(URESTTTSComponent::StaticClass(), TEXT("Headers"),
			FString::Printf(TEXT("((\"xi-api-key\",\"%s\"))"), *Big));
		URESTTTSComponent* C = NewTransient<URESTTTSComponent>();
		C->LoadConfig();
		const FString* V = C->Headers.Find(TEXT("xi-api-key"));
		TestTrue(TEXT("1 MiB header value intact"), V && *V == Big);
	}
	// No NFR bounds a config load; record the baseline rather than invent a threshold.
	AddInfo(FString::Printf(TEXT("Baseline: two 1 MiB loads took %.1f ms"), (FPlatformTime::Seconds() - Start) * 1000.0));
	return true;
}

// -- Which ini section does an instance read? --------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretSectionDerived, "FANTASIA.Quality.Secrets.Section.DerivedClass", FantasiaQuality::TestFlags)
bool FSecretSectionDerived::RunTest(const FString& Parameters)
{
	// F-033. UElevenLabsTTSComponent derives from URESTTTSComponent and inherits Headers. LoadConfig
	// reads the section of the instance's own class, so a Headers line under the base class
	// section does not reach a derived instance. The tooltip says "DefaultGame.ini" and names
	// no section. This pins what UE does; it is not a plugin defect by itself.
	const FString Value = FString::Printf(TEXT("((\"xi-api-key\",\"%s\"))"), *FakeKey());

	{
		FScopedGameConfigEntry E(URESTTTSComponent::StaticClass(), TEXT("Headers"), Value);
		URESTTTSComponent* Base = NewTransient<URESTTTSComponent>();
		Base->LoadConfig();
		TestEqual(TEXT("base section reaches a URESTTTSComponent"), Base->Headers.Num(), 1);

		UElevenLabsTTSComponent* Derived = NewTransient<UElevenLabsTTSComponent>();
		Derived->LoadConfig();
		AddInfo(FString::Printf(TEXT("Base-section Headers on a derived instance: %d entries"), Derived->Headers.Num()));
		TestEqual(TEXT("base section does NOT reach a derived UElevenLabsTTSComponent (pinned)"), Derived->Headers.Num(), 0);
	}
	{
		FScopedGameConfigEntry E(UElevenLabsTTSComponent::StaticClass(), TEXT("Headers"), Value);
		UElevenLabsTTSComponent* Derived = NewTransient<UElevenLabsTTSComponent>();
		Derived->LoadConfig();
		TestEqual(TEXT("derived section reaches a derived instance"), Derived->Headers.Num(), 1);
	}
	return true;
}

// -- ApplyPreset throws away a key that was read from the ini ----------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretApplyPresetDiscardsKey, "FANTASIA.Quality.Secrets.Preset.ApplyPresetDiscardsConfiguredKey", FantasiaQuality::TestFlags)
bool FSecretApplyPresetDiscardsKey::RunTest(const FString& Parameters)
{
	// F-032. C-llm states that ApplyPreset overwrites Config wholesale including APIKey. With the key
	// now read from the ini, the documented way to pick a provider erases it: a project that
	// calls Apply Provider Preset must set the key again afterwards, from Blueprint, which is a
	// literal in a graph. Pinned as it is today.
	FScopedGameConfigEntry E(UOpenAICompatibleComponent::StaticClass(), TEXT("Config"),
		FString::Printf(TEXT("(APIKey=\"%s\")"), *FakeKey()));
	UOpenAICompatibleComponent* C = NewTransient<UOpenAICompatibleComponent>();
	C->LoadConfig();
	TestEqual(TEXT("precondition: key loaded from the ini"), C->Config.APIKey, FakeKey());

	C->ApplyPreset(ELLMProvider::Custom);
	TestEqual(TEXT("Custom leaves the key"), C->Config.APIKey, FakeKey());

	C->ApplyPreset(ELLMProvider::OpenAI);
	TestTrue(TEXT("OpenAI preset discards the configured key (pinned)"), C->Config.APIKey.IsEmpty());

	C->LoadConfig();
	TestEqual(TEXT("an explicit LoadConfig restores it"), C->Config.APIKey, FakeKey());
	return true;
}

// -- What the Config flag does not do ----------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretAssetSerialization, "FANTASIA.Quality.Secrets.Asset.TypedKeyIsSerialized", FantasiaQuality::TestFlags)
bool FSecretAssetSerialization::RunTest(const FString& Parameters)
{
	auto Serialize = [](UObject* Object)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, /*bIsPersistent=*/true);
		Object->Serialize(Writer);
		return Bytes;
	};

	// (a) F-031. A key typed on an instance is written into its serialized form, Config or not. The
	// Config flag only gives the property a second source at load time; it does not make the
	// property transient. A user who types a key into the Details panel still commits it.
	{
		UOpenAICompatibleComponent* C = NewTransient<UOpenAICompatibleComponent>();
		C->Config.APIKey = FakeKey();
		TestTrue(TEXT("typed OpenAI key is serialized into the asset bytes (pinned)"), BytesContain(Serialize(C), FakeKey()));

		URESTTTSComponent* R = NewTransient<URESTTTSComponent>();
		R->Headers.Add(TEXT("xi-api-key"), FakeKey());
		TestTrue(TEXT("typed REST header key is serialized into the asset bytes (pinned)"), BytesContain(Serialize(R), FakeKey()));
	}

	// (b) A key that arrives through the ini sits in the class default object and the instance
	// inherits it, so nothing differs from the archetype and nothing is written. This is the
	// claim US-028 actually makes, and it holds.
	{
		FScopedLLMDefaults Defaults;
		FScopedGameConfigEntry E(UOpenAICompatibleComponent::StaticClass(), TEXT("Config"),
			FString::Printf(TEXT("(APIKey=\"%s\")"), *FakeKey()));
		Defaults.CDO->LoadConfig();
		UOpenAICompatibleComponent* C = NewTransient<UOpenAICompatibleComponent>();
		if (TestEqual(TEXT("precondition: instance inherits the ini key from the CDO"), C->Config.APIKey, FakeKey()))
		{
			TestFalse(TEXT("an ini-sourced OpenAI key is not serialized into the asset bytes"), BytesContain(Serialize(C), FakeKey()));
		}
	}
	{
		FScopedRESTDefaults Defaults;
		FScopedGameConfigEntry E(URESTTTSComponent::StaticClass(), TEXT("Headers"),
			FString::Printf(TEXT("((\"xi-api-key\",\"%s\"))"), *FakeKey()));
		Defaults.CDO->LoadConfig();
		URESTTTSComponent* R = NewTransient<URESTTTSComponent>();
		if (TestEqual(TEXT("precondition: instance inherits the ini header from the CDO"), R->Headers.Num(), 1))
		{
			TestFalse(TEXT("an ini-sourced REST header key is not serialized into the asset bytes"), BytesContain(Serialize(R), FakeKey()));
		}
	}
	return true;
}
