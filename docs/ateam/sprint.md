# Current sprint
- As of: J-0244
- Current sprint: S-03
- Sprint length: one working session
- Definition of Ready: story passes INVEST; acceptance criteria in Given/When/Then; a Tests path is named; the modules it touches list it in their Satisfies lines
- Definition of Done: the quality gate passes; tester report with no open blocker finding; reviewer verdict "meets request" with no blocker; implementation decisions logged as DEC entries; checker clean; no ownership violation logged; the user accepted the story at review

Note on the gate clause: `gates.task_completed` is
`powershell -NoProfile -ExecutionPolicy Bypass -File FANTASIA/run-tests.ps1`, and "the
quality gate passes" means that command. It runs 198 tests, holds an exclusive harness lock
so only one run proceeds at a time, and fails below its `-MinTests` floor of 134.
`gates.quality` is still empty for both cpp and csharp.

### S-03: not planned
- Status: planned

There is no goal yet. The goal is the user's and the plan is theirs to confirm; what the
sprint starts from is below.

## Two change requests are owed first, and both are the user's

- **US-034's first criterion.** It asks `UOpenAICompatibleComponent::Config`'s metadata to
  say the key must be set again after `Apply Provider Preset`. Printing that advice is what
  ADR-0001 exists to prevent, so the product is right and the criterion is wrong. The change
  moves the clause onto `Config` and replaces set-again with the ini route the metadata
  already gives. Until it lands US-034 cannot be `done`: the verdict standing against it is
  J-0214 "does not meet request", and the order is the change request, then a fresh verdict
  against the present state, then `done`. The precedent is CR-009 on US-029 (J-0143), where
  the criterion was amended and the reviewer then issued "meets request against the criterion
  as amended" — not US-028, whose verdict was "meets request" with a residual finding.
- **US-014's first acceptance criterion** is now coarser than the behaviour. It promises that
  handshake, authentication and query exchange succeed against a local install "with or
  without TLS"; after US-031 a self-signed certificate on `bolt+s://` is refused and needs
  `bolt+ssc://`. US-014 is `done` on the criterion as written, which is why this is a change
  request and not a defect.

## Carried from S-02

- **US-034** (`in-progress`) — built, gate green, accepted by the user at J-0242, and blocked
  only on the change request above and the verdict that follows it. Its residuals F-049 and
  F-064 are minor.

## The candidates, in the order S-02 left them

- **US-032** (`ready`) — removes `UseSSL` (CR-010, approved). It depends on the seam US-031
  built, so holding it cost no rework. **F-062** (minor, open) records a false premise inside
  its criterion; it is the planner's and should be fixed before the story is dispatched.
- **US-033** (`ready`) — the injectable capture seam, closing F-018. Two tasks: speech-dev's
  seam, then test-dev's tests. It is M-asr and was off S-02's goal, which is why it waited.

## What the retrospective put on the table

- **F-070** (major, open) — a gate report is evidence for the tree it ran on and no other.
  S-02 used one report as standing evidence across changes made after it, which is how a red
  suite reached `origin/Development` and came within a step of main. The preventive measure is
  written into the finding.
- **F-052** (major, open) — `.ateam/ownership.jsonl` does not exist, so it records nothing,
  and two cross-ownership edits happened unlogged. Enforcement cannot be argued from an empty
  log until a sprint measures ownership for real.
- **`gates.quality` is empty for cpp and csharp**, unchanged since S-01, so the coders'
  clean-code promise still has no mechanical check. This is the third sprint it has been owed.
- **Scope discipline was tested and failed**, exactly as S-01's retrospective predicted: S-02
  fixed five findings that blocked none of its stories and that it never planned — F-014,
  F-016, F-050, F-051 and F-059.
- **Eight major findings opened during S-02 are still open**: F-048, F-052, F-055, F-056,
  F-060, F-061, F-069 and F-070. The sprint opened 23 findings in all, 9 major and 13 minor,
  and closed 3 of them.

## Not in the next sprint unless the user says otherwise

- **F-012 and the Neo4j half of F-007.** P-19 keeps both off the 5.8 line. Closing F-012
  needs a change request, since nothing in `BoltProtocol.h` carries `FANTASIA_API` and
  exporting it widens C-neo4j's surface.
- **F-047** (minor) — the unreachable regions in `Neo4jComponent.cpp`. US-032 takes the
  `ws://` to `wss://` upgrade with the property it refers to; the branch at `:166-178` is
  F-047's and must not be swept into it (P-07).
- **Measures for NFR-001 to NFR-003**, unchanged from S-01.
