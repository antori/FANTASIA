/**
 * @file Neo4jTlsVerificationTests.cpp
 * @brief F-020: an encrypted Bolt connection authenticates the server, and `+ssc` still does not.
 *
 * F-020 was not a defect in deciding to use TLS — that part was already right — but in
 * building a context that verified nobody: a fresh `SSL_CTX` is `SSL_VERIFY_NONE`, hostname
 * checking is never automatic, and the component set neither. So these tests are written
 * against the *applying*: every assertion about verification is read off an `SSL_CTX` this
 * file owns and the component configured, never inferred from a connection that did or did
 * not fail, and never read off a context the test configured itself.
 *
 * Which test answers which US-031 criterion:
 *
 * | US-031 criterion | Tests |
 * | :--- | :--- |
 * | the decision is a static member reading only its arguments — no socket, no component state | every `Decision.*` test runs with no component in existence and no network |
 * | the decision returns as data whether to encrypt, verify the chain, verify the hostname, and which hostname | `Decision.SchemeMatrix` asserts all four fields of all forty decisions |
 * | ten schemes on each transport: `+s`/`https`/`wss` verify, `+ssc` verifies neither, `bolt`/`ws`/`http` do not encrypt, `neo4j://` follows `UseSSL` | `Decision.SchemeMatrix` (10 schemes × 2 transports × both values of `UseSSL`), `Decision.UnrecognisedSchemeFollowsConfiguredSsl` |
 * | `SSL_CTX_get_verify_mode` reports `SSL_VERIFY_PEER`, the X509 parameters carry the hostname, a trust store is loaded — asserted on the context | `Context.VerifyingSchemeConfiguresTheContext`, and its dual `Context.FreshContextVerifiesNothing` |
 * | the hostname checked is the host connected to, not the endpoint, on a routing reconnect | `Decision.HostnameIsTheHostConnectedTo`, `Context.VerifiedNameIsTheOneDecided` |
 * | a certificate whose names do not match the host is refused | `Context.VerifiedNameIsTheOneDecided`, and its dual in the same test |
 * | the component does not connect and does not send `LOGON`, and the log names the check and the `+ssc` migration | `Refusal.RefusedVerificationNeverSendsLogon`, `Refusal.EmptyHostnameWithVerificationIsRefused` |
 * | both transports take the decision from the same function | `Decision.SchemeMatrix` compares the two framings field by field |
 * | the decision is taken once per connection and no per-message path consults it | not a runtime assertion: `DecideTransportSecurity` has one call site, in `InitiateConnection`, which the diff decides. `Refusal.FreshComponentClaimsNoSecurity` pins the other half — a component that has not connected claims nothing |
 *
 * **The duals, which are what make the rest worth trusting.** Every "verification is
 * configured" assertion here would also pass against a context that verified *everything*,
 * including the `+ssc` case where the user deliberately opted out; and the refusal
 * assertions would pass against a component that refused every connection. So each side is
 * pinned against its opposite:
 *
 * - `Context.FreshContextVerifiesNothing` establishes what an unconfigured context reports
 *   (`SSL_VERIFY_NONE`, zero host flags, an empty trust store). It is the exact state F-020
 *   shipped, so it is also the measure of the fix: without the fix,
 *   `Context.VerifyingSchemeConfiguresTheContext` asserts nothing but what this test asserts.
 * - `Context.SelfSignedSchemeVerifiesNothing` is the opt-out honoured: `+ssc` leaves the
 *   context stating `SSL_VERIFY_NONE` with no anchors and no host flags. A context that
 *   verified everything fails here.
 * - `Context.VerifiedNameIsTheOneDecided` pairs a certificate that carries the decided name
 *   (accepted) with one that does not (refused as a hostname mismatch). Either alone proves
 *   little: with no host set at all the first still passes.
 * - `Refusal.EmptyHostnameWithVerificationIsRefused` pairs the refusal with the same empty
 *   hostname under `+ssc`, which is accepted — the guard is about verifying against no name,
 *   not a blanket requirement for a name.
 *
 * **Reading the hostname back.** The engine ships OpenSSL 1.1.1t, where `X509_VERIFY_PARAM`
 * is opaque and `X509_VERIFY_PARAM_get0_host` does not exist (it arrives in 3.0), so the
 * name set on a context cannot be read as a string — verified here, not assumed: this file
 * compiles against those headers and uses no such getter. Two things stand in for it, and
 * the second is the real assertion:
 *   1. `X509_VERIFY_PARAM_get_hostflags` reports `X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS`,
 *      a non-default value set on the same call path as the host, on a context nothing else
 *      in the process can reach. `Context.FreshContextVerifiesNothing` proves it is not the
 *      default, so it is a witness that the host parameters were written here.
 *   2. The parameters are given a certificate and asked. That reads the host by exercising
 *      it rather than by printing it, and it is the only assertion in this file that can
 *      tell the decided name from any other name.
 *
 * **What no offline test here can reach.** The certificate refusals in `TcpConnect` —
 * the untrusted chain at the aborted handshake and the independent `SSL_get_verify_result`
 * gate after a successful one — need a server holding a certificate, so they are not
 * asserted. What is asserted is that a refusal from `ApplyTlsVerification` returns from
 * `TcpConnect` before a socket exists, which is the same structural claim one branch earlier:
 * `ConnectionState` is `Failed`, no transport exists, and `SendBoltHandshake` — and so
 * `HELLO` and the `LOGON` carrying `user` and `password` — is never reached.
 *
 * Nothing here opens a socket, resolves a host or needs a server.
 */

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#include "Neo4jComponent.h"
#include "Neo4jTestAccess.h"

