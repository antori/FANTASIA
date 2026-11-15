/**
 * @file Neo4jReceivePathQualityTests.cpp
 * @brief US-030 / F-021, tester pass: the receive path's message cap, driven the way a socket drives it.
 *
 * `Private/Security/BoltPackStreamBoundsTests.cpp` pins the cap from the hostile side: a stream
 * that never terminates is refused at 16 MiB. This file asks the questions the cap raises for a
 * *legitimate* server, and what the refusal leaves behind. Everything goes through
 * `FNeo4jTestAccess::FeedTransportBytes`, one call per transport read, with no socket, no server
 * and no connection; the component sits in its constructed `Disconnected` state, where a
 * decoded message is dropped by `HandleServerMessage`. A message is therefore "delivered" when
 * the dechunker completed it (the unresolved-byte counter returns to zero) and parsing it logged
 * nothing; a stalled or garbled stream shows up as a non-zero counter or a parse error.
 *
 *   - `Neo4j.Cap.LargeResultSetIsNeverRefused`: more than 16 MiB in total, as many small RECORD
 *     messages, is never refused. The cap bounds a message, not a result set.
 *   - `Neo4j.Cap.LargeMessageUnderCapIsDelivered`: one message of 15 MiB arrives, and the message
 *     behind it is delivered intact.
 *   - `Neo4j.Cap.UnderCapMessageRefusedWhenReadStraddles` pins a boundary effect of the counter: it
 *     counts bytes handed over, so a read that finishes a message under the cap and carries the
 *     first bytes of the next one can push the count past it. Logged as F-057.
 *   - `Neo4j.Cap.RefusalLeavesStreamUnrecovered` pins F-048 from the stress side: after a refusal
 *     the connection is standing, and decodes nothing the server sends next.
 *
 * Nothing here opens a socket. Payload bytes are 'a' (0x61), never zero, which is what text and
 * identifiers look like on the wire and the case in which a misaligned dechunker has nothing to
 * accidentally realign on.
 */

#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#include "BoltPackStream.h"
#include "../Security/Neo4jTestAccess.h"
#include "SecretTestSupport.h"

