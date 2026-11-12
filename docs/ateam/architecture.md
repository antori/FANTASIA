# Architecture: FANTASIA
- Design version: 3
- Status: approved
- As of: J-0054
- Approved: yes
- Origin: adopted 2026-11-12

## Context and constraints

FANTASIA is two Unreal Engine plugins in one repository: `FANTASIA`, the core plugin,
and `FANTASIAACE`, an optional companion that adds NVIDIA Audio2Face lip-sync. The
core plugin builds two engine modules — `FANTASIA` (Runtime) and `FANTASIATests`
(UncookedOnly) — and the companion builds one.

The modules below are finer than the engine's build modules. Unreal gives no link
boundary inside a single module, and the core plugin's first-party code lives in one
flat `Private`/`Public` pair, so nothing in the directory layout separates the
components. What separates them is the include graph, and it is remarkably clean: each
component reaches the shared types and nothing else. The modules named here are that
structure made explicit, so that ownership and contracts can rest on it. They are not a
proposal to split the build.

Binding constraints, from the brief: Win64 only; Unreal 5.8 on the live line; prebuilt
binaries are part of the product and must stay in step with the source; heavy artifacts
live in release assets; C++20, exceptions on, RTTI off, no unity build; no CI.

Scope of this adoption: every first-party source, script and manifest file in the
repository. Vendored third-party code is outside it (see Unmapped areas).

## Decisions

- Secrets reach the components as Config UPROPERTYs read from DefaultGame.ini, never as literals in source (ADR-0001).
- Synthesized speech is buffered per call and consumed by a paced, ordered consumer that signals end of turn, rather than played as it arrives (ADR-0002).
- Neo4j speaks Bolt over plain TCP by default; WebSocket framing stays as an alternative transport (ADR-0003).
- The AWS and Azure components are absent, not deprecated: no vendor SDK is carried (ADR-0004).
- Audio2Face lip-sync lives in a separate optional plugin, so the core plugin never references NVIDIA ACE (ADR-0005).
- Whisper GPU support is runtime dispatch to a separately built shared library, not a compile-time switch (ADR-0006).
- Models, GPU stacks, build-time static libraries and vendor bundles are published as release assets and fetched by bootstrap (ADR-0007).
- Bootstrap installs the whole turnkey stack by default and is narrowed by opt-out flags (ADR-0008).
- aGrUM is vendored as per-module static libraries, linked ID then BN then BASE (ADR-0009).
- aGrUM moves to the 3.x line, with KTBN dynamic Bayesian networks exposed first; the target version and the static library targets are established by building, never asserted, and the contracts follow when the migration lands (ADR-0015, amending ADR-0009).
- Word and character timing from ElevenLabs is captured internally but not exposed to Blueprints (ADR-0010).
- Whisper CPU inference is statically linked into the plugin module; linking a prebuilt CUDA whisper requires an explicit opt-in (ADR-0011).
- One configurable OpenAI-compatible component replaces the provider-specific components (ADR-0012).
- The live line targets Unreal 5.8; the outgoing engine version is branched and kept before each switch (ADR-0013).

## Modules

### M-core: Shared types and module entry
- Responsibility: Own the plugin module's lifecycle and the Blueprint-visible data types the components exchange.
- Paths: FANTASIA/Source/FANTASIA/Public/FANTASIA.h, FANTASIA/Source/FANTASIA/Private/FANTASIA.cpp, FANTASIA/Source/FANTASIA/Public/FANTASIATypes.h, FANTASIA/Source/FANTASIA/Public/paramObject.h, FANTASIA/Source/FANTASIA/Public/EnableGrpcIncludes.h, FANTASIA/Source/FANTASIA/Public/DisableGrpcIncludes.h
- Language: cpp
- Satisfies: NFR-004
- Depends on: none
- Provides: C-fantasia-types
- Consumes: none
- Hides: the Blueprint-facing shape of shared data, and the module's startup and shutdown
- Size: S
- Provenance: recovered
- Confidence: high