// OpenSSL's ossl_typ.h defines 'UI', which collides with UE's UI namespace — the same guard
// the component's own translation unit uses.
#define UI UI_ST
THIRD_PARTY_INCLUDES_START
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>
THIRD_PARTY_INCLUDES_END
#undef UI

namespace FantasiaSecurityNeo4jTls
{
constexpr EAutomationTestFlags TestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

/**
 * The tail of the refusal `TcpConnect` logs when `ApplyTlsVerification` says no.
 *
 * One contiguous substring of one log line, so a single expected-error entry asserts all
 * four things the criterion asks of that message: that it refuses, that no connection was
 * opened, that no credential was sent, and that the migration is named — both schemes of it.
 */
inline const TCHAR* RefusalLogTail =
	TEXT("No connection was opened and no credentials were sent. Use 'bolt+ssc://' or ")
	TEXT("'neo4j+ssc://' if you intend to accept a self-signed certificate.");

/** The host every scheme in the matrix is pointed at. */
inline const TCHAR* MatrixHost = TEXT("neo4j.example.com");

// ── An SSL_CTX the test owns ─────────────────────────────────────────────────────────
//
// The context is created here and destroyed here, and the only code that touches it in
// between is the code under test. That is what makes every value read off it evidence.

struct FOwnedContext
{
	SSL_CTX* Ctx = SSL_CTX_new(TLS_client_method());

	FOwnedContext() = default;
	FOwnedContext(const FOwnedContext&) = delete;
	FOwnedContext& operator=(const FOwnedContext&) = delete;
	~FOwnedContext() { if (Ctx) SSL_CTX_free(Ctx); }
};

/** Trust anchors loaded into a context's certificate store. Zero on a fresh context. */
int32 TrustAnchorCount(SSL_CTX* Ctx)
{
	X509_STORE* const Store = SSL_CTX_get_cert_store(Ctx);
	return Store ? sk_X509_OBJECT_num(X509_STORE_get0_objects(Store)) : -1;
}

/** Host-check flags on a context's X509 verification parameters. Zero on a fresh context. */
int32 HostFlags(SSL_CTX* Ctx)
{
	return static_cast<int32>(X509_VERIFY_PARAM_get_hostflags(SSL_CTX_get0_param(Ctx)));
}

/**
 * Asserts one boolean against the value it is supposed to carry.
 *
 * `TestEqual` has no `bool` overload — a `bool` is ambiguous between its integral ones — so
 * a parameterised expectation has to dispatch to `TestTrue` or `TestFalse` itself.
 */
void CheckFlag(FAutomationTestBase& Test, const FString& What, bool Actual, bool Expected)
{
	if (Expected)
	{
		Test.TestTrue(*What, Actual);
	}
	else
	{
		Test.TestFalse(*What, Actual);
	}
}

/** Asserts a context states, in the three places it can, that it verifies nothing. */
void CheckContextVerifiesNothing(FAutomationTestBase& Test, const TCHAR* What, SSL_CTX* Ctx)
{
	Test.TestEqual(*FString::Printf(TEXT("%s: the verify mode is SSL_VERIFY_NONE"), What),
		SSL_CTX_get_verify_mode(Ctx), SSL_VERIFY_NONE);
	Test.TestEqual(*FString::Printf(TEXT("%s: no host-check flag is set"), What),
		HostFlags(Ctx), 0);
	Test.TestEqual(*FString::Printf(TEXT("%s: the trust store holds no anchor"), What),
		TrustAnchorCount(Ctx), 0);
}

// ── A certificate to ask the verification parameters about ───────────────────────────

/** A key and a self-signed certificate naming one host, both freed with the struct. */
struct FSelfSignedCert
{
	EVP_PKEY* Key = nullptr;
	X509* Cert = nullptr;

