/**
 * @file Neo4jTestAccess.h
 * @brief The test module's half of UNeo4jComponent's `friend struct FNeo4jTestAccess` seam.
 *
 * `UNeo4jComponent` is declared `class FANTASIA_API`, so the class-level export covers its
 * private members and the receive path can be driven from here with no socket, no server and
 * no exported symbol added to C-neo4j's surface. The component declares the friendship; this
 * header is the only definition of the struct in the suite, which is why it lives in a header
 * rather than in one test file: two translation units defining the same struct differently
 * would be an ODR violation the linker resolves silently.
 *
 * Every member is a reader or a single call. Nothing here reaches into `FBoltDechunker`, which
 * carries no `FANTASIA_API` and cannot be linked from this module at all (F-012). One member,
 * `TcpConnect`, can open a socket; it says so and says when it does not.
 */

#pragma once

#include "CoreMinimal.h"
#include "Neo4jComponent.h"

struct FNeo4jTestAccess
{
	/**
	 * Hand @p Bytes to the component exactly as a transport read would.
	 *
	 * One call is one transport read, which is the unit the message cap is enforced in.
	 * The dechunker is built in the component's constructor, so this is safe on a component
	 * that has never connected and never run BeginPlay.
	 */
	static void FeedTransportBytes(UNeo4jComponent& Component, const TArray<uint8>& Bytes)
	{
		Component.OnDataReceived(Bytes.GetData(), static_cast<SIZE_T>(Bytes.Num()));
	}

	/** Bytes handed to the dechunker since it last produced a complete message. */
	static int32 UnresolvedMessageBytes(const UNeo4jComponent& Component)
	{
		return Component.UnresolvedMessageBytes;
	}

	/** The cap the component enforces, read from the component rather than restated here. */
	static int32 MaxBoltMessageBytes()
	{
		return UNeo4jComponent::MaxBoltMessageBytes;
	}

	/** Rows buffered for the response in flight; non-zero only once a RECORD has been handled. */
	static int32 NumBufferedRecords(const UNeo4jComponent& Component)
	{
		return Component.CurrentRecords.Num();
	}

	/** Field names from the RUN in flight; non-zero only once a SUCCESS has been handled. */
	static int32 NumFieldNames(const UNeo4jComponent& Component)
	{
		return Component.CurrentFieldNames.Num();
	}

	static EBoltQueryPhase QueryPhase(const UNeo4jComponent& Component)
	{
		return Component.QueryPhase;
	}

	static EBoltConnectionState ConnectionState(const UNeo4jComponent& Component)
	{
		return Component.ConnectionState;
	}

	// ── Transport security (F-020) ───────────────────────────────────────────────────
	//
	// The decision and its application are separate members of the component for the
	// reason F-020 exists: the deciding was already right and the applying was not, so a
	// test that can only see the decision does not pin the defect. `ApplyTlsVerification`
	// therefore takes a context the caller owns, and these two need no component at all.

	/** What @p SchemeUrl, @p Transport and @p bConfiguredUseSSL decide for a connection to @p TargetHost. */
	static FNeo4jTlsDecision DecideTransportSecurity(const FString& SchemeUrl, const FString& TargetHost,
		EBoltTransport Transport, bool bConfiguredUseSSL)
	{
		return UNeo4jComponent::DecideTransportSecurity(SchemeUrl, TargetHost, Transport, bConfiguredUseSSL);
	}

	/**
	 * Apply @p Security to an `SSL_CTX` the test owns, passed as `void*`.
	 *
	 * Opens nothing and reaches no network: the context is configured and handed back for
	 * inspection. False means the component would refuse to connect, and @p OutFailure is
	 * the sentence its log carries.
	 */
	static bool ApplyTlsVerification(void* SslCtxHandle, const FNeo4jTlsDecision& Security, FString& OutFailure)
	{
		return UNeo4jComponent::ApplyTlsVerification(SslCtxHandle, Security, OutFailure);
	}

	/** The security decided for the connection in flight, the host it targets included. */
	static const FNeo4jTlsDecision& ActiveSecurity(const UNeo4jComponent& Component)
	{
		return Component.ActiveSecurity;
	}

	/**
	 * Run the component's TCP/TLS connect step, as `InitiateConnection` does.
	 *
	 * A @p Security that `ApplyTlsVerification` refuses is answered before any socket is
	 * created, so the call stays offline and leaves `ConnectionState` at `Failed` with no
	 * transport — which is how a test shows that a failed verification puts `LOGON` out of
	 * reach. Any other @p Security opens a socket, so a test passes one only with a host
	 * that cannot be reached.
	 */
	static void TcpConnect(UNeo4jComponent& Component, const FNeo4jTlsDecision& Security, int32 InPort)
	{
		Component.TcpConnect(Security, InPort);
	}

	/** Whether a transport exists — the direct evidence of whether a connection was opened. */
	static bool HasOpenTransport(const UNeo4jComponent& Component)
	{
		return Component.TcpBio != nullptr;
	}

	/** The hostname this URL resolves to, which is the name a certificate has to carry. */
	static FString ExtractHostname(const FString& Url)
	{
		return UNeo4jComponent::ExtractHostname(Url);
	}
};