### M-asr: Speech recognition
- Responsibility: Turn audio — a file, a buffer or a live microphone — into timed text on the machine, on CPU or GPU.
- Paths: FANTASIA/Source/FANTASIA/Public/WhisperTypes.h, FANTASIA/Source/FANTASIA/Public/WhisperSubsystem.h, FANTASIA/Source/FANTASIA/Public/WhisperCaptureComponent.h, FANTASIA/Source/FANTASIA/Public/WhisperBlueprintLibrary.h, FANTASIA/Source/FANTASIA/Private/WhisperBackend.h, FANTASIA/Source/FANTASIA/Private/WhisperBackend.cpp, FANTASIA/Source/FANTASIA/Private/WhisperSubsystem.cpp, FANTASIA/Source/FANTASIA/Private/WhisperCaptureComponent.cpp, FANTASIA/Source/FANTASIA/Private/WhisperBlueprintLibrary.cpp
- Language: cpp
- Satisfies: US-001, US-002, US-003, US-004, NFR-001, NFR-004
- Depends on: M-dist
- Provides: C-asr
- Consumes: C-build
- Hides: the whisper.cpp ABI, and whether inference runs on the statically linked CPU build or a dynamically loaded CUDA library
- Size: M
- Provenance: recovered
- Confidence: high

### M-tts: Speech synthesis
- Responsibility: Turn text into audio through an HTTP service and play it back in request order as it streams, with ElevenLabs as the shipped implementation.
- Paths: FANTASIA/Source/FANTASIA/Public/RESTTTSComponent.h, FANTASIA/Source/FANTASIA/Private/RESTTTSComponent.cpp, FANTASIA/Source/FANTASIA/Public/ElevenLabsTTSComponent.h, FANTASIA/Source/FANTASIA/Private/ElevenLabsTTSComponent.cpp, FANTASIA/Source/FANTASIA/Public/ElevenLabsProtocol.h, FANTASIA/Source/FANTASIA/Private/ElevenLabsProtocol.cpp
- Language: cpp
- Satisfies: US-005, US-006, US-007, NFR-002, NFR-004
- Depends on: M-core
- Provides: C-rest-tts, C-elevenlabs-protocol
- Consumes: C-fantasia-types
- Hides: HTTP streaming, chunk decoding, resampling to 16 kHz, and the per-call buffer and submission-order bookkeeping a consumer drains
- Size: M
- Provenance: recovered
- Confidence: high

### M-llm: Language back ends
- Responsibility: Reach a language model or an agent graph over HTTP and deliver its answer, streaming or whole.
- Paths: FANTASIA/Source/FANTASIA/Public/OpenAICompatibleComponent.h, FANTASIA/Source/FANTASIA/Private/OpenAICompatibleComponent.cpp, FANTASIA/Source/FANTASIA/Public/LangGraphComponent.h, FANTASIA/Source/FANTASIA/Private/LangGraphComponent.cpp
- Language: cpp
- Satisfies: US-008, US-009, NFR-004
- Depends on: M-core
- Provides: C-llm
- Consumes: C-fantasia-types
- Hides: each provider's endpoint shape and streaming format, including Anthropic content-block arrays
- Size: M
- Provenance: recovered
- Confidence: high

### M-prolog: Symbolic reasoning
- Responsibility: Run an embedded SWI-Prolog engine off the game thread, and let Blueprints build terms, assert, retract and query.
- Paths: FANTASIA/Source/FANTASIA/Public/SWIPrologComponent.h, FANTASIA/Source/FANTASIA/Private/SWIPrologComponent.cpp, FANTASIA/Source/FANTASIA/Public/SWIPrologThread.h, FANTASIA/Source/FANTASIA/Private/SWIPrologThread.cpp, FANTASIA/Source/FANTASIA/Public/SWIPrologHelpers.h, FANTASIA/Source/FANTASIA/Private/SWIPrologHelpers.cpp
- Language: cpp
- Satisfies: US-010, US-011, NFR-004
- Depends on: M-core
- Provides: C-prolog
- Consumes: C-fantasia-types
- Hides: that the Prolog engine is single-threaded and owns a dedicated thread, and the SWI-Prolog C++ API
- Size: M
- Provenance: recovered
- Confidence: high

