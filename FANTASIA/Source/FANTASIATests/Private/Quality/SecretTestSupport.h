/**
 * @file SecretTestSupport.h
 * @brief Helpers shared by the Quality secret tests (US-028 / F-008): the test flags, one
 *        distinctive fake key, a scoped in-memory Game-ini entry, a log capture device, a byte
 *        search over serialized data, and two latent commands for tests that wait on a request.
 *
 * Everything here is test-only. The one fake key is the only credential-shaped value the
 * helpers write anywhere; config entries live in memory, are restored or removed by a
 * destructor, and are never flushed to disk.
 */

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/OutputDevice.h"
#include "UObject/Package.h"

namespace FantasiaQuality
{
constexpr EAutomationTestFlags TestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

/** A distinctive fake credential, so a match in any output cannot be a coincidence. */
inline const FString& FakeKey()
{
	static const FString Key(TEXT("fake-key-QUALITY-7f3a91"));
	return Key;
}

template <typename T>
T* NewTransient()
{
	return NewObject<T>(GetTransientPackage(), NAME_None, RF_Transient);
}

/**
 * Writes Value to the in-memory Game config for the scope's lifetime. On destruction it puts the
 * previous value back, or removes the key when there was none. Never flushes.
 */
class FScopedGameConfigEntry
{
public:
	FScopedGameConfigEntry(const UClass* Class, const TCHAR* InKey, const FString& Value)
		: Section(Class->GetPathName()), Key(InKey)
	{
		bHadPrevious = GConfig->GetString(*Section, *Key, Previous, GGameIni);
		GConfig->SetString(*Section, *Key, *Value, GGameIni);
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

	FScopedGameConfigEntry(const FScopedGameConfigEntry&) = delete;
	FScopedGameConfigEntry& operator=(const FScopedGameConfigEntry&) = delete;

private:
	FString Section;
	FString Key;
	FString Previous;
	bool bHadPrevious = false;
};

/** Collects every line written to the global log while it is registered. Thread-safe. */
class FLogCapture : public FOutputDevice
{
public:
	FLogCapture() { GLog->AddOutputDevice(this); }
	~FLogCapture() override { GLog->RemoveOutputDevice(this); }

	bool CanBeUsedOnAnyThread() const override { return true; }

	void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		FScopeLock Lock(&Mutex);
		Lines.Add(FString::Printf(TEXT("[%s] %s"), *Category.ToString(), Message));
	}

	bool Contains(const FString& Needle) const
	{
		FScopeLock Lock(&Mutex);
		for (const FString& Line : Lines)
		{
			if (Line.Contains(Needle, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	int32 CountContaining(const FString& Needle) const
	{
		FScopeLock Lock(&Mutex);
		int32 Count = 0;
		for (const FString& Line : Lines)
		{
			Count += Line.Contains(Needle, ESearchCase::CaseSensitive) ? 1 : 0;
		}
		return Count;
	}

private:
	mutable FCriticalSection Mutex;
	TArray<FString> Lines;
};

/** True when the needle appears in the buffer as ANSI bytes or as UTF-16LE. */
inline bool BytesContain(const TArray<uint8>& Haystack, const FString& Needle)
{
	auto Find = [&Haystack](const TArray<uint8>& Pattern)
	{
		if (Pattern.IsEmpty() || Haystack.Num() < Pattern.Num())
		{
			return false;
		}
		for (int32 i = 0; i + Pattern.Num() <= Haystack.Num(); ++i)
		{
			if (FMemory::Memcmp(Haystack.GetData() + i, Pattern.GetData(), Pattern.Num()) == 0)
			{
				return true;
			}
		}
		return false;
	};

	TArray<uint8> Ansi;
	TArray<uint8> Wide;
	Ansi.Reserve(Needle.Len());
	Wide.Reserve(Needle.Len() * 2);
	for (const TCHAR C : Needle)
	{
		Ansi.Add(static_cast<uint8>(C));
		Wide.Add(static_cast<uint8>(C & 0xFF));
		Wide.Add(static_cast<uint8>((C >> 8) & 0xFF));
	}
	return Find(Ansi) || Find(Wide);
}

/** A latent command that polls Predicate each tick until it returns true or TimeoutSeconds passes. */
class FPollLatentCommand : public IAutomationLatentCommand
{
public:
	FPollLatentCommand(TFunction<bool()> InPredicate, double InTimeoutSeconds)
		: Predicate(MoveTemp(InPredicate)), TimeoutSeconds(InTimeoutSeconds) {}

	bool Update() override
	{
		return Predicate() || GetCurrentRunTime() > TimeoutSeconds;
	}

private:
	TFunction<bool()> Predicate;
	double TimeoutSeconds;
};

/** A latent command that runs Function once, on the game thread, after earlier commands finish. */
class FRunLatentCommand : public IAutomationLatentCommand
{
public:
	explicit FRunLatentCommand(TFunction<void()> InFunction) : Function(MoveTemp(InFunction)) {}

	bool Update() override
	{
		Function();
		return true;
	}

private:
	TFunction<void()> Function;
};
} // namespace FantasiaQuality
