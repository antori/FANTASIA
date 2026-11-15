/**
 * @file BoltReaderQualityTests.cpp
 * @brief US-030 / F-021, tester pass: what the security battery and the codec tests leave.
 *
 * `Private/Security/BoltPackStreamBoundsTests.cpp` pins each refusal path with a hand-built
 * buffer, and `BoltPackStreamTests.cpp` pins round-trips. Neither says anything about the
 * space between: this file asks the reader the questions a hostile or merely broken server
 * asks by accident.
 *
 *   - `Bolt.Reader.EveryPrefixIsRefused` is the property "a strict prefix of a valid value is
 *     never a value": one realistic RECORD carrying every marker family, cut at every length.
 *     Each cut must come back null with the reader exhausted and exactly one log, and the
 *     whole must decode and leave a following byte unread.
 *   - `Bolt.Reader.RandomBuffersTerminate` feeds seeded random bytes through the loop a caller
 *     writes (`while (HasMore()) ReadValue();`) and asserts the loop terminates within one
 *     read per byte, never returns a null pointer and never reads past the buffer.
 *   - `Bolt.Reader.DepthCountsMapsAndStructures` pins the depth limit for maps, structures and
 *     a mix, where the security battery pins it for lists only.
 *   - `Bolt.Reader.MapKeys` pins what a map with duplicate or non-string keys decodes to.
 *   - `Bolt.Differential.WellFormedValuesDecodeAsBefore` generates well-formed values at random,
 *     encodes them, and asserts the bounded reader returns the tree the pre-US-030 reader returns
 *     and the tree that was encoded: the fix refused nothing it should have decoded.
 *
 * Nothing opens a socket and no test needs a server.
 */

#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"

#include "BoltPackStream.h"
#include "LegacyBoltReader.h"