namespace FantasiaQualityNeo4j
{
using FantasiaQuality::FLogCapture;

constexpr EAutomationTestFlags TestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

/** The size of one transport read on the plain TCP path (the recv buffer in TcpRecvLoop). */
constexpr int32 TransportRead = 16384;

/** A fresh component that has never connected. Its dechunker is built by the constructor. */
TStrongObjectPtr<UNeo4jComponent> MakeComponent()
{
	return TStrongObjectPtr<UNeo4jComponent>(
		NewObject<UNeo4jComponent>(GetTransientPackage(), NAME_None, RF_Transient));
}

/** Frame @p Payload as Bolt chunks of at most @p MaxChunk bytes, ended by a zero-length chunk. */
TArray<uint8> Frame(const TArray<uint8>& Payload, int32 MaxChunk = TransportRead)
{
	TArray<uint8> Out;
	Out.Reserve(Payload.Num() + (Payload.Num() / MaxChunk + 2) * 2);
	for (int32 Offset = 0; Offset < Payload.Num(); Offset += MaxChunk)
	{
		const int32 N = FMath::Min(MaxChunk, Payload.Num() - Offset);
		Out.Add(static_cast<uint8>(N >> 8));
		Out.Add(static_cast<uint8>(N & 0xFF));
		Out.Append(Payload.GetData() + Offset, N);
	}
	Out.Add(0);
	Out.Add(0);
	return Out;
}

/** A RECORD whose single field is a string of @p Chars 'a' - valid PackStream, built by hand. */
TArray<uint8> RecordWithString(int32 Chars)
{
	TArray<uint8> Payload;
	Payload.Reserve(Chars + 8);
	Payload.Add(0xB1);
	Payload.Add(0x71);
	Payload.Add(0xD2);
	Payload.Add(static_cast<uint8>((Chars >> 24) & 0xFF));
	Payload.Add(static_cast<uint8>((Chars >> 16) & 0xFF));
	Payload.Add(static_cast<uint8>((Chars >> 8) & 0xFF));
	Payload.Add(static_cast<uint8>(Chars & 0xFF));
	Payload.AddUninitialized(Chars);
	FMemory::Memset(Payload.GetData() + Payload.Num() - Chars, 'a', Chars);
	return Payload;
}

/** A small SUCCESS message, framed. */
TArray<uint8> FramedSuccess(const TCHAR* Key)
{
	FBoltPackStreamWriter W;
	W.WriteStructureHeader(0x70, 1);
	W.WriteMapHeader(1);
	W.WriteString(Key);
	W.WriteInt(1);
	return Frame(W.GetBuffer());
}

/** Hand @p Bytes[From..] to the component in reads of @p ReadSize; returns the largest counter seen. */
int32 FeedInReads(UNeo4jComponent& Component, const TArray<uint8>& Bytes, int32 From, int32 ReadSize)
{
	int32 MaxUnresolved = 0;
	TArray<uint8> Read;
	Read.Reserve(ReadSize);
	for (int32 Offset = From; Offset < Bytes.Num(); Offset += ReadSize)
	{
		const int32 N = FMath::Min(ReadSize, Bytes.Num() - Offset);
		Read.Reset();
		Read.Append(Bytes.GetData() + Offset, N);
		FNeo4jTestAccess::FeedTransportBytes(Component, Read);
		MaxUnresolved = FMath::Max(MaxUnresolved, FNeo4jTestAccess::UnresolvedMessageBytes(Component));
	}
	return MaxUnresolved;
}

/** Lines the receive path logs when it refuses, or fails to parse, what it was handed. */
int32 TroubleLines(const FLogCapture& Log)
{
	return Log.CountContaining(TEXT("Bolt: "))
		+ Log.CountContaining(TEXT("BoltPackStream:"))
		+ Log.CountContaining(TEXT("-byte limit"));
}

/** Feed one framed message as one read; true when it completed and logged no trouble. */
bool DeliveredIntact(UNeo4jComponent& Component, const FLogCapture& Log, const TArray<uint8>& Framed)
{
	const int32 Before = TroubleLines(Log);
	FNeo4jTestAccess::FeedTransportBytes(Component, Framed);
	return FNeo4jTestAccess::UnresolvedMessageBytes(Component) == 0 && TroubleLines(Log) == Before;
}
} // namespace FantasiaQualityNeo4j

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualityNeo4jLargeResultSetIsNeverRefused,
	"FANTASIA.Quality.Neo4j.Cap.LargeResultSetIsNeverRefused", FantasiaQualityNeo4j::TestFlags)
bool FQualityNeo4jLargeResultSetIsNeverRefused::RunTest(const FString& Parameters)
{
	using namespace FantasiaQualityNeo4j;

	// A PULL answers with one RECORD per row. Seventeen thousand five hundred rows of a thousand
	// characters each is more than the cap in total and nowhere near it per message. No error is
	// expected: any Error-level log fails this test, a cap refusal included.
	const int32 Cap = FNeo4jTestAccess::MaxBoltMessageBytes();

	TArray<uint8> Stream;
	Stream.Reserve(Cap + Cap / 8);
	for (int32 Row = 0; Row < 17500; ++Row)
	{
		FBoltPackStreamWriter W;
		W.WriteStructureHeader(0x71, 1);
		W.WriteListHeader(1);
		W.WriteString(FString::ChrN(1000, TEXT('r')));
		Stream.Append(Frame(W.GetBuffer()));
	}
	if (!TestTrue(TEXT("the result set is larger than the cap in total"), Stream.Num() > Cap))
	{
		return true;
	}

	const TStrongObjectPtr<UNeo4jComponent> Component = MakeComponent();
	if (!TestTrue(TEXT("the component is created"), Component.IsValid()))
	{
		return true;
	}

	const double Start = FPlatformTime::Seconds();
	const int32 MaxUnresolved = FeedInReads(*Component, Stream, 0, TransportRead);
	const double Seconds = FPlatformTime::Seconds() - Start;

	TestEqual(TEXT("every message completed: nothing is left in flight"),
		FNeo4jTestAccess::UnresolvedMessageBytes(*Component), 0);
	TestTrue(TEXT("the in-flight count never exceeded two reads, so the cap never came near"),
		MaxUnresolved <= 2 * TransportRead);
	AddInfo(FString::Printf(TEXT("%d bytes in 17500 messages through the receive path in %.3f s"),
		Stream.Num(), Seconds));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualityNeo4jLargeMessageUnderCapIsDelivered,
	"FANTASIA.Quality.Neo4j.Cap.LargeMessageUnderCapIsDelivered", FantasiaQualityNeo4j::TestFlags)