	FSelfSignedCert() = default;
	FSelfSignedCert(const FSelfSignedCert&) = delete;
	FSelfSignedCert& operator=(const FSelfSignedCert&) = delete;
	~FSelfSignedCert()
	{
		if (Cert) X509_free(Cert);
		if (Key) EVP_PKEY_free(Key);
	}
};

/**
 * Build a self-signed certificate whose subjectAltName is @p DnsName.
 *
 * A subjectAltName and not only a common name, because that is the name a verifier built
 * this decade actually checks, and so the name the component's parameters have to match.
 */
bool MakeSelfSignedCert(const char* DnsName, FSelfSignedCert& Out)
{
	BIGNUM* const Exponent = BN_new();
	RSA* Rsa = RSA_new();
	if (!Exponent || !Rsa)
	{
		if (Exponent) BN_free(Exponent);
		if (Rsa) RSA_free(Rsa);
		return false;
	}
	BN_set_word(Exponent, RSA_F4);
	const bool bKeyGenerated = RSA_generate_key_ex(Rsa, 2048, Exponent, nullptr) == 1;
	BN_free(Exponent);
	if (!bKeyGenerated)
	{
		RSA_free(Rsa);
		return false;
	}

	Out.Key = EVP_PKEY_new();
	if (!Out.Key || EVP_PKEY_assign_RSA(Out.Key, Rsa) != 1)
	{
		RSA_free(Rsa);
		return false;
	}
	// Out.Key owns Rsa from here, and frees it.

	Out.Cert = X509_new();
	if (!Out.Cert) return false;

	X509_set_version(Out.Cert, 2);
	ASN1_INTEGER_set(X509_get_serialNumber(Out.Cert), 1);
	X509_gmtime_adj(X509_getm_notBefore(Out.Cert), 0);
	X509_gmtime_adj(X509_getm_notAfter(Out.Cert), 60 * 60);
	X509_set_pubkey(Out.Cert, Out.Key);

	X509_NAME* const Subject = X509_get_subject_name(Out.Cert);
	X509_NAME_add_entry_by_txt(Subject, "CN", MBSTRING_ASC,
		reinterpret_cast<const unsigned char*>(DnsName), -1, -1, 0);
	X509_set_issuer_name(Out.Cert, Subject);

	const FString San = FString::Printf(TEXT("DNS:%hs"), DnsName);
	X509_EXTENSION* const AltName =
		X509V3_EXT_conf_nid(nullptr, nullptr, NID_subject_alt_name, TCHAR_TO_UTF8(*San));
	if (!AltName) return false;
	X509_add_ext(Out.Cert, AltName, -1);
	X509_EXTENSION_free(AltName);

	return X509_sign(Out.Cert, Out.Key, EVP_sha256()) > 0;
}

/**
 * Verify @p Cert using the X509 verification parameters the code under test wrote on @p Ctx.
 *
 * @p Cert is trusted for the duration, and `PARTIAL_CHAIN` makes a trusted leaf a complete
 * chain, so chain building is not what the result turns on: the names are. Returns the X509
 * error code — `X509_V_OK` when the parameters accept the certificate's names,
 * `X509_V_ERR_HOSTNAME_MISMATCH` when they reject them, and, if no host was ever set,
 * `X509_V_OK` for any name at all, which is the state F-020 shipped and is why this is the
 * one assertion here that distinguishes the decided name from any other.
 */
int32 VerifyNamesAgainstContext(SSL_CTX* Ctx, X509* Cert)
{
	X509_STORE* const Trust = X509_STORE_new();
	X509_STORE_CTX* const StoreCtx = X509_STORE_CTX_new();
	if (!Trust || !StoreCtx)
	{
		if (StoreCtx) X509_STORE_CTX_free(StoreCtx);
		if (Trust) X509_STORE_free(Trust);
		return -1;
	}

	X509_STORE_add_cert(Trust, Cert);
	if (X509_STORE_CTX_init(StoreCtx, Trust, Cert, nullptr) != 1)
	{
		X509_STORE_CTX_free(StoreCtx);
		X509_STORE_free(Trust);
		return -1;
	}

	// Copied after init, because init resets the parameters from the store's own.
	X509_VERIFY_PARAM* const Param = X509_STORE_CTX_get0_param(StoreCtx);
	X509_VERIFY_PARAM_set1(Param, SSL_CTX_get0_param(Ctx));
	X509_VERIFY_PARAM_set_flags(Param, X509_V_FLAG_PARTIAL_CHAIN);

	const int Result = X509_verify_cert(StoreCtx);
	const int32 Error = Result == 1 ? X509_V_OK : X509_STORE_CTX_get_error(StoreCtx);

	X509_STORE_CTX_free(StoreCtx);
	X509_STORE_free(Trust);
	return Error;
}

// ── The scheme matrix ────────────────────────────────────────────────────────────────

/** What a scheme promises, independently of how the connection is framed. */
enum class ESchemePromise : uint8
{
	Plaintext,              ///< Not encrypted, so there is nothing to verify.
	EncryptedAndVerified,   ///< Encrypted, chain and hostname both checked.
	EncryptedUnverified,    ///< `+ssc`: encrypted, peer unauthenticated, by request.
	FollowsConfiguredSsl    ///< `neo4j://`: the `UseSSL` property decides, until US-032.
};

struct FSchemeRow
{
	const TCHAR* Scheme;
	ESchemePromise Promise;
};

/** The ten schemes the component meets, and what each one means. */
inline const FSchemeRow SchemeMatrix[] =
{
	{ TEXT("bolt://"),       ESchemePromise::Plaintext            },
	{ TEXT("bolt+s://"),     ESchemePromise::EncryptedAndVerified },
	{ TEXT("bolt+ssc://"),   ESchemePromise::EncryptedUnverified  },
	{ TEXT("neo4j://"),      ESchemePromise::FollowsConfiguredSsl },
	{ TEXT("neo4j+s://"),    ESchemePromise::EncryptedAndVerified },
	{ TEXT("neo4j+ssc://"),  ESchemePromise::EncryptedUnverified  },
	{ TEXT("ws://"),         ESchemePromise::Plaintext            },
	{ TEXT("wss://"),        ESchemePromise::EncryptedAndVerified },
	{ TEXT("http://"),       ESchemePromise::Plaintext            },
	{ TEXT("https://"),      ESchemePromise::EncryptedAndVerified }
};

/** The two framings, which must make the same promise per scheme. */
inline const EBoltTransport Transports[] = { EBoltTransport::PlainTCP, EBoltTransport::WebSocket };

const TCHAR* TransportName(EBoltTransport Transport)
{
	return Transport == EBoltTransport::PlainTCP ? TEXT("plain TCP") : TEXT("WebSocket");
}

/** Asserts all four fields of @p Decision against what @p What is supposed to promise. */
void CheckDecision(FAutomationTestBase& Test, const FString& What, const FNeo4jTlsDecision& Decision,
	bool bEncrypt, bool bVerifyChain, bool bVerifyHostname, const TCHAR* Hostname)
{
	CheckFlag(Test, FString::Printf(TEXT("%s: encrypts"), *What), Decision.bEncrypt, bEncrypt);
	CheckFlag(Test, FString::Printf(TEXT("%s: verifies the chain"), *What), Decision.bVerifyChain, bVerifyChain);
	CheckFlag(Test, FString::Printf(TEXT("%s: verifies the hostname"), *What), Decision.bVerifyHostname, bVerifyHostname);
	Test.TestEqual(*FString::Printf(TEXT("%s: carries the host it targets"), *What), Decision.Hostname, FString(Hostname));
}

/** The decision for a `bolt+s://` endpoint at @p Host — encrypted, verified, named. */
FNeo4jTlsDecision VerifyingDecisionFor(const TCHAR* Host)
{
	return FNeo4jTestAccess::DecideTransportSecurity(
		FString::Printf(TEXT("bolt+s://%s:7687"), Host), Host, EBoltTransport::PlainTCP, false);
}

/** A fresh component that has never connected. */
TStrongObjectPtr<UNeo4jComponent> MakeComponent()
{
	return TStrongObjectPtr<UNeo4jComponent>(
		NewObject<UNeo4jComponent>(GetTransientPackage(), NAME_None, RF_Transient));
}
} // namespace FantasiaSecurityNeo4jTls

