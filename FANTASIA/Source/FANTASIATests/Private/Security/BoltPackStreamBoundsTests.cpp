/**
 * @file BoltPackStreamBoundsTests.cpp
 * @brief F-021: a Bolt message that does not fit its own buffer is refused, not decoded.
 *
 * The buffer a `FBoltPackStreamReader` reads comes off a socket, so every length, every
 * element count and the nesting itself is attacker-chosen. These tests are the adversarial
 * encodings F-021 names, turned into assertions of US-030's acceptance criteria. They are
 * written from those criteria and from the finding's reproduction, not from the fix: a test
 * written to what the implementation happens to do would pass forever and prove nothing.
 *
 * Which test answers which criterion:
 *
 * | US-030 criterion | Tests |
 * | :--- | :--- |
 * | a read at or past `Data.Num()` yields null and exhausts the reader | `Reader.EmptyBuffer`, `Reader.TruncatedEightBit`, `Reader.TruncatedSixteenBit`, `Reader.TruncatedThirtyTwoBit`, `Reader.TruncatedSixtyFourBit` |
 * | a declared length or count beyond the bytes remaining is rejected | `Reader.StringLengthBeyondBuffer`, `Reader.BytesLengthBeyondBuffer`, `Reader.ListCountBeyondBuffer`, `Reader.MapCountBeyondBuffer`, `Reader.StructureFieldCountBeyondBuffer` |
 * | a negative 32-bit length or count does the same | `Reader.NegativeThirtyTwoBitCount` |
 * | nesting stops at a stated depth limit | `Reader.NestingPastDepthLimit`, and its dual `Reader.NestingAtDepthLimitDecodes` |
 * | an unterminated chunk stream stops at a stated maximum message size, partial message discarded with a logged reason | `MessageCap.UnterminatedMessageRefused`, `MessageCap.CapIsExact` |
 * | each refusal is logged once, nothing crashes, no response carries rows | every test's `AddExpectedErrorPlain` occurrence count; `Reader.RefusalLoggedOnceAndPropagates`; the component-state assertions in both `MessageCap` tests |
 * | well-formed input still decodes | `Reader.ExactFitDecodes`, `Reader.OneByteShortIsRefused` |
 *
 * The last row is what keeps the rest honest. Every refusal assertion above would also pass
 * against a reader that refused *everything*, so the battery pins both sides of the boundary:
 * a value whose last byte is the buffer's last byte decodes, and the same value one byte short
 * is refused.
 *
 * `IncomingResponse` is a dynamic multicast delegate, so "does not fire" is only observable
 * with a `UFUNCTION` binding and this module declares no `UCLASS`. What is observable, and is
 * the same claim, is that the refusal produces no server message to handle at all: after it
 * the component holds no row, no field name, an idle query phase and an untouched connection
 * state, none of which can be true if `HandleServerMessage` ran.
 *
 * Nothing here opens a socket, resolves a host or needs a server.
 */

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#include "BoltPackStream.h"
#include "Neo4jTestAccess.h"

