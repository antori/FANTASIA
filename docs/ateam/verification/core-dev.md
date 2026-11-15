# Verification profile: core-dev
- Coder: core-dev
- Modules: M-core, M-llm, M-neo4j, M-demo
- Language: cpp
- Reviewer: reviewer
- Tester: tester
- As of: J-0059
## This sprint (S-01)

One task: **T-04** (US-028) — add the `Config` specifier to
`UOpenAICompatibleComponent::Config` and say in its doc comment where the value now comes
from. This is F-008's first half: a live credential leak, admitted to the 5.8 line by the
user's own decision in CR-001.

What the reviewer examines hardest:

1. **The specifier is on the owning property**, not added again to the struct's inner
   fields. The inner `Config` specifiers are exactly the mistake the finding describes:
   `LoadConfig` walks the class's own `Config` properties and never descends into a struct
   property that is not itself `Config`, so they have never fired.
2. **The specifier alone is not the fix.** A project that already typed a key into an
   asset keeps it there until the asset is re-saved, so the doc comment — which is the
   tooltip the user reads in the editor — must say that the value is read from the
   project's `DefaultGame.ini` and that an existing asset must be re-saved for the key to
   leave it. A task that reports "the specifier is present" and stops has not finished.
3. **The round-trip is demonstrated, not assumed.** A `Config` specifier on a
   struct-typed property serialises through `ExportText`; T-03's test proves it with a
   real reload rather than reading back a value the test itself set.
4. **Nothing else in the header moved.** Every public thing in this tree is Blueprint ABI;
   the property keeps its name, type, category and `EditAnywhere, BlueprintReadWrite`
   specifiers, which is also the clause of P-19 that lets this fix onto the shipped line
   at all.
5. **P-07, hard.** F-011 is in this owner's other module and is *not* admissible on this
   line (its fix changes C-neo4j). Do not fix it while passing through, however obviously
   wrong it is.

## Reviewer checklist

- Contracts provided: **C-fantasia-types** — every type in it is reflected, so renaming one, reordering an enum, retyping a property or changing a delegate signature breaks compiled Blueprints in users' projects. Appending an enum value is safe; inserting one is not. It has no error channel: result carriers report outcome in their own fields (`USWIPrologSolution::verified`, an empty `FSWIPrologResponse::results`). **C-llm** — an HTTP failure is logged and *no* response event fires, so a caller must not assume a reply per request; `ApplyPreset` overwrites `Config` wholesale including `APIKey`. **C-neo4j** — a Cypher error returns as a Bolt FAILURE, logged, with `FNeo4jResponse` arriving empty rather than carrying an error; `ReadValue` past the end returns null and `HasMore()` is the caller's guard.
- Contracts consumed: **C-fantasia-types** (by M-llm, M-neo4j, M-demo — the same owner provides and consumes it, so a change here is invisible to the ownership hook and must still go through a CR), **C-build** (M-demo reads `WITH_AGRUM_BINDING`), and in M-demo every other component's contract.
- Invariants: requests run off the game thread and every delegate is broadcast on the game thread, in M-llm and M-neo4j both. Conversation history is the caller's — neither LLM component keeps it between calls. The five preset factories return exactly their provider's endpoint, model and auth requirement. Every PackStream value round-trips exactly. `PeekMarker()` does not advance the reader. Bolt chunking is 16384 bytes, the protocol maximum. M-core holds no state beyond the module object and reaches no other module.
- Language checks: see `references/languages/cpp.md`; here especially — no raw `new`/`delete` in the UE paths, `UPROPERTY` on anything holding a `UObject*` that must survive GC, `Category` on every `BlueprintReadWrite` property (5.8's UHT requires it), IWYU includes, and no `std::` type crossing a module boundary.
- Principles at risk: **P-15** — F-008's first half is here: `UOpenAICompatibleComponent::Config` lacks the `Config` specifier, so `APIKey` is written into committed asset defaults; the `Config` specifiers on the struct's fields do nothing. **P-03** — any edit to `FANTASIATypes.h` is a Blueprint-ABI change and major however small the diff. **P-19** — F-011's fix touches C-neo4j, so it is not admissible on the 5.8 line; do not "just fix it" while in this module. **P-12** — these modules compile into the shipped DLL.
- NFRs touched: **NFR-004** (M-core satisfies it, and is simultaneously where F-001 breaks it: five components' vocabularies in one header).
- Known weak spots: **F-001** (major) `FANTASIATypes.h`, fan-in of eleven headers, 17 commits — the second-highest churn in the project. **F-011** (major) `BeginPlay` overwrites a user-set `UseSSL` from the endpoint scheme, silently. **F-012** (minor) nothing in `BoltProtocol.h` carries `FANTASIA_API`, so the Bolt message layer cannot be linked from the test module. **F-006** (minor) `paramObject.h` and the two gRPC guard headers are reachable from nothing. **F-004** (minor) `FANTASIATestActor` ships in the runtime module and includes all eight component headers. Hotspots: `Neo4jComponent.cpp` 1244 lines and 14 commits, `LangGraphComponent.cpp` 12, `FANTASIA.cpp` 12. Only the PackStream codec and the LLM presets are tested; the connection, handshake, auth, transport selection and result mapping are not.
- Evidence to produce: the story's `Tests:` path run; the gate; for any `FANTASIATypes.h` edit, a statement of which Blueprint-visible types changed and why it is not an ABI break, or the CR that says it is.

## Tester plan

- Unit: the Bolt dechunker across chunk boundaries and short reads; `FBoltValue` round-trips for every marker including the sizes not yet covered (long strings past 65535, deeply nested structures); `ReadValue` past the buffer end; the LLM stream parsers on split SSE lines, on an Anthropic content-block array, and on a stream that ends mid-object; `FChatTurn` and the Neo4j cell hierarchy defaults. Edge cases: empty Cypher result, a row whose cell is null, a map with duplicate keys, a `FANTASIATypes` enum at its maximum value.
- Security surface: `docs/ateam/threat-model.md` § M-core, M-llm, M-neo4j, M-demo — the security engineer writes it in T-01, this sprint, and reviews T-04 against it. In outline, what it must cover: Cypher built from strings with parameters passed separately or not; the `APIKey` and `password` paths and where they are written (F-008); TLS selection taken from the endpoint scheme rather than the property (F-011); and the Bolt reader as a parser of bytes from the network.
- Stress: a Neo4j result set large enough to force many chunks, measuring time and peak memory against NFR-004's cost rather than a stated budget, since none exists; a streaming LLM response of several thousand deltas, checking the game thread is never blocked. Report a baseline where there is no Measure.
- Tools: the UE automation framework (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`), which is what the suite already uses. No external harness; a new tool is a dependency and needs an ADR (P-06).
- Paths: `FANTASIA/Source/FANTASIATests/Private/` for unit and stress; `FANTASIA/Source/FANTASIATests/Private/Security/` is the security engineer's, not the tester's.