// ============================================================================
// The decision: what each scheme promises, on each framing, from its arguments alone
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeo4jTlsSchemeMatrix,
	"FANTASIA.Security.Neo4jTls.Decision.SchemeMatrix", FantasiaSecurityNeo4jTls::TestFlags)
bool FNeo4jTlsSchemeMatrix::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityNeo4jTls;

	// No component exists in this test and nothing is connected: the decision reads its
	// four arguments and nothing else, which is the first criterion, asserted structurally.
	for (const FSchemeRow& Row : SchemeMatrix)
	{
		const FString Endpoint = FString::Printf(TEXT("%s%s:7687"), Row.Scheme, MatrixHost);

		for (const EBoltTransport Transport : Transports)
		{
			for (const bool bConfiguredUseSSL : { false, true })
			{
				const FNeo4jTlsDecision Decision = FNeo4jTestAccess::DecideTransportSecurity(
					Endpoint, MatrixHost, Transport, bConfiguredUseSSL);

				const FString What = FString::Printf(TEXT("%s over %s with UseSSL=%s"),
					Row.Scheme, TransportName(Transport), bConfiguredUseSSL ? TEXT("true") : TEXT("false"));

				switch (Row.Promise)
				{
				case ESchemePromise::Plaintext:
					// Also the assertion that UseSSL cannot encrypt a scheme that says it
					// will not: the property is not consulted for these three.
					CheckDecision(*this, What, Decision, false, false, false, MatrixHost);
					break;
				case ESchemePromise::EncryptedAndVerified:
					CheckDecision(*this, What, Decision, true, true, true, MatrixHost);
					break;
				case ESchemePromise::EncryptedUnverified:
					// The opt-out: encrypted and deliberately unauthenticated. This row is
					// the dual of the one above — a decision that verified everything would
					// fail here, which is the whole difference `+ssc` exists to make.
					CheckDecision(*this, What, Decision, true, false, false, MatrixHost);
					break;
				case ESchemePromise::FollowsConfiguredSsl:
					// `neo4j://` is the one scheme that says nothing, so the property
					// decides — and when it encrypts, it verifies: only `+ssc` opts out.
					CheckDecision(*this, What, Decision, bConfiguredUseSSL, bConfiguredUseSSL,
						bConfiguredUseSSL, MatrixHost);
					break;
				}
			}
		}

		// Both framings take their decision from this one function, so the promise cannot
		// differ between them. Asserted field by field rather than left to the two switch
		// arms above having happened to agree.
		for (const bool bConfiguredUseSSL : { false, true })
		{
			const FNeo4jTlsDecision OverTcp = FNeo4jTestAccess::DecideTransportSecurity(
				Endpoint, MatrixHost, EBoltTransport::PlainTCP, bConfiguredUseSSL);
			const FNeo4jTlsDecision OverWebSocket = FNeo4jTestAccess::DecideTransportSecurity(
				Endpoint, MatrixHost, EBoltTransport::WebSocket, bConfiguredUseSSL);

			const FString What = FString::Printf(TEXT("%s with UseSSL=%s: the two framings agree on"),
				Row.Scheme, bConfiguredUseSSL ? TEXT("true") : TEXT("false"));

			CheckFlag(*this, FString::Printf(TEXT("%s encryption"), *What), OverWebSocket.bEncrypt, OverTcp.bEncrypt);
			CheckFlag(*this, FString::Printf(TEXT("%s chain verification"), *What), OverWebSocket.bVerifyChain, OverTcp.bVerifyChain);
			CheckFlag(*this, FString::Printf(TEXT("%s hostname verification"), *What), OverWebSocket.bVerifyHostname, OverTcp.bVerifyHostname);
			TestEqual(*FString::Printf(TEXT("%s the host"), *What), OverWebSocket.Hostname, OverTcp.Hostname);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeo4jTlsUnrecognisedScheme,
	"FANTASIA.Security.Neo4jTls.Decision.UnrecognisedSchemeFollowsConfiguredSsl", FantasiaSecurityNeo4jTls::TestFlags)
bool FNeo4jTlsUnrecognisedScheme::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityNeo4jTls;

	// An endpoint whose scheme the component does not know, and one with no scheme at all,
	// are the other half of `neo4j://`'s branch. They must not fall into the verified case
	// by accident, nor into the `+ssc` case: an unrecognised scheme that encrypts verifies.
	const TCHAR* const Endpoints[] = { TEXT("gopher://neo4j.example.com:7687"), TEXT("neo4j.example.com:7687") };

	for (const TCHAR* Endpoint : Endpoints)
	{
		for (const bool bConfiguredUseSSL : { false, true })
		{
			const FNeo4jTlsDecision Decision = FNeo4jTestAccess::DecideTransportSecurity(
				Endpoint, MatrixHost, EBoltTransport::PlainTCP, bConfiguredUseSSL);

			CheckDecision(*this, FString::Printf(TEXT("'%s' with UseSSL=%s"), Endpoint,
				bConfiguredUseSSL ? TEXT("true") : TEXT("false")),
				Decision, bConfiguredUseSSL, bConfiguredUseSSL, bConfiguredUseSSL, MatrixHost);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeo4jTlsHostnameIsTheHostConnectedTo,
	"FANTASIA.Security.Neo4jTls.Decision.HostnameIsTheHostConnectedTo", FantasiaSecurityNeo4jTls::TestFlags)
bool FNeo4jTlsHostnameIsTheHostConnectedTo::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityNeo4jTls;

	// A `neo4j://` cluster answers the routing query with the backend to talk to, so the
	// second connection of a session targets a host the *server* named. The certificate that
	// has to be checked is that backend's, and the scheme's promise still comes from the
	// endpoint the user configured. One field carries both, so they cannot drift apart.
	const TCHAR* const Endpoint = TEXT("neo4j+s://cluster.example.com:7687");
	const TCHAR* const Backend = TEXT("backend-3.databases.neo4j.io");

	const FNeo4jTlsDecision Reconnect = FNeo4jTestAccess::DecideTransportSecurity(
		Endpoint, Backend, EBoltTransport::PlainTCP, false);
	CheckDecision(*this, TEXT("a routing reconnect"), Reconnect, true, true, true, Backend);

	// The dual: the host moved, the promise did not. A `bolt://` endpoint does not start
	// verifying because the host it was given looks like a cloud backend, and a `neo4j+s://`
	// endpoint does not stop verifying because the host is a bare name.
	const FNeo4jTlsDecision Plaintext = FNeo4jTestAccess::DecideTransportSecurity(
		TEXT("bolt://cluster.example.com:7687"), Backend, EBoltTransport::PlainTCP, false);
	CheckDecision(*this, TEXT("a plaintext endpoint redirected to a backend"), Plaintext,
		false, false, false, Backend);

	// The host is the name, with no scheme, no port and no path attached, because that is
	// what a certificate carries.
	TestEqual(TEXT("the scheme, port and path are not part of the host"),
		FNeo4jTestAccess::ExtractHostname(TEXT("neo4j+s://cluster.example.com:7687/neo4j")),
		FString(TEXT("cluster.example.com")));
	TestEqual(TEXT("a bare host is already the host"),
		FNeo4jTestAccess::ExtractHostname(Backend), FString(Backend));

	return true;
}

// ============================================================================
// The application: what the component writes on an SSL_CTX, read off that context
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeo4jTlsFreshContextVerifiesNothing,
	"FANTASIA.Security.Neo4jTls.Context.FreshContextVerifiesNothing", FantasiaSecurityNeo4jTls::TestFlags)