namespace FantasiaSecurityBolt
{
constexpr EAutomationTestFlags TestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

/** Substring common to every reader refusal; see FBoltPackStreamReader::Refuse. */
inline const TCHAR* ReaderRefusalLog = TEXT("BoltPackStream: refused a");

/**
 * The nesting limit C-neo4j states, restated here because the constant is file-local to
 * BoltPackStream.cpp. A limit a test reads out of the implementation is not a limit.
 */
constexpr int32 StatedNestingDepth = 64;

/**
 * Reads one value from @p Buffer and asserts it is a refusal: a null value is returned, and
 * the reader reports itself exhausted so no caller can read on past the damage.
 */
void CheckRefused(FAutomationTestBase& Test, const TCHAR* What, const TArray<uint8>& Buffer)
{
	FBoltPackStreamReader Reader(Buffer);
	const FBoltValuePtr Value = Reader.ReadValue();

	Test.TestTrue(*FString::Printf(TEXT("%s: a value is returned rather than nothing"), What), Value.IsValid());
	if (Value.IsValid())
	{
		Test.TestEqual(*FString::Printf(TEXT("%s: the value is null"), What), Value->Type, EBoltValueType::Null);
	}
	Test.TestFalse(*FString::Printf(TEXT("%s: the reader reports itself exhausted"), What), Reader.HasMore());
}

/** Reads one value from @p Buffer and asserts it decodes: @p ExpectedType, reader exhausted. */
FBoltValuePtr CheckDecodes(FAutomationTestBase& Test, const TCHAR* What, const TArray<uint8>& Buffer,
	EBoltValueType ExpectedType)
{
	FBoltPackStreamReader Reader(Buffer);
	const FBoltValuePtr Value = Reader.ReadValue();

	Test.TestTrue(*FString::Printf(TEXT("%s: a value is returned"), What), Value.IsValid());
	if (Value.IsValid())
	{
		Test.TestEqual(*FString::Printf(TEXT("%s: the expected type"), What), Value->Type, ExpectedType);
	}
	Test.TestFalse(*FString::Printf(TEXT("%s: the value consumed the whole buffer"), What), Reader.HasMore());
	return Value;
}

/** A buffer of @p Count repeated @p Byte values. */
TArray<uint8> Repeated(uint8 Byte, int32 Count)
{
	TArray<uint8> Buffer;
	Buffer.Init(Byte, Count);
	return Buffer;
}

/**
 * One Bolt chunk that is not a message terminator: a 2-byte big-endian payload length, then
 * that many zero bytes. A stream of these never carries the zero-length chunk that ends a
 * message, which is the shape F-021's fourth path arrives in.
 */
TArray<uint8> UnterminatedChunk(int32 PayloadBytes)
{
	TArray<uint8> Chunk;
	Chunk.Reserve(PayloadBytes + 2);
	Chunk.Add(static_cast<uint8>((PayloadBytes >> 8) & 0xFF));
	Chunk.Add(static_cast<uint8>(PayloadBytes & 0xFF));
	Chunk.AddZeroed(PayloadBytes);
	return Chunk;
}

/** A fresh component that has never connected. Its dechunker is built by the constructor. */
TStrongObjectPtr<UNeo4jComponent> MakeComponent()
{
	return TStrongObjectPtr<UNeo4jComponent>(
		NewObject<UNeo4jComponent>(GetTransientPackage(), NAME_None, RF_Transient));
}

/**
 * Asserts the component is in the state a refusal leaves behind: nothing decoded, nothing
 * buffered towards a response, the connection still standing (DEC-021 discards the partial
 * message rather than tearing the connection down).
 */
void CheckNoResponseAccumulated(FAutomationTestBase& Test, const UNeo4jComponent& Component)
{
	Test.TestEqual(TEXT("the partial message is discarded"),
		FNeo4jTestAccess::UnresolvedMessageBytes(Component), 0);
	Test.TestEqual(TEXT("no row is buffered towards a response"),
		FNeo4jTestAccess::NumBufferedRecords(Component), 0);
	Test.TestEqual(TEXT("no field name is buffered towards a response"),
		FNeo4jTestAccess::NumFieldNames(Component), 0);
	Test.TestEqual(TEXT("the query phase is untouched, so no server message was handled"),
		static_cast<int32>(FNeo4jTestAccess::QueryPhase(Component)),
		static_cast<int32>(EBoltQueryPhase::Idle));
	Test.TestEqual(TEXT("the connection is left standing"),
		static_cast<int32>(FNeo4jTestAccess::ConnectionState(Component)),
		static_cast<int32>(EBoltConnectionState::Disconnected));
}
} // namespace FantasiaSecurityBolt

