# Current sprint
- As of: J-0159
- Current sprint: none active
- Sprint length: one working session
- Definition of Ready: story passes INVEST; acceptance criteria in Given/When/Then; a Tests path is named; the modules it touches list it in their Satisfies lines
- Definition of Done: the quality gate passes; tester report with no open blocker finding; reviewer verdict "meets request" with no blocker; implementation decisions logged as DEC entries; checker clean; no ownership violation logged; the user accepted the story at review

Note on the gate clause: the gate exists. `gates.task_completed` is
`powershell -NoProfile -ExecutionPolicy Bypass -File FANTASIA/run-tests.ps1`, and
"the quality gate passes" means that command. It runs 150 tests, warm in 30–45 s and
84.8 s when four test files recompile, holds an exclusive harness lock so only one run
proceeds at a time, and fails below its `-MinTests` floor of 134. `gates.quality` is still
empty for both cpp and csharp.

## No sprint is active

The next sprint's goal is the user's to set. S-01's record is in the journal
(`ateam_log.py show S-01`); nothing below is planned or committed to.

## Candidates, in the order their dependencies allow

- **US-030** (`ready`, M) — Bolt PackStream bounds, carrying F-021 (blocker, security). The
  exported codec is already linkable from the test module and already has 36 tests, so this
  is the one of the three Bolt fixes whose evidence needs no new infrastructure, and the one
  with no user-visible behaviour change on a well-behaved server.
- **US-031** (`ready`, S, depends on US-030) — TLS verification, carrying F-020 (blocker,
  security). **It breaks a connection that works today:** a self-signed certificate on a
  `bolt+s://` URL stops connecting, and the migration is one character — `bolt+ssc://`. The
  user confirms that trade before it is planned.
- **US-032** (`draft`, S) — F-011, the silent `UseSSL` downgrade. **Not ready:** its shape is
  the user's to choose between removing `UseSSL`, refusing to connect on mismatch, and
  warning and continuing.
- **US-033** (`ready`, S) — the injectable capture seam in M-asr, which makes `StartCapture`'s
  two hardware failure paths reachable from a test and closes F-018. Two tasks: speech-dev's
  seam, then test-dev's tests.

All four are admissible on the 5.8 line: the first three by P-19's security carve-out
(CR-004), and US-033 because it adds no exported symbol and no Blueprint-facing signature.
US-020 and US-021, the 5.8 ship, wait on at least US-030 and US-031, and on the NVIDIA ACE
5.8 port, which also blocks F-014 and F-016.

## What the user decides before the next sprint is planned

- **US-028's tooltip scope**: whether F-032's and F-033's sentences ride into the next
  sprint or become a story of their own.
- **US-032's shape**, above, which is what makes it `ready`.
- **F-046**: whether the three module `CLAUDE.md` state documents become tracked. They are
  ignored today, so neither F-045's fix nor the runtime module's J-0113 rewrite is in the
  repository. The recommendation on the finding is to narrow the ignore to `/CLAUDE.md`,
  root-anchored, and to make the hard-coded script paths relative first.
- **The prebuilt DLL refresh** (P-12), which four changed runtime headers now oblige: a clean
  rebuild of Win64 Development and DebugGame, `dumpbin`-verified, as its own commit. It
  belongs with the promotion to main.

## Standing exclusions

- **F-012 and the Neo4j half of F-007.** P-19 keeps both off the 5.8 line and CR-004 does not
  reach them: F-012 is minor and quality, and exporting `BoltProtocol.h` to close it widens
  C-neo4j's surface. Closing F-012 needs a change request on the 5.9 line.
- **`gates.quality` for C++.** There is no `.clang-format`, and choosing a format for a tree
  written by many hands is its own request with a very large first diff.
- **Measures for NFR-001 to NFR-003.** The gate makes them possible; none is written.
- **FANTASIA-LangGraphDemo**, out of scope and managed separately as the wiki is (CR-002).
