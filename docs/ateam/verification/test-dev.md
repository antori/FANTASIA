# Verification profile: test-dev
- Coder: test-dev
- Modules: M-tests
- Language: cpp
- Reviewer: reviewer
- Tester: tester
- As of: J-0059
## This sprint (S-01)

Two tasks, and both are what the sprint is for: this sprint is conformance and
characterisation, so this coder writes the evidence everything else is judged by.

**T-03 (US-028, `Private/SecretConfigTests.cpp`, new file)** — pin the `Config` flag of
every secret-bearing property in the plugin, all six: the two that lack it today
(`UOpenAICompatibleComponent::Config`, `URESTTTSComponent::Headers`) and the four that
have it (`UElevenLabsTTSComponent::Key`, `UNeo4jComponent::password`,
`URESTTTSComponent::URL`, `URESTTTSComponent::BodyTemplate`). Written **before** the two
fixes, so it begins as a failing test marked expected against F-008, and the fixes are
what turn it green. Examined hardest:

1. The flag is read off the class's reflected property, not inferred from a value, so the
   test cannot pass because something happened to be set.
2. The reload case proves serialisation for a struct property and for a map property —
   the two shapes nobody has verified — rather than asserting back a value the test wrote
   to the object.
3. No real key, no write to the user's `DefaultGame.ini`, nothing left behind in `GConfig`
   after the test (P-15: a test fixture holding a credential is a leak with extra steps).
4. This file is a standing guard, so it is written to be extended: a new secret-bearing
   property should be one line to add.

**T-07 (US-029, `Private/WhisperCaptureComponentTests.cpp`, new file, after T-06's seam)**
— characterise the microphone capture path: the largest file in the project, 1250 lines,
a real-time path with threading and no test at all (F-007). Examined hardest:

1. **Characterisation, not correction.** Every assertion pins what the code does today. A
   value that looks wrong is pinned as it is and reported as a finding; "fixing" it here
   is the neighbourhood (P-07) and would also hide a behaviour change inside a test commit.
2. **No device, no model, no network, under a second.** A test that needs a microphone
   fails on a machine without one and teaches people to ignore red.
3. The seam is used to *read* private state, not to drive the component into a state the
   public API cannot reach and then assert on it — that pins an accident.
4. Each test's name says which C-asr claim it pins, because C-asr currently ends by
   admitting that every invariant without a test name beside it is read from the code
   rather than pinned, and this task is how that sentence starts shrinking.

Not in reach this sprint, and not a gap to feel bad about: the Bolt message layer. F-012
makes it unlinkable from here and the export is a C-neo4j change P-19 keeps off the 5.8
line.

## Reviewer checklist

- Contracts provided: none. M-tests exists to observe, not to be depended on, and nothing
  may import from it.
- Contracts consumed: ten — `C-fantasia-types`, `C-asr`, `C-rest-tts`,
  `C-elevenlabs-protocol`, `C-llm`, `C-prolog`, `C-probabilistic`, `C-math`, `C-neo4j`
  and `C-build`. A test may only use a contract's documented surface: a test that reaches
  past it, or that depends on behaviour the contract does not state, pins an accident.
  When a test cannot be written through the contract, that is a finding about the
  contract, not a licence to go around it.
- Invariants: a test is named `FANTASIA.<Area>.<Case>` and lives in a file named after
  what it tests. Tests pin behaviour and never fix source — a defect becomes a finding and
  the fix is the owning coder's. A test needing the CUDA stack, a live endpoint or a
  database **skips cleanly** when it is absent; a test that fails for want of an
  environment is worse than no test, because it teaches people to ignore red. Anything
  guarded by a build define in the module under test is guarded the same way here. A
  failing test documenting an open finding carries the expected-failure marker with the
  finding ID, so the suite stays green until the fix removes it.
- Language checks: see `references/languages/cpp.md`; here especially — no production
  logic in a test helper, no sleep-based synchronisation (use the framework's latent
  commands), and no test that depends on another test having run.
- Principles at risk: **P-04** — this module *is* most of P-04's mechanism, so a test that
  passes for the wrong reason weakens every story's evidence. **P-07** — a
  characterisation test records what the code does now, including its bugs; "fixing" the
  behaviour while writing the test is the neighbourhood, not the story. **P-05** — a test
  that over-fits (asserting an incidental string, an iteration order, a timing) is the
  opposite of lean and breaks on every unrelated change.
- NFRs touched: none directly. M-tests satisfies no requirement: a suite observes
  modifiability, it cannot provide it. NFR-004's measure is read off the dependency view,
  not off this module.
- Known weak spots: **F-007** (major) is this module's own gap and the first sprint's
  work: `WhisperCaptureComponent.cpp` and `Neo4jComponent.cpp`, the two largest files in
  the project, have no tests, and both are real-time paths with threading. **F-012**
  (minor) makes part of the gap structural, not merely undone: nothing in `BoltProtocol.h`
  carries `FANTASIA_API`, so the Bolt message layer cannot be linked from here at all —
  the fix is `core-dev`'s and it is a contract change. Twelve `Tests:` paths named in the
  backlog do not exist yet; two of them are this sprint's (US-028, US-029). 16 of the 109 tests sit behind `WITH_AGRUM_BINDING`, so the
  count is 109 only on Win64.
- Evidence to produce: the suite run, with the before-and-after test count; for a
  characterisation test, a sentence on what behaviour it pins and whether that behaviour
  is believed correct or merely current — the second kind needs a finding beside it.

## Tester plan

The tester's adversarial work lives in `Private/Quality/` and `Private/Stress/` and is
planned from each coder's own profile, not from this one. What belongs here is the
meta-check on the suite itself:

- Unit: that every test skips rather than fails when its environment is absent; that no
  test depends on execution order; that the expected-failure markers in the suite still
  name open findings, and that a marker whose finding was closed is removed.
- Security surface: `docs/ateam/threat-model.md` § M-tests — written by the security
  engineer in T-01, this sprint. Thin by nature, but not empty: test fixtures must carry
  no real credential, and a test that reads a key from `DefaultGame.ini` to talk to a live
  endpoint is a secret in CI's output waiting to happen (P-15). T-03 sits directly on this
  surface: it is a test *about* credentials.
- Stress: the suite's own runtime. It needs the editor, which is why
  `gates.task_completed` is `null`; US-027 produces the command and measures it this
  sprint, and the number it records decides whether a per-task gate runs the whole suite
  or a prefix of it. Two tests added here raise that number, so keep them cheap: T-07's
  file is required to finish in under a second.
- Tools: the UE automation framework only. A new harness is a dependency and needs an ADR
  (P-06).
- Paths: `FANTASIA/Source/FANTASIATests/Private/` and `Public/` are this coder's;
  `Private/Quality/`, `Private/Stress/` and `Private/Security/` are not.
