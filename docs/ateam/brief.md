# Brief: FANTASIA
- As of: J-0027
- Approved: pending
- Provenance: recovered
- Kind: software
- Platforms: Windows (Win64)
- Engine: Unreal 5.8
- Real-time: yes

## Problem

Research groups building Embodied Conversational Agents in Unreal Engine rewrite the
same plumbing for every character and every study: microphone capture and speech
recognition, a language model or agent back end, symbolic and probabilistic reasoning,
a knowledge store, speech synthesis, and facial animation driven by that speech. The
code is written once per project, by people whose expertise is linguistics or
interaction design rather than C++, and it does not transfer between characters.

FANTASIA is the re-engineered form of that plumbing, built at the University of Padua
and the University of Naples Federico II: a set of independent, Blueprint-friendly
Unreal components that cover the whole conversational pipeline, so a capability built
for one agent can be moved to another with minimal effort.

Source: README.md §About the Project; FANTASIA.uplugin §Description.

## Stakeholders

- Researchers and students building Embodied Conversational Agents in Unreal, mostly
  from Blueprints and without a C++ toolchain. The primary audience.
- The maintainer (Antonio Origlia, University of Naples Federico II), who holds the
  repository and promotes releases.
- Funded projects that depend on the plugin as research infrastructure: SPECIAL
  (health literacy about epilepsy) and VISIT3D (VR guide for the visual arts).
- Academic colleagues who cite FANTASIA in publications.
- Student contributors, who arrive with one component's prototype and leave.

Source: README.md §About the Project, §Projects using FANTASIA, §Acknowledgements,
§Contact.

## Goals

- G-1: Let researchers wire a full conversational pipeline — speech in, reasoning,
  speech out — entirely from Blueprints, without writing C++.
- G-2: Keep every capability an independent, opt-in component, so that what works for
  one agent transfers to another with minimal effort and enabling one component never
  drags in another.
- G-3: Ship prebuilt: installation is clone, bootstrap, enable, with no compilation
  step for the user.
- G-4: Cover the interactive pipeline out of the box — local ASR, streaming TTS,
  OpenAI-compatible and agent-graph language back ends, Prolog and Bayesian/influence
  reasoning, Neo4j graph data, SVM and KDE, and MetaHuman lip-sync.
- G-5: Stay usable and citable as academic research infrastructure for the groups and
  funded projects that depend on it.

Provenance: recovered from README.md and FANTASIA.uplugin.

## Non-goals

- NG-1: Not a dialogue manager or agent architecture. FANTASIA supplies components;
  the application wires them into a conversation. No built-in turn-taking policy,
  dialogue state machine or agent framework.
- NG-2: No cross-platform parity. Win64 is the supported platform. The Linux and macOS
  bootstrap scripts are best effort; aGrUM, gRPC, OpenSSL and the prebuilt DLLs are
  Win64-only.
- NG-3: No commercial product scope. Academic and non-commercial: no SLAs, no paid
  support, no storefront listing obligations, and redistribution confined to what
  upstream licences allow.
- NG-4: No model training, fine-tuning, hosting or serving. FANTASIA calls models —
  whisper.cpp locally, OpenAI-compatible, ElevenLabs and LangGraph endpoints remotely —
  and ships no weights of its own beyond the fetched Whisper model.

Provenance: stated. No document in the repository states a non-goal; these were
elicited from the user, and the journal records when.

## Constraints

- Engine: Unreal 5.8 (FANTASIA.uplugin, FANTASIAACE.uplugin, EngineVersion 5.8.0).
  The newest engine version lives on main; before each engine switch the preceding
  version is branched and kept, so earlier lines stay downloadable but are not
  development targets (branches 5.6, 5.7).
- Platform: Win64 for the full stack. aGrUM, gRPC, OpenSSL, the whisper static libs
  and every prebuilt DLL exist for Win64 only.
- Prebuilt binaries are part of the product: Binaries/ stays tracked in git, because
  Blueprint-only users never recompile and the shipped DLL is their build.
- Engine compatibility: a prebuilt module's .modules BuildId must equal the running
  engine's CompatibleChangelist, or the editor refuses the plugin.
- Language and build: C++20, exceptions enabled, RTTI off, unity builds off, explicit
  or shared PCHs, in all three modules.
- Heavy runtime artifacts are not in git: the Whisper model, the CUDA whisper stack,
  the Win64 build-time static libs and the NVIDIA ACE bundles are fetched by
  bootstrap from GitHub Release assets.
- Licensing: THIRD_PARTY_LICENSES.md must carry the upstream licence text of every
  bundled or fetched component. The NVIDIA ACE bundles are redistributed in good faith
  for this non-commercial project; DLSS and Streamline cannot be redistributed at all.
- Secrets: API keys and endpoint headers are Config UPROPERTYs read from
  DefaultGame.ini, never committed and never defaulted to a real key.
- No continuous integration exists. Verification is local: the FANTASIATests
  automation suite plus a dumpbin check on refreshed DLLs.
- Single maintainer, no funded development time: machinery must survive long gaps
  between sessions.

Source: the two .uplugin files, FANTASIA.Build.cs, .gitignore, THIRD_PARTY_LICENSES.md,
README.md §Installation and §Third-party licenses.

## Success criteria

- A Blueprint-only user can clone the repository, run bootstrap, enable the plugins and
  run the demo pipeline with no C++ compilation, on the engine version named in
  FANTASIA.uplugin.
- The FANTASIATests automation suite passes in full on the supported engine version
  before anything is promoted to main (109 tests at adoption).
- Every component is usable alone: enabling one requires no other beyond the shared
  types, and core FANTASIA builds and runs with NVIDIA ACE absent.
- The committed .modules BuildId equals the shipped engine's CompatibleChangelist, so
  no user is told the plugin was built with a different engine version.

Provenance: stated. No document in the repository states a success criterion; these
were elicited from the user, and the journal records when.

## Open questions

- .gitignore excludes `.claude/` and `CLAUDE.md`. Whether the team definition and the
  per-module state documents are tracked with the repository or stay local to the
  maintainer's machine is deferred to staffing.
- No NFR has a measure. The real-time paths (microphone-to-transcript latency, text-to-
  first-sound for TTS, audio-to-blendshape sync for Audio2Face) have no stated budget,
  so NFR-001 to NFR-003 carry `Measure: unknown` until one is set or measured.
- Whether the Linux and macOS bootstrap scripts are maintained or merely tolerated:
  NG-2 denies parity, but the scripts exist and are offered in the README.
- Whether `FANTASIA/Source/FANTASIA/Public/paramObject.h` should be deleted. It is
  reachable from nothing (F-006).
