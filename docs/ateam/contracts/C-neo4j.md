# C-neo4j: Cypher queries over the Bolt protocol
- As of: J-0201
- Version: 3
- Provider: M-neo4j
- Consumers: M-demo, M-tests
- Tests: FANTASIA/Source/FANTASIATests/Private/BoltPackStreamTests.cpp; FANTASIA/Source/FANTASIATests/Private/Security/BoltPackStreamBoundsTests.cpp; FANTASIA/Source/FANTASIATests/Private/Security/Neo4jTlsVerificationTests.cpp
- Provenance: recovered

## Specification

This contract is also part of the plugin's public Blueprint surface: it is what projects using FANTASIA call. A change to it invalidates compiled Blueprint assets in user projects, so the consumer list below understates who is affected.

Headers: `Neo4jComponent.h`, `BoltProtocol.h`, `BoltPackStream.h`. The result types come
from C-fantasia-types.

`UNeo4jComponent : UActorComponent` — the application-facing surface:

- Configuration: `endpoint`, `port`, `user`, `password` (Config UPROPERTYs per
  ADR-0001), `Transport` (`EBoltTransport`, default `PlainTCP`), and `UseSSL` (default
  false), which `BeginPlay` overwrites from the endpoint scheme — see the invariants
- `response` — the last raw response, for diagnostics
- Event: `IncomingResponse(FNeo4jResponse)`
- `submitQuery(FString query, Neo4jOperation operation, FString transactionID, TMap<FString, FString> parameters, FString database)`

`BoltPackStream.h` — the wire codec, used by the tests directly:

- `EBoltValueType`, `FBoltStructure`, `FBoltValue` with `MakeNull`, `MakeBool`,
  `MakeInt`, `MakeFloat`, `MakeString`, `MakeList`, `MakeMap`, `MakeStructure`, and
  `AsString() const`
- `FBoltPackStreamWriter` — `WriteNull`, `WriteBool`, `WriteInt`, `WriteFloat`,
  `WriteString`, `WriteBytes`, `WriteListHeader`, `WriteMapHeader`,
  `WriteStructureHeader`, `WriteValue`, `GetBuffer() const`, `Reset()`
- `FBoltPackStreamReader` — `ReadValue()`, `HasMore() const`, `PeekMarker() const`.
  `ReadValue` refuses a message it cannot read within its own buffer rather than decoding
  it; what that means for a caller is in Errors, and the two limits it enforces are in
  Invariants.

`BoltProtocol.h` — message construction and framing. Nothing in this header carries
`FANTASIA_API`, so none of it is exported: it is usable only inside M-neo4j, and no
test can reach it from the separate `FANTASIATests` module (F-012). That is a constraint on
this contract and not only on its coverage — the cap on one message's size is enforced on
the component's side of `FBoltDechunker` rather than inside it, because only the component
is reachable from a test (DEC-021).

- `FBoltServerMessage`, `ParseBoltServerMessage(const TArray<uint8>&)`
- `FBoltDechunker` — `Accumulate(const void*, SIZE_T)`, `HasCompleteMessage() const`,
  `PopMessage(TArray<uint8>&)`, `Reset()`
- `ChunkMessage(const TArray<uint8>&, int32 MaxChunkSize = 16384)`, `BuildHandshake()`,
  `BuildHello`, `BuildHello51`, `BuildHello53`, `BuildLogon`, `BuildRun`, `BuildPull`,
  `BuildBegin`, `BuildCommit`, `BuildRollback`, `BuildReset`, `BuildGoodbye`, their
  pre-framed variants, and `BuildRunAndPull`

A connection's security is decided once and applied separately, and neither part is
Blueprint surface. `FNeo4jTlsDecision` (in `Neo4jComponent.h`, a plain struct and not a
`USTRUCT`) carries `bEncrypt`, `bVerifyChain`, `bVerifyHostname` and the `Hostname` whose
certificate is checked; `DecideTransportSecurity` and `ApplyTlsVerification` are private
statics, the second taking the `SSL_CTX` as a `void*` so that no OpenSSL header enters a
public header. M-tests reaches all three through `friend struct FNeo4jTestAccess`
(`FANTASIA/Source/FANTASIATests/Private/Security/Neo4jTestAccess.h`), which is what makes
the guarantees below assertable without a server (DEC-024). Nothing in that seam carries
`FANTASIA_API` and no `UPROPERTY`, `UFUNCTION`, delegate or enum declaration is part of it,
so no compiled Blueprint asset is invalidated: what moves between Version 2 and Version 3 is
behaviour, not signatures.

