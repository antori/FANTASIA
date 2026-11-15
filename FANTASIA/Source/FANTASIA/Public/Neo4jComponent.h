/**
 * @file Neo4jComponent.h
 * @brief Actor component for executing Cypher queries against a Neo4j database via the Bolt protocol.
 */

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FANTASIATypes.h"
#include "IWebSocket.h"
#include "BoltPackStream.h"
#include "BoltProtocol.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Neo4jComponent.generated.h"

/** Internal connection state of the Bolt transport layer. */
enum class EBoltConnectionState : uint8
{
	Disconnected,       ///< No connection attempt in progress.
	Connecting,         ///< TCP / WebSocket connection is being established.
	HandshakePending,   ///< Bolt handshake sent, awaiting version negotiation response.
	Authenticating,     ///< HELLO / LOGON sent, awaiting SUCCESS.
	LogonPending,       ///< Bolt 5.1+ two-phase auth: HELLO succeeded, LOGON in flight.
	RoutingDiscovery,   ///< Sent dbms.routing.getRoutingTable, awaiting records + SUCCESS.
	Ready,              ///< Authenticated and idle — ready to accept queries.
	Streaming,          ///< A RUN/PULL cycle is in progress (auto-commit).
	TxReady,            ///< Inside an explicit transaction, idle.
	TxStreaming,        ///< Inside an explicit transaction, RUN/PULL in progress.
	Failed,             ///< A protocol or transport error has occurred.
	Closed              ///< Connection was cleanly closed.
};

/** Phase within the routing discovery query (RUN + PULL). */
enum class EBoltRoutingPhase : uint8
{
	Idle,
	WaitingRunSuccess,
	WaitingPullRecords
};

/** Phase within a single Cypher query execution (RUN → PULL). */
enum class EBoltQueryPhase : uint8
{
	Idle,                ///< No query in progress.
	WaitingRunSuccess,   ///< RUN message sent, waiting for SUCCESS with field names.
	WaitingPullRecords   ///< PULL message sent, collecting RECORD / SUCCESS messages.
};

/**
 * Transport framing for the Bolt protocol. Orthogonal to TLS — combined with
 * @c UseSSL this yields four matrices (TCP/WS x plain/TLS). Plain TCP is the
 * protocol native Neo4j drivers (Java/Python/cypher-shell) use and is the
 * recommended default. WebSocket is only needed for browser-derived clients
 * or environments that proxy Bolt over HTTP.
 */
UENUM(BlueprintType)
enum class EBoltTransport : uint8
{
	PlainTCP    UMETA(DisplayName = "Plain TCP (recommended)"),
	WebSocket   UMETA(DisplayName = "WebSocket (browser/legacy)")
};

/**
 * What a connection's URL scheme, framing and configuration decide about its security.
 *
 * Taken once per connection by @c UNeo4jComponent::DecideTransportSecurity from its
 * arguments alone, and applied by @c UNeo4jComponent::ApplyTlsVerification. The two are
 * separate because the defect they close was never in deciding to use TLS — that part was
 * already right — but in building a context that verified nobody, so the application has
 * to be observable on its own.
 */
struct FNeo4jTlsDecision
{
	/** Encrypt the connection: TLS for plain TCP, @c wss:// for WebSocket. */
	bool bEncrypt = false;

	/** Require the server's certificate to chain to a trust anchor. False for @c +ssc. */
	bool bVerifyChain = false;

	/** Require the certificate's names to match @c Hostname. False for @c +ssc. */
	bool bVerifyHostname = false;

	/**
	 * The host this connection targets, and so the name its certificate must carry.
	 *
	 * One string for both, because the two must not be able to differ: with @c neo4j://
	 * the server names the backend the client reconnects to, so verifying the original
	 * endpoint's name against a routing-supplied host would check the wrong string.
	 */
	FString Hostname;
};

/** A queued Bolt command awaiting execution. */
struct FBoltPendingCommand
{
	Neo4jOperation Operation;              ///< Auto-commit, begin, commit, rollback, or add-to-tx.
	FString Query;                         ///< Cypher query text.
	TMap<FString, FString> Parameters;     ///< Query parameters (string values).
	FString Database;                      ///< Target database name (empty = default).
	FString TransactionID;                 ///< Transaction ID for explicit-transaction ops.
};

