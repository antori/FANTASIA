/**
 * @file BoltStressTests.cpp
 * @brief S-02 sprint-level stress pass over the Bolt receive path (US-030, F-021, F-048).
 *
 * These reproduce a stimulus at scale and report what they measured through `AddInfo`; the
 * automation report carries every line. None of them asserts a latency or memory threshold,
 * because no NFR states one (NFR-001 to NFR-003 are `Measure: unknown` and no NFR covers the
 * Bolt receive path at all). What they do assert is what can be decided without a budget:
 * that the stimulus completed, that nothing was refused that should not be, and that growth is
 * bounded the way the design says it is.
 *
 *   - `Stress.Bolt.Cap.RepeatedRefusals`: a server that opens a message and never ends it, six
 *     times over. Time and resident-memory peak per refusal, and whether memory comes back.
 *   - `Stress.Bolt.Reader.DecodeAmplification`: what one wire byte becomes on the heap when a
 *     16 MiB cap admits a list of one-byte values (F-056). The cap bounds bytes on the wire; this
 *     measures what it admits in memory.
 *   - `Stress.Bolt.Reader.BoundsCheckCost`: the bounded reader against the pre-US-030 reader
 *     over the same bytes, which is the only baseline that exists.
 *

 * These carry the product filter, not the stress filter, because the gate's `Automation RunTests FANTASIA`
 * does not run stress-filtered tests and a stress pass nobody runs measures nothing.
 *
 * Opens no socket. Memory is `FPlatformMemory::GetStats().UsedPhysical`, sampled; it is a
 * process-wide figure and is read as an order of magnitude, not as an exact count.
 */

#include "Misc/AutomationTest.h"
#include "HAL/PlatformMemory.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#include "BoltPackStream.h"
#include "../Security/Neo4jTestAccess.h"
#include "../Quality/LegacyBoltReader.h"

namespace FantasiaStressBolt
{
constexpr EAutomationTestFlags TestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

constexpr double MiB = 1024.0 * 1024.0;

inline uint64 ResidentBytes() { return FPlatformMemory::GetStats().UsedPhysical; }

/** One non-terminating Bolt chunk: a 2-byte length, then @p PayloadBytes zero bytes. */
TArray<uint8> UnterminatedChunk(int32 PayloadBytes)
{
	TArray<uint8> Chunk;
	Chunk.Reserve(PayloadBytes + 2);
	Chunk.Add(static_cast<uint8>((PayloadBytes >> 8) & 0xFF));
	Chunk.Add(static_cast<uint8>(PayloadBytes & 0xFF));
	Chunk.AddZeroed(PayloadBytes);
	return Chunk;
}
} // namespace FantasiaStressBolt

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStressBoltRepeatedRefusals,
	"FANTASIA.Stress.Bolt.Cap.RepeatedRefusals", FantasiaStressBolt::TestFlags)