## Errors

- A connection, handshake or authentication failure is logged; no `IncomingResponse`
  fires. Callers must not assume a response per query.
- A Cypher error comes back as a Bolt FAILURE message and is logged; `FNeo4jResponse`
  arrives with no rows rather than with an error field.
- `PopMessage` returns false when no complete message has accumulated.
- An unknown PackStream marker yields a null value rather than throwing.
- **`ReadValue` refuses rather than decodes.** Four conditions end a read. Each yields a
  null value, leaves the reader reporting itself exhausted — `HasMore()` is false, so no
  caller reads on past the damage — and logs its reason once per message:
  - a read that would touch a byte at or past `Data.Num()`: `ReadByte` on an empty buffer,
    where `Data.GetData()` is null, and each of the 16-, 32- and 64-bit readers truncated
    mid-value;
  - a string or bytes length, or a list, map or structure element count, taken from the
    message and larger than the bytes remaining. `{0xD2, 0x7F, 0xFF, 0xFF, 0xFF}`, which
    asks for a 2 GB string from five bytes, is refused before anything is reserved, and so
    is a count larger than the bytes left, because every element costs at least one byte;
  - a negative length or count, which a 32-bit field read from the message can carry;
  - a value nested inside more than 63 containers (the limit is stated in Invariants).

  A refusal at any level of the recursion propagates outwards, so a partially decoded value
  is never returned and a caller is never handed one it would mistake for a whole one.
  `HasMore()` answers whether a byte remains, and a malformed message is the reader's
  problem rather than a caller's (F-021).
- **An unterminated chunk stream is discarded, and the connection does not recover.** When
  the bytes in flight would take one message past the 16 MiB cap, the partial message is
  dropped, the dechunker is reset, the reason is logged once naming the limit, and the
  connection is left standing; no `IncomingResponse` fires for the discarded message.
  Because the reset happens mid-stream the framing cannot be resynchronised: every
  subsequent byte is read as a chunk header at an arbitrary offset, so the component reports
  itself connected while decoding nothing and delivers no error to its delegates (F-048,
  major, open). Reaching this takes a server that sends 16 MiB without a terminator — a
  hostile or badly faulty one, not a large result.
- **A TLS connection that cannot be verified is refused before any credential is sent.** On
  the plain TCP transport, a missing or unloadable trust store, a verifying decision with no
  hostname to check, a certificate that does not chain to the trust store, and a certificate
  whose names do not match the host each leave `ConnectionState` at `Failed` with no
  transport, so `SendBoltHandshake` is never reached and the `LOGON` that carries `user` and
  `password` is unreachable by construction rather than by a check. Each refusal names the
  check that failed — `SSL_get_verify_result`'s own reason where OpenSSL supplies one, and
  the trust store path that was used — and names `bolt+ssc://` or `neo4j+ssc://` as the
  scheme that accepts a self-signed certificate deliberately.
  **Version 3 refuses one connection that Version 2 makes**: a self-signed certificate on
  `bolt+s://` or `neo4j+s://`. That is this version's only compatibility break, and the
  migration is one character — change the scheme to `bolt+ssc://` or `neo4j+ssc://` — which
  the failure's message states as well as this contract, the condition the change was
  approved under (J-0163).

## Invariants

- Every PackStream value round-trips exactly: null, bool, the four integer widths,
  float, empty and short and medium strings, empty lists, lists of ints, maps, nested
  structures and structures — each pinned by its own test.
- `PeekMarker()` does not advance the reader (pinned by `Reader.PeekNoAdvance`).
- `Reset()` empties the writer's buffer and nothing else (pinned by `Writer.Reset`).
- `AsString()` is a diagnostic rendering, not a wire format, and is pinned for null,
  bool, int, list, map and structure.