// ============================================================================
// Out-of-bounds read: a read at or past the end of the buffer
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsEmptyBuffer,
	"FANTASIA.Security.Bolt.Reader.EmptyBuffer", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsEmptyBuffer::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;

	// The marker read is the reader's first act, and on an empty TArray the base pointer is
	// null, so this is a null dereference rather than a mere over-read.
	AddExpectedErrorPlain(ReaderRefusalLog, EAutomationExpectedErrorFlags::Contains, 1);

	const TArray<uint8> Empty;
	FBoltPackStreamReader Before(Empty);
	TestFalse(TEXT("an empty buffer has nothing to read"), Before.HasMore());
	TestEqual(TEXT("peeking an empty buffer yields no marker"), Before.PeekMarker(), static_cast<uint8>(0));

	CheckRefused(*this, TEXT("empty buffer"), Empty);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsTruncatedEightBit,
	"FANTASIA.Security.Bolt.Reader.TruncatedEightBit", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsTruncatedEightBit::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;
	AddExpectedErrorPlain(ReaderRefusalLog, EAutomationExpectedErrorFlags::Contains, 2);

	// Int8 marker with its payload byte missing.
	CheckRefused(*this, TEXT("0xC8 with no payload"), TArray<uint8>{ 0xC8 });
	// String8 marker with its length byte missing.
	CheckRefused(*this, TEXT("0xD0 with no length byte"), TArray<uint8>{ 0xD0 });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsTruncatedSixteenBit,
	"FANTASIA.Security.Bolt.Reader.TruncatedSixteenBit", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsTruncatedSixteenBit::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;
	AddExpectedErrorPlain(ReaderRefusalLog, EAutomationExpectedErrorFlags::Contains, 2);

	// Int16 with one of its two payload bytes present: the 16-bit reader indexes
	// Data.GetData() + Cursor for two bytes with one left in the buffer.
	CheckRefused(*this, TEXT("0xC9 truncated mid-value"), TArray<uint8>{ 0xC9, 0x01 });
	// The same reader again, reading a String16 length.
	CheckRefused(*this, TEXT("0xD1 with half a length"), TArray<uint8>{ 0xD1, 0x00 });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsTruncatedThirtyTwoBit,
	"FANTASIA.Security.Bolt.Reader.TruncatedThirtyTwoBit", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsTruncatedThirtyTwoBit::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;
	AddExpectedErrorPlain(ReaderRefusalLog, EAutomationExpectedErrorFlags::Contains, 2);

	CheckRefused(*this, TEXT("0xCA truncated mid-value"), TArray<uint8>{ 0xCA, 0x01, 0x02 });
	CheckRefused(*this, TEXT("0xD2 with three of four length bytes"),
		TArray<uint8>{ 0xD2, 0x00, 0x00, 0x01 });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsTruncatedSixtyFourBit,
	"FANTASIA.Security.Bolt.Reader.TruncatedSixtyFourBit", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsTruncatedSixtyFourBit::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;
	AddExpectedErrorPlain(ReaderRefusalLog, EAutomationExpectedErrorFlags::Contains, 2);

	CheckRefused(*this, TEXT("0xCB truncated mid-value"),
		TArray<uint8>{ 0xCB, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 });
	// Float64 reads through the same 64-bit path.
	CheckRefused(*this, TEXT("0xC1 truncated mid-value"),
		TArray<uint8>{ 0xC1, 0x01, 0x02, 0x03, 0x04 });
	return true;
}