### M-probabilistic: Bayesian networks and influence diagrams
- Responsibility: Build discrete probabilistic models and influence diagrams from Blueprints and run exact inference over them.
- Paths: FANTASIA/Source/FANTASIA/Public/BayesianNetwork.h, FANTASIA/Source/FANTASIA/Private/BayesianNetwork.cpp, FANTASIA/Source/FANTASIA/Public/InfluenceDiag.h, FANTASIA/Source/FANTASIA/Private/InfluenceDiag.cpp
- Language: cpp
- Satisfies: US-012, US-013, US-022, US-023, NFR-004
- Depends on: M-core, M-ml, M-dist
- Provides: C-probabilistic
- Consumes: C-fantasia-types, C-math, C-build
- Hides: the aGrUM 2.x API and the choice of inference algorithm
- Size: M
- Provenance: recovered
- Confidence: high

### M-ml: Math and machine learning
- Responsibility: Classification with support vector machines, and non-parametric density estimation with the helpers the probabilistic models need.
- Paths: FANTASIA/Source/FANTASIA/Public/MathUtilities.h, FANTASIA/Source/FANTASIA/Private/MathUtilities.cpp, FANTASIA/Source/FANTASIA/Public/LibSVM.h, FANTASIA/Source/FANTASIA/Private/LibSVM.cpp
- Language: cpp
- Satisfies: US-015, US-016, NFR-004
- Depends on: M-core
- Provides: C-math
- Consumes: C-fantasia-types
- Hides: the libsvm C API and the kdepp header-only estimator
- Size: S
- Provenance: recovered
- Confidence: high

### M-neo4j: Graph data access
- Responsibility: Speak Bolt to a Neo4j server over the configured transport and return Cypher results as typed rows.
- Paths: FANTASIA/Source/FANTASIA/Public/Neo4jComponent.h, FANTASIA/Source/FANTASIA/Private/Neo4jComponent.cpp, FANTASIA/Source/FANTASIA/Public/BoltProtocol.h, FANTASIA/Source/FANTASIA/Private/BoltProtocol.cpp, FANTASIA/Source/FANTASIA/Public/BoltPackStream.h, FANTASIA/Source/FANTASIA/Private/BoltPackStream.cpp
- Language: cpp
- Satisfies: US-014, NFR-004
- Depends on: M-core
- Provides: C-neo4j
- Consumes: C-fantasia-types
- Hides: PackStream encoding, Bolt framing and versioning, and whether the bytes travel over plain TCP or a WebSocket, with or without TLS
- Size: M
- Provenance: recovered
- Confidence: high

### M-ace: Audio2Face lip-sync
- Responsibility: Feed synthesized speech to NVIDIA Audio2Face so a MetaHuman face animates from it, and close the session at end of turn.
- Paths: FANTASIAACE/Source/FANTASIAACE/Public/, FANTASIAACE/Source/FANTASIAACE/Private/
- Language: cpp
- Satisfies: US-017, US-021, NFR-003
- Depends on: M-tts, M-core
- Provides: C-ace-tts
- Consumes: C-rest-tts, C-elevenlabs-protocol, C-fantasia-types
- Hides: the NV_ACE_Reference API, A2F provider selection and session lifetime
- Size: S
- Provenance: recovered
- Confidence: high

### M-demo: In-editor exercise actor
- Responsibility: Exercise every component from one actor, by hand, in the editor.
- Paths: FANTASIA/Source/FANTASIA/Public/FANTASIATestActor.h, FANTASIA/Source/FANTASIA/Private/FANTASIATestActor.cpp
- Language: cpp
- Satisfies: none
- Depends on: M-core, M-asr, M-tts, M-llm, M-prolog, M-probabilistic, M-ml, M-neo4j, M-dist
- Provides: none
- Consumes: C-fantasia-types, C-asr, C-rest-tts, C-llm, C-prolog, C-probabilistic, C-math, C-neo4j, C-build
- Hides: nothing
- Size: S
- Provenance: recovered
- Confidence: high