- **Two stated limits, and what each means for a server on the other end.**
  - *Nesting.* `ReadValue` decodes a value nested inside at most 63 containers and refuses
    one nested deeper. The constant and the refusal message both say 64, which is the bound
    on a zero-based depth counter: the 64th container is entered and the value inside it is
    refused. So the deepest message a server may send carries 63 nested containers with a
    payload inside the innermost, and 63 — not 64 — is the number to build to. Pinned from
    both sides, by `Bolt.Reader.NestingPastDepthLimit` and
    `Bolt.Reader.NestingAtDepthLimitDecodes`. Eight levels is the deepest shape a Neo4j
    server builds from stored data (RECORD → field list → Path → node list → Node →
    property map → list), so the limit bounds the reader's stack to a few kilobytes without
    bounding any real result (DEC-020).
  - *Message size.* One Bolt message is capped at 16 MiB. `OnDataReceived` counts the bytes
    handed to the dechunker since it last produced a complete message and refuses the read
    that would take that count past the cap, so the count itself never exceeds it — pinned
    by `Bolt.MessageCap.CapIsExact`, which accumulates exactly 16 MiB of non-terminating
    chunks and then refuses one byte more. The guarantee is **the cap plus at most one
    transport delivery**, not the round number: the count is zeroed when a message
    completes, and the read that completed it may already carry the first bytes of the next
    message, which the dechunker holds while the count reads zero. That slack is one receive
    buffer — 16384 bytes — on the plain TCP transport, and on WebSocket it is whatever the
    engine's client hands over in one binary-message callback, which the plugin does not
    choose. An exact bound would need `FBoltDechunker` to report its partial size, and
    nothing in `BoltProtocol.h` is reachable from a test (F-012). The cap bounds one message
    and not one result set: a `PULL` answers with one RECORD message per row plus a
    terminating SUCCESS, so a large result is many bounded messages and never one big one
    (DEC-021).
- Chunking defaults to 16384 bytes, the Bolt maximum chunk size.
- Transport is `PlainTCP` by default — the same framing native Neo4j drivers use —
  because Neo4j 2026.x stopped serving WebSocket-over-Bolt on 7687 (ADR-0003).
  WebSocket framing remains selectable.
- **The endpoint scheme decides the connection's security, and the decision is data.**
  `DecideTransportSecurity` reads the configured `endpoint`'s scheme and the host being
  connected to, and nothing else — no socket, no component state — and returns the four
  fields once per connection:
  - `bolt+s://`, `neo4j+s://`, `https://`, `wss://` — encrypt, verify the chain, verify the
    hostname. What Neo4j's documentation says these schemes mean.
  - `bolt+ssc://`, `neo4j+ssc://` — encrypt, verify neither. The documented opt-out: a
    self-signed certificate is accepted *because the scheme asked for it*.
  - `bolt://`, `http://`, `ws://` — do not encrypt, so there is nothing to verify.
  - `neo4j://`, and any scheme the component does not recognise — `UseSSL` decides, and a
    connection encrypted this way is fully verified; only `+ssc` opts out of verification.
    This is the case US-032 removes.

  The hostname verified is the host being connected to, carried in the same field used for
  `BIO_set_conn_hostname` and for SNI, so the host reached and the name checked cannot drift
  apart. Pinned for all ten schemes on both framings and both values of `UseSSL` by
  `Neo4jTls.Decision.SchemeMatrix`. What applying that decision guarantees is not the same
  on both transports; the section below states each separately.
- `UseSSL` remains a UPROPERTY and `BeginPlay` still overwrites it from the scheme, so
  setting `UseSSL = true` against a `bolt://` endpoint connects in plaintext, silently
  (F-011, open; US-032 removes the property, which is a Blueprint-ABI change of its own).
- Routing is likewise taken from the scheme: `neo4j://` and its secure variants request
  routing, `bolt://` does not. Under routing the server names the backend host the client
  reconnects to and the client obeys — that is the protocol, and it is safe only once the
  backend authenticates itself. On plain TCP it does: the scheme's promise comes from the
  configured endpoint, the name verified comes from the backend actually being connected to,
  and the address is stripped of any scheme before it is used, so a routing record of
  `wss+insecure://evil` resolves nowhere. On WebSocket it does not (F-055, below).
- Bolt version negotiation happens in the handshake; the HELLO variant used follows from
  the negotiated version, which is why three builders exist.