/** Fired when a Cypher query result is available. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FIncomingNeo4jResponseEvent, FNeo4jResponse, Neo4jResponse);

/**
 * Actor component that connects to a Neo4j instance over the Bolt binary
 * protocol and executes Cypher queries exposed to Blueprints.
 *
 * Two orthogonal transport choices:
 *   - @c Transport selects framing: plain TCP (default, native drivers) or
 *     WebSocket (browser-driver legacy).
 *   - @c UseSSL selects encryption: off (plain) or on (TLS, via OpenSSL for
 *     TCP transport, via libwebsockets for WebSocket transport). Aura URLs
 *     (@c bolt+s://, @c neo4j+s://) auto-enable @c UseSSL.
 *
 * What an encrypted connection guarantees depends on which of those two transports carries
 * it, so there are two promises here rather than one:
 *   - Plain TCP (the default) applies the decision, and there an encrypted connection is an
 *     authenticated one: @c bolt+s:// and @c neo4j+s:// require the server's certificate to
 *     chain to a trust anchor and to carry the host's name, and the connection is refused
 *     — before any credential is sent — when it does not. @c bolt+ssc:// and
 *     @c neo4j+ssc:// accept a self-signed certificate, and are the one-character migration
 *     for a server that has one.
 *   - WebSocket only decides; the engine's WebSockets module owns verification, so the
 *     decision is advisory in both directions. @c +ssc cannot be honoured at all, which
 *     makes a self-signed server unreachable over this transport. And @c +s can encrypt
 *     without authenticating: the engine-wide @c [LwsWebSocket] @c bDisableCertValidation
 *     key, the @c -lws-insecure=true switch, and a server-supplied routing record carrying
 *     @c wss+insecure:// (F-055) each defeat it, and none of them is observable from inside
 *     this component, so none of them is warned about.
 * A caller who needs the guarantee uses @c PlainTCP. C-neo4j, section "What an encrypted
 * connection guarantees, by transport", states both promises in full.
 *
 * Queries may run as auto-commit or inside explicit transactions
 * (BEGIN / ADD / COMMIT / ROLLBACK).
 *
 * @see UBoltProtocol, UBoltPackStream
 */