### M-tests: Automation test suite
- Responsibility: Pin current behaviour through the engine's automation framework, as the project's only mechanical verification, and observe component independence.
- Paths: FANTASIA/Source/FANTASIATests/Public/, FANTASIA/Source/FANTASIATests/Private/
- Language: cpp
- Satisfies: none
- Depends on: M-core, M-asr, M-tts, M-llm, M-prolog, M-probabilistic, M-ml, M-neo4j, M-dist
- Provides: none
- Consumes: C-fantasia-types, C-asr, C-rest-tts, C-elevenlabs-protocol, C-llm, C-prolog, C-probabilistic, C-math, C-neo4j, C-build
- Hides: nothing
- Size: M
- Provenance: recovered
- Confidence: high

### M-dist: Build, bootstrap and distribution
- Responsibility: Decide how the plugins are compiled, linked, staged and installed, and how a user gets from a clone to a running plugin.
- Paths: FANTASIA/Source/FANTASIA/FANTASIA.Build.cs, FANTASIA/Source/FANTASIATests/FANTASIATests.Build.cs, FANTASIAACE/Source/FANTASIAACE/FANTASIAACE.Build.cs, FANTASIA/FANTASIA.uplugin, FANTASIAACE/FANTASIAACE.uplugin, FANTASIA/bootstrap.bat, FANTASIA/bootstrap.sh, FANTASIA/build_whisper_cuda.bat, FANTASIA/build_whisper_cuda.sh, FANTASIA/setup_whisper.bat, FANTASIA/setup_whisper.ps1, FANTASIA/setup_whisper.sh, FANTASIA/enable-plugins.ps1, FANTASIA/Binaries/, FANTASIAACE/Binaries/, README.md, THIRD_PARTY_LICENSES.md, LICENSE, .gitignore, .gitattributes
- Language: csharp
- Satisfies: US-018, US-019, US-020, US-021, US-022, NFR-005
- Depends on: none
- Provides: C-build, C-install
- Consumes: none
- Hides: where release artifacts live, how third-party libraries are found and linked, which DLLs are staged as runtime dependencies, and how NVIDIA plugins are placed
- Size: M
- Provenance: recovered
- Confidence: high

## Dependency view

Three layers and one aside.

- `M-dist` is below everything. It is the only module with no code dependency on
  another, and the only one every other module depends on at build time rather than
  through an include: it defines `WITH_AGRUM_BINDING` and `FANTASIA_WITH_CUDA`, decides
  what is linked, and stages the DLLs the components load. That coupling is invisible to
  the include graph, so it is declared and contracted as `C-build`.
- `M-core` is the foundation of the runtime code, with a fan-out of nothing. Eleven
  headers include `FANTASIATypes.h` and three more reach `FANTASIA.h`, so fourteen of
  the twenty-seven first-party headers depend on this one module. `M-ml` sits just above it and is the only component module another component
  module depends on (`M-probabilistic` needs the kernel density estimator).
- The seven component modules — `M-asr`, `M-tts`, `M-llm`, `M-prolog`,
  `M-probabilistic`, `M-ml`, `M-neo4j` — are siblings. None includes another's header.
  `M-asr` does not even reach `M-core`: it carries its own `WhisperTypes.h`. This
  sibling independence is what NFR-004 measures and what G-2 promises.
- `M-ace` sits above `M-tts`, in its own plugin, extending `URESTTTSComponent` and
  reusing the ElevenLabs protocol helpers. It also names `FTTSData` in a public override
  signature, so it depends on `M-core` directly and not only through its base class,
  even though neither of its headers includes `FANTASIATypes.h`. Nothing depends on it.
- `M-demo` and `M-tests` are the aside: both reach every component on purpose. For
  `M-tests` that is its job. For `M-demo` it is a defect, because that actor ships
  inside the runtime module (F-004).

No cycles exist at module level.

## Quality-attribute mapping