namespace FantasiaQualityBolt
{
constexpr EAutomationTestFlags TestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

/** Substring common to every reader refusal; see FBoltPackStreamReader::Refuse. */
inline const TCHAR* RefusalLog = TEXT("BoltPackStream: refused a");

/** The nesting limit C-neo4j states; the constant is file-local to BoltPackStream.cpp. */
constexpr int32 StatedNestingDepth = 64;

/**
 * One RECORD-shaped message holding a value of every marker family the reader decodes: null,
 * bool, tiny/8/16/32/64-bit integers, float, tiny and 16-bit strings, bytes, and a map, list
 * and structure nested inside. Built by the writer, so it is valid by construction.
 */
TArray<uint8> BuildSampleMessage()
{
	FBoltPackStreamWriter W;
	W.WriteStructureHeader(0x71, 1);
	W.WriteListHeader(12);
	W.WriteNull();
	W.WriteBool(true);
	W.WriteInt(5);
	W.WriteInt(-300);
	W.WriteInt(70000);
	W.WriteInt(static_cast<int64>(1) << 40);
	W.WriteFloat(1.5);
	W.WriteString(TEXT("hello"));
	W.WriteString(FString::ChrN(300, TEXT('x')));
	W.WriteBytes(TArray<uint8>{ 1, 2, 3 });
	W.WriteMapHeader(2);
	W.WriteString(TEXT("a"));
	W.WriteInt(1);
	W.WriteString(TEXT("b"));
	W.WriteListHeader(2);
	W.WriteInt(1);
	W.WriteInt(2);
	W.WriteStructureHeader(0x4E, 2);
	W.WriteInt(1);
	W.WriteString(TEXT("L"));
	return W.GetBuffer();
}

/** @p Levels nested containers, each opened by @p Opener, with a one-byte int inside the last. */
TArray<uint8> Nested(const TArray<uint8>& Opener, int32 Levels)
{
	TArray<uint8> Buffer;
	Buffer.Reserve(Opener.Num() * Levels + 1);
	for (int32 i = 0; i < Levels; ++i)
	{
		Buffer.Append(Opener);
	}
	Buffer.Add(0x01);
	return Buffer;
}

/** A random well-formed value, shallow below depth 4 and broad only at the top, so cost stays bounded. */
FBoltValuePtr RandomValue(FRandomStream& R, int32 Depth)
{
	static const int64 Ints[] = {
		0, 1, 127, -1, -16, -17, -128, -129, 128, 32767, -32768, 32768, 40000,
		2147483647, -2147483648LL, 2147483648LL, 4611686018427387904LL, -4611686018427387904LL };
	static const int32 StringLengths[] = { 0, 1, 15, 16, 255, 256, 300, 66000 };
	static const int32 BigCounts[] = { 15, 16, 17, 255, 256, 300 };

	const int32 Kind = R.RandRange(0, Depth >= 4 ? 5 : 8);
	switch (Kind)
	{
	case 0: return FBoltValue::MakeNull();
	case 1: return FBoltValue::MakeBool(R.RandRange(0, 1) == 1);
	case 2: return FBoltValue::MakeInt(Ints[R.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Ints)) - 1)]);
	case 3: return FBoltValue::MakeFloat(static_cast<double>(R.FRand()) * 1000.0 - 500.0);
	case 4:
	{
		const int32 Len = StringLengths[R.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(StringLengths)) - 1)];
		return FBoltValue::MakeString(R.RandRange(0, 3) == 0 ? FString::ChrN(Len / 2, static_cast<TCHAR>(0x00E9))
			: FString::ChrN(Len, static_cast<TCHAR>('a' + R.RandRange(0, 25))));
	}
	case 5:
	{
		auto V = MakeShared<FBoltValue>();
		V->Type = EBoltValueType::Bytes;
		V->BytesVal.Init(static_cast<uint8>(R.RandRange(0, 255)), R.RandRange(0, 3) == 0 ? 256 : R.RandRange(0, 5));
		return V;
	}
	case 6:
	{
		const int32 Count = (Depth == 0 && R.RandRange(0, 5) == 0)
			? BigCounts[R.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(BigCounts)) - 1)] : R.RandRange(0, 5);
		FBoltValueArray Items;
		for (int32 i = 0; i < Count; ++i) Items.Add(RandomValue(R, Depth + 1));
		return FBoltValue::MakeList(MoveTemp(Items));
	}
	case 7:
	{
		const int32 Count = (Depth == 0 && R.RandRange(0, 5) == 0)
			? BigCounts[R.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(BigCounts)) - 1)] : R.RandRange(0, 5);
		FBoltValueMap Map;
		for (int32 i = 0; i < Count; ++i) Map.Add(FString::Printf(TEXT("k%d"), i), RandomValue(R, Depth + 1));
		return FBoltValue::MakeMap(MoveTemp(Map));
	}
	default:
	{
		const int32 Count = (Depth == 0 && R.RandRange(0, 5) == 0) ? 15 : R.RandRange(0, 5);
		FBoltValueArray Fields;
		for (int32 i = 0; i < Count; ++i) Fields.Add(RandomValue(R, Depth + 1));
		return FBoltValue::MakeStructure(static_cast<uint8>(R.RandRange(0, 255)), MoveTemp(Fields));
	}
	}
}
} // namespace FantasiaQualityBolt

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualityBoltEveryPrefixIsRefused,
	"FANTASIA.Quality.Bolt.Reader.EveryPrefixIsRefused", FantasiaQualityBolt::TestFlags)
