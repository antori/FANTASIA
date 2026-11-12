# Team: FANTASIA
- As of: J-0055
- Approved: yes
- Mode: subagents
- Design version: 3
- Ownership: observe only

## Roster

| Agent | Role | Language | Modules | Model | Why |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `planner` | planner | — | — | opus | Evaluates every request and plans accepted work. Owns every state document, including the three module `CLAUDE.md` files. |
| `core-dev` | coder | cpp | M-core, M-llm, M-neo4j, M-demo | opus | Owns `C-fantasia-types`, the foundation eight modules consume and part of the Blueprint ABI, so a mistake invalidates users' compiled assets. Also the two HTTP language back ends and `Neo4jComponent.cpp` at 1244 lines. F-001, F-006, F-011 and F-012 are all here. |
| `speech-dev` | coder | cpp | M-asr, M-tts, M-ace | opus | The real-time half: microphone capture with VAD, streaming synthesis, and Audio2Face. Carries NFR-001, NFR-002 and NFR-003, none of which has a measure. Owns `C-rest-tts`, the project's only subclassing agreement, which M-ace extends — so F-003's duplication and F-009's missing consumer are one owner's to fix. |
| `reasoning-dev` | coder | cpp | M-prolog, M-probabilistic, M-ml | opus | Two niche third-party APIs: SWI-Prolog embedded on its own thread, and aGrUM. US-022, the 3.x migration at estimate L, lands here, and so does F-005. `M-ml` provides `C-math` to `M-probabilistic`. |
| `build-dev` | coder | csharp | M-dist | opus | `FANTASIA.Build.cs` is the most-churned first-party file in the project (29 commits), and `C-build` and `C-install` have four consumer modules between them. The whole first sprint (US-020, US-021) is here, and a mistake ships broken binaries to users who cannot rebuild (P-12). F-002, F-013, F-014 and F-015 are all M-dist. |
| `test-dev` | coder | cpp | M-tests | sonnet | Owns the automation suite, the project's only mechanical verification, and the characterisation tests the first sprint owes against F-007. Sonnet: it provides no contract and owns no foundation module, and it works from the verification profiles rather than holding ten contracts in its head. |
| `tester` | tester | — | — | sonnet | Designs and runs the adversarial unit and stress tests per story, in `FANTASIATests/Private/Quality/` and `Stress/`. Sonnet because the security engineer runs the security tests. |
| `security` | security | — | — | opus | Credentials in four components, two live credential-exposure findings, Prolog goals and Cypher built from strings, and a third-party tree of seven vendored projects. Owns `threat-model.md` and the security tests; owns no module and writes no source. |
| `reviewer` | reviewer | — | — | opus | Read-only verification per `verification/<coder>.md`. Owns nothing. |

The lead owns `sprint.md`, the team files, the root `CLAUDE.md` and `.claude/`, and is `restricted`, so it delegates rather than implements. Nobody owns `HISTORY.md`.

## Mode: subagents

The checker reports a strict parallel width of 6 and a contract-first width of 12. The
contract-first number is not real yet, and acting on it would be the mistake it warns
about: all twelve contracts are `Provenance: recovered`, version 1, and **not one has a
contract test**. Several were corrected during the adoption review, which is what an
immature contract looks like. Until characterisation tests pin them, parallelism beyond
the strict width buys rework waves.

The work in hand is also sequential: US-020 must land before US-021, because FANTASIAACE
links `ACERuntime` and `ACECore` and cannot be rebuilt until NV_ACE_Reference exists for
5.8. So the lead delegates one task at a time to the owning coder and integrates.

Cost scales with how many agents are active at once, not with how many are defined.
In `subagents` mode that is usually one coder plus, at the end of a story, the tester and
the reviewer — so two or three concurrent agents against a roster of nine. Five of the
nine are on opus, which is where the model policy puts them: every coder cluster here
contains a contract provider with two or more consumer modules, and a mistake in this
project ships as a broken prebuilt binary to users who cannot rebuild it.

## Rejected alternatives

- **`team` mode.** Rejected: it needs stable contracts, and none of the twelve is tested.
  It also needs `CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS=1` and in-process teammates. Worth
  revisiting after the first sprint, when the contract tests exist.
- **`single` mode.** Rejected: twelve modules across two plugins in four distinct
  technical domains is more than one coder should hold in context, and the C# build
  surface has nothing in common with the C++ one.
- **Fewer coders, one C++ owner plus one build owner.** Rejected: one coder would own
  every C++ module, which makes the ownership boundary meaningless and loses the reason
  for having module boundaries at all.
- **More coders, one per module.** Rejected: twelve coders for 18k lines, and the model
  policy caps implementers at four.
- **Four coders, holding the cap, by folding `M-tests` into `core-dev`.** This is the
  live alternative and the only one that satisfies the cap. `core-dev` already owns the
  project's shared surfaces, so giving it the shared test module is thematically
  consistent, and the adversarial work would still sit with the tester and the security
  engineer in their own directories. Not taken, because it makes one coder the maintainer
  of the tests covering two other coders' modules: a change to `WhisperCaptureComponent`
  would need `speech-dev` to ask `core-dev` to update the test that pins it, turning every
  behavioural change into a cross-coder request. The cap exists to limit coordination
  overhead, and in `subagents` mode the lead serialises everything, so the fifth coder
  costs little — whereas routing test changes through a second owner costs on every story.
  Worth revisiting the other way round: if `M-tests` were dissolved and its per-component
  test files folded into each module's `Paths` (a change request against the
  architecture), every coder would own the tests for its own modules, which is the
  arrangement the method prefers, and the roster would drop to four coders with no
  cross-coder test requests at all. That is the right end state; it is an architecture
  change, not a staffing decision.