// ============================================================================
// Attacker-chosen length and count: a value claiming more than the buffer holds
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsStringLengthBeyondBuffer,
	"FANTASIA.Security.Bolt.Reader.StringLengthBeyondBuffer", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsStringLengthBeyondBuffer::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;
	AddExpectedErrorPlain(ReaderRefusalLog, EAutomationExpectedErrorFlags::Contains, 4);

	CheckRefused(*this, TEXT("tiny string claiming 15 bytes of an empty tail"), TArray<uint8>{ 0x8F });
	CheckRefused(*this, TEXT("0xD0 claiming 255 bytes of one"), TArray<uint8>{ 0xD0, 0xFF, 0x61 });
	CheckRefused(*this, TEXT("0xD1 claiming 65535 bytes of one"),
		TArray<uint8>{ 0xD1, 0xFF, 0xFF, 0x61 });
	// F-021's reproduction: a 5-byte message asking for a 2 GB string. The bytes it would
	// return are whatever follows the message buffer on the heap, and for a RECORD field they
	// reach Blueprint, so this is information disclosure as well as a crash.
	CheckRefused(*this, TEXT("0xD2 claiming 2 GB of nothing"),
		TArray<uint8>{ 0xD2, 0x7F, 0xFF, 0xFF, 0xFF });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsBytesLengthBeyondBuffer,
	"FANTASIA.Security.Bolt.Reader.BytesLengthBeyondBuffer", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsBytesLengthBeyondBuffer::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;
	AddExpectedErrorPlain(ReaderRefusalLog, EAutomationExpectedErrorFlags::Contains, 3);

	CheckRefused(*this, TEXT("0xCC claiming 255 bytes of one"), TArray<uint8>{ 0xCC, 0xFF, 0x00 });
	CheckRefused(*this, TEXT("0xCD claiming 65535 bytes of one"),
		TArray<uint8>{ 0xCD, 0xFF, 0xFF, 0x00 });
	CheckRefused(*this, TEXT("0xCE claiming 2 GB of nothing"),
		TArray<uint8>{ 0xCE, 0x7F, 0xFF, 0xFF, 0xFF });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsListCountBeyondBuffer,
	"FANTASIA.Security.Bolt.Reader.ListCountBeyondBuffer", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsListCountBeyondBuffer::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;
	AddExpectedErrorPlain(ReaderRefusalLog, EAutomationExpectedErrorFlags::Contains, 4);

	// Every element of a list costs at least one byte, so a count larger than the bytes
	// remaining cannot be met and must be rejected before anything is reserved for it.
	CheckRefused(*this, TEXT("tiny list claiming 15 elements of none"), TArray<uint8>{ 0x9F });
	CheckRefused(*this, TEXT("0xD4 claiming 255 elements of none"), TArray<uint8>{ 0xD4, 0xFF });
	CheckRefused(*this, TEXT("0xD5 claiming 65535 elements of none"),
		TArray<uint8>{ 0xD5, 0xFF, 0xFF });
	CheckRefused(*this, TEXT("0xD6 claiming 2 billion elements of none"),
		TArray<uint8>{ 0xD6, 0x7F, 0xFF, 0xFF, 0xFF });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsMapCountBeyondBuffer,
	"FANTASIA.Security.Bolt.Reader.MapCountBeyondBuffer", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsMapCountBeyondBuffer::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;
	AddExpectedErrorPlain(ReaderRefusalLog, EAutomationExpectedErrorFlags::Contains, 4);

	CheckRefused(*this, TEXT("tiny map claiming 15 pairs of none"), TArray<uint8>{ 0xAF });
	CheckRefused(*this, TEXT("0xD8 claiming 255 pairs of none"), TArray<uint8>{ 0xD8, 0xFF });
	CheckRefused(*this, TEXT("0xD9 claiming 65535 pairs of none"), TArray<uint8>{ 0xD9, 0xFF, 0xFF });
	CheckRefused(*this, TEXT("0xDA claiming 2 billion pairs of none"),
		TArray<uint8>{ 0xDA, 0x7F, 0xFF, 0xFF, 0xFF });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsStructureFieldCountBeyondBuffer,
	"FANTASIA.Security.Bolt.Reader.StructureFieldCountBeyondBuffer", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsStructureFieldCountBeyondBuffer::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;
	AddExpectedErrorPlain(ReaderRefusalLog, EAutomationExpectedErrorFlags::Contains, 2);

	// US-030's criterion names string, bytes, list and map. A structure's field count is the
	// same primitive read from the same wire and is asserted here for that reason: its cost
	// is one tag byte plus at least one byte per field.
	CheckRefused(*this, TEXT("0xBF claiming 15 fields and no tag"), TArray<uint8>{ 0xBF });
	CheckRefused(*this, TEXT("0xB1 whose single field is not there"), TArray<uint8>{ 0xB1, 0x70 });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsNegativeThirtyTwoBitCount,
	"FANTASIA.Security.Bolt.Reader.NegativeThirtyTwoBitCount", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsNegativeThirtyTwoBitCount::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;
	AddExpectedErrorPlain(ReaderRefusalLog, EAutomationExpectedErrorFlags::Contains, 4);

	// The 32-bit lengths and counts are read as signed int32, so 0xFFFFFFFF is -1 and a
	// comparison against the bytes remaining passes trivially. A negative value must be
	// refused in its own right, before it reaches Reserve or Append.
	CheckRefused(*this, TEXT("0xD2 with a negative length"),
		TArray<uint8>{ 0xD2, 0xFF, 0xFF, 0xFF, 0xFF });
	CheckRefused(*this, TEXT("0xCE with a negative length"),
		TArray<uint8>{ 0xCE, 0xFF, 0xFF, 0xFF, 0xFF });
	CheckRefused(*this, TEXT("0xD6 with a negative count"),
		TArray<uint8>{ 0xD6, 0xFF, 0xFF, 0xFF, 0xFF });
	CheckRefused(*this, TEXT("0xDA with a negative count"),
		TArray<uint8>{ 0xDA, 0xFF, 0xFF, 0xFF, 0xFF });
	return true;
}