| Attribute | Scenario | Modules that carry it |
| :--- | :--- | :--- |
| Latency, speech to text | NFR-001 | M-asr (capture thread, VAD, backend dispatch) |
| Latency, text to first sound | NFR-002 | M-tts decodes and buffers as chunks arrive; the paced consumer that turns that into ordered playback lives in M-ace (F-009) |
| Audio/animation sync | NFR-003 | M-ace, with M-tts supplying the samples |
| Modifiability (component independence) | NFR-004 | the seven sibling modules and M-core, whose types are the one shared surface they all touch (F-001). M-tests observes it; a test suite cannot satisfy it |
| Installability (no compilation) | NFR-005 | M-dist and the committed binaries |

Real-time paths: the microphone capture loop in `M-asr` and the playback consumer in
`M-tts` both run against a clock and must never block the game thread; `M-prolog` moves
its engine off the game thread for the same reason. These three are where the
real-time discipline applies and where no measure exists yet.

## Walking skeleton

The skeleton already exists and ships, and lives entirely in this repository: a
`UWhisperCaptureComponent` hears an utterance, an `UOpenAICompatibleComponent` or
`ULangGraphComponent` answers it, a `UElevenLabsTTSComponent` speaks the answer, and —
with `FANTASIAACE` enabled — a `UACEElevenLabsTTSComponent` animates a MetaHuman face
from the same audio. Every piece of that path belongs to a module here, so the skeleton
is verifiable from this repository alone.

The FANTASIA-LangGraphDemo project illustrates the path wired into a running application,
and is maintained as its own project in its own repository, in the same arrangement as
the project wiki (ADR-0014). No module here owns it, no contract covers it, and work on
it is not planned from this backlog. It is an illustration, not evidence: nothing in this
architecture may rest on it.

## Accepted risks

- No continuous integration. Every gate is a local run on the maintainer's machine, so
  "the suite was green" is a claim with no artifact behind it.
- The prebuilt binaries are a second source of truth. Source and DLL can disagree, and
  only a rebuild plus a dumpbin check catches it (P-12).
- A large vendored third-party surface (SWI-Prolog, aGrUM, gRPC, OpenSSL, whisper.cpp,
  libsvm, kdepp) is carried in-repo or fetched, with upgrades done by hand.
- Win64 only, by choice (NG-2). The Linux and macOS scripts are not verified.
- Single maintainer with long gaps between sessions; anything that needs continuous
  attention will not get it.

## Unmapped areas

Deliberately outside every module. Vendored upstream code is not ours to own, and build
output is not source.

- `FANTASIA/ThirdParty/` — SWI-Prolog, aGrUM, gRPC with protobuf and abseil, OpenSSL,
  whisper.cpp with ggml, kdepp, nlohmann. Vendored upstream; upgraded wholesale, never
  edited. Note that the gRPC, protobuf and abseil trees are linked but used by no
  first-party code (F-002).
- `FANTASIA/Source/FANTASIA/Private/whisper_staged/` — upstream whisper.cpp and ggml
  sources staged for in-tree compilation of the static CPU backend. Generated from the
  vendored tree, not authored here.
- `FANTASIA/Source/FANTASIA/Private/svm.cpp`, `FANTASIA/Source/FANTASIA/Public/svm.h` —
  vendored libsvm. `M-ml` owns the wrapper around it, not the library.
- `FANTASIA/Intermediate/`, `FANTASIAACE/Intermediate/` — build output.
- `FANTASIA/Resources/`, `FANTASIAACE/Resources/`, `images/` — icons, the banner and the
  fetched Whisper model.
- `FANTASIA/Config/` — one file, `FilterPlugin.ini`, a packaging filter with no active
  entries. The plugin ships no default configuration: the `DefaultGame.ini` that
  ADR-0001 and four contracts refer to is the **host project's**, not one of ours.
- `FANTASIA/NVIDIA-UE57-Bundle/` — staging directory for the fetched NVIDIA plugins,
  present only after bootstrap runs. NVIDIA's code, redistributed unmodified. The
  directory name still says UE57 while the 5.8 line fetches `nv-*-ue58` assets into it
  (F-013).
