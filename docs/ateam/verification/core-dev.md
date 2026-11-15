# Verification profile: core-dev
- Coder: core-dev
- Modules: M-core, M-llm, M-neo4j, M-demo
- Language: cpp
- Reviewer: reviewer
- Tester: tester
- As of: J-0165
## This sprint (S-02)

Three tasks, and the first two are the sprint: **T-24** (US-030) and **T-27** (US-031), the
two blocker security findings in M-neo4j, in that order and not in parallel, because both
touch `Neo4jComponent.cpp` or `BoltPackStream.cpp`. **T-29** (US-034, M-llm) is one sentence
in a doc comment and touches nothing either of them touches. P-19 admits all three on the
5.8 line: the first two by CR-004's security carve-out, the third because only a comment
changes.

**T-24 — bound the Bolt reader (F-021, blocker).** This reader is the project's only parser
of attacker-controlled bytes that a test can reach. Examined hardest:

1. **Every bound is checked before the act, not recovered from after it.** The length or
   count check precedes `Reserve` and `Append` on all four of the string, bytes, list and
   map paths; the depth counter is decremented on every return path including the refusals;
   and a refusal leaves the reader in a state where the next `ReadValue` also refuses rather
   than reading past the end. A bound checked after a `Reserve` is F-021 with a log line.
2. **The message cap is enforced where a test can see it.** `FBoltDechunker` carries no
   `FANTASIA_API` and gains none here — F-012 stays deferred, and widening C-neo4j's surface
   is not what CR-004 admits. `UNeo4jComponent` is declared `class FANTASIA_API`, so its
   private members are already exported and a cap on the component's side of the dechunker
   is reachable through a `friend struct FNeo4jTestAccess`. If the cap can only live inside
   the dechunker, the task stops and asks for a CR rather than exporting the class while
   passing through.
3. **The two numbers are chosen and justified, not defaulted.** The depth limit and the
   maximum message size go into C-neo4j, and the cap has to be defensible against a large
   `PULL` response, which is a legitimately big message.
4. **All 36 codec tests pass unchanged.** None of them is near a limit, so a changed
   expectation in an existing test means the bound is in the wrong place.
5. **P-07.** The reader and the cap. Not F-012, not F-007's other half, not the dead branch
   in `InitiateConnection`.

**T-27 — verify the server (F-020, blocker), and the seam that makes it assertable.**
Examined hardest:

1. **The applying, not the deciding.** `ShouldUseSSL` already decides correctly; F-020 is
   that nothing is applied. Four things must each be present and each checked:
   `SSL_CTX_set_verify` with `SSL_VERIFY_PEER`, a trust store whose load *return value* is
   tested, the hostname set on the X509 verification parameters, and `SSL_get_verify_result`
   inspected after the handshake. Three of the four is F-020 again with more code.
2. **`LOGON` is unreachable on a failed verification.** The credentials are the first thing
   on the session, so a teardown that happens after they are sent has mitigated nothing.
3. **The hostname verified is the host connected to.** `ReconnectToBackendAddress` connects
   to a host the *server* named through the routing table, so that host — not the original
   endpoint's — is the one whose certificate must match.
4. **The seam costs no surface.** Private static members, a `friend struct
   FNeo4jTestAccess`, no `UPROPERTY`, `UFUNCTION`, delegate or enum declaration touched, no
   new `FANTASIA_API`. This is US-033's rule, which the user approved for M-asr under
   CR-009, applied to M-neo4j.
5. **The trust store is a recorded decision.** The engine's certificate bundle and
   `SSL_CTX_set_default_verify_paths` give different answers on a Windows machine with a
   corporate root; whichever is chosen goes into a DEC and into C-neo4j.
6. **`bolt+ssc://` is in the failure message.** The user accepted that this story breaks a
   working self-signed connection (J-0163) on the condition that the one-character migration
   appears in the log line and in the contract.
