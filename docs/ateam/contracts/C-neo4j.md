# C-neo4j: Cypher queries over the Bolt protocol
- As of: J-0027
- Version: 1
- Provider: M-neo4j
- Consumers: M-demo, M-tests
- Tests: FANTASIA/Source/FANTASIATests/Private/BoltPackStreamTests.cpp
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
- `FBoltPackStreamReader` — `ReadValue()`, `HasMore() const`, `PeekMarker() const`

`BoltProtocol.h` — message construction and framing. Nothing in this header carries
`FANTASIA_API`, so none of it is exported: it is usable only inside M-neo4j, and no
test can reach it from the separate `FANTASIATests` module (F-012). The PackStream codec
above *is* exported, which is why it has 36 tests and this has none.

- `FBoltServerMessage`, `ParseBoltServerMessage(const TArray<uint8>&)`
- `FBoltDechunker` — `Accumulate(const void*, SIZE_T)`, `HasCompleteMessage() const`,
  `PopMessage(TArray<uint8>&)`, `Reset()`
- `ChunkMessage(const TArray<uint8>&, int32 MaxChunkSize = 16384)`, `BuildHandshake()`,
  `BuildHello`, `BuildHello51`, `BuildHello53`, `BuildLogon`, `BuildRun`, `BuildPull`,
  `BuildBegin`, `BuildCommit`, `BuildRollback`, `BuildReset`, `BuildGoodbye`, their
  pre-framed variants, and `BuildRunAndPull`

## Errors

- A connection, handshake or authentication failure is logged; no `IncomingResponse`
  fires. Callers must not assume a response per query.
- A Cypher error comes back as a Bolt FAILURE message and is logged; `FNeo4jResponse`
  arrives with no rows rather than with an error field.
- `PopMessage` returns false when no complete message has accumulated.
- `ReadValue` past the end of the buffer returns a null value; `HasMore()` is the
  caller's guard (pinned by `Reader.HasMoreEmpty`).
- An unknown PackStream marker yields a null value rather than throwing.

## Invariants

- Every PackStream value round-trips exactly: null, bool, the four integer widths,
  float, empty and short and medium strings, empty lists, lists of ints, maps, nested
  structures and structures — each pinned by its own test.
- `PeekMarker()` does not advance the reader (pinned by `Reader.PeekNoAdvance`).
- `Reset()` empties the writer's buffer and nothing else (pinned by `Writer.Reset`).
- `AsString()` is a diagnostic rendering, not a wire format, and is pinned for null,
  bool, int, list, map and structure.
- Transport is `PlainTCP` by default — the same framing native Neo4j drivers use —
  because Neo4j 2026.x stopped serving WebSocket-over-Bolt on 7687 (ADR-0003).
  WebSocket framing remains selectable.
- **TLS is decided by the endpoint scheme, not by the property.** `BeginPlay` sets
  `UseSSL` true for `neo4j+s://`, `neo4j+ssc://`, `bolt+s://`, `bolt+ssc://`, `https://`
  and `wss://`, false for `bolt://`, `http://` and `ws://`, and respects the property
  only for a scheme it does not recognise. Setting `UseSSL = true` against a `bolt://`
  endpoint therefore connects in plaintext, silently (F-011).
- Routing is likewise taken from the scheme: `neo4j://` and its secure variants request
  routing, `bolt://` does not.
- Bolt version negotiation happens in the handshake; the HELLO variant used follows from
  the negotiated version, which is why three builders exist.
- Queries run off the game thread; `IncomingResponse` is broadcast on the game thread.
- `password` is a Config UPROPERTY and is never logged (P-15).
- Chunking defaults to 16384 bytes, the Bolt maximum chunk size.
- Only the codec is tested. The connection, handshake, authentication, transport
  selection and result mapping in `Neo4jComponent.cpp`, at 1244 lines the second
  largest file in the project, have no tests at adoption (F-007) — and `BoltProtocol.h`
  could not be tested from the test module even if someone tried (F-012).
