/**
 * @file Neo4jTlsStressTests.cpp
 * @brief S-02 sprint-level stress pass over the TLS verification setup (US-031, F-020).
 *
 * Verification adds one thing to every encrypted connection that was not there before: the
 * component builds a fresh `SSL_CTX` per connection and loads the engine's PEM trust store into
 * it (`ApplyTlsVerification`, DEC-024). That is per connection, not per message, and it is a
 * reconnect cost: `neo4j://` routing reconnects once per session, and every reconnect after a
 * dropped link pays it again. This test reproduces that stimulus at scale: build a context, apply
 * a verifying decision to it, free it, many times over, against the trust store the component
 * resolves, and reports the cost and whether memory comes back.
 *
 * No handshake is performed: that needs a server and a certificate, and its cost on top of the
 * context (the chain walk) is CPU work that cannot be separated from the network here. What this
 * measures is the share that this sprint added and the test module can reach.
 *
 * No threshold is asserted for the latency, because no NFR states one. The one assertion that
 * does not need a budget is that nothing leaks: a context that is freed returns its trust store.
 * If the engine's bundle cannot be found (a host without it), the test says so and ends.
 */

#include "Misc/AutomationTest.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"

#include "Neo4jComponent.h"
#include "../Security/Neo4jTestAccess.h"

// OpenSSL's ossl_typ.h defines 'UI', which collides with UE's UI namespace; the component's own
// translation unit and the security tests use the same guard.
#define UI UI_ST
THIRD_PARTY_INCLUDES_START
#include <openssl/ssl.h>
THIRD_PARTY_INCLUDES_END
#undef UI

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStressNeo4jTlsTrustStoreLoadCost,
	"FANTASIA.Stress.Neo4jTls.TrustStoreLoadCost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStressNeo4jTlsTrustStoreLoadCost::RunTest(const FString& Parameters)
{
	constexpr double MiB = 1024.0 * 1024.0;
	constexpr int32 Iterations = 200;
	constexpr int32 WarmUp = 20;

	const FNeo4jTlsDecision Verifying = FNeo4jTestAccess::DecideTransportSecurity(
		TEXT("bolt+s://db.example.com:7687"), TEXT("db.example.com"), EBoltTransport::PlainTCP, false);
	const FNeo4jTlsDecision SelfSigned = FNeo4jTestAccess::DecideTransportSecurity(
		TEXT("bolt+ssc://db.example.com:7687"), TEXT("db.example.com"), EBoltTransport::PlainTCP, false);

	// Probe once, so a host with no trust store is reported rather than counted as a failure.
	{
		SSL_CTX* Probe = SSL_CTX_new(TLS_client_method());
		FString Failure;
		const bool bLoaded = FNeo4jTestAccess::ApplyTlsVerification(Probe, Verifying, Failure);
		SSL_CTX_free(Probe);
		if (!bLoaded)
		{
			AddInfo(FString::Printf(TEXT("No trust store on this host, so the load cost is not measured: %s"), *Failure));
			return true;
		}
	}

	TArray<double> VerifyingMs;
	TArray<double> SelfSignedMs;
	VerifyingMs.Reserve(Iterations);
	SelfSignedMs.Reserve(Iterations);
	uint64 ResidentAfterWarmUp = 0;
	int32 Failed = 0;

	for (int32 i = 0; i < Iterations; ++i)
	{
		{
			const double Start = FPlatformTime::Seconds();
			SSL_CTX* Ctx = SSL_CTX_new(TLS_client_method());
			FString Failure;
			Failed += FNeo4jTestAccess::ApplyTlsVerification(Ctx, Verifying, Failure) ? 0 : 1;
			SSL_CTX_free(Ctx);
			VerifyingMs.Add((FPlatformTime::Seconds() - Start) * 1000.0);
		}
		{
			const double Start = FPlatformTime::Seconds();
			SSL_CTX* Ctx = SSL_CTX_new(TLS_client_method());
			FString Failure;
			Failed += FNeo4jTestAccess::ApplyTlsVerification(Ctx, SelfSigned, Failure) ? 0 : 1;
			SSL_CTX_free(Ctx);
			SelfSignedMs.Add((FPlatformTime::Seconds() - Start) * 1000.0);
		}
		if (i == WarmUp)
		{
			ResidentAfterWarmUp = FPlatformMemory::GetStats().UsedPhysical;
		}
	}
	const uint64 ResidentAtEnd = FPlatformMemory::GetStats().UsedPhysical;

	TestEqual(TEXT("every application succeeded"), Failed, 0);

	auto Describe = [](TArray<double>& Samples, double& OutMedian, double& OutP95, double& OutMax)
	{
		Samples.Sort();
		OutMedian = Samples[Samples.Num() / 2];
		OutP95 = Samples[FMath::Min(Samples.Num() - 1, Samples.Num() * 95 / 100)];
		OutMax = Samples.Last();
	};
	double VMed, VP95, VMax, SMed, SP95, SMax;
	Describe(VerifyingMs, VMed, VP95, VMax);
	Describe(SelfSignedMs, SMed, SP95, SMax);
	AddInfo(FString::Printf(TEXT("context + verifying decision (loads the trust store), %d runs: median %.2f ms, p95 %.2f ms, max %.2f ms"),
		Iterations, VMed, VP95, VMax));
	AddInfo(FString::Printf(TEXT("context + bolt+ssc decision (loads nothing), %d runs: median %.3f ms, p95 %.3f ms, max %.3f ms"),
		Iterations, SMed, SP95, SMax));

	const double Growth = (ResidentAtEnd > ResidentAfterWarmUp ? ResidentAtEnd - ResidentAfterWarmUp : 0) / MiB;
	AddInfo(FString::Printf(TEXT("resident growth over the last %d iterations: %.1f MiB"), Iterations - WarmUp, Growth));
	// A context that kept its store would hold a trust bundle's worth per iteration (hundreds of
	// kilobytes), which over 180 iterations is tens of megabytes. Sixteen leaves the allocator room.
	TestTrue(TEXT("freed contexts return their trust store"), Growth < 16.0);
	return true;
}