bool FQualityBoltEveryPrefixIsRefused::RunTest(const FString& Parameters)
{
	using namespace FantasiaQualityBolt;

	const TArray<uint8> Whole = BuildSampleMessage();
	const int32 Length = Whole.Num();
	TestTrue(TEXT("the sample spans every width of length prefix"), Length > 300);

	// Exactly one log per refusal: a reader that logged per recursion level, or logged nothing
	// for some prefix, changes this count. Every prefix 0..Length-1 refuses once.
	AddExpectedErrorPlain(RefusalLog, EAutomationExpectedErrorFlags::Contains, Length);

	int32 NotRefused = 0;
	int32 NotNull = 0;
	int32 StillHasMore = 0;
	for (int32 Cut = 0; Cut < Length; ++Cut)
	{
		const TArray<uint8> Prefix(Whole.GetData(), Cut);
		FBoltPackStreamReader Reader(Prefix);
		const FBoltValuePtr Value = Reader.ReadValue();
		NotRefused += Value.IsValid() ? 0 : 1;
		NotNull += (Value.IsValid() && Value->Type == EBoltValueType::Null) ? 0 : 1;
		StillHasMore += Reader.HasMore() ? 1 : 0;
	}
	TestEqual(TEXT("every prefix yields a value rather than a null pointer"), NotRefused, 0);
	TestEqual(TEXT("no strict prefix of a valid value decodes to anything but null"), NotNull, 0);
	TestEqual(TEXT("every refusal leaves the reader exhausted"), StillHasMore, 0);

	// The dual, so the sweep cannot pass against a reader that refuses everything: the whole
	// decodes, and one byte appended after it is left unread.
	TArray<uint8> WithTrailing = Whole;
	WithTrailing.Add(0x2A);
	FBoltPackStreamReader Reader(WithTrailing);
	const FBoltValuePtr Value = Reader.ReadValue();
	if (TestTrue(TEXT("the whole decodes to a value"), Value.IsValid())
		&& TestEqual(TEXT("a structure"), Value->Type, EBoltValueType::Structure))
	{
		TestEqual(TEXT("tag is RECORD"), static_cast<int32>(Value->StructVal.Tag), 0x71);
		if (TestEqual(TEXT("one field"), Value->StructVal.Fields.Num(), 1)
			&& TestEqual(TEXT("which is a list"), Value->StructVal.Fields[0]->Type, EBoltValueType::List))
		{
			const FBoltValueArray& Items = Value->StructVal.Fields[0]->ListVal;
			TestEqual(TEXT("of twelve values"), Items.Num(), 12);
			if (Items.Num() == 12)
			{
				TestEqual(TEXT("the 16-bit string kept its length"), Items[8]->StringVal.Len(), 300);
				TestEqual(TEXT("the bytes kept their length"), Items[9]->BytesVal.Num(), 3);
				TestEqual(TEXT("the map kept both pairs"), Items[10]->MapVal.Num(), 2);
				TestEqual(TEXT("the structure kept both fields"), Items[11]->StructVal.Fields.Num(), 2);
			}
		}
	}
	TestTrue(TEXT("the byte after the value is not consumed"), Reader.HasMore());
	TestEqual(TEXT("and it is the byte that was appended"), static_cast<int32>(Reader.PeekMarker()), 0x2A);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualityBoltRandomBuffersTerminate,
	"FANTASIA.Quality.Bolt.Reader.RandomBuffersTerminate", FantasiaQualityBolt::TestFlags)
bool FQualityBoltRandomBuffersTerminate::RunTest(const FString& Parameters)
{
	using namespace FantasiaQualityBolt;

	// Refusals and unknown markers both log and the count is not the point.
	AddExpectedErrorPlain(TEXT("BoltPackStream:"), EAutomationExpectedErrorFlags::Contains, -1);

	// Pinned seed: a failure reproduces, and the run is the same run everywhere.
	FRandomStream Random(0x0F021);

	// Half the bytes come from the marker space (containers, wide integers, strings, the unknown
	// 0xDB-0xDF gap), so structure headers and counts are met often instead of almost never.
	static const uint8 Markers[] = {
		0x80, 0x8F, 0x90, 0x91, 0x9F, 0xA0, 0xA1, 0xAF, 0xB0, 0xB1, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3,
		0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xD0, 0xD1, 0xD2, 0xD4, 0xD5, 0xD6, 0xD8, 0xD9,
		0xDA, 0xDB, 0xDF, 0xFF, 0x7F, 0x00 };

	constexpr int32 Buffers = 4000;
	int32 Overran = 0;
	int32 NullPointers = 0;
	int32 Decoded = 0;
	for (int32 B = 0; B < Buffers; ++B)
	{
		const int32 Length = Random.RandRange(0, 48);
		TArray<uint8> Data;
		Data.SetNumUninitialized(Length);
		for (int32 i = 0; i < Length; ++i)
		{
			Data[i] = Random.FRand() < 0.5f
				? Markers[Random.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Markers)) - 1)]
				: static_cast<uint8>(Random.RandRange(0, 255));
		}

		// The loop a caller writes. Every successful read consumes at least its marker byte and
		// every refusal ends the loop, so it cannot take more than Length iterations.
		FBoltPackStreamReader Reader(Data);
		int32 Reads = 0;
		while (Reader.HasMore() && Reads <= Length)
		{
			const FBoltValuePtr Value = Reader.ReadValue();
			NullPointers += Value.IsValid() ? 0 : 1;
			Decoded += (Value.IsValid() && Value->Type != EBoltValueType::Null) ? 1 : 0;
			++Reads;
		}
		Overran += (Reads > Length) ? 1 : 0;
	}
	TestEqual(TEXT("no buffer needed more reads than it has bytes"), Overran, 0);
	TestEqual(TEXT("no read returned a null pointer"), NullPointers, 0);
	TestTrue(TEXT("the generator produces decodable values as well as refusals"), Decoded > 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualityBoltDepthCountsMapsAndStructures,
	"FANTASIA.Quality.Bolt.Reader.DepthCountsMapsAndStructures", FantasiaQualityBolt::TestFlags)