bool FQualityNeo4jLargeMessageUnderCapIsDelivered::RunTest(const FString& Parameters)
{
	using namespace FantasiaQualityNeo4j;

	// 15 MiB in one message: the largest row the cap admits with room to spare. It completes, and
	// the stream behind it is not disturbed.
	const TArray<uint8> Big = Frame(RecordWithString(15 * 1024 * 1024));
	const TStrongObjectPtr<UNeo4jComponent> Component = MakeComponent();
	if (!TestTrue(TEXT("the component is created"), Component.IsValid()))
	{
		return true;
	}
	FLogCapture Log;

	const int32 Peak = FeedInReads(*Component, Big, 0, TransportRead);
	TestEqual(TEXT("the large message completed"), FNeo4jTestAccess::UnresolvedMessageBytes(*Component), 0);
	TestTrue(TEXT("it was accumulated, so the counter climbed to within a read of its 15 MiB"), Peak > 14 * 1024 * 1024);
	TestTrue(TEXT("and stayed under the cap"), Peak <= FNeo4jTestAccess::MaxBoltMessageBytes());
	TestTrue(TEXT("the message behind it is delivered intact"),
		DeliveredIntact(*Component, Log, FramedSuccess(TEXT("after_big"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualityNeo4jUnderCapMessageRefusedWhenReadStraddles,
	"FANTASIA.Quality.Neo4j.Cap.UnderCapMessageRefusedWhenReadStraddles", FantasiaQualityNeo4j::TestFlags)
bool FQualityNeo4jUnderCapMessageRefusedWhenReadStraddles::RunTest(const FString& Parameters)
{
	using namespace FantasiaQualityNeo4j;

	// F-057. The counter is bytes handed to the dechunker since it last completed a message, and
	// the check runs on the whole read, so a read that ends one message and starts the next is
	// charged to the first. A message of Cap - 5 bytes is under the cap, and whether it is
	// delivered depends on where the transport happened to cut the stream.
	const int32 Cap = FNeo4jTestAccess::MaxBoltMessageBytes();

	// Framing adds a 2-byte header per 16384-byte chunk and a 2-byte terminator. 1024 chunks
	// carry Cap - 2055 payload bytes, which frames to exactly Cap - 5.
	const int32 PayloadBytes = Cap - 2055;
	const int32 Chars = PayloadBytes - 7; // 0xB1 0x71 0xD2 and a 4-byte length precede the text
	const TArray<uint8> Message = Frame(RecordWithString(Chars));
	if (!TestEqual(TEXT("the message is 5 bytes under the cap"), Message.Num(), Cap - 5))
	{
		return true;
	}

	TArray<uint8> Stream = Message;
	Stream.Append(FramedSuccess(TEXT("next")));

	// Cut A: reads of 16384 from the start. The read that ends the message is charged exactly
	// the cap, which is allowed.
	{
		const TStrongObjectPtr<UNeo4jComponent> Component = MakeComponent();
		FLogCapture Log;
		FeedInReads(*Component, Stream, 0, TransportRead);
		TestEqual(TEXT("aligned reads: no refusal"), Log.CountContaining(TEXT("-byte limit")), 0);
		TestEqual(TEXT("aligned reads: the stream completed"),
			FNeo4jTestAccess::UnresolvedMessageBytes(*Component), 0);
	}

	// Cut B: the same bytes, the first read one byte long. Every later read is shifted by one, so
	// the read that ends the message also carries 6 bytes of the next, and the count passes the
	// cap by one. The message is the same size and is refused.
	{
		const FString CapSentence = FString::Printf(TEXT("past the %d-byte limit"), Cap);
		AddExpectedErrorPlain(CapSentence, EAutomationExpectedErrorFlags::Contains, 1);
		// The bytes after the refused read are read from an arbitrary offset and may not parse.
		AddExpectedErrorPlain(TEXT("Bolt: "), EAutomationExpectedErrorFlags::Contains, -1);
		AddExpectedErrorPlain(TEXT("BoltPackStream:"), EAutomationExpectedErrorFlags::Contains, -1);

		const TStrongObjectPtr<UNeo4jComponent> Component = MakeComponent();
		FLogCapture Log;
		TArray<uint8> One;
		One.Add(Stream[0]);
		FNeo4jTestAccess::FeedTransportBytes(*Component, One);
		FeedInReads(*Component, Stream, 1, TransportRead);
		TestEqual(TEXT("shifted by one byte: the under-cap message is refused (F-057, pinned)"),
			Log.CountContaining(CapSentence), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualityNeo4jRefusalLeavesStreamUnrecovered,
	"FANTASIA.Quality.Neo4j.Cap.RefusalLeavesStreamUnrecovered", FantasiaQualityNeo4j::TestFlags)
bool FQualityNeo4jRefusalLeavesStreamUnrecovered::RunTest(const FString& Parameters)
{
	using namespace FantasiaQualityNeo4j;

	// F-048, from the side the cap's stress plan looks at: after a refusal the connection is still
	// standing, and the question that matters is whether it still decodes. The server in this model
	// sends one 20 MiB message and then answers three later requests, each in a read of its own.
	// Pinned as it is today; the day the receive path recovers or reports the failure, this test
	// fails and is rewritten against the new behaviour.
	const int32 Cap = FNeo4jTestAccess::MaxBoltMessageBytes();
	const FString CapSentence = FString::Printf(TEXT("past the %d-byte limit"), Cap);

	// The refusal, and whatever the misaligned dechunker makes of the rest, may log parse errors.
	AddExpectedErrorPlain(CapSentence, EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedErrorPlain(TEXT("Bolt: "), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedErrorPlain(TEXT("BoltPackStream:"), EAutomationExpectedErrorFlags::Contains, -1);

	const TArray<uint8> Oversize = Frame(RecordWithString(20 * 1024 * 1024));
	const TArray<uint8> Followers[3] = {
		FramedSuccess(TEXT("first")), FramedSuccess(TEXT("second")), FramedSuccess(TEXT("third")) };

	// Control: the same three answers on a component that never saw the oversize message.
	{
		const TStrongObjectPtr<UNeo4jComponent> Control = MakeComponent();
		FLogCapture Log;
		int32 Delivered = 0;
		for (const TArray<uint8>& Message : Followers)
		{
			Delivered += DeliveredIntact(*Control, Log, Message) ? 1 : 0;
		}
		TestEqual(TEXT("control: the three answers are delivered when nothing was refused"), Delivered, 3);
	}

	const TStrongObjectPtr<UNeo4jComponent> Component = MakeComponent();
	if (!TestTrue(TEXT("the component is created"), Component.IsValid()))
	{
		return true;
	}
	FLogCapture Log;
	const int32 StateBefore = static_cast<int32>(FNeo4jTestAccess::ConnectionState(*Component));

	FeedInReads(*Component, Oversize, 0, TransportRead);
	TestEqual(TEXT("the cap refused the message exactly once"), Log.CountContaining(CapSentence), 1);
	TestEqual(TEXT("the connection is left standing, as DEC-021 says"),
		static_cast<int32>(FNeo4jTestAccess::ConnectionState(*Component)), StateBefore);

	int32 Delivered = 0;
	for (const TArray<uint8>& Message : Followers)
	{
		Delivered += DeliveredIntact(*Component, Log, Message) ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("After the refusal, %d of 3 following messages were delivered intact; "
		"%d parse or refusal lines logged in all; counter now %d"),
		Delivered, TroubleLines(Log), FNeo4jTestAccess::UnresolvedMessageBytes(*Component)));
	TestEqual(TEXT("none of the three answers that follow is delivered (F-048, pinned)"), Delivered, 0);
	return true;
}