// ============================================================================
// Unbounded recursion: nesting one container per byte
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsNestingPastDepthLimit,
	"FANTASIA.Security.Bolt.Reader.NestingPastDepthLimit", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsNestingPastDepthLimit::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;

	// The limit must be stated, so the test matches the number in the message rather than
	// merely that a refusal happened. A reader that ran out of buffer instead of reaching a
	// depth limit would log the byte-count reason and fail here.
	const FString DepthRefusal = FString::Printf(TEXT("nested past %d containers"), StatedNestingDepth);
	AddExpectedErrorPlain(DepthRefusal, EAutomationExpectedErrorFlags::Contains, 2);

	// 0x91 is a one-element list, so N of them are N nested lists and one stack frame per
	// byte. Messages are assembled across chunks, so N is not bounded by the 16 KiB chunk
	// size and the result is an unrecoverable stack overflow.
	CheckRefused(*this, TEXT("200 nested single-element lists"), Repeated(0x91, 200));

	// One past the limit, with a value at the bottom that would otherwise decode: the
	// refusal is the depth and nothing else.
	TArray<uint8> OnePastLimit = Repeated(0x91, StatedNestingDepth);
	OnePastLimit.Add(0x01);
	CheckRefused(*this, TEXT("one container past the limit"), OnePastLimit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsNestingAtDepthLimitDecodes,
	"FANTASIA.Security.Bolt.Reader.NestingAtDepthLimitDecodes", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsNestingAtDepthLimitDecodes::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;

	// The dual of the test above, and the reason the limit is a limit rather than a refusal
	// of all nesting. Any log at all fails this test: no expected error is declared.
	TArray<uint8> AtLimit = Repeated(0x91, StatedNestingDepth - 1);
	AtLimit.Add(0x01);

	const FBoltValuePtr Value =
		CheckDecodes(*this, TEXT("nesting within the limit"), AtLimit, EBoltValueType::List);
	if (!Value.IsValid())
	{
		return true;
	}

	FBoltValuePtr Node = Value;
	int32 Levels = 0;
	while (Node.IsValid() && Node->Type == EBoltValueType::List && Node->ListVal.Num() == 1)
	{
		Node = Node->ListVal[0];
		++Levels;
	}
	TestEqual(TEXT("every container within the limit decoded"), Levels, StatedNestingDepth - 1);
	TestTrue(TEXT("the innermost value exists"), Node.IsValid());
	if (Node.IsValid())
	{
		TestEqual(TEXT("the innermost value is the int that ended the buffer"),
			Node->Type, EBoltValueType::Int);
		TestEqual(TEXT("the innermost value is 1"), Node->IntVal, static_cast<int64>(1));
	}
	return true;
}

// ============================================================================
// One refusal, one log, and nothing partly decoded returned
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsRefusalLoggedOnceAndPropagates,
	"FANTASIA.Security.Bolt.Reader.RefusalLoggedOnceAndPropagates", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsRefusalLoggedOnceAndPropagates::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;

	// Exactly one, across a refusal three levels down and two further reads afterwards. A
	// reader that logged per recursion level, or per later call, fails here.
	AddExpectedErrorPlain(ReaderRefusalLog, EAutomationExpectedErrorFlags::Contains, 1);

	// Three nested one-element lists whose innermost value is a truncated Int16.
	const TArray<uint8> Buffer{ 0x91, 0x91, 0x91, 0xC9, 0x01 };
	FBoltPackStreamReader Reader(Buffer);

	const FBoltValuePtr First = Reader.ReadValue();
	TestTrue(TEXT("a value is returned"), First.IsValid());
	if (First.IsValid())
	{
		// Not a list holding a null: a partially decoded value is never returned, at any
		// level of the recursion.
		TestEqual(TEXT("the refusal propagates out as a null value"), First->Type, EBoltValueType::Null);
	}
	TestFalse(TEXT("the reader reports itself exhausted"), Reader.HasMore());

	for (int32 Attempt = 0; Attempt < 2; ++Attempt)
	{
		const FBoltValuePtr Again = Reader.ReadValue();
		TestTrue(TEXT("a later read still returns a value"), Again.IsValid());
		if (Again.IsValid())
		{
			TestEqual(TEXT("a later read refuses too"), Again->Type, EBoltValueType::Null);
		}
		TestFalse(TEXT("the reader stays exhausted"), Reader.HasMore());
	}
	return true;
}