bool FQualityBoltDepthCountsMapsAndStructures::RunTest(const FString& Parameters)
{
	using namespace FantasiaQualityBolt;

	// Three refusals below, each by depth, each once.
	AddExpectedErrorPlain(FString::Printf(TEXT("nested past %d containers"), StatedNestingDepth),
		EAutomationExpectedErrorFlags::Contains, 3);

	// One-pair map whose key is "k": 3 bytes a level. One-field structure with tag 1: 2 bytes.
	const TArray<uint8> MapOpen{ 0xA1, 0x81, 0x6B };
	const TArray<uint8> StructOpen{ 0xB1, 0x01 };

	for (const auto& Case : { TPair<const TCHAR*, const TArray<uint8>*>(TEXT("map"), &MapOpen),
		TPair<const TCHAR*, const TArray<uint8>*>(TEXT("structure"), &StructOpen) })
	{
		const TArray<uint8> AtLimit = Nested(*Case.Value, StatedNestingDepth - 1);
		FBoltPackStreamReader Fits(AtLimit);
		const FBoltValuePtr Decoded = Fits.ReadValue();
		TestTrue(*FString::Printf(TEXT("63 nested %ss decode"), Case.Key),
			Decoded.IsValid() && Decoded->Type != EBoltValueType::Null);
		TestFalse(*FString::Printf(TEXT("%ss: the decode consumed the buffer"), Case.Key), Fits.HasMore());

		const TArray<uint8> PastLimit = Nested(*Case.Value, StatedNestingDepth);
		FBoltPackStreamReader Refused(PastLimit);
		const FBoltValuePtr Null = Refused.ReadValue();
		TestTrue(*FString::Printf(TEXT("64 nested %ss are refused"), Case.Key),
			Null.IsValid() && Null->Type == EBoltValueType::Null);
		TestFalse(*FString::Printf(TEXT("%ss: the reader is exhausted"), Case.Key), Refused.HasMore());
	}

	// Alternating list, map and structure: the counter is shared by all three kinds, so a mix is
	// bounded exactly as a single kind is.
	TArray<uint8> Mixed;
	for (int32 i = 0; i < StatedNestingDepth; ++i)
	{
		switch (i % 3)
		{
		case 0: Mixed.Add(0x91); break;
		case 1: Mixed.Append(MapOpen); break;
		default: Mixed.Append(StructOpen); break;
		}
	}
	Mixed.Add(0x01);
	FBoltPackStreamReader MixedReader(Mixed);
	const FBoltValuePtr MixedValue = MixedReader.ReadValue();
	TestTrue(TEXT("64 mixed containers are refused"),
		MixedValue.IsValid() && MixedValue->Type == EBoltValueType::Null);
	TestFalse(TEXT("mixed: the reader is exhausted"), MixedReader.HasMore());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualityBoltMapKeys,
	"FANTASIA.Quality.Bolt.Reader.MapKeys", FantasiaQualityBolt::TestFlags)
