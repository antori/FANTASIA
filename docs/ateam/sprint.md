# Current sprint
- As of: J-0061
- Current sprint: S-01
- Sprint length: one working session
- Definition of Ready: story passes INVEST; acceptance criteria in Given/When/Then; a Tests path is named; the modules it touches list it in their Satisfies lines
- Definition of Done: the quality gate passes; tester report with no open blocker finding; reviewer verdict "meets request" with no blocker; implementation decisions logged as DEC entries; checker clean; no ownership violation logged; the user accepted the story at review

Note on the gate clause: the gate exists. `gates.task_completed` is
`powershell -NoProfile -ExecutionPolicy Bypass -File FANTASIA/run-tests.ps1`, set in
J-0095 from T-02's measurement and re-run by the lead at 150 tests, 0 failures, 39.9 s.
`gates.quality` is still empty. The Definition of Done clause "the quality gate passes"
means that command.

### S-01: Conformance and characterisation
- Status: active
- Goal: Give the project a gate it can run, stop the credential leak, and pin the worst-tested real-time path, so that from this sprint on "done" is something the project can check rather than claim.
- Stories: US-027, US-028, US-029
- Tasks: T-01 (none, security, docs/ateam/threat-model.md, focus: the surfaces the verification profiles point at — secrets in four components, Prolog goals and Cypher built from strings, the vendored dependency tree) [no-gate], T-02 (US-027, build-dev, FANTASIA/run-tests.ps1, focus: that the measured warm time is real and the proposed gate command follows from it rather than from hope; security: it generates source and runs a compiler over paths taken from the environment, must touch nothing in the host project and put no credential in a generated file), T-03 (US-028, test-dev, FANTASIA/Source/FANTASIATests/Private/SecretConfigTests.cpp, focus: that the test fails before the fix and covers all six secret-bearing properties, not only the two being fixed; security: a test about credentials — fake keys only, nothing left in GConfig), T-04 (US-028, core-dev, FANTASIA/Source/FANTASIATests/Private/SecretConfigTests.cpp, focus: that the specifier is on the owning property and not only on the struct's fields, where it does nothing; security: the APIKey path), T-05 (US-028, speech-dev, FANTASIA/Source/FANTASIATests/Private/SecretConfigTests.cpp, focus: same, for the map an xi-api-key goes into; security: the Headers path), T-06 (US-029, speech-dev, FANTASIA/Source/FANTASIATests/Private/WhisperCaptureComponentTests.cpp, focus: that the seam exposes nothing beyond the test and adds no exported surface), T-07 (US-029, test-dev, FANTASIA/Source/FANTASIATests/Private/WhisperCaptureComponentTests.cpp, focus: that the tests pin what the code does today, bugs included, and that each one says whether that behaviour is believed correct or merely current), T-08 (none, planner, docs/ateam/, focus: that every contract, story and Satisfies line matches what actually landed) [no-gate]

## Dispatch order and effort

| Task | Owner | Story | Est | Effort | Depends on |
| :--- | :--- | :--- | :--- | :--- | :--- |
| T-01 | security | — (method prerequisite) | M | high | — |
| T-02 | build-dev | US-027 | M | **xhigh** | — |
| T-03 | test-dev | US-028 | S | medium | — |
| T-06 | speech-dev | US-029 | XS | low | — |
| T-04 | core-dev | US-028 | XS | medium | T-03 |
| T-05 | speech-dev | US-028 | XS | medium | T-03 |
| T-07 | test-dev | US-029 | M | high | T-06 |
| T-08 | planner | — | S | medium | T-02, T-04, T-05, T-07 |

T-01, T-02, T-03 and T-06 start together. T-02 is the only xhigh task: the warm run time
cannot be assumed, and what `gates.task_completed` becomes follows from measuring it.

US-029 is the story to drop if the session runs short, and T-06 is XS, so the drop point
is clean.

## Not in this sprint

- US-020 and US-021, the 5.8 ship: mostly the user's machine (multi-hour NVIDIA plugin
  builds, a ~5 GB release upload), and the user chose the method's order instead.
- **F-012 and the Neo4j half of F-007:** P-19 keeps both off the 5.8 line, and still does
  after CR-004 — F-012 is minor and quality, so the security carve-out does not reach it,
  and exporting `BoltProtocol.h` to close it widens C-neo4j's surface. The exported half
  of the Bolt layer already has 36 tests, so a Neo4j characterisation story would have
  added little this sprint; closing F-012 needs a change request on the 5.9 line.
- **F-011, F-020 and F-021 are admissible on the 5.8 line, and none was worked in S-01.**
  P-19's security carve-out (CR-004) admits a security finding whatever its surface, so all
  three are carried by US-030, US-031 and US-032 rather than deferred. US-020 and US-021,
  the 5.8 ship, wait on at least the first two.
- `gates.quality` for C++: there is no `.clang-format`, and choosing a format for a tree
  written by many hands is its own request with a very large first diff.
- Measures for NFR-001 to NFR-003: nothing can check a latency budget until the gate
  exists.
- The nine other open findings.