// ============================================================================
// The boundary is one byte wide: what fits decodes, what does not is refused
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsExactFitDecodes,
	"FANTASIA.Security.Bolt.Reader.ExactFitDecodes", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsExactFitDecodes::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;

	// No expected error is declared, so any refusal fails this test. These are the tightest
	// well-formed shapes the encoding has: each ends on the buffer's last byte, which is
	// where an off-by-one in the bounds check would show.
	CheckDecodes(*this, TEXT("an empty list header alone"), TArray<uint8>{ 0x90 }, EBoltValueType::List);
	CheckDecodes(*this, TEXT("an empty map header alone"), TArray<uint8>{ 0xA0 }, EBoltValueType::Map);

	const FBoltValuePtr Full = CheckDecodes(*this, TEXT("a 15-element tiny list exactly filled"),
		TArray<uint8>{ 0x9F, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
		EBoltValueType::List);
	if (Full.IsValid())
	{
		TestEqual(TEXT("all 15 elements decoded"), Full->ListVal.Num(), 15);
	}

	const FBoltValuePtr Struct = CheckDecodes(*this, TEXT("a structure whose fields end the buffer"),
		TArray<uint8>{ 0xB2, 0x70, 0x01, 0x02 }, EBoltValueType::Structure);
	if (Struct.IsValid())
	{
		TestEqual(TEXT("the tag is read"), Struct->StructVal.Tag, static_cast<uint8>(0x70));
		TestEqual(TEXT("both fields decoded"), Struct->StructVal.Fields.Num(), 2);
	}

	const FBoltValuePtr Str8 = CheckDecodes(*this, TEXT("a String8 that ends the buffer"),
		TArray<uint8>{ 0xD0, 0x03, 0x61, 0x62, 0x63 }, EBoltValueType::String);
	if (Str8.IsValid())
	{
		TestEqual(TEXT("the string is abc"), Str8->StringVal, FString(TEXT("abc")));
	}

	const FBoltValuePtr Str32 = CheckDecodes(*this, TEXT("a String32 that ends the buffer"),
		TArray<uint8>{ 0xD2, 0x00, 0x00, 0x00, 0x03, 0x61, 0x62, 0x63 }, EBoltValueType::String);
	if (Str32.IsValid())
	{
		TestEqual(TEXT("the string is abc"), Str32->StringVal, FString(TEXT("abc")));
	}

	const FBoltValuePtr Raw = CheckDecodes(*this, TEXT("a Bytes8 that ends the buffer"),
		TArray<uint8>{ 0xCC, 0x02, 0xDE, 0xAD }, EBoltValueType::Bytes);
	if (Raw.IsValid())
	{
		TestEqual(TEXT("both bytes decoded"), Raw->BytesVal.Num(), 2);
	}

	const FBoltValuePtr Map = CheckDecodes(*this, TEXT("a one-pair map that ends the buffer"),
		TArray<uint8>{ 0xA1, 0x81, 0x6B, 0x2A }, EBoltValueType::Map);
	if (Map.IsValid() && TestEqual(TEXT("the map holds one pair"), Map->MapVal.Num(), 1))
	{
		const FBoltValuePtr* Entry = Map->MapVal.Find(TEXT("k"));
		if (TestNotNull(TEXT("the key decoded"), Entry) && Entry->IsValid())
		{
			TestEqual(TEXT("the value decoded"), (*Entry)->IntVal, static_cast<int64>(42));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsOneByteShortIsRefused,
	"FANTASIA.Security.Bolt.Reader.OneByteShortIsRefused", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsOneByteShortIsRefused::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;
	AddExpectedErrorPlain(ReaderRefusalLog, EAutomationExpectedErrorFlags::Contains, 5);

	// Each of these is a shape from Reader.ExactFitDecodes with its last byte removed.
	CheckRefused(*this, TEXT("a 15-element tiny list one element short"),
		TArray<uint8>{ 0x9F, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 });
	CheckRefused(*this, TEXT("a structure one field short"), TArray<uint8>{ 0xB2, 0x70, 0x01 });
	CheckRefused(*this, TEXT("a String8 one byte short"), TArray<uint8>{ 0xD0, 0x03, 0x61, 0x62 });
	CheckRefused(*this, TEXT("a String32 one byte short"),
		TArray<uint8>{ 0xD2, 0x00, 0x00, 0x00, 0x03, 0x61, 0x62 });
	CheckRefused(*this, TEXT("a Bytes8 one byte short"), TArray<uint8>{ 0xCC, 0x02, 0xDE });
	return true;
}

// ============================================================================
// Unbounded accumulation: a message that never ends
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsUnterminatedMessageRefused,
	"FANTASIA.Security.Bolt.MessageCap.UnterminatedMessageRefused", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsUnterminatedMessageRefused::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;

	const int32 Cap = FNeo4jTestAccess::MaxBoltMessageBytes();

	// The maximum must be stated, so the match is built from the cap the component enforces
	// and the component's own message has to name it.
	AddExpectedErrorPlain(FString::Printf(TEXT("past the %d-byte limit"), Cap),
		EAutomationExpectedErrorFlags::Contains, 1);

	TestEqual(TEXT("the stated maximum message size is 16 MiB"), Cap, 16 * 1024 * 1024);

	// A server that opens a message and never sends the zero-length chunk that ends it, read
	// off the wire one transport-sized read at a time, which is how F-021's fourth path
	// arrives: the dechunker accumulates until the process dies.
	const TArray<uint8> Chunk = UnterminatedChunk(16380);
	const TStrongObjectPtr<UNeo4jComponent> Component = MakeComponent();
	if (!TestTrue(TEXT("the component is created"), Component.IsValid()))
	{
		return true;
	}

	const int32 ReadLimit = Cap / Chunk.Num() + 8;
	int32 Reads = 0;
	int32 BytesHandedOver = 0;
	int32 UnresolvedBeforeRefusal = -1;

	while (Reads < ReadLimit && UnresolvedBeforeRefusal < 0)
	{
		const int32 Before = FNeo4jTestAccess::UnresolvedMessageBytes(*Component);
		FNeo4jTestAccess::FeedTransportBytes(*Component, Chunk);
		++Reads;
		BytesHandedOver += Chunk.Num();

		// No message can complete in this stream, so the counter only ever grows — unless
		// the component refused the read and discarded the partial message.
		if (FNeo4jTestAccess::UnresolvedMessageBytes(*Component) == 0)
		{
			UnresolvedBeforeRefusal = Before;
		}
	}

	TestTrue(TEXT("accumulation stops rather than running until the process dies"),
		UnresolvedBeforeRefusal >= 0);
	TestTrue(TEXT("nothing is accumulated past the stated maximum"),
		UnresolvedBeforeRefusal <= Cap);
	TestTrue(TEXT("the refusal comes no later than one transport read past the maximum"),
		BytesHandedOver <= Cap + Chunk.Num());

	CheckNoResponseAccumulated(*this, *Component);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoltBoundsMessageCapIsExact,
	"FANTASIA.Security.Bolt.MessageCap.CapIsExact", FantasiaSecurityBolt::TestFlags)
bool FBoltBoundsMessageCapIsExact::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityBolt;

	const int32 Cap = FNeo4jTestAccess::MaxBoltMessageBytes();
	AddExpectedErrorPlain(FString::Printf(TEXT("past the %d-byte limit"), Cap),
		EAutomationExpectedErrorFlags::Contains, 1);

	// Exactly the cap in non-terminating chunk framing: 65535-byte chunks, the last one
	// deliberately short of its declared length so the dechunker waits rather than
	// completing a message. No zero-length chunk appears anywhere in it.
	TArray<uint8> AtCap;
	AtCap.Reserve(Cap);
	while (AtCap.Num() + 2 <= Cap)
	{
		AtCap.Add(0xFF);
		AtCap.Add(0xFF);
		AtCap.AddZeroed(FMath::Min(65535, Cap - AtCap.Num()));
	}
	while (AtCap.Num() < Cap)
	{
		AtCap.Add(0x00);
	}
	TestEqual(TEXT("the first read is exactly the stated maximum"), AtCap.Num(), Cap);

	const TStrongObjectPtr<UNeo4jComponent> Component = MakeComponent();
	if (!TestTrue(TEXT("the component is created"), Component.IsValid()))
	{
		return true;
	}

	// The cap is a maximum, not a smaller number enforced early: a message of exactly the
	// stated size is accumulated.
	FNeo4jTestAccess::FeedTransportBytes(*Component, AtCap);
	TestEqual(TEXT("a message of exactly the maximum is accumulated"),
		FNeo4jTestAccess::UnresolvedMessageBytes(*Component), Cap);

	// One byte more, and the partial message goes.
	FNeo4jTestAccess::FeedTransportBytes(*Component, TArray<uint8>{ 0x00 });
	CheckNoResponseAccumulated(*this, *Component);
	return true;
}