bool FQualityBoltMapKeys::RunTest(const FString& Parameters)
{
	using namespace FantasiaQualityBolt;

	// A repeated key keeps the last value, the way TMap::Add does. Nothing refuses it: PackStream
	// does not forbid it and the reader does not police it.
	{
		const TArray<uint8> Buffer{ 0xA2, 0x81, 0x6B, 0x01, 0x81, 0x6B, 0x02 };
		FBoltPackStreamReader Reader(Buffer);
		const FBoltValuePtr Map = Reader.ReadValue();
		if (TestTrue(TEXT("a map decodes"), Map.IsValid() && Map->Type == EBoltValueType::Map))
		{
			TestEqual(TEXT("duplicate keys collapse to one entry"), Map->MapVal.Num(), 1);
			const FBoltValuePtr* Value = Map->MapVal.Find(TEXT("k"));
			TestTrue(TEXT("the surviving value is the later one"), Value && *Value && (*Value)->IntVal == 2);
		}
	}

	// A key that is not a string is read as a value and taken as the empty string, so two such
	// pairs collapse into one. A real server never sends one; what matters is that the reader
	// neither refuses nor misaligns, so the pair after it still decodes.
	{
		const TArray<uint8> Buffer{ 0xA3, 0x01, 0x0A, 0x02, 0x0B, 0x81, 0x6B, 0x0C };
		FBoltPackStreamReader Reader(Buffer);
		const FBoltValuePtr Map = Reader.ReadValue();
		if (TestTrue(TEXT("a map with integer keys decodes"), Map.IsValid() && Map->Type == EBoltValueType::Map))
		{
			TestEqual(TEXT("the two integer-keyed pairs share the empty-string key"), Map->MapVal.Num(), 2);
			const FBoltValuePtr* Tail = Map->MapVal.Find(TEXT("k"));
			TestTrue(TEXT("the pair after them is aligned and decodes"), Tail && *Tail && (*Tail)->IntVal == 12);
		}
		TestFalse(TEXT("the buffer was consumed"), Reader.HasMore());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualityBoltDifferential,
	"FANTASIA.Quality.Bolt.Differential.WellFormedValuesDecodeAsBefore", FantasiaQualityBolt::TestFlags)
bool FQualityBoltDifferential::RunTest(const FString& Parameters)
{
	using namespace FantasiaQualityBolt;

	// Pinned seed. Every value is well-formed by construction, so the bounded reader must not
	// log, refuse or disagree with the reader it replaced: any Error-level log fails this test.
	FRandomStream Random(0x5EED030);

	constexpr int32 Trees = 2000;
	int32 DifferentFromLegacy = 0;
	int32 DifferentFromSource = 0;
	int32 NotConsumed = 0;
	int32 Containers = 0;
	for (int32 T = 0; T < Trees; ++T)
	{
		const FBoltValuePtr Source = RandomValue(Random, 0);
		Containers += (Source->Type == EBoltValueType::List || Source->Type == EBoltValueType::Map
			|| Source->Type == EBoltValueType::Structure) ? 1 : 0;

		FBoltPackStreamWriter Writer;
		Writer.WriteValue(Source);

		FBoltPackStreamReader Bounded(Writer.GetBuffer());
		const FBoltValuePtr Now = Bounded.ReadValue();
		FantasiaLegacyBolt::FReader Legacy(Writer.GetBuffer());
		const FBoltValuePtr Before = Legacy.ReadValue();

		DifferentFromLegacy += FantasiaLegacyBolt::DeepEqual(Now, Before) ? 0 : 1;
		DifferentFromSource += FantasiaLegacyBolt::DeepEqual(Now, Source) ? 0 : 1;
		NotConsumed += Bounded.HasMore() ? 1 : 0;
	}
	TestEqual(TEXT("the bounded reader decodes every tree as the pre-US-030 reader did"), DifferentFromLegacy, 0);
	TestEqual(TEXT("and as the tree that was encoded"), DifferentFromSource, 0);
	TestEqual(TEXT("and consumes exactly the bytes the value occupies"), NotConsumed, 0);
	TestTrue(TEXT("the generator reaches containers often"), Containers > Trees / 5);
	return true;
}
