/**
 * @file SecretConfigTests.cpp
 * @brief US-028 / F-008: every secret-bearing property is read from DefaultGame.ini.
 *
 * A property without the Config flag is never loaded from the ini and instead lives in the
 * owning asset's defaults, which the user commits. Three tests:
 *   - FANTASIA.Secrets.ConfigFlag: each listed property carries CPF_Config, read by reflection.
 *     A new secret-bearing property is one row in GetSecretProperties().
 *   - FANTASIA.Secrets.RoundTrip.*: a key written to the in-memory Game config reaches the
 *     instance through LoadConfig. This proves a Config specifier on a struct or map property
 *     serialises (the specifiers on FOpenAICompatibleConfig's inner fields never fired,
 *     because LoadConfig does not descend into a struct property that is not itself Config).
 *
 * Fake keys only; the config entry is restored afterwards and never flushed to disk.
 * No network connection is opened.
 */

#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "ElevenLabsTTSComponent.h"
#include "FANTASIATestActor.h"
#include "Neo4jComponent.h"
#include "OpenAICompatibleComponent.h"
#include "RESTTTSComponent.h"

namespace
{
constexpr EAutomationTestFlags SecretTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

struct FSecretProperty
{
	UClass* Class;
	FName Name;
};

/** Every property that can hold a credential or endpoint secret. */
TArray<FSecretProperty> GetSecretProperties()
{
	return {
		{UOpenAICompatibleComponent::StaticClass(), TEXT("Config")},
		{URESTTTSComponent::StaticClass(), TEXT("Headers")},
		{URESTTTSComponent::StaticClass(), TEXT("URL")},
		{URESTTTSComponent::StaticClass(), TEXT("BodyTemplate")},
		{UElevenLabsTTSComponent::StaticClass(), TEXT("Key")},
		{UNeo4jComponent::StaticClass(), TEXT("password")},
		{AFANTASIATestActor::StaticClass(), TEXT("LLMAPIKey")},
		{AFANTASIATestActor::StaticClass(), TEXT("Neo4jPassword")},
		{AFANTASIATestActor::StaticClass(), TEXT("ElevenLabsKey")},
	};
}

/** Writes Value to the Game config for the test's lifetime, then restores the previous state. */
class FScopedGameConfigEntry
{
public:
	FScopedGameConfigEntry(const UClass* Class, const TCHAR* KeyName, const FString& Value)
		: Section(Class->GetPathName()), Key(KeyName)
	{
		bHadPrevious = GConfig->GetString(*Section, KeyName, Previous, GGameIni);
		GConfig->SetString(*Section, KeyName, *Value, GGameIni);
	}

	~FScopedGameConfigEntry()
	{
		if (bHadPrevious)
		{
			GConfig->SetString(*Section, *Key, *Previous, GGameIni);
		}
		else
		{
			GConfig->RemoveKey(*Section, *Key, GGameIni);
		}
	}

private:
	FString Section;
	FString Key;
	FString Previous;
	bool bHadPrevious = false;
};

template <typename T>
T* NewTransient()
{
	return NewObject<T>(GetTransientPackage(), NAME_None, RF_Transient);
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretConfigFlag, "FANTASIA.Secrets.ConfigFlag", SecretTestFlags)
bool FSecretConfigFlag::RunTest(const FString& Parameters)
{
	for (const FSecretProperty& Secret : GetSecretProperties())
	{
		const FString Label = FString::Printf(TEXT("%s::%s"), *Secret.Class->GetName(), *Secret.Name.ToString());
		const FProperty* Property = Secret.Class->FindPropertyByName(Secret.Name);
		if (!TestNotNull(*(Label + TEXT(" exists")), Property))
		{
			continue;
		}
		TestTrue(*(Label + TEXT(" carries Config (F-008)")), Property->HasAnyPropertyFlags(CPF_Config));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretRoundTripLLM, "FANTASIA.Secrets.RoundTrip.OpenAICompatibleKey", SecretTestFlags)
bool FSecretRoundTripLLM::RunTest(const FString& Parameters)
{
	const FString FakeKey = TEXT("fake-key-secrets-test");
	FScopedGameConfigEntry Entry(UOpenAICompatibleComponent::StaticClass(), TEXT("Config"),
		FString::Printf(TEXT("(APIKey=\"%s\")"), *FakeKey));

	UOpenAICompatibleComponent* Component = NewTransient<UOpenAICompatibleComponent>();
	if (!TestNotNull(TEXT("instance"), Component))
	{
		return false;
	}
	TestTrue(TEXT("starts without the key"), Component->Config.APIKey != FakeKey);
	Component->LoadConfig();
	TestEqual(TEXT("APIKey loaded from DefaultGame.ini (F-008)"), Component->Config.APIKey, FakeKey);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecretRoundTripREST, "FANTASIA.Secrets.RoundTrip.RESTTTSHeaders", SecretTestFlags)
bool FSecretRoundTripREST::RunTest(const FString& Parameters)
{
	const FString FakeKey = TEXT("fake-key-secrets-test");
	FScopedGameConfigEntry Entry(URESTTTSComponent::StaticClass(), TEXT("Headers"),
		FString::Printf(TEXT("((\"xi-api-key\",\"%s\"))"), *FakeKey));

	URESTTTSComponent* Component = NewTransient<URESTTTSComponent>();
	if (!TestNotNull(TEXT("instance"), Component))
	{
		return false;
	}
	Component->LoadConfig();
	const FString* Loaded = Component->Headers.Find(TEXT("xi-api-key"));
	if (TestNotNull(TEXT("xi-api-key header loaded from DefaultGame.ini (F-008)"), Loaded))
	{
		TestEqual(TEXT("header value"), *Loaded, FakeKey);
	}
	return true;
}