bool FNeo4jTlsFreshContextVerifiesNothing::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityNeo4jTls;

	// This is F-020 itself, stated as an assertion: a client context nobody configures
	// verifies no chain, checks no name and trusts no anchor. It is here so that the next
	// test's three assertions are witnesses of something the component did, rather than of
	// OpenSSL's defaults — and so that the difference between these two tests is exactly the
	// content of the fix.
	FOwnedContext Owned;
	if (!TestTrue(TEXT("a client context is created"), Owned.Ctx != nullptr))
	{
		return true;
	}

	CheckContextVerifiesNothing(*this, TEXT("a context nobody configured"), Owned.Ctx);

	// And a context in that state accepts a certificate for a host it was never pointed at,
	// which is what the absence of a host check means on the wire.
	FSelfSignedCert Cert;
	if (TestTrue(TEXT("a certificate for 'other.example.com' is built"),
		MakeSelfSignedCert("other.example.com", Cert)))
	{
		TestEqual(TEXT("an unconfigured context raises no name objection to any certificate"),
			VerifyNamesAgainstContext(Owned.Ctx, Cert.Cert), static_cast<int32>(X509_V_OK));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeo4jTlsVerifyingSchemeConfiguresTheContext,
	"FANTASIA.Security.Neo4jTls.Context.VerifyingSchemeConfiguresTheContext", FantasiaSecurityNeo4jTls::TestFlags)