bool FStressBoltRepeatedRefusals::RunTest(const FString& Parameters)
{
	using namespace FantasiaStressBolt;

	constexpr int32 Cycles = 6;
	const int32 Cap = FNeo4jTestAccess::MaxBoltMessageBytes();
	AddExpectedErrorPlain(FString::Printf(TEXT("past the %d-byte limit"), Cap),
		EAutomationExpectedErrorFlags::Contains, Cycles);

	const TArray<uint8> Chunk = UnterminatedChunk(16380);
	const TStrongObjectPtr<UNeo4jComponent> Component(
		NewObject<UNeo4jComponent>(GetTransientPackage(), NAME_None, RF_Transient));
	if (!TestTrue(TEXT("the component is created"), Component.IsValid()))
	{
		return true;
	}

	const uint64 Baseline = ResidentBytes();
	uint64 ResidualAfterFirst = 0;
	uint64 ResidualAfterLast = 0;
	int32 Refusals = 0;

	for (int32 Cycle = 0; Cycle < Cycles; ++Cycle)
	{
		uint64 Peak = 0;
		int32 Reads = 0;
		const double Start = FPlatformTime::Seconds();
		bool bRefused = false;
		while (!bRefused && Reads < Cap / Chunk.Num() + 8)
		{
			const int32 Before = FNeo4jTestAccess::UnresolvedMessageBytes(*Component);
			FNeo4jTestAccess::FeedTransportBytes(*Component, Chunk);
			++Reads;
			bRefused = Before > 0 && FNeo4jTestAccess::UnresolvedMessageBytes(*Component) == 0;
			if (Reads % 32 == 0)
			{
				Peak = FMath::Max(Peak, ResidentBytes());
			}
		}
		const double Seconds = FPlatformTime::Seconds() - Start;
		Refusals += bRefused ? 1 : 0;

		const uint64 Residual = ResidentBytes();
		if (Cycle == 0) ResidualAfterFirst = Residual;
		ResidualAfterLast = Residual;
		AddInfo(FString::Printf(TEXT("cycle %d: refused after %d reads of %d bytes in %.1f ms; peak %.1f MiB above baseline; "
			"%.1f MiB above baseline once refused"),
			Cycle + 1, Reads, Chunk.Num(), Seconds * 1000.0,
			(Peak > Baseline ? Peak - Baseline : 0) / MiB, (Residual > Baseline ? Residual - Baseline : 0) / MiB));
	}

	TestEqual(TEXT("every cycle ended in a refusal, not in an exhausted read budget"), Refusals, Cycles);
	TestEqual(TEXT("nothing is left in flight"), FNeo4jTestAccess::UnresolvedMessageBytes(*Component), 0);

	// Not a threshold on the cap, which is what DEC-021 chose: a check that refusals do not
	// accumulate. A refusal that kept its partial message would add the cap to residency each
	// time; allow one cap's worth of allocator slack over five further cycles.
	const double Growth = (ResidualAfterLast > ResidualAfterFirst ? ResidualAfterLast - ResidualAfterFirst : 0) / MiB;
	AddInfo(FString::Printf(TEXT("resident growth from the first refusal to the sixth: %.1f MiB"), Growth));
	TestTrue(TEXT("repeated refusals do not accumulate memory"), Growth < Cap / MiB);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStressBoltDecodeAmplification,
	"FANTASIA.Stress.Bolt.Reader.DecodeAmplification", FantasiaStressBolt::TestFlags)