UCLASS(ClassGroup = (Neo4j), meta = (BlueprintSpawnableComponent), config=Game)
class FANTASIA_API UNeo4jComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNeo4jComponent();

	// ── Configuration ────────────────────────────────────────────────────

	/** Neo4j Bolt endpoint URL or hostname (e.g. "bolt://localhost"). */
	UPROPERTY(EditAnywhere, Category = "Configuration", Config)
	FString endpoint;

	/** Bolt listener port (default 7687). */
	UPROPERTY(EditAnywhere, Category = "Configuration", Config)
	int port;

	/** Authentication username. */
	UPROPERTY(EditAnywhere, Category = "Configuration", Config)
	FString user;

	/** Authentication password. */
	UPROPERTY(EditAnywhere, Category = "Configuration", Config)
	FString password;

	/**
	 * When true, the connection is encrypted (TLS for plain TCP, WSS for
	 * WebSocket). Auto-enabled for @c bolt+s:// / @c neo4j+s:// endpoints.
	 */
	UPROPERTY(EditAnywhere, Category = "Configuration", Config)
	bool UseSSL = false;

	/**
	 * Bolt framing to use. PlainTCP (default) is what native Neo4j drivers
	 * use and is the right choice for Aura, local Neo4j, and almost every
	 * production deployment. WebSocket is only needed when connecting to a
	 * browser-style HTTP-Upgrade endpoint.
	 */
	UPROPERTY(EditAnywhere, Category = "Configuration", Config)
	EBoltTransport Transport = EBoltTransport::PlainTCP;

	// ── Events ───────────────────────────────────────────────────────────

	/** Broadcast when a query result (or transaction acknowledgement) is received. */
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Neo4j")
	FIncomingNeo4jResponseEvent IncomingResponse;

	/** Raw text of the most recent server response (debugging aid). */
	UPROPERTY(BlueprintReadOnly, Category = "Neo4j")
	FString response;

	// ── Public API ───────────────────────────────────────────────────────

	/**
	 * Submit a Cypher query for execution.
	 *
	 * @param query          Cypher query text.
	 * @param operation      Transaction mode (single, begin, add, commit, rollback).
	 * @param transactionID  ID returned by a previous BEGIN operation (empty for auto-commit).
	 * @param parameters     Query parameter map (string → string).
	 * @param database       Target database (empty = server default).
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Submit query", AutoCreateRefTerm = "parameters"), Category = "Neo4j")
	void submitQuery(FString query, Neo4jOperation operation, FString transactionID, TMap<FString, FString> parameters, FString database);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	// ── Transport-agnostic state ─────────────────────────────────────────
	EBoltConnectionState ConnectionState = EBoltConnectionState::Disconnected;
	EBoltQueryPhase QueryPhase = EBoltQueryPhase::Idle;
	int32 NegotiatedBoltMajor = 0;
	int32 NegotiatedBoltMinor = 0;
	TUniquePtr<FBoltDechunker> Dechunker;

	/**
	 * Largest Bolt message this component will let the dechunker accumulate.
	 *
	 * A Bolt message ends at a zero-length chunk, so a server that never sends one
	 * grows the dechunker's buffer until the process dies. The cap bounds **one
	 * message**, not one result set: a `PULL` answers with one RECORD message per
	 * row plus a terminating SUCCESS, so a large result set is many bounded messages
	 * and never one big one. 16 MiB is 1024 maximum-size (16 KiB) chunks, which is
	 * three orders of magnitude above any row the component can usefully convert
	 * into `UNeo4jResultCell` objects for Blueprint, and small enough that a refusal
	 * costs a transient allocation rather than the process.
	 */
	static constexpr int32 MaxBoltMessageBytes = 16 * 1024 * 1024;

	/**
	 * Bytes handed to the dechunker since it last produced a complete message.
	 *
	 * This is the component's view of an in-flight message: it is never less than
	 * the partial message the dechunker holds, so testing it against
	 * MaxBoltMessageBytes bounds that message without reaching inside the dechunker.
	 */
	int32 UnresolvedMessageBytes = 0;
	TArray<FBoltPendingCommand> PendingCommands;
	int32 PendingCommandIndex = 0;
	TArray<FString> CurrentFieldNames;
	TArray<FBoltValueArray> CurrentRecords;
	Neo4jOperation CurrentOperation = Neo4jOperation::SINGLE_REQUEST;
	FString CurrentDatabase;
	FString ActiveTransactionID;
	int32 TransactionCounter = 0;
	TArray<uint8> HandshakeBuffer;

	// ── Server-side routing (Neo4j cluster / Aura) ───────────────────────
	bool bNeedsRouting = false;         ///< URL scheme indicates routing-aware driver required.
	bool bRoutingDone = false;          ///< Backend has been discovered and we are connected to it.
	EBoltRoutingPhase RoutingPhase = EBoltRoutingPhase::Idle;
	int32 ActivePort = 0;               ///< Current port we are connected to.
	TArray<FBoltValueArray> RoutingRecords; ///< Buffered records from the routing discovery call.

	// ── WebSocket transport (Transport == WebSocket) ─────────────────────
	TSharedPtr<IWebSocket> WebSocket;
	int32 ConnectionEpoch = 0;  ///< Incremented on every new connection; stale callbacks are filtered.
	void OnWebSocketConnected(int32 Epoch);
	void OnWebSocketConnectionError(int32 Epoch, const FString& Error);
	void OnWebSocketClosed(int32 Epoch, int32 StatusCode, const FString& Reason, bool bWasClean);
	void OnWebSocketBinaryMessage(int32 Epoch, const void* Data, SIZE_T Size, SIZE_T BytesRemaining);

	// ── TCP transport (Transport == PlainTCP, optionally TLS-wrapped) ────
	void* SslCtx = nullptr;     ///< OpenSSL context (TLS only; nullptr for plain TCP).
	void* SslHandle = nullptr;  ///< SSL handle (TLS only).
	void* TcpBio = nullptr;     ///< OpenSSL BIO chain — TLS+TCP or plain TCP.
	FRunnableThread* RecvThread = nullptr;
	std::atomic<bool> bRecvThreadRunning{false};
	void TcpConnect(const FNeo4jTlsDecision& Security, int32 InPort);
	void TcpDisconnect();
	void TcpRecvLoop();

	/**
	 * Security decided for the connection in flight — the host it targets included.
	 *
	 * Written once per connection, in InitiateConnection, and read from there on: no
	 * per-message or per-byte path consults it.
	 */
	FNeo4jTlsDecision ActiveSecurity;

	// ── Transport dispatch ───────────────────────────────────────────────
	void SendRawBytes(const uint8* Data, int32 Len);
	void OnDataReceived(const void* Data, SIZE_T Size);

	// ── Bolt protocol logic ─────────────────────────────────────────────
	void SendBoltHandshake();
	void HandleHandshakeResponse();
	void SendAuthentication();
	void HandleServerMessage(const FBoltServerMessage& Msg);
	void ProcessNextCommand();
	void SendChunkedMessage(const TArray<uint8>& MessageData);
	void DispatchCommand(const FBoltPendingCommand& Cmd);
	FNeo4jResponse ConvertBoltRecordsToResponse();
	UNeo4jResultCell* ConvertBoltValueToCell(const FBoltValuePtr& Value);
	FBoltValueMap ConvertParameters(const TMap<FString, FString>& InParams);

	// ── Routing discovery (Bolt routing for clustered / Aura deployments) ───
	void InitiateConnection(const FString& Hostname, int32 InPort);
	void TearDownCurrentConnection();
	void OnAuthenticationComplete();
	void SendRoutingQuery();
	void HandleRoutingMessage(const FBoltServerMessage& Msg);
	void ReconnectToBackendAddress(const FString& AddressHostPort);
	FString PickWriteAddressFromRoutingRecord(const FBoltValueArray& RecordFields);

	// ── Helpers ──────────────────────────────────────────────────────────
	// All four read their arguments and nothing else, which is why they are static: a
	// connection's security has to be decidable, and assertable, without a component,
	// a socket or a server.
	static FString ExtractHostname(const FString& Url);
	static bool ShouldUseSSL(const FString& Url);
	static bool SchemeRequiresRouting(const FString& Url);

	/**
	 * Decide what @p SchemeUrl, @p InTransport and @p bConfiguredUseSSL mean for a
	 * connection to @p TargetHost.
	 *
	 * @param SchemeUrl          The configured endpoint. Its scheme carries the user's
	 *                           intent and is the only thing read from it here.
	 * @param TargetHost         The host actually being connected to — the endpoint's own
	 *                           host on the first connection, the backend the routing
	 *                           table named on a reconnect. Separate from @p SchemeUrl
	 *                           because the scheme's promise outlives the host it was
	 *                           written for.
	 * @param InTransport        Framing. Not a branch: both framings make the same promise
	 *                           per scheme, which is the point — C-neo4j states one
	 *                           guarantee per scheme, not one per scheme and framing. It is
	 *                           an argument so that the promise is asserted over both, and
	 *                           so a framing difference, if one is ever accepted, has one
	 *                           place to live.
	 * @param bConfiguredUseSSL  The @c UseSSL property, which decides only for the schemes
	 *                           that say nothing (@c neo4j:// and an unrecognised scheme).
	 */
	static FNeo4jTlsDecision DecideTransportSecurity(const FString& SchemeUrl, const FString& TargetHost,
		EBoltTransport InTransport, bool bConfiguredUseSSL);

	/**
	 * Apply @p Security to an OpenSSL client context.
	 *
	 * @param SslCtxHandle  An @c SSL_CTX*, as @c void* so that OpenSSL stays out of this
	 *                      header — the same reason @c SslCtx itself is @c void*.
	 * @param OutFailure    On false, what could not be established, phrased for a log.
	 * @return              False when the context cannot be made to verify what
	 *                      @p Security asks it to. A caller that ignores this builds the
	 *                      hole it was trying to close, so the caller must not connect.
	 */
	static bool ApplyTlsVerification(void* SslCtxHandle, const FNeo4jTlsDecision& Security, FString& OutFailure);

	friend class FNeo4jRecvRunnable;

	/** Test-only access to the private receive path; defined in the test module. */
	friend struct FNeo4jTestAccess;
};