bool FNeo4jTlsVerifyingSchemeConfiguresTheContext::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityNeo4jTls;

	// All four schemes that promise a verified peer, because the promise is per scheme and a
	// fix applied to two of them is three quarters of a fix.
	const TCHAR* const Schemes[] = { TEXT("bolt+s://"), TEXT("neo4j+s://"), TEXT("https://"), TEXT("wss://") };

	for (const TCHAR* Scheme : Schemes)
	{
		for (const EBoltTransport Transport : Transports)
		{
			const FString What = FString::Printf(TEXT("%s over %s"), Scheme, TransportName(Transport));
			const FNeo4jTlsDecision Decision = FNeo4jTestAccess::DecideTransportSecurity(
				FString::Printf(TEXT("%s%s:7687"), Scheme, MatrixHost), MatrixHost, Transport, false);

			FOwnedContext Owned;
			if (!TestTrue(*FString::Printf(TEXT("%s: a client context is created"), *What), Owned.Ctx != nullptr))
			{
				continue;
			}

			FString Failure;
			TestTrue(*FString::Printf(TEXT("%s: the context is accepted"), *What),
				FNeo4jTestAccess::ApplyTlsVerification(Owned.Ctx, Decision, Failure));
			TestEqual(*FString::Printf(TEXT("%s: nothing is reported as unestablished"), *What),
				Failure, FString());

			// The three assertions the criterion names, each read off the context the
			// component configured. Against the pre-fix code every one of them reports what
			// FreshContextVerifiesNothing asserts instead.
			TestEqual(*FString::Printf(TEXT("%s: the verify mode is SSL_VERIFY_PEER"), *What),
				SSL_CTX_get_verify_mode(Owned.Ctx), SSL_VERIFY_PEER);
			TestEqual(*FString::Printf(TEXT("%s: the host parameters were written on this context"), *What),
				HostFlags(Owned.Ctx), static_cast<int32>(X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS));
			TestTrue(*FString::Printf(TEXT("%s: a trust store is loaded"), *What),
				TrustAnchorCount(Owned.Ctx) > 0);

			// What the BIO chain will actually use: an SSL created from this context carries
			// the mode and the parameters, which is why they are applied before the BIO
			// exists rather than after.
			SSL* const Ssl = SSL_new(Owned.Ctx);
			if (TestTrue(*FString::Printf(TEXT("%s: an SSL is created from the context"), *What), Ssl != nullptr))
			{
				TestEqual(*FString::Printf(TEXT("%s: the SSL inherits the verify mode"), *What),
					SSL_get_verify_mode(Ssl), SSL_VERIFY_PEER);
				TestEqual(*FString::Printf(TEXT("%s: the SSL inherits the host parameters"), *What),
					static_cast<int32>(X509_VERIFY_PARAM_get_hostflags(SSL_get0_param(Ssl))),
					static_cast<int32>(X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS));
				SSL_free(Ssl);
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeo4jTlsVerifiedNameIsTheOneDecided,
	"FANTASIA.Security.Neo4jTls.Context.VerifiedNameIsTheOneDecided", FantasiaSecurityNeo4jTls::TestFlags)
bool FNeo4jTlsVerifiedNameIsTheOneDecided::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityNeo4jTls;

	// The hostname cannot be read back as a string on OpenSSL 1.1.1, so it is read by being
	// exercised: give the parameters the component wrote a certificate for the host they were
	// pointed at, and one for a different host, and the pair says which name is set. Either
	// alone proves nothing — with no host set at all, both are accepted, which is exactly
	// what FreshContextVerifiesNothing shows.
	const TCHAR* const DecidedHost = TEXT("backend-3.databases.neo4j.io");

	FSelfSignedCert Matching;
	FSelfSignedCert Mismatching;
	if (!TestTrue(TEXT("a certificate for the decided host is built"),
			MakeSelfSignedCert("backend-3.databases.neo4j.io", Matching))
		|| !TestTrue(TEXT("a certificate for a different host is built"),
			MakeSelfSignedCert("attacker.example.com", Mismatching)))
	{
		return true;
	}

	FOwnedContext Owned;
	if (!TestTrue(TEXT("a client context is created"), Owned.Ctx != nullptr))
	{
		return true;
	}

	FString Failure;
	if (!TestTrue(TEXT("the context is accepted"), FNeo4jTestAccess::ApplyTlsVerification(
		Owned.Ctx, VerifyingDecisionFor(DecidedHost), Failure)))
	{
		return true;
	}

	TestEqual(TEXT("a certificate carrying the decided name raises no name objection"),
		VerifyNamesAgainstContext(Owned.Ctx, Matching.Cert), static_cast<int32>(X509_V_OK));

	// The assertion F-020 is about. An attacker who can answer for the host presents a
	// certificate of their own; the name is what refuses it.
	TestEqual(TEXT("a certificate carrying another name is a hostname mismatch"),
		VerifyNamesAgainstContext(Owned.Ctx, Mismatching.Cert),
		static_cast<int32>(X509_V_ERR_HOSTNAME_MISMATCH));

	// And the name is the host of the connection, not of the configured endpoint: the same
	// context, configured for a routing-supplied backend, still refuses the endpoint's name.
	FOwnedContext ForBackend;
	FString BackendFailure;
	const FNeo4jTlsDecision Reconnect = FNeo4jTestAccess::DecideTransportSecurity(
		TEXT("neo4j+s://cluster.example.com:7687"), DecidedHost, EBoltTransport::PlainTCP, false);
	if (TestTrue(TEXT("a second client context is created"), ForBackend.Ctx != nullptr)
		&& TestTrue(TEXT("the reconnect's context is accepted"),
			FNeo4jTestAccess::ApplyTlsVerification(ForBackend.Ctx, Reconnect, BackendFailure)))
	{
		FSelfSignedCert FrontendCert;
		if (TestTrue(TEXT("a certificate for the configured endpoint's host is built"),
			MakeSelfSignedCert("cluster.example.com", FrontendCert)))
		{
			TestEqual(TEXT("on a reconnect the backend's name is checked, not the endpoint's"),
				VerifyNamesAgainstContext(ForBackend.Ctx, FrontendCert.Cert),
				static_cast<int32>(X509_V_ERR_HOSTNAME_MISMATCH));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeo4jTlsSelfSignedSchemeVerifiesNothing,
	"FANTASIA.Security.Neo4jTls.Context.SelfSignedSchemeVerifiesNothing", FantasiaSecurityNeo4jTls::TestFlags)
bool FNeo4jTlsSelfSignedSchemeVerifiesNothing::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityNeo4jTls;

	// The dual of VerifyingSchemeConfiguresTheContext, and the reason this battery is not
	// just "verification is on": `+ssc` is an opt-out a user asked for, so a fix that
	// verified everything would be a different defect and every other test here would still
	// pass. The context has to state the decision either way.
	const TCHAR* const Schemes[] = { TEXT("bolt+ssc://"), TEXT("neo4j+ssc://") };

	for (const TCHAR* Scheme : Schemes)
	{
		const FString What = FString::Printf(TEXT("%s"), Scheme);
		const FNeo4jTlsDecision Decision = FNeo4jTestAccess::DecideTransportSecurity(
			FString::Printf(TEXT("%s%s:7687"), Scheme, MatrixHost), MatrixHost,
			EBoltTransport::PlainTCP, false);

		FOwnedContext Owned;
		if (!TestTrue(*FString::Printf(TEXT("%s: a client context is created"), *What), Owned.Ctx != nullptr))
		{
			continue;
		}

		FString Failure;
		TestTrue(*FString::Printf(TEXT("%s: the context is accepted"), *What),
			FNeo4jTestAccess::ApplyTlsVerification(Owned.Ctx, Decision, Failure));
		TestEqual(*FString::Printf(TEXT("%s: nothing is reported as unestablished"), *What),
			Failure, FString());

		CheckContextVerifiesNothing(*this, *What, Owned.Ctx);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeo4jTlsNoContextIsRefused,
	"FANTASIA.Security.Neo4jTls.Context.NoContextIsRefused", FantasiaSecurityNeo4jTls::TestFlags)
bool FNeo4jTlsNoContextIsRefused::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityNeo4jTls;

	// Verification that cannot be applied is refused rather than reported as applied: the
	// one way the component's own caller could be told "verified" with nothing behind it.
	FString Failure;
	TestFalse(TEXT("a verifying decision with no context is refused"),
		FNeo4jTestAccess::ApplyTlsVerification(nullptr, VerifyingDecisionFor(MatrixHost), Failure));
	TestFalse(TEXT("the refusal says what was missing"), Failure.IsEmpty());

	return true;
}

// ============================================================================
// The refusal: what a failed verification leaves the component unable to do
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeo4jTlsEmptyHostnameWithVerificationIsRefused,
	"FANTASIA.Security.Neo4jTls.Refusal.EmptyHostnameWithVerificationIsRefused", FantasiaSecurityNeo4jTls::TestFlags)
bool FNeo4jTlsEmptyHostnameWithVerificationIsRefused::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityNeo4jTls;

	// An endpoint that parses to no host at all — 'bolt+s://:7687', which is what a typo or
	// a half-filled configuration field produces. `X509_VERIFY_PARAM_set1_host` with an
	// empty string clears the host list and *returns success*, so without a guard this
	// connection would verify the chain and silently check no name: a verified connection
	// to whoever answers.
	const FNeo4jTlsDecision NoHost = FNeo4jTestAccess::DecideTransportSecurity(
		TEXT("bolt+s://:7687"), FNeo4jTestAccess::ExtractHostname(TEXT("bolt+s://:7687")),
		EBoltTransport::PlainTCP, false);

	TestTrue(TEXT("the endpoint still asks for a verified connection"), NoHost.bVerifyHostname);
	TestEqual(TEXT("and it names no host to verify"), NoHost.Hostname, FString());

	FOwnedContext Owned;
	if (!TestTrue(TEXT("a client context is created"), Owned.Ctx != nullptr))
	{
		return true;
	}

	FString Failure;
	TestFalse(TEXT("a verifying decision with no host is refused"),
		FNeo4jTestAccess::ApplyTlsVerification(Owned.Ctx, NoHost, Failure));
	TestTrue(TEXT("the refusal names the missing hostname"), Failure.Contains(TEXT("hostname")));

	// A refusal leaves nothing half-applied: the context is as unverifying as it was, so a
	// caller that ignored the return value would not get a context that looks configured.
	CheckContextVerifiesNothing(*this, TEXT("the refused context"), Owned.Ctx);

	// The dual. The guard is about verifying against no name, not about names in general:
	// the same nameless endpoint under `+ssc` is accepted, because there is nothing to
	// weaken when nothing is being checked.
	const FNeo4jTlsDecision NoHostSelfSigned = FNeo4jTestAccess::DecideTransportSecurity(
		TEXT("bolt+ssc://:7687"), FString(), EBoltTransport::PlainTCP, false);

	FOwnedContext SelfSigned;
	FString SelfSignedFailure;
	if (TestTrue(TEXT("a second client context is created"), SelfSigned.Ctx != nullptr))
	{
		TestTrue(TEXT("the same nameless endpoint under '+ssc' is accepted"),
			FNeo4jTestAccess::ApplyTlsVerification(SelfSigned.Ctx, NoHostSelfSigned, SelfSignedFailure));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeo4jTlsRefusedVerificationNeverSendsLogon,
	"FANTASIA.Security.Neo4jTls.Refusal.RefusedVerificationNeverSendsLogon", FantasiaSecurityNeo4jTls::TestFlags)
bool FNeo4jTlsRefusedVerificationNeverSendsLogon::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityNeo4jTls;

	// One line, and it has to carry all of it: that the connection was refused, that no
	// socket was opened, that no credential was sent, and the one-character migration for a
	// user whose server has a self-signed certificate. The pattern is the whole tail of the
	// message, so a message that drops any of that fails this test.
	AddExpectedErrorPlain(RefusalLogTail, EAutomationExpectedErrorFlags::Contains, 1);

	const TStrongObjectPtr<UNeo4jComponent> Component = MakeComponent();
	if (!TestTrue(TEXT("the component is created"), Component.IsValid()))
	{
		return true;
	}

	// The one refusal that is deterministic without a server. Everything a hostile
	// certificate would be refused by is one branch later, in the handshake, and needs a
	// server holding one; what this shows is the structure both share — the refusal returns
	// from TcpConnect before any socket exists.
	const FNeo4jTlsDecision NoHost = FNeo4jTestAccess::DecideTransportSecurity(
		TEXT("bolt+s://:7687"), FString(), EBoltTransport::PlainTCP, false);

	FNeo4jTestAccess::TcpConnect(*Component, NoHost, 7687);

	TestFalse(TEXT("no transport was opened"), FNeo4jTestAccess::HasOpenTransport(*Component));
	TestEqual(TEXT("the connection is failed, so the Bolt handshake was never sent"),
		static_cast<int32>(FNeo4jTestAccess::ConnectionState(*Component)),
		static_cast<int32>(EBoltConnectionState::Failed));

	// HELLO and the LOGON that carries `user` and `password` follow the Bolt handshake, which
	// `TcpConnect` sends only after writing `HandshakePending`, so `Failed` above is the
	// assertion that neither was written to a socket. Nothing else was entered either.
	TestEqual(TEXT("no query phase was entered"),
		static_cast<int32>(FNeo4jTestAccess::QueryPhase(*Component)),
		static_cast<int32>(EBoltQueryPhase::Idle));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeo4jTlsFreshComponentClaimsNoSecurity,
	"FANTASIA.Security.Neo4jTls.Refusal.FreshComponentClaimsNoSecurity", FantasiaSecurityNeo4jTls::TestFlags)
bool FNeo4jTlsFreshComponentClaimsNoSecurity::RunTest(const FString& Parameters)
{
	using namespace FantasiaSecurityNeo4jTls;

	// Deny by default. A component that has not connected makes no claim about a connection:
	// the decision is written once, in InitiateConnection, and before that it is the
	// struct's defaults. A default that claimed encryption or verification would be read as
	// a promise by anything that consults it before the connection is made.
	const TStrongObjectPtr<UNeo4jComponent> Component = MakeComponent();
	if (!TestTrue(TEXT("the component is created"), Component.IsValid()))
	{
		return true;
	}

	const FNeo4jTlsDecision& Security = FNeo4jTestAccess::ActiveSecurity(*Component);
	TestFalse(TEXT("a component that has not connected claims no encryption"), Security.bEncrypt);
	TestFalse(TEXT("it claims no chain verification"), Security.bVerifyChain);
	TestFalse(TEXT("it claims no hostname verification"), Security.bVerifyHostname);
	TestEqual(TEXT("and it names no host"), Security.Hostname, FString());

	return true;
}