- **A separate owner for `M-core`.** Considered, because it is the foundation and F-001
  says its types belong to five different components. Rejected for now: 680 lines is not
  an agent's worth of work, and the fix for F-001 is a change request, not an ownership
  split. If that CR lands and the types are split per component, `M-core` shrinks to the
  module interface and this question disappears.
- **Cheaper coders on sonnet.** Every cluster here contains a contract provider with two
  or more consumer modules, so the model policy puts all four on opus. For a
  single-maintainer project that is the expensive answer; `advisorModel` (experimental,
  Anthropic API only) is the middle road, letting a sonnet coder consult a stronger model
  at decision points. Worth trying on `reasoning-dev` or `speech-dev` once the contracts
  are tested and a mistake is cheaper to catch.
- **An interface designer and a personas designer.** Rejected: the product has no screens.
  Its people-facing surface is a Blueprint API, whose ergonomics live in the contracts and
  are checked by the planner and the reviewer. This changes if an editor UI appears, or if
  REQ-002's tutorials become a tracked deliverable with an audience to design for.
- **No security engineer, tester keeps the security tests.** Rejected: the two findings
  that matter most right now, F-008 and F-011, are both credential exposures, and both
  were found by accident rather than by anyone looking. That is the argument for someone
  whose job is to look.

## Deviations from the standard structure, and why

- **The three `CLAUDE.md` module state documents belong to the planner, and there is one
  per source tree rather than one per module.** Nine of the twelve modules live in the
  same two directories, `FANTASIA/Source/FANTASIA/Public/` and `Private/`, because the
  plugin has no directory structure expressing its components (that is why the modules are
  declared as file lists). A per-module `CLAUDE.md` is therefore impossible, and a
  per-tree one covers several coders' modules, so no single coder can own it. The planner
  owns state documents, so it owns these. If F-001's fix ever splits the shared types and
  the tree gains per-component directories, these become per-module files owned by their
  coders. **`M-dist` has no directory of its own at all** — its files are the three
  `.Build.cs`, the two `.uplugin` manifests, the scripts at `FANTASIA/`, both `Binaries/`
  trees and five files at the repository root — so it gets no `CLAUDE.md`. Its state lives
  in `C-build` and `C-install`, which are unusually detailed for that reason.
- **`M-tests` gets its own coder, `test-dev`, and that puts the roster one over the
  policy's cap of four coders.** This is the one place where the rules cannot all be
  satisfied at once, so it is the user's to settle at approval. The constraints:
  the checker requires every module to be assigned to an agent, and forbids a tester from
  owning modules — so the automation suite needs a coder. `M-dist` needs a C# coder of its
  own, since a coder is an expert in one language. That leaves the nine C++ component
  modules for two coders if the cap is to hold, which would mean one owner for M-core,
  M-llm, M-neo4j, M-demo, M-prolog, M-probabilistic and M-ml — seven modules spanning
  shared types, two HTTP back ends, Bolt framing, Prolog threading, aGrUM and libsvm, four
  of them in the same dependency layer, which is exactly the cluster the splitting rule
  says to split. The alternative that does hold the cap is in the rejected list below.
  `build-dev` owns the `FANTASIATests.Build.cs` beside the suite, which is packaging;
  the tester owns `Private/Quality/` and `Private/Stress/`, and the security engineer
  `Private/Security/`, all more specific than `test-dev`'s prefix, so the most-specific
  rule gives each its own ground inside the one test module.
- **`gates.task_completed` is `null` and `gates.quality` is empty.** This is the honest
  state, not an omission. There are no contract tests to run, and the automation suite
  needs the editor, so there is no fast command to gate a task on. For C++ there is no
  `.clang-format` and no `.clang-tidy` in the repository, neither tool is installed, and
  UBT emits no compilation database for clang-tidy to read. For C# the `Build.cs` files
  are compiled by UBT and belong to no solution, so `dotnet format` has nothing to act on.
  Filling these in with commands that cannot run would make the clean-code promise prose,
  which is the failure P-19's own rationale warns about. The first sprint should produce a
  real `task_completed`, and a `.clang-format` is worth its own request.

## Topology

Everyone reports to the lead. The planner decides interfaces and may message anyone.

Peer channels exist only where a contract runs between two coders:

- `core-dev` ↔ `speech-dev`, `reasoning-dev` — they consume `C-fantasia-types`.
- `build-dev` ↔ every other coder and the tester — they consume `C-build`.
- `test-dev` ↔ every other coder — it consumes ten contracts, so it has a channel to all
  of them, and it is the only coder that does.
- `speech-dev`, `reasoning-dev` ↔ `tester` — it consumes their contracts.
- `speech-dev` ↮ `reasoning-dev`: **no channel**, and that is the design. They are sibling
  component modules sharing no contract, which is what NFR-004 measures.

The tester may message the coders whose work it tests, for clarification only; findings go
through the lead as tasks. The reviewer reports to the lead and may message the planner.
The security engineer may message the planner, the tester and the coders on its surface;
its findings also go through the lead, and only the user accepts a risk.

The runtime does not enforce any of this — teams are flat. It lives in the prompts and is
audited through `.ateam/comms.jsonl`.

## Escalation

- **Interfaces, requests, story scope** → the planner. It answers from the state documents
  and cites file and ID; a question they cannot answer is a design gap and goes to
  `/ateam:change`.
- **Scheduling, blockers, cross-module work** → the lead.
- **Principles, team and ownership changes, breaking contract changes, accepting a sprint,
  accepting a security risk, promoting to main** → the user. P-13 keeps promotion theirs,
  and P-19's bar decides what the shipped line admits.