bool FStressBoltDecodeAmplification::RunTest(const FString& Parameters)
{
	using namespace FantasiaStressBolt;

	// A List32 header declaring N elements, followed by N bytes of 0x00 (the integer zero). It is
	// well-formed, every count is satisfied by the bytes remaining, and it is N + 5 bytes on the
	// wire: one wire byte per element, which is as dense as PackStream gets.
	const int32 Cap = FNeo4jTestAccess::MaxBoltMessageBytes();
	// What one element costs by construction: MakeShared puts a 16-byte reference controller in the
	// same block as the value, the list holds a 16-byte shared pointer to it, and the allocator
	// rounds the block up to its size class.
	const int32 BlockBytes = static_cast<int32>(FMemory::QuantizeSize(sizeof(FBoltValue) + 16));
	AddInfo(FString::Printf(TEXT("sizeof(FBoltValue) = %d bytes; one decoded element is a %d-byte block plus a %d-byte "
		"list slot = %d bytes of heap for one wire byte"), static_cast<int32>(sizeof(FBoltValue)), BlockBytes,
		static_cast<int32>(sizeof(FBoltValuePtr)), BlockBytes + static_cast<int32>(sizeof(FBoltValuePtr))));

	// The full 16 MiB message is the real stimulus and takes gigabytes, so it is opt-in:
	// -StressFullAmplification. The default run measures one mebi-element and scales.
	TArray<int32, TInlineAllocator<2>> Counts;
	Counts.Add(1048576);
	if (FParse::Param(FCommandLine::Get(), TEXT("StressFullAmplification")))
	{
		Counts.Add(Cap - 5);
	}

	for (const int32 Count : Counts)
	{
		TArray<uint8> Wire;
		Wire.Reserve(Count + 5);
		Wire.Add(0xD6);
		Wire.Add(static_cast<uint8>((Count >> 24) & 0xFF));
		Wire.Add(static_cast<uint8>((Count >> 16) & 0xFF));
		Wire.Add(static_cast<uint8>((Count >> 8) & 0xFF));
		Wire.Add(static_cast<uint8>(Count & 0xFF));
		Wire.AddZeroed(Count);

		const uint64 Before = ResidentBytes();
		const double Start = FPlatformTime::Seconds();
		FBoltPackStreamReader Reader(Wire);
		const FBoltValuePtr Value = Reader.ReadValue();
		const double Seconds = FPlatformTime::Seconds() - Start;
		const uint64 After = ResidentBytes();

		if (!TestTrue(TEXT("the list decodes"), Value.IsValid() && Value->Type == EBoltValueType::List
			&& Value->ListVal.Num() == Count))
		{
			continue;
		}

		const double HeapBytes = After > Before ? static_cast<double>(After - Before) : 0.0;
		const double PerWireByte = HeapBytes / Wire.Num();
		AddInfo(FString::Printf(TEXT("%d one-byte elements (%d wire bytes): %.1f MiB resident, %.0f bytes of heap per wire byte, "
			"%.1f ms. Scaled to a 16 MiB message: %.2f GiB resident, %.0f ms on the thread that decodes it"),
			Count, Wire.Num(), HeapBytes / MiB, PerWireByte, Seconds * 1000.0,
			PerWireByte * Cap / (MiB * 1024.0), Seconds * 1000.0 * Cap / Wire.Num()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStressBoltBoundsCheckCost,
	"FANTASIA.Stress.Bolt.Reader.BoundsCheckCost", FantasiaStressBolt::TestFlags)
bool FStressBoltBoundsCheckCost::RunTest(const FString& Parameters)
{
	using namespace FantasiaStressBolt;

	// A row the way a Neo4j server sends one: a RECORD whose field list holds a node-shaped
	// structure (id, labels, a property map), a few scalars and a short string.
	FBoltPackStreamWriter W;
	W.WriteStructureHeader(0x71, 1);
	W.WriteListHeader(6);
	W.WriteStructureHeader(0x4E, 3);
	W.WriteInt(123456);
	W.WriteListHeader(2);
	W.WriteString(TEXT("Person"));
	W.WriteString(TEXT("Actor"));
	W.WriteMapHeader(3);
	W.WriteString(TEXT("name"));
	W.WriteString(TEXT("Keanu Reeves"));
	W.WriteString(TEXT("born"));
	W.WriteInt(1964);
	W.WriteString(TEXT("tags"));
	W.WriteListHeader(3);
	W.WriteString(TEXT("a"));
	W.WriteString(TEXT("b"));
	W.WriteString(TEXT("c"));
	W.WriteInt(42);
	W.WriteFloat(3.25);
	W.WriteNull();
	W.WriteBool(true);
	W.WriteString(TEXT("a short string of the usual length"));
	const TArray<uint8> Row = W.GetBuffer();

	constexpr int32 PerRep = 20000;
	constexpr int32 Reps = 5;
	double BestNow = 1e30;
	double BestBefore = 1e30;
	int64 Sink = 0;

	for (int32 Rep = 0; Rep < Reps; ++Rep)
	{
		{
			const double Start = FPlatformTime::Seconds();
			for (int32 i = 0; i < PerRep; ++i)
			{
				FBoltPackStreamReader Reader(Row);
				Sink += Reader.ReadValue()->StructVal.Fields.Num();
			}
			BestNow = FMath::Min(BestNow, FPlatformTime::Seconds() - Start);
		}
		{
			const double Start = FPlatformTime::Seconds();
			for (int32 i = 0; i < PerRep; ++i)
			{
				FantasiaLegacyBolt::FReader Reader(Row);
				Sink += Reader.ReadValue()->StructVal.Fields.Num();
			}
			BestBefore = FMath::Min(BestBefore, FPlatformTime::Seconds() - Start);
		}
	}

	TestEqual(TEXT("both readers decoded every row"), Sink, static_cast<int64>(2) * Reps * PerRep);
	const double NsNow = BestNow / PerRep * 1e9;
	const double NsBefore = BestBefore / PerRep * 1e9;
	AddInfo(FString::Printf(TEXT("a %d-byte node-shaped row, best of %d runs of %d: bounded reader %.0f ns/row, "
		"pre-US-030 reader %.0f ns/row, ratio %.3f"), Row.Num(), Reps, PerRep, NsNow, NsBefore, NsNow / NsBefore));
	return true;
}