- Queries run off the game thread; `IncomingResponse` is broadcast on the game thread.
- `password` is a Config UPROPERTY and is never logged (P-15).
- Most of the connection has no tests, and every test it does have runs without a server.
  What the suite reaches, through `FNeo4jTestAccess`, is the PackStream codec and its four
  refusals, the message cap, the security decision for every scheme on both framings, and
  the application of that decision to an `SSL_CTX` the test itself owns. What it does not
  reach is the handshake, authentication, result mapping and the dechunker: the rest of
  `Neo4jComponent.cpp`, the second largest file in the project, has no tests (F-007),
  nothing in `BoltProtocol.h` can be linked from the test module (F-012), and the two
  certificate refusals that need a server holding a certificate are reached only in the
  structure they share — a refusal returns before a socket exists.

### What an encrypted connection guarantees, by transport

The decision above is taken for both framings, as it should be, but only one transport
applies it. Choosing a transport is therefore choosing between two different promises, and
this contract states them separately rather than making one unqualified claim that only the
plain TCP evidence supports.

**Plain TCP (`Transport = PlainTCP`, the default).** Here an encrypted connection is an
authenticated one, by four steps that are all present and all asserted. Before the BIO
exists — required, not stylistic, because an `SSL` inherits its context's verify mode and
verification parameters at creation and not afterwards — `ApplyTlsVerification` sets
`SSL_VERIFY_PEER`, loads a trust store and tests the load's return value, and writes the
decided hostname on the context's `X509_VERIFY_PARAM` with partial wildcards off (a full
leftmost wildcard still matches, which is what Aura serves). The trust store is the engine's
own, found the way the engine finds it: the host project's `Content/Certificates/cacert.pem`
if it exists, otherwise `Engine/Content/Certificates/ThirdParty/cacert.pem` — so a server
the engine's own HTTPS trusts is a server this component trusts, a project has one trust
store rather than two, and the project's bundle is where a site adds a private or corporate
root (DEC-023). After a successful handshake `SSL_get_verify_result` is inspected again,
because a completed handshake is a verified peer only on a context that was built to verify,
and reading the one as the other is exactly what F-020 was. Each of the four steps is read
off the context by the suite rather than inferred from a connection that failed, because
three of the four is the same hole with more code in it (DEC-024, DEC-025). `+ssc` is
honoured here, and only here.

**WebSocket (`Transport = WebSocket`).** Here the component decides and the engine disposes:
the branch builds a `ws://` or `wss://` URL and hands it to the engine's WebSockets module,
which owns verification entirely. The decision is advisory on this transport, and it is
advisory in both directions.

- *Stricter than the decision.* `+ssc` cannot be honoured: the engine's client verifies the
  chain itself and offers no way to relax it, so a self-signed server is unreachable over
  WebSocket. The component logs a warning at connect time saying so, rather than leaving a
  failed handshake to explain itself.
- *Weaker than the decision, two ways, neither of them observable from inside the plugin.*
  The engine-wide `[LwsWebSocket] bDisableCertValidation` key in `GEngineIni` and the
  `-lws-insecure=true` command-line switch each make the engine accept any certificate for
  every `wss://` connection in the process, this one included; `neo4j+s://` over WebSocket
  then encrypts without authenticating, and the plugin reads neither input and so cannot
  warn. And a routing record can carry a scheme: `ReconnectToBackendAddress` splits a
  server-supplied record on its last colon only, the WebSocket branch builds its URL from
  that raw address and rewrites eight schemes of which `wss+insecure://` is not one, and the
  engine's client accepts `wss+insecure` as "encrypt, allow self-signed". So a server
  reached over a *verified* `neo4j+s://` WebSocket connection can redirect the client to an
  unverified connection anywhere and receive its `LOGON` (F-055, major, open).

So on plain TCP, `+s` means the server proved its name to a trust anchor before the password
moved. On WebSocket, `+s` means the engine was asked to check, `+ssc` cannot be asked for at
all, and a project-level switch or a hostile server can mean nothing was checked. A caller
who needs the guarantee uses `PlainTCP`, which is already the default for an unrelated
reason (ADR-0003) and is now the recommended one for this reason too.
