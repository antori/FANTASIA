# Current sprint
- As of: J-0169
- Current sprint: S-02
- Sprint length: one working session
- Definition of Ready: story passes INVEST; acceptance criteria in Given/When/Then; a Tests path is named; the modules it touches list it in their Satisfies lines
- Definition of Done: the quality gate passes; tester report with no open blocker finding; reviewer verdict "meets request" with no blocker; implementation decisions logged as DEC entries; checker clean; no ownership violation logged; the user accepted the story at review

Note on the gate clause: `gates.task_completed` is
`powershell -NoProfile -ExecutionPolicy Bypass -File FANTASIA/run-tests.ps1`, and "the
quality gate passes" means that command. It runs 150 tests, holds an exclusive harness lock
so only one run proceeds at a time, and fails below its `-MinTests` floor of 134.
`gates.quality` is still empty for both cpp and csharp.

### S-02: A Neo4j connection safe to point at a real server
- Status: active
- Goal: Close the two blocker findings that make the Bolt connection unsafe against a real server - an unbounded message reader (F-021) and a TLS path that authenticates nobody (F-020) - so that US-014 returns to done and the 5.8 ship stops waiting on them.
- Budget: $160
- Stories: US-030, US-031, US-034
- Target line: 5.8 for all three

**Why two stories and not three.** US-014's `Blocked by:` line names F-020 and F-021 and
not F-011, so closing exactly these two returns US-014 to `done` and unblocks US-020 and
US-021. F-011 sits on its `Known defect:` line and on no critical path. US-032 is refined
to `ready` and **held for S-03**: it depends on the seam US-031 builds, so holding it costs
no rework, and shipping `UseSSL`'s removal in the same release as US-031's verification
change would put two breaking migrations of the same property of the same component into
one release note.

**US-033 is out.** It is `ready` and would occupy otherwise idle coders, but it is M-asr and
does not serve this goal; S-01's retrospective named scope discipline as the thing most
likely to be tested next. It heads the S-03 candidates with US-032.

**US-034 is off-goal and included because the user asked for it** — F-032's and F-033's
tooltips, two doc comments and one assertion.

## Tasks

| Task | Story | Module | Owner | Est | Effort | Depends on |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| T-24 | US-030 | M-neo4j | core-dev | M | **high** | — |
| T-26 | US-031 | M-dist | build-dev | XS | low | — |
| T-29 | US-034 | M-llm | core-dev | XS | low | — |
| T-30 | US-034 | M-tts | speech-dev | XS | low | — |
| T-25 | US-030 | M-tests | security | S | high | T-24 |
| T-27 | US-031 | M-neo4j | core-dev | M | **high** | T-25 |
| T-31 | US-034 | M-tests | test-dev | XS | medium | T-29, T-30 |
| T-28 | US-031 | M-tests | security | S | high | T-26, T-27 |
| T-32 | US-030, US-031 | contracts | planner | S | high | T-25, T-28 |

The serial chain is **T-24 → T-25 → T-27 → T-28 → T-32**, and it is the sprint: all of
M-neo4j is core-dev's, so one coder runs the critical path. T-26 starts first in parallel
(it only adds OpenSSL to the test module so T-28 can assert the TLS *context*, not merely
the TLS *decision*). T-29, T-30 and T-31 are off the chain entirely.

Task detail — each task's `focus:` for the reviewer and its `security:` line — is in the
journal at J-0165. The stories carry their own acceptance criteria.

## Decisions this sprint runs under

- **DEC-019** (J-0164, ruling at J-0166): the security engineer writes the batteries under
  `FANTASIA/Source/FANTASIATests/Private/Security/`, which it owns. No security review runs
  on T-24 or T-27, the two implementation tasks whose tests it wrote. The backstop is the
  tester's independent pass on the landed fix, which is where independence showed in S-01.
- **CR-010** (approved, J-0161): US-032 removes `UseSSL`. Not in this sprint.
- **US-031 breaks a working configuration** (J-0163, user-confirmed): a self-signed
  certificate on `bolt+s://` stops connecting. The one-character migration, `bolt+ssc://`,
  must appear in the failure's log message and in C-neo4j.
- **No CR for the dechunker.** `FBoltDechunker` carries no `FANTASIA_API` (F-012), so
  US-030's message cap is enforced on `UNeo4jComponent`'s side. If core-dev concludes the
  cap can only live inside the dechunker, **the task stops for a CR** rather than exporting
  a class in passing.

## Not in this sprint

- **US-032** (`ready`) and **US-033** (`ready`) — the S-03 candidates, above.
- **F-012 and the Neo4j half of F-007.** P-19 keeps both off the 5.8 line and CR-004 does
  not reach them: F-012 is minor and quality, and exporting `BoltProtocol.h` widens
  C-neo4j's surface. Closing F-012 needs a change request on the 5.9 line.
- **F-047** (minor) — two unreachable regions in `Neo4jComponent.cpp`. US-032 takes the
  `ws://`→`wss://` upgrade with the property it refers to; the unreachable branch at
  `:166-178` is F-047's and must not be swept into either story (P-07).
- **The prebuilt DLL refresh** (P-12), which belongs with the promotion to main.
- **`gates.quality` for C++**, and **measures for NFR-001 to NFR-003**, both unchanged
  from S-01.