7. **P-07.** `UseSSL` is **not** removed here. That is US-032, which is not in this sprint,
   however convenient it would be to take while the file is open.

**T-29 — one sentence (F-032, minor).** `UOpenAICompatibleComponent::Config`'s doc comment
gains the fact that Apply Provider Preset replaces the whole configuration, key included.
Examined hardest: that the comment is the whole diff, and that it names the Blueprint node a
user calls ("Apply Provider Preset") and not the C++ `ApplyPreset`. The behaviour is not
fixed on this line: preserving the key across the preset changes C-llm's documented
behaviour, and P-19 refuses it.

## Reviewer checklist

- Contracts provided: **C-fantasia-types** — every type in it is reflected, so renaming one, reordering an enum, retyping a property or changing a delegate signature breaks compiled Blueprints in users' projects. Appending an enum value is safe; inserting one is not. It has no error channel: result carriers report outcome in their own fields (`USWIPrologSolution::verified`, an empty `FSWIPrologResponse::results`). **C-llm** — an HTTP failure is logged and *no* response event fires, so a caller must not assume a reply per request; `ApplyPreset` overwrites `Config` wholesale including `APIKey`. **C-neo4j** — a Cypher error returns as a Bolt FAILURE, logged, with `FNeo4jResponse` arriving empty rather than carrying an error; `ReadValue` past the end returns null and `HasMore()` is the caller's guard.
- Contracts consumed: **C-fantasia-types** (by M-llm, M-neo4j, M-demo — the same owner provides and consumes it, so a change here is invisible to the ownership hook and must still go through a CR), **C-build** (M-demo reads `WITH_AGRUM_BINDING`), and in M-demo every other component's contract.
- Invariants: requests run off the game thread and every delegate is broadcast on the game thread, in M-llm and M-neo4j both. Conversation history is the caller's — neither LLM component keeps it between calls. The five preset factories return exactly their provider's endpoint, model and auth requirement. Every PackStream value round-trips exactly. `PeekMarker()` does not advance the reader. Bolt chunking is 16384 bytes, the protocol maximum. M-core holds no state beyond the module object and reaches no other module.
- Language checks: see `references/languages/cpp.md`; here especially — no raw `new`/`delete` in the UE paths, `UPROPERTY` on anything holding a `UObject*` that must survive GC, `Category` on every `BlueprintReadWrite` property (5.8's UHT requires it), IWYU includes, and no `std::` type crossing a module boundary.
- Principles at risk: **P-19** — the carve-out (CR-004) admits exactly three findings in this owner's modules, F-011, F-020 and F-021, and only their fixes. F-012 and F-007's Neo4j half are still refused, in the same module as all three, which is the whole point of the carve-out's wording: the module is not the criterion, the finding's kind and severity are. **P-03** — removing `UseSSL` is a Blueprint ABI change and needed CR-010 for that reason alone; it is **not** in this sprint, and any edit to `FANTASIATypes.h` is the same kind of change however small the diff. **P-07** — two of this sprint's three tasks sit in the file carrying the most unfixed findings in the project; the admitted item is the fix, never the refactor convenient to carry with it. **P-15** — `user` and `password` cross the wire on the first message of every session and are logged nowhere; a diagnostic added while debugging TLS is how that breaks. **P-12** — these modules compile into the shipped DLL.
- NFRs touched: **NFR-004** (M-core satisfies it, and is simultaneously where F-001 breaks it: five components' vocabularies in one header).
- Known weak spots: **F-020** and **F-021** (both blocker, security) are this sprint's first two tasks and are described in the section above. **F-011** (major, security) is in the same file and is **not** this sprint — CR-010 settled its shape and US-032 carries it. **F-001** (major) `FANTASIATypes.h`, fan-in of eleven headers, 17 commits, the second-highest churn in the project. **F-031** (major, security) is F-008's accepted residual: `Config` adds an ini source and does not make a property transient, so a key typed into the Details panel still reaches the asset. **F-012** (minor) nothing in `BoltProtocol.h` carries `FANTASIA_API`, which is why T-24's message cap has to be enforced on the component's side of the dechunker. **F-006** (minor) `paramObject.h` and the two gRPC guard headers are reachable from nothing. **F-004** (minor) `FANTASIATestActor` ships in the runtime module and includes all eight component headers. Unlogged, and found while planning S-02: `InitiateConnection`'s full-URL branch (`Neo4jComponent.cpp:172-181`) and the `ws://`-to-`wss://` upgrade at `:186` are both unreachable, because both call sites pass a bare hostname. Hotspots: `Neo4jComponent.cpp` 1244 lines and 14 commits, `LangGraphComponent.cpp` 12, `FANTASIA.cpp` 12. Tested: the PackStream codec (36 tests), the LLM presets, the secret specifiers. Untested: the connection, handshake, authentication, transport selection and result mapping.
- Evidence to produce: the story's `Tests:` path run and the gate, on every task. For T-24, the depth limit and the message cap with the reasoning that chose them, and a statement of where the cap is enforced and how a test reaches it without a new exported symbol. For T-27, the four applied TLS calls named with their line numbers, the trust store as a DEC, and a statement that the seam adds no exported symbol and no reflected member. For any `FANTASIATypes.h` edit, which Blueprint-visible types changed and why it is not an ABI break, or the CR that says it is.

## Tester plan

- Unit: `ReadValue` past the buffer end, at every integer width and on every length-prefixed marker — which T-25 turns from this plan's suggestion into US-030's acceptance test, so the tester's own pass goes after what the acceptance test leaves: a truncated structure header, a map whose declared count exceeds its pairs, a list nested exactly to the depth limit and exactly one past it. The Bolt dechunker **cannot** be unit-tested from the test module (F-012), which is the constraint T-24 works under; chunk-boundary behaviour is reachable only through `UNeo4jComponent`. `FBoltValue` round-trips for the sizes not yet covered (long strings past 65535, deeply nested structures); the LLM stream parsers on split SSE lines, on an Anthropic content-block array, and on a stream that ends mid-object; `FChatTurn` and the Neo4j cell hierarchy defaults. Edge cases: empty Cypher result, a row whose cell is null, a map with duplicate keys, a `FANTASIATypes` enum at its maximum value.
- Security surface: `docs/ateam/threat-model.md` § M-core, M-llm, M-neo4j, M-demo, which the security engineer owns. § M-neo4j's two unmitigated threats are this sprint's first two tasks — *Spoofing the server, and disclosure of the credentials and the whole session* (F-020) and *Memory corruption and exhaustion from a server message* (F-021) — and the *Redirection through the routing table* entry closes with the first, because `neo4j://` lets the server name the host the client reconnects to. Two things in that section are stale and are the security engineer's to correct: it says F-011 is excluded from the 5.8 line by CR-001, where CR-004 admitted it and CR-010 chose its shape, and it lists `UseSSL` among M-neo4j's entry points, which US-032 removes. *Cypher injection* stays in Open risks: the string-only parameter map is a C-neo4j version and a Blueprint-ABI change, so it is a change request and not a defect.
- Stress: a Bolt message assembled from chunks just under and just over T-24's cap, confirming that the cap is what stops it and not an allocation failure, and that the connection survives the refusal; a Neo4j result set large enough to force many chunks, measuring time and peak memory against NFR-004's cost rather than a stated budget, since none exists; a streaming LLM response of several thousand deltas, checking the game thread is never blocked. Report a baseline where there is no Measure.
- Tools: the UE automation framework (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`), which is what the suite already uses. No external harness; a new tool is a dependency and needs an ADR (P-06).
- Paths: `FANTASIA/Source/FANTASIATests/Private/` for unit and stress; `FANTASIA/Source/FANTASIATests/Private/Security/` is the security engineer's, not the tester's.
