# History

Append-only journal of how this project got to its current state. Written only by
`ateam_log.py append`; never edited or reordered. To learn what IS, read the state
documents (the rest of docs/ateam/ and module CLAUDE.md files); read this to learn WHY.

## J-0001 | 2026-11-12 | ADR-0001 | lead
- Status: accepted
- Occurred: 2026-04-26
- Provenance: recovered
- Title: Secrets reach components through configuration, not code
- Changes: none

### Context
Every service component talks to a paid endpoint, and the repository is public. Keys
and request headers were set on component instances, which means they are serialised
into Blueprint assets and committed with them.

### Decision
API keys, credentials and request headers are Config UPROPERTYs read from
DefaultGame.ini. No component defaults a key to a real value, and none logs one.

### Consequences
A key never enters a Blueprint asset or a commit. Users edit DefaultGame.ini, which the
Wiki documents, and changing a key needs neither a recompile nor an asset resave. This
became P-15.

### Alternatives
Environment variables: not reachable from Blueprints and awkward in packaged builds.
An editor settings object: still serialises into a config asset that is usually
committed.

## J-0002 | 2026-11-12 | ADR-0002 | lead
- Status: accepted
- Occurred: 2026-05-02
- Provenance: recovered
- Title: Synthesized speech is buffered per call and played by a paced, ordered consumer
- Changes: none

### Context
Streaming TTS chunks were pushed straight at the audio device as they arrived.
Concurrent synthesis calls interleaved, utterances overlapped, and the application had
no way to see where a turn ended.

### Decision
Each TTSSynthesize call gets its own buffer keyed by its id. A paced consumer plays
buffers in request order at playback rate. An explicit end-of-turn signal and a
PlaybackComplete delegate mark the boundaries.

### Consequences
Ordering is guaranteed and turns are observable, while playback still begins before
synthesis finishes (NFR-002). The application must close every turn; a missed close
truncates the current utterance and bleeds into the next. That later became a hard
requirement in the ACE path, where an open turn leaks the Audio2Face session.

### Alternatives
One shared buffer behind a mutex: serialises access but still gives no ordering
guarantee across calls. Waiting for complete synthesis before playing: destroys the
latency that streaming exists for.

## J-0003 | 2026-11-12 | ADR-0003 | lead
- Status: accepted
- Occurred: 2026-05-13
- Provenance: recovered
- Title: Neo4j speaks Bolt over plain TCP by default
- Changes: none

### Context
The component spoke Bolt inside a WebSocket, via libwebsockets. Neo4j 2026.x stopped
serving WebSocket-over-Bolt on port 7687, so the component could no longer reach a
local install at all. Aura kept working, which made the failure look like a
configuration problem.

### Decision
Add EBoltTransport with PlainTCP as the default, connecting through OpenSSL
BIO_new_connect, parallel to the existing WebSocket path. TLS stays orthogonal to the
transport choice.

### Consequences
Local installs work again, and the default transport is now the same framing native
Neo4j drivers use. Two transports to maintain and test, neither covered by a test. A
defensive localhost-to-127.0.0.1 substitution was added on the WebSocket path, and the
WebSocket connection errors were made more informative.

### Alternatives
Drop WebSocket entirely: it still works against Aura and some deployments proxy it.
Ask users to enable a server-side WebSocket connector: not in our gift, and it would
make a working default depend on server configuration.

## J-0004 | 2026-11-12 | ADR-0004 | lead
- Status: accepted
- Occurred: 2026-05-14
- Provenance: recovered
- Title: The AWS and Azure components are deleted, not deprecated
- Changes: none

### Context
AWS Polly TTS and the Azure ASR, TTS and NLU components carried large proprietary SDKs,
redistributable DLLs and their own enums, and nobody was maintaining them.

### Decision
Delete the components, their SDK headers, their redist DLLs and EAzureASREnum from the
live line.

### Consequences
The link surface and the clone shrink, and nothing has to build against vendor SDKs.
Projects using them must move to the ElevenLabs or generic REST TTS path. The gRPC
stack the Azure speech SDK needed was left linked and is now dead weight (F-002).

### Alternatives
Keep them deprecated for back-compatibility: they would still have to compile and ship,
which is the cost we wanted to shed. Keep Azure on an older line only: in effect what
happened, since the archived 5.6 branch still ships it.

## J-0005 | 2026-11-12 | ADR-0005 | lead
- Status: accepted
- Occurred: 2026-05-14
- Provenance: recovered
- Title: Audio2Face lip-sync lives in a separate companion plugin
- Changes: none

### Context
The Audio2Face components lived in the core module, so core FANTASIA referenced NVIDIA
ACE. Every user needed the ACE plugins installed whether or not they wanted lip-sync,
against the promise that components are independent and you use only what you need.

### Decision
Move the ACE TTS components into a separate FANTASIAACE plugin that depends on FANTASIA
and NV_ACE_Reference. Core FANTASIA becomes ACE-agnostic and never names ACE.

### Consequences
Core FANTASIA works on any system with no NVIDIA dependency, which is what G-2 and the
third success criterion ask for. Two plugins to version, build and ship together. The
ElevenLabs configuration surface ended up duplicated across the split, because the ACE
ElevenLabs component derives from the ACE base rather than from the ElevenLabs one
(F-003).

### Alternatives
A compile-time switch in the core module: Blueprint-only users cannot recompile, so the
shipped DLL would have to pick one answer for everyone. Runtime detection of ACE from
the .uproject: tried earlier and rejected as fragile.

## J-0006 | 2026-11-12 | ADR-0006 | lead
- Status: accepted
- Occurred: 2026-05-14
- Provenance: recovered
- Title: Whisper GPU support is runtime dispatch, not a compile-time switch
- Changes: none

### Context
CUDA whisper had to be compiled into the module, so a GPU user needed a full plugin
rebuild. That is exactly what the Blueprint-only audience cannot do.

### Decision
Route every whisper_* call through a dispatcher that is either the statically linked CPU
build or a dynamically loaded fantasia_whisper_cuda.dll. Backend is a property read at
LoadModel time; if the library or the CUDA runtime fails to load, log a warning and fall
back to CPU.

### Consequences
GPU becomes a property flip with no rebuild (G-3). The GPU stack ships as a 540 MB
release asset that bootstrap fetches. The silent fallback can mask a broken CUDA
install: the caller sees a normal, slower result.

### Alternatives
Ship two DLL variants: doubles the binaries to build, dumpbin-verify and commit.
Compile-time only: excludes the primary audience.

## J-0007 | 2026-11-12 | ADR-0007 | lead
- Status: accepted
- Occurred: 2026-05-20
- Provenance: recovered
- Title: Heavy runtime artifacts live in release assets, not in git
- Changes: none

### Context
The Whisper model, the GPU stack and the Win64 build-time static libraries were tracked.
They dominated clone size, and LFS bandwidth was charged on every clone.

### Decision
Publish them as GitHub Release assets, fetch them with bootstrap, and gitignore them.

### Consequences
Clones stay lean. Bootstrap becomes load-bearing: it is the only place that knows where
the assets live, so a move breaks installation for everyone. Release assets have to be
re-cut whenever the artifacts change, which is why a dependency-tarball repackage is
deferred to release time rather than done per commit. This became P-17.

### Alternatives
Git LFS: the bandwidth cost lands on every clone, including the ones that only want the
source. Commit them: the clone-size problem we started from.

## J-0008 | 2026-11-12 | ADR-0008 | lead
- Status: accepted
- Occurred: 2026-06-16
- Provenance: recovered
- Title: Bootstrap installs the whole turnkey stack by default
- Changes: none

### Context
Bootstrap required a flag per component, so a new user had to know which parts they
needed before they could get started. The common case -- a working interactive
MetaHuman -- needed the most flags.

### Decision
Install everything needed for a typical interactive MetaHuman by default and narrow with
--no-* flags. Place the NVIDIA plugins into the host project's Plugins folder
automatically, since bootstrap runs from inside it. --enable-plugins switches the stack
on in the .uproject after writing a .uproject.bak.

### Consequences
The default path needs no decisions, at the cost of a multi-gigabyte first run.
Idempotence became a property users rely on. Three Windows batch bugs had to be fixed
to make it work: SCRIPT_DIR must be captured before the argument-parsing loop because
shift also shifts %0, tar must be pinned to the bundled bsdtar so drive-letter extract
paths work, and set assignments must carry no trailing space. bootstrap.sh is immune to
the first, because bash's shift leaves $0 alone.

### Alternatives
Opt-in flags: the previous state, where new users could not tell what to ask for. An
interactive prompt: breaks unattended runs.

## J-0009 | 2026-11-12 | ADR-0009 | lead
- Status: accepted
- Occurred: 2026-06-21
- Provenance: recovered
- Title: aGrUM is vendored as per-module static libraries
- Changes: none

### Context
aGrUM 1.7.1 shipped one monolithic library. 2.x ships per-module libraries, renames
Potential to Tensor, and its check() collides with Unreal's check() macro.

### Decision
Upgrade to 2.3.2. Vendor only the modules FANTASIA uses -- agrumID, agrumBN, agrumBASE
-- and link them in that order, because MSVC's single-pass linker needs each library to
precede its dependency and BASE is the leaf. Build them /MD without OpenMP.

### Consequences
Only the needed modules are linked. The link order is load-bearing and breaks silently
if reordered, so it is written into C-build. The whole suite passed after the upgrade
(107 tests at the time; 109 today).
The libraries are not tracked, so a release-asset repackage is owed at release time.

### Alternatives
Stay on 1.7.1: upstream has moved and the API diverges further each release. Link the
full aGrUM module set: size, for modules nothing uses.

## J-0010 | 2026-11-12 | ADR-0010 | lead
- Status: accepted
- Occurred: 2026-06-21
- Provenance: recovered
- Title: Word and character timing is captured but not exposed to Blueprints
- Changes: none

### Context
ElevenLabs can return character-level timing, and a Blueprint-facing timing API with
segment-played events was built on it. NVIDIA ACE is expected to deliver native
animation events that would supersede it.

### Decision
Keep the capture and the FTTSSegmentTiming struct as a plain USTRUCT. Expose nothing:
no delegates, no UFUNCTION getters. Drop the A2F segment-played events and wait for
ACE's own OnAnimationEvent.

### Consequences
The data is there when something needs it, with no public surface to support or migrate
away from. Nothing can use it from Blueprints today, and re-exposing it is a change
request against this decision rather than an addition.

### Alternatives
Expose it now: a public Blueprint surface that ACE's native events would likely
supersede, and a Blueprint surface cannot be withdrawn without breaking user assets.
Delete the capture: cheap to keep and awkward to re-derive.

## J-0011 | 2026-11-12 | ADR-0011 | lead
- Status: accepted
- Occurred: 2026-07-08
- Provenance: recovered
- Title: Whisper CPU is linked statically; linking a prebuilt CUDA whisper is opt-in
- Changes: none

### Context
A stray whisper.lib left under ThirdParty/whisper_cpp/build/ flipped the build to shared
whisper, so UnrealEditor-FANTASIA.dll imported whisper.dll. Since that DLL is not
shipped, every clone broke, the ACE line included. The source looked correct throughout.

### Decision
Link whisper CPU statically into the module. Linking a prebuilt CUDA whisper requires an
explicit opt-in rather than being inferred from whatever is lying in the build tree.
Verify with dumpbin that the shipped DLL imports no whisper.dll.

### Consequences
The shipped DLL is self-contained for CPU inference, and only libswipl.dll and the
SWI-Prolog packs appear among its imports. A stray library can still change the link, so
the dumpbin check is part of the release procedure rather than a build-time guard; that
is the mechanical half of P-12.

### Alternatives
Ship whisper.dll alongside: another binary to version, stage and verify, for no benefit
over static linking. Detect the stray library and fail the build: not done, and still
worth doing.

## J-0012 | 2026-11-12 | ADR-0012 | lead
- Status: accepted
- Occurred: 2026-07-24
- Provenance: recovered
- Title: One configurable OpenAI-compatible component replaces the per-provider ones
- Changes: none

### Context
UOpenAIComponent, UGroqComponent and UOllamaComponent duplicated the same request shape
against the same API. Each new provider meant another near-identical component.

### Decision
One UOpenAICompatibleComponent configured by an FOpenAICompatibleConfig, with preset
factories for OpenAI, Groq, Ollama, LM Studio and vLLM. Delete the provider-specific
components.

### Consequences
Switching provider is a configuration change, not a code change, which is what G-1
promises for a Blueprint user. Projects using the old components must migrate; only a
doc comment in OpenAICompatibleComponent.h still names them. A new OpenAI-compatible
provider now needs only a preset.

### Alternatives
Keep the old components deprecated: three near-identical components to maintain and
ship. A provider interface with subclasses: the only difference between providers is
configuration, so subclassing buys nothing and costs a class per provider.

## J-0013 | 2026-11-12 | ADR-0013 | lead
- Status: accepted
- Occurred: 2026-07-24
- Provenance: recovered
- Title: Unreal 5.8 is the live line; the outgoing engine version is branched and kept
- Changes: none

### Context
An engine upgrade breaks prebuilt plugins through the BuildId check, and users are spread
across engine versions and cannot all move at once. 5.8 brought no API break, only
strictness: UHT now requires Category on BlueprintReadWrite properties (81 of them),
IWYU include hygiene is enforced, and SWI-cpp2 needed a warning suppression.

### Decision
main carries the newest supported engine version. Before each engine switch, the
outgoing version is branched under its number and kept downloadable. 5.8 is the live
line; 5.6 and 5.7 are archives, not development targets.

### Consequences
One line to develop and verify, while earlier engines stay available without being
maintained. A user on an older engine gets that branch's feature set, not the current
one, and the 5.6 branch still ships the Azure components this project otherwise deleted.
The Alice project line stays on 5.7 and is fed from the 5.7 branch. This became P-14.

### Alternatives
Maintain several lines in parallel: beyond a single unfunded maintainer. Support only
the newest and delete the rest: strands users mid-project, which is the opposite of
being usable research infrastructure (G-5).

## J-0014 | 2026-11-12 | F-001 | lead
- Event: opened
- Story: NFR-004
- Module: M-core
- Kind: drift
- Severity: major
- Repro: FANTASIA/Source/FANTASIA/Public/FANTASIATypes.h, 614 lines, included by 11 first-party headers; 17 commits, the second-highest churn in first-party code
- Changes: none

FANTASIATypes.h is a cross-component shared header. It holds the Prolog term class
hierarchy (USWIPrologObject and thirteen subclasses), the chat model (GPTRoleType,
FChatTurn and the two GPT response delegates), the probabilistic enums
(InfluenceNodeType, BayesianNodeType, InferenceAlgs, InferenceIDAlgs), the TTS model
(TTSVoiceType, FTTSTimedStruct, FTTSSegmentTiming, FTTSData and four delegates) and the
Neo4j result model (Neo4jOperation, FNeo4jResponse, UNeo4jResultRow, UNeo4jResultCell
and three cell subclasses).

Five components' vocabularies therefore live in one file owned by M-core, giving it a
fan-in of eleven headers. Changing any one component's types recompiles and
semantically couples every other, which is the opposite of what G-2 promises and what
NFR-004 measures. Every type in it is reflected, so each is also part of the Blueprint
ABI: splitting it is a major change under P-03 and needs a change request, not a tidy-up.

Not fixed here. Adoption records what is.

## J-0015 | 2026-11-12 | F-002 | lead
- Event: opened
- Story: US-019, NFR-005
- Module: M-dist
- Kind: drift
- Severity: major
- Repro: FANTASIA.Build.cs lines 42-71 link gRPC, protobuf and abseil and define WITH_GRPC_BINDING=1; no first-party source includes a grpc, protobuf or absl header, uses grpc::, google::protobuf or absl::, or reads the define
- Changes: none

The build links the full gRPC stack with protobuf and abseil, adds three include paths
for it, and defines WITH_GRPC_BINDING=1. Nothing in FANTASIA or FANTASIAACE uses any of
it. The two helper headers that existed to wrap gRPC includes, EnableGrpcIncludes.h and
DisableGrpcIncludes.h, are included by nothing either.

This is a leftover of the Azure speech SDK, which needed gRPC and was deleted
(ADR-0004). The cost is roughly 330 MB of the fetched build-time static libraries, three
include paths on every translation unit, and a large link surface, all against the lean
-clone goal that ADR-0007 and P-17 exist to serve.

Removing it changes C-build and the release asset that carries the libraries, so it is a
change request, not a cleanup.

## J-0016 | 2026-11-12 | F-003 | lead
- Event: opened
- Story: US-006, US-017
- Module: M-ace
- Kind: drift
- Severity: major
- Repro: FANTASIAACE/Source/FANTASIAACE/Public/ACEElevenLabsTTSComponent.h declares VoiceID, ModelID, Key, language_code, stability, similarity_boost, style, use_speaker_boost, speed and PronunciationMap, the same ten as FANTASIA/Source/FANTASIA/Public/ElevenLabsTTSComponent.h
- Changes: none

UACEElevenLabsTTSComponent derives from UACERESTTTSComponent, not from
UElevenLabsTTSComponent, so it cannot inherit the ElevenLabs configuration and declares
all ten properties again. The two sets are kept in step by hand.

They have already diverged in behaviour: the ACE component requires an explicit end of
turn to close the Audio2Face session, while the pre-split non-ACE component did not, so
a project migrating between them loses the tail of each utterance and bleeds it into the
next. Both ElevenLabs files are also among the churn hotspots (13 and 12 commits), which
is where hand-synchronised duplicates fail.

The fix is a structural one -- extract the ElevenLabs configuration and protocol wiring
so both components share it -- and it crosses M-tts and M-ace, so it needs a change
request against C-rest-tts and C-ace-tts.

## J-0017 | 2026-11-12 | F-004 | lead
- Event: opened
- Story: NFR-004
- Module: M-demo
- Kind: drift
- Severity: minor
- Repro: FANTASIA/Source/FANTASIA/Private/FANTASIATestActor.cpp includes 15 first-party headers spanning all eight component modules; the file is compiled into the shipped FANTASIA runtime module
- Changes: none

FANTASIATestActor is a manual exercise harness: one actor that drives every component
from the editor. It is compiled into the Runtime module and therefore ships, so the
shipped plugin contains a class that depends on all eight component modules at once.

Unreal gives no link boundary inside a module, so this costs nothing at runtime unless
the actor is placed. What it costs is the claim: the one file in the runtime module that
contradicts component independence is also the file a reader would look at to learn how
the components relate.

Severity is minor because churn on it is low and the behaviour is correct. The
candidate fixes -- move it to the UncookedOnly FANTASIATests module, or into a sample
content pack -- both change module paths and need a change request.

## J-0018 | 2026-11-12 | F-005 | lead
- Event: opened
- Story: US-012, US-013
- Module: M-probabilistic
- Kind: drift
- Severity: minor
- Repro: FANTASIA.Build.cs line 36 defines WITH_AGRUM_BINDING as 1 on Win64 and 0 elsewhere; FANTASIATestActor.cpp and the two aGrUM test files guard on it; BayesianNetwork.h, BayesianNetwork.cpp, InfluenceDiag.h and InfluenceDiag.cpp never mention it and include agrum/ headers unconditionally
- Changes: none

The build computes WITH_AGRUM_BINDING to say whether the aGrUM binding is available, and
the tests and the demo actor honour it. The module the define exists for does not: both
probabilistic headers include agrum/BN and agrum/ID headers with no guard.

So the define cannot actually disable the binding. A build where it is 0 -- any non-Win64
target -- fails to compile M-probabilistic rather than building without it. The guard
reads as protection that is not there.

In practice this never bites, because Win64 is the only supported platform (NG-2, P-09),
which is why the severity is minor rather than major. It is recorded because the next
person to read the guard will believe it.

## J-0019 | 2026-11-12 | F-006 | lead
- Event: opened
- Story: NFR-004
- Module: M-core
- Kind: quality
- Severity: minor
- Repro: paramObject.h, EnableGrpcIncludes.h and DisableGrpcIncludes.h are named by no #include in FANTASIA/Source or FANTASIAACE/Source
- Changes: none

Three headers in M-core are reachable from nothing.

paramObject.h declares a reflected UObject with its own generated.h, so it is compiled
and its UHT output generated on every build, for a class nobody constructs.
EnableGrpcIncludes.h and DisableGrpcIncludes.h exist to bracket gRPC includes and are
dead for the same reason gRPC is (F-002).

Deleting paramObject.h removes a BlueprintType, so it is a change to C-fantasia-types
however small the diff; the brief records it as an open question. The two gRPC guards
should go with F-002.

## J-0020 | 2026-11-12 | F-007 | lead
- Event: opened
- Story: US-002, US-014
- Module: M-asr, M-neo4j
- Kind: quality
- Severity: major
- Repro: 109 automation tests exist; WhisperCaptureComponent.cpp (1250 lines), Neo4jComponent.cpp (1244 lines), LangGraphComponent.cpp (607), WhisperSubsystem.cpp (785), SWIPrologThread.cpp (567) and all of FANTASIAACE have none
- Changes: none

The automation suite is good where it reaches, and it reaches only the pure helpers. The
PackStream codec has 36 tests and the Bolt connection, handshake, authentication and
transport selection have none. The subtitle formatters and config builders are covered
and the capture loop, voice activity detection, noise gate and backend dispatch are not.
The ElevenLabs protocol is well covered; the playback consumer is covered only through
the PCM16 decoder.

The two largest first-party files are both untested, and both are real-time paths with
threading: microphone capture feeding transcription, and a socket feeding a result
mapper. Those are where an untested regression costs most, and where a regression is
hardest to notice -- the behaviour degrades rather than failing.

This is the first sprint's work: characterisation tests for the twelve recovered
contracts, starting with these two modules. Six Tests: paths in the backlog and the
contracts already name the files that are owed -- WhisperCaptureComponentTests.cpp,
WhisperBackendTests.cpp, LangGraphComponentTests.cpp, SWIPrologComponentTests.cpp,
BootstrapContractTests.cpp, and ACERESTTTSComponentTests.cpp in a FANTASIAACETests
module that does not exist yet. Until then, C-asr, C-neo4j and C-ace-tts say
in their own text which of their claims are read from the code rather than pinned.

## J-0021 | 2026-11-12 | F-008 | lead
- Event: opened
- Story: US-008, US-005
- Module: M-llm, M-tts
- Kind: security
- Severity: major
- Repro: grep -n 'FOpenAICompatibleConfig Config;' -B2 OpenAICompatibleComponent.h shows UPROPERTY(EditAnywhere, BlueprintReadWrite, Category) with no Config specifier (line 85); the same for TMap<FString,FString> Headers in RESTTTSComponent.h line 312. Compare ElevenLabsTTSComponent.h:57 Key, Neo4jComponent.h:116 password, RESTTTSComponent.h:308 URL and :319 BodyTemplate, which all carry Config
- Changes: none

Two secret-bearing properties are not marked Config, so they are never loaded from
DefaultGame.ini and instead live in the owning Blueprint or actor asset's defaults --
which is committed with the user's project.

UOpenAICompatibleComponent::Config is a struct-typed property. Its inner fields
(URL, APIKey, DefaultModel, TimeoutSeconds, bRequiresAuth) do carry Config, but
UObject::LoadConfig walks the class's own Config properties and never descends into a
struct property that is not itself Config, so those specifiers do nothing. The API key
is the field that matters.

URESTTTSComponent::Headers is the map an ElevenLabs or generic REST user puts an
xi-api-key into, and it sits between URL and BodyTemplate, both of which are Config.

The omission is anomalous rather than a pattern: every other secret-bearing property in
the plugin carries Config on the owning property. This contradicts P-15 and the brief's
Secrets constraint, and it is the one finding in this adoption where the artifact and
the code were wrong in the same direction -- the contracts asserted the safe behaviour
because the neighbouring properties have it. Fixing it is two specifiers plus a note
that existing assets must be re-saved; verify in the editor first.

## J-0022 | 2026-11-12 | F-009 | lead
- Event: opened
- Story: US-005, US-017
- Module: M-tts
- Kind: drift
- Severity: major
- Repro: grep -rn 'PlaybackComplete' over both Source trees: the only Broadcast is FANTASIAACE/Source/FANTASIAACE/Private/ACERESTTTSComponent.cpp:182. URESTTTSComponent::EndTurn is an empty body at FANTASIA/Source/FANTASIA/Private/RESTTTSComponent.cpp:217-219. The pacing is ACERESTTTSComponent.cpp:41-67
- Changes: none

URESTTTSComponent synthesizes, decodes, resamples, buffers per call and records
submission order in PlaybackOrder. Nothing in M-tts drains any of it.

So the three properties the component's name and its delegates promise -- playback in
request order, paced consumption, and PlaybackComplete per id -- exist only when a
subclass supplies a consumer, and the only consumer in the repository is in M-ace.
EndTurn() in core is an empty body. A project using core FANTASIA without FANTASIAACE
gets synthesis, buffering and a USoundWaveProcedural from TTSGetSound, and no
PlaybackComplete event at all.

The component's own doc comment states a third, different story: that for the bare core
component PlaybackComplete fires immediately after synthesis completes. It does not fire.

This is a boundary in the wrong place rather than a bug: the behaviour works for the ACE
path that everyone uses. It matters because C-rest-tts is the contract across the one
plugin split in the project, it is the only subclassing agreement, and it carries
NFR-002 -- so every consumer of it was reading a description of a different component.
The contract and the architecture are corrected; moving the consumer, or documenting the
split as intended and renaming around it, is a change request.

## J-0023 | 2026-11-12 | F-010 | lead
- Event: opened
- Story: US-016
- Module: M-ml
- Kind: quality
- Severity: minor
- Repro: grep -rn UMathUtilities over both Source trees returns nothing; Linspace is declared at FANTASIA/Source/FANTASIA/Public/MathUtilities.h:23 as FANTASIA_API std::vector<float> Linspace(float, float, int) at global scope
- Changes: none

Linspace is a free function at global scope returning std::vector<float>, exported from
a public header. There is no UMathUtilities class; the only UCLASS in MathUtilities.h is
UKernelDensityEstimator.

Two consequences. A Blueprint cannot call Linspace, in a module whose whole premise is a
Blueprint-facing surface (G-1). And a std::vector crosses the module boundary, which
works only because both sides are built with the same toolchain and settings -- fragile
in a plugin distributed as a prebuilt binary.

It is minor because the function is small, correct and well tested (four pinned cases),
and its only caller is C++. Wrapping it on a UBlueprintFunctionLibrary returning
TArray<float> would make it usable and remove the std::vector from the interface.

## J-0024 | 2026-11-12 | F-011 | lead
- Event: opened
- Story: US-014
- Module: M-neo4j
- Kind: bug
- Severity: major
- Repro: FANTASIA/Source/FANTASIA/Private/Neo4jComponent.cpp:117-122, in BeginPlay: if (ShouldUseSSL(endpoint)) UseSSL = true; else if (endpoint.StartsWith('bolt://') || 'http://' || 'ws://') UseSSL = false. ShouldUseSSL at :94-99 matches neo4j+s, neo4j+ssc, bolt+s, bolt+ssc, https, wss
- Changes: none

UseSSL is presented as a configuration property, with a default of false, and BeginPlay
overwrites it from the endpoint's scheme.

A user who sets UseSSL = true against a bolt:// endpoint gets an unencrypted connection
and is told nothing: credentials then cross the network in the clear, on a component
whose password field is a committed-config secret. The reverse also holds -- bolt+s://
gets TLS whatever the property says -- and that direction is harmless.

The behaviour is defensible as a design (the scheme is the more specific statement, and
it is what native drivers do) but not as a silent one. Either the property should be
removed in favour of the scheme, or a mismatch should refuse to connect, or at minimum
log at warning level. Any of those is a change to C-neo4j.

Routing is taken from the scheme the same way (SchemeRequiresRouting at :101-106), and
that one has no property to contradict, so it is only an omission from the contract --
now recorded there.

## J-0025 | 2026-11-12 | F-012 | lead
- Event: opened
- Story: US-014
- Module: M-neo4j
- Kind: quality
- Severity: minor
- Repro: grep -c FANTASIA_API on FANTASIA/Source/FANTASIA/Public/BoltProtocol.h returns 0; BoltPackStream.h exports FBoltValue, FBoltPackStreamWriter and FBoltPackStreamReader at lines 47, 93 and 131
- Changes: none

BoltProtocol.h declares FBoltServerMessage, FBoltDechunker, ParseBoltServerMessage,
ChunkMessage and the whole Build* message family in a Public folder, and exports none of
them.

So the Bolt message layer cannot be linked from any other module, including
FANTASIATests. That is the direct reason the project has 36 tests for the PackStream
codec, which is exported, and none for handshake, HELLO version selection, LOGON, RUN,
PULL or the dechunker -- the untested half of F-007 is untestable as it stands, not
merely untested.

Adding FANTASIA_API to the types the tests need is a one-line-per-type change and does
not alter behaviour, but it widens C-neo4j's exported surface, so it is a change
request.

## J-0026 | 2026-11-12 | F-013 | lead
- Event: opened
- Story: US-018
- Module: M-dist
- Kind: quality
- Severity: minor
- Repro: FANTASIA/bootstrap.bat:51-54 fetches nv-ace-reference-ue58-full.tar.gz and nv-audio2face-{mark,james,claire}-ue58.tar.gz, into NVIDIA-UE57-Bundle per lines 15-21; README and architecture.md repeat the UE57 name
- Changes: none

The 5.8 line fetches ue58 assets and stages them in a directory called
NVIDIA-UE57-Bundle/.

Nothing breaks -- the name is just a name, and the README tells users to move the
folder's contents into their Plugins/ directory. But the one place a user looks to
confirm they have the right NVIDIA build for their engine says the wrong engine version,
and the path appears in the README, in bootstrap's own banner and in the plugin's
gitignore, so it is the kind of mismatch that gets copied forward rather than noticed.

Renaming it touches bootstrap.bat, bootstrap.sh, .gitignore and the README together, and
the old name has to keep working for anyone who already ran the previous bootstrap, so
it is small but not free.

## J-0027 | 2026-11-12 | ADOPT | lead
- Mode: live
- Baseline: ateam-baseline-20261112 (db2dfbd6)
- Scope: whole repository: FANTASIA and FANTASIAACE, all first-party source, scripts and manifests; ThirdParty and vendored in-tree sources unmapped
- Changes: brief.md, principles.md, backlog.md, architecture.md, contracts/C-fantasia-types.md, contracts/C-asr.md, contracts/C-rest-tts.md, contracts/C-elevenlabs-protocol.md, contracts/C-llm.md, contracts/C-prolog.md, contracts/C-probabilistic.md, contracts/C-math.md, contracts/C-neo4j.md, contracts/C-ace-tts.md, contracts/C-build.md, contracts/C-install.md

FANTASIA is brought under the ATeam method at the repository root, covering both
plugins. The recovered picture matches the code: the reflexion check found no
undeclared dependency between modules.

### Goal recovery

The README and the two `.uplugin` descriptors carry the goal, the stakeholders, the
feature inventory and most of the constraints, and the brief is drafted from them with
each section citing its source. Nothing in the repository stated a non-goal or a
success criterion — the inventory confirmed it: of 21 candidate markdown files, four
mention a goal or purpose and only the root README is ours; none mentions a non-goal or
a success criterion.

Four non-goals and four success criteria were therefore elicited from the user on
2026-11-12 and recorded with `Provenance: stated`:

- NG-1 not a dialogue manager, NG-2 no cross-platform parity, NG-3 no commercial
  product scope, NG-4 no model training or hosting.
- Blueprint-only install works; FANTASIATests green before promotion; components stay
  independent; the prebuilt BuildId matches the engine.

The engine-support rule was elicited as well and is the one answer that did not match
either option offered: the newest engine version lives on main, and the outgoing
version is branched and kept before each switch. That is recorded as a constraint and
as P-14, and it is why the 5.6 and 5.7 branches are archives rather than development
targets. The adoption scope is the 5.8 line.

### In-flight work

Nothing is half-written. The working tree is clean apart from
`FANTASIA/ThirdParty/aGrUM/Libraries/`, which is gitignored and holds the static
libraries bootstrap fetches, so it is expected rather than work in progress.

- HEAD is `db2dfbd6` on `Development`, one commit ahead of `main` (`db9f4244`). Tagged
  `ateam-baseline-20261112` at the user's go-ahead; not pushed.
- Five worktrees share this repository: `main` at D:\Git\FANTASIA, `Development` here,
  and `Alice`, `Bastian`, `Jason` under D:\Perforce\MetaFamily. Alice is 28 commits
  behind; Bastian and Jason are long divergent and not live targets.
- Four stashes exist, three of them from the Alice worktree and one written by GitHub
  Desktop on Development. The stash stack is shared across worktrees; none of them
  belongs to this session and none was touched.
- Two `claude/*` branches sit at the same commit (`9133490b`), leftovers from cloud
  sessions, and several branches are marked gone on the remote.
- Known pending work, not yet a story: the NVIDIA ACE re-port to 5.8, and promoting the
  5.8 upgrade to main.

No other Claude Code session was working on the repository, so nothing had to be
flushed and no decision was lost to a running context.

### Existing configuration

| Item | Disposition |
| :--- | :--- |
| `FANTASIA/.claude/settings.local.json` — nine permission allow rules, all git plus one PowerShell path | keep |
| `FANTASIA/.claude/scheduled_tasks.lock` | keep (harness artifact) |
| CLAUDE.md / AGENTS.md | none exist anywhere in the repository |
| Agents, skills, commands, hooks, MCP servers, `permissions.deny` | none |
| CI configuration | none; the gate must be the local FANTASIATests run plus the dumpbin check |
| `.gitignore` excluding `.claude/`, `.claude`, `CLAUDE.md`, `claude.md` | **conflict** |

The `.gitignore` exclusion is the one real conflict with the method: ATeam writes a
`CLAUDE.md` state document per module and agent definitions under `.claude/agents/`,
and both are currently untracked by rule. Everything adoption itself wrote lives under
`docs/ateam/`, which is tracked, so nothing is lost today. The user chose to settle it
at staffing; it is recorded as an open question in the brief.

One detail worth noting rather than changing: the single PowerShell allow rule names
`D:\Git\FANTASIA\FANTASIA\setup_whisper.ps1`, a path in the `main` worktree, so it does
not match this worktree. Harmless, and not adoption's business to edit.

### CLAUDE.md separation

Nothing to separate. The repository has no CLAUDE.md, AGENTS.md or any other
agent-facing document, so no history had to be lifted out of state and no file of the
user's was rewritten. The README and THIRD_PARTY_LICENSES.md are written for humans and
were left alone; they belong to the project, not to the method.

This also means the project's conventions were nowhere written down. The seven project
principles (P-12 to P-18) are recovered from practice — commit discipline, the
prebuilt-binary rule, the branch rule, the secrets rule, the licence rule, the release
-asset rule and the no-historical-narration rule — not copied from a document.

### Activity

- 359 commits in the repository; 121 touch first-party source, 73 of them during 2026.
- Last commit 2026-07-24. The active front over the last 30 days is empty, so adoption
  had no moving target; the cost of that is that none of the recovered behaviour could
  be observed changing under hand.
- First-party churn hotspots: `FANTASIA.Build.cs` (29 commits) and `FANTASIATypes.h`
  (17) lead, then `RESTTTSComponent.h` and `.cpp` (15 each), `Neo4jComponent.cpp` (14),
  `ElevenLabsTTSComponent.h`/`.cpp` (13 and 12), `LangGraphComponent.cpp` (12). The two
  leaders are exactly the two files findings were opened against (F-001, F-002 and
  F-005 all touch them).
- Co-change: `FANTASIA/Binaries/Win64` with `FANTASIA/Source/FANTASIA` 22 times — the
  prebuilt-DLL discipline, now written down as P-12 — and `Source/FANTASIA` with
  `Source/FANTASIATests` 6 times.
- Repository-level churn is dominated by vendored `ThirdParty/` trees and by historical
  per-engine-version directories (`5.2/`, `5.3/`, `5.4/`) that no longer exist. Neither
  is a module boundary; both were ignored.

### Recovery summary

| Artifact | Recovered |
| :--- | :--- |
| brief.md | 5 goals, 4 non-goals, 4 success criteria, 11 constraints, 4 open questions |
| principles.md | 18 principles: 7 ATeam seeds, 4 from the non-goals, 7 from project practice. 12 are judgement-only (P-06 and P-08 to P-18): without CI, the project half has no mechanical check behind it |
| backlog.md | 19 stories, all `done`, plus 5 NFRs. 3 NFRs carry `Measure: unknown` |
| architecture.md | 12 modules, design v1 draft, 7 unmapped areas |
| contracts/ | 12 contracts, all `Provenance: recovered`, version 1 |

Coverage and verification:

- 66 of 68 first-party source files are owned by a module. The two that are not are
  `svm.cpp` and `svm.h`, vendored libsvm, declared unmapped alongside the 25
  `whisper_staged/` files and all of `ThirdParty/`.
- Reflexion found **0 divergent edges**: there is no dependency in the code that the
  architecture does not declare.
- 24 of the 29 declared module edges are backed by real `#include` directives, each
  checked file by file. The other five are non-include couplings, every one declared on
  purpose: `M-asr`, `M-probabilistic`, `M-demo` and `M-tests` all reach `M-dist` through
  the preprocessor defines `FANTASIA_WITH_CUDA` and `WITH_AGRUM_BINDING`, which is why
  `C-build` exists as a contract, and `M-ace` reaches `M-core` by naming `FTTSData` in a
  public override signature rather than by including its header.
- A tool limitation is worth recording. `ateam_recover.py reflexion` resolves C++
  includes by relative path, but Unreal includes are bare basenames resolved by UBT
  against each module's `Public/` and `Private/` roots. The script therefore reported 18
  edges as absent and coverage as 35%. A basename-resolving cross-check confirmed 24 of
  those edges against the code and found no undeclared ones, and the whole reported
  coverage gap is the vendored whisper and ggml files. The script also returned no
  `symbols`, so every contract specification was written by reading the headers.

### Findings opened

Thirteen in all: 7 major (F-001, F-002, F-003, F-007, F-008, F-009, F-011) and 6 minor;
by kind, 6 drift, 5 quality, 1 security, 1 bug. Seven came from reading the code during
recovery rather than from running it:

- F-001 (drift, major) `FANTASIATypes.h` is a cross-component god-header: Prolog term
  classes, chat types, probabilistic enums, TTS types and Neo4j result classes all live
  in `M-core`, giving it a fan-in of eleven. Any component's type change recompiles and
  semantically couples every other. Directly against G-2 and NFR-004. Second-highest
  churn in the project.
- F-002 (drift, major) gRPC, protobuf and abseil are linked, include-pathed and given
  `WITH_GRPC_BINDING=1`, and no first-party source uses any of them — a leftover of the
  deleted Azure and AWS components. Roughly 330 MB of fetched static libraries and a
  large link surface for nothing.
- F-003 (drift, major) The ElevenLabs configuration surface is duplicated field for
  field between `UElevenLabsTTSComponent` and `UACEElevenLabsTTSComponent`, which
  inherits from `UACERESTTTSComponent` rather than from the former. Ten properties kept
  in step by hand, and they have already diverged in end-of-turn behaviour.
- F-004 (drift, minor) `FANTASIATestActor` ships inside the runtime module and includes
  every component's header, so the shipped plugin couples to all eight modules for the
  sake of a manual exercise harness.
- F-005 (drift, minor) `WITH_AGRUM_BINDING` is honoured by the tests and by
  `FANTASIATestActor` but not by `M-probabilistic` itself, which includes aGrUM headers
  unconditionally. The define cannot actually disable the binding.
- F-006 (quality, minor) Dead code: `paramObject.h`, `EnableGrpcIncludes.h` and
  `DisableGrpcIncludes.h` are reachable from nothing.
- F-007 (quality, major) The two largest and most timing-sensitive files —
  `WhisperCaptureComponent.cpp` (1250 lines) and `Neo4jComponent.cpp` (1244 lines) —
  have no tests. The 109 automation tests cover the pure helpers around them: the
  PackStream codec but not the connection, the subtitle formatters but not the capture
  loop.

Six `Tests:` paths in the backlog and the contracts name files that do not exist yet,
and they are the first sprint's shopping list: `WhisperCaptureComponentTests.cpp`,
`WhisperBackendTests.cpp`, `LangGraphComponentTests.cpp`,
`SWIPrologComponentTests.cpp`, `BootstrapContractTests.cpp` and, in a test module that
does not exist yet either, `FANTASIAACETests/Private/ACERESTTTSComponentTests.cpp`. The
other eleven paths point at real files.

The review of the recovered artifacts found six more, in the code rather than in the
artifacts, and each was verified against the source before being accepted:

- F-008 (security, major) Two secret-bearing properties are not marked `Config`, so they
  are never read from DefaultGame.ini and instead live in the owning asset's defaults,
  which is committed: `UOpenAICompatibleComponent::Config` (the `Config` specifiers on
  its struct's fields do nothing, because `LoadConfig` does not descend into a non-Config
  struct property) and `URESTTTSComponent::Headers`, which is where an `xi-api-key` goes.
  Every other secret-bearing property in the plugin carries `Config` on the owning
  property. This contradicts P-15 and the brief's Secrets constraint.
- F-009 (drift, major) `URESTTTSComponent` buffers per call and records submission order
  but has no consumer. Ordering, pacing, `PlaybackComplete` and a non-empty `EndTurn()`
  all come from a subclass, and the only one is in M-ace. A core-only project gets no
  `PlaybackComplete` at all, and the delegate's own doc comment describes a third
  behaviour that does not happen.
- F-010 (quality, minor) `Linspace` is a free function returning `std::vector<float>`
  from a public header; there is no `UMathUtilities`. Not Blueprint-reachable, and a
  standard-library type crosses the module boundary of a prebuilt plugin.
- F-011 (bug, major) `UNeo4jComponent::BeginPlay` overwrites `UseSSL` from the endpoint
  scheme, so `UseSSL = true` with a `bolt://` endpoint connects in plaintext silently,
  carrying the password with it.
- F-012 (quality, minor) `BoltProtocol.h` exports nothing, so the Bolt message layer
  cannot be linked from `FANTASIATests`. The untested half of F-007 is untestable as it
  stands.
- F-013 (quality, minor) The 5.8 line fetches `nv-*-ue58` assets into a directory still
  named `NVIDIA-UE57-Bundle/`, a name repeated in the README and the gitignore.

### Review

A reviewer with the design checklist read all sixteen artifacts and spot-checked three
modules and two contracts against the code, as the adoption procedure requires. It
returned 28 findings. I verified every load-bearing one against the source myself before
acting; all of those held.

Three module path lists were checked mechanically and are sound: all 71 declared paths
exist, no two modules overlap, and every git-tracked first-party file falls into exactly
one module or one declared unmapped area, with no remainder. C-asr's type section matches
`WhisperTypes.h` field for field, and signature fidelity across C-llm, C-neo4j,
C-elevenlabs-protocol, C-math and C-prolog was character-exact. F-002, F-003 and F-005
were confirmed as recorded.

What the review changed, in 65 edits across all sixteen documents:

- Six new findings, above, all of them defects in the code that recovery had mis-read.
- One dependency restored. I had dropped `M-ace → M-core` because neither ACE header
  includes `FANTASIATypes.h`. It names `FTTSData` in a public override signature, and
  `FANTASIAACE.Build.cs` declares `FANTASIA` as a public dependency, so the dependency is
  real and the include graph was the wrong evidence to settle it on. C-fantasia-types had
  already contradicted me in its own body.
- Seven principles had their `Enforced by` corrected. P-06, P-12, P-13, P-15, P-16, P-17
  and P-18 claimed `checker` or `gate`; no such check exists, because the project has no
  CI and the ATeam checker validates artifact structure only. They are judgement-enforced
  and now say so, with a note that four of them are mechanizable and none is mechanized.
- Corrections to claims the code contradicts: `C-rest-tts` said the component does not
  resample (it resamples to 16 kHz and hard-codes that output rate) and understated its
  own extension surface as two hooks when it is five virtuals plus protected buffer
  state; `C-asr` overstated the GPU fallback, which checks only that the CUDA library
  loads and not that the runtime resolves, as the subsystem header says of itself;
  `C-elevenlabs-protocol` said pronunciation substitution replaces every occurrence of
  every key when it is whole-word, case-insensitive and capitalisation-preserving;
  `C-fantasia-types` claimed `FANTASIA_API` as its own and universal when UBT generates
  it and it is applied unevenly; `C-probabilistic` listed one completion delegate when
  there are two and M-demo binds the C++-only one; `C-build` said the C standard is set
  in all three modules when only `FANTASIA` sets it; `C-install` omitted `--no-model` and
  never named the release tag the whole install depends on (`runtime-v3.0`).
- Trace fixes: NFR-004 was `Satisfies` on M-tests, which can only observe modifiability,
  not satisfy it; NFR-002's artifact was M-tts alone when the paced consumer is in M-ace.
- A finding-ID collision: F-004 named both the demo-actor coupling and the dead code.
  The dead code is F-006.
- Dates and change narration removed from state documents, including from the principle
  that forbids exactly that (P-18).

The question the review asked that the recovery should have asked itself is worth
keeping: **which of these claims did we read out of the code, and which out of a
comment?** Every wrong invariant in the set came from a doc comment, a neighbouring
property, a function's name or a filename rather than from the code --
`PlaybackComplete fires once per id` from the delegate's comment instead of grepping for
`Broadcast`, `Headers is a Config UPROPERTY` from the two properties either side of it,
`UMathUtilities` from the filename. The contracts were careful to name the test that pins
each claim where one exists; the claims no test pins got no such discipline. A
`Provenance: recovered` artifact needs the same standard for both.

### Decisions mined

Thirteen ADR entries, `Occurred` between 2026-04-26 and 2026-07-24, recovered from
commit messages and the code they changed. They are the decisions that would be
expensive to reverse: the secrets-in-configuration rule, the TTS playback model, the
Bolt transport default, the deletion of the AWS and Azure components, the ACE split,
whisper's runtime backend dispatch, release assets instead of LFS, the turnkey
bootstrap, the aGrUM 2.x link order, the un-exposed TTS timing, the static CPU whisper
with opt-in CUDA, the single OpenAI-compatible component, and the 5.8 engine line.

Two items carried in the maintainer's notes as pending were checked against the code
and are in fact done: the provider-specific OpenAI, Groq and Ollama components are
gone (only a doc comment in `OpenAICompatibleComponent.h` still names them), and the
orphaned Audio2Face mirror structs are no longer in `FANTASIATypes.h`.

### Control status

Observe only. Adoption wrote state documents, the journal and the project graph, and
created one local tag. It installed no hook, no agent and no gate, and changed no
source file, no `.gitignore` and no CLAUDE.md. Nothing is enforced yet.

Two findings should not wait for a sprint. F-008 puts an API key in a committed asset
and F-011 silently downgrades a database connection to plaintext; both are small fixes in
the code, and both contradict a principle the brief states. They are the obvious first
change requests.

Enforcement follows the method's order: `/ateam:staff` next, with `observe_only: true`,
the gate built from the local FANTASIATests run and the dumpbin check since there is no
CI, and the `.gitignore` question settled there. The first sprint is conformance and
characterisation — tests for the untested paths named in F-007, and contract tests for
the twelve recovered contracts — and the second is the in-flight work: the ACE re-port
to 5.8 and the promotion to main. The project counts as under control when the checker
is clean, reflexion stays clean, and one sprint passes with no entry in
`.ateam/ownership.jsonl`; then a change request flips `observe_only` to false.

## J-0028 | 2026-11-12 | REQ-001 | lead
- Event: received
- Source: user
- Text: Ship the UE 5.8 line on main accepting minor fixes only, because the 5.9 preview is coming. Plan the big changes for 5.9 instead. This makes 5.8 a maintenance line and 5.9 the target for feature work.
- Changes: none

## J-0029 | 2026-11-12 | REQ-002 | lead
- Event: received
- Source: user
- Text: Update the tutorials on the project YouTube channel, produced with AI: screen-record the editor to show the examples, plan the voice-over text together with the user, and generate the narration with the user's cloned ElevenLabs voice through a connector.
- Changes: none

## J-0030 | 2026-11-12 | REQ-003 | lead
- Event: received
- Source: user
- Text: Update the demo project, FANTASIA-LangGraphDemo, a separate repository at codeberg.org/antori/FANTASIA-LangGraphDemo.
- Changes: none

## J-0031 | 2026-11-12 | REQ-004 | lead
- Event: received
- Source: user
- Text: Update the aGrUM version in use (the repository vendors 2.3.2) and expand support to more of aGrUM's model families, to explore new ways of conducting decisions for 'raison d'exprimer'-like AI.
- Changes: none

## J-0032 | 2026-11-12 | F-014 | lead
- Event: opened
- Story: US-018, US-019
- Module: M-dist
- Kind: bug
- Severity: major
- Repro: gh release view runtime-v3.0 --repo antori82/FANTASIA lists exactly three assets: fantasia-build-deps-win64.tar.gz, fantasia-gpu-cuda13-win64.tar.gz, whisper-large-v3-turbo-q5_0.bin. bootstrap.bat on Development sets RELEASE_TAG=runtime-v3.0 (line 46) and ACE_ASSET/MARK_ASSET/JAMES_ASSET/CLAIRE_ASSET to nv-ace-reference-ue58-full.tar.gz and nv-audio2face-{mark,james,claire}-ue58.tar.gz (lines 51-54). None of those four exists on that release; the NVIDIA bundles exist only on runtime-v2.0, as ue57 builds.
- Changes: none

The 5.8 line's default install cannot complete. ADR-0008 makes the unflagged bootstrap
run install everything, and on Development that run asks runtime-v3.0 for four NVIDIA
assets the release does not contain, so the whole Audio2Face half fails for every user
who does not pass --no-nvidia.

This contradicts US-018's acceptance criteria as written, C-install's invariant that the
default run needs no flags and no decisions, NFR-005 and G-3. US-018 and US-019 are
recorded Status: done, which was true of the 5.7 line and is not true of the 5.8 line.

Falling back to the ue57 bundles on runtime-v2.0 is not a fix: NVIDIA's 5.7-built
plugins fail the engine BuildId check under 5.8, which is the whole reason the assets
are versioned per engine. The fix is to build the NVIDIA ACE 5.8 bundles and publish
them to runtime-v3.0 -- the ACE 5.8 re-port that was already known to be outstanding.

This blocks shipping 5.8 on main (REQ-001): a release whose documented one-command
install fails by default is not a release. It should be settled before promotion, not
after.

## J-0033 | 2026-11-12 | F-015 | lead
- Event: opened
- Story: US-018
- Module: M-dist
- Kind: bug
- Severity: minor
- Repro: bootstrap.sh line 33 sets GPU_ASSET=fantasia-gpu-cuda13-linux.tar.gz. That asset is on neither runtime-v3.0 nor runtime-v2.0; both carry only fantasia-gpu-cuda13-win64.tar.gz.
- Changes: none

The Linux bootstrap's GPU fetch names an asset that has never been published, so
--no-gpu is effectively mandatory on Linux and the default run reports a failed
download.

Severity is minor because NG-2 and P-09 make Win64 the supported platform and the Linux
script is explicitly best effort. It is recorded because the script offers the option
without saying it cannot work, which is the same shape of problem as F-013: the
documented surface and the published reality disagree.

Either publish a Linux GPU stack or have bootstrap.sh say the GPU path is Win64-only and
skip it.

## J-0034 | 2026-11-12 | F-016 | lead
- Event: opened
- Story: US-017, US-019
- Module: M-ace, M-dist
- Kind: bug
- Severity: major
- Repro: git show Development:FANTASIAACE/Binaries/Win64/UnrealEditor.modules | grep BuildId gives 47537391 while the same file under FANTASIA/Binaries gives 55116800; git show --stat 94e906a8 shows it touched only FANTASIA/Binaries
- Changes: none

FANTASIAACE.uplugin on Development declares EngineVersion 5.8.0, and every committed
FANTASIAACE binary is still the 5.7 build.

Committed BuildIds on Development:
  FANTASIA/Binaries/Win64/UnrealEditor.modules                     55116800  (5.8, correct)
  FANTASIA/Binaries/Win64/UnrealEditor-Win64-DebugGame.modules      55116800  (5.8, correct)
  FANTASIAACE/Binaries/Win64/UnrealEditor.modules                   47537391  (5.7)
  FANTASIAACE/Binaries/Win64/UnrealEditor-Win64-DebugGame.modules   47537391  (5.7)

47537391 is UE 5.7's CompatibleChangelist. The installed UE 5.8 is a promoted
++UE5+Release-5.8 build with Changelist 55116800 and CompatibleChangelist 0, so its
effective compatible changelist is 55116800 -- which core matches and the companion does
not.

The cause is visible in the commit: 94e906a8 "Refresh prebuilt FANTASIA DLLs for UE 5.8"
touched six files, all under FANTASIA/Binaries/. FANTASIAACE/Binaries/ was not rebuilt.
The working-tree ACE .modules files are dated 17 June and the core ones 24 July.

Effect: a user on 5.8 who enables FANTASIAACE is told the plugin was built with a
different engine version, and lip-sync does not load. Core FANTASIA is fine, so the
failure looks like an ACE problem rather than a packaging one.

This is exactly the omission P-12 exists to prevent and exactly the failure the fourth
success criterion names, and it blocks shipping 5.8 on main (REQ-001) independently of
F-014. The fix is to rebuild the FANTASIAACE Win64 Development and DebugGame binaries
against 5.8, dumpbin-verify them, and commit them as their own refresh commit.

Note for whoever does it: FANTASIAACE links ACERuntime and ACECore, so building it needs
NVIDIA's NV_ACE_Reference present and itself built for 5.8. No 5.8 build of NV_ACE_Reference
exists on this machine -- the copies under Alice, FedericoII and FANTASIA-LangGraphDemo are
all 5.7/47537391, and UE_5.8 has no Engine/Plugins/Marketplace directory at all. So F-014
and this finding share a prerequisite.

## J-0035 | 2026-11-12 | REQ-001 | lead
- Event: received
- Source: user
- Text: Clarification on the 5.8 prerequisites: the DLLs already built for 5.8 are the FANTASIA core ones (commit 94e906a8). The NVIDIA ACE 5.8 re-port has NOT been done. So F-014 (ue58 NVIDIA bundles absent from runtime-v3.0) and F-016 (FANTASIAACE binaries still the 5.7 build) both stand, and the ACE 5.8 port is a genuine prerequisite story for this request rather than a publish step.
- Changes: none

## J-0036 | 2026-11-12 | REQ-001 | planner
- Event: evaluated
- Evaluation: accepted
- Serves: G-3 (ship prebuilt: clone, bootstrap, enable, no compilation), and G-5
- Principles: P-01 n/a; P-02 n/a; P-03 engaged -- the 'minor fixes only' bar and the 5.9 feature target are a new standing rule, so that half of the request is held for a CR of type principle; P-04 ok; P-05 ok; P-06 n/a (the NVIDIA bundles are an existing shipped dependency rebuilt for 5.8, not a new one); P-07 ok (US-020 is the prerequisite F-014 names, nothing beyond it); P-08 ok; P-09 ok; P-10 ok; P-11 ok (NVIDIA's A2F models are redistributed, not trained or served by us); P-12 ok -- the 5.8 prebuilt refresh already landed (94e906a8) and any further runtime source change re-triggers it; P-13 ok -- no story promotes anything; the cherry-pick or fast-forward stays the user's; P-14 ok and its precondition is met (the 5.7 branch exists at main's tip db9f4244), but the policy half narrows it, so the CR must say whether a preview engine counts as 'newest'; P-15 ok; P-16 carried into US-020's acceptance, since the bundles are republished artifacts; P-17 ok -- the bundles stay release assets, not git; P-18 ok.
- Reason: Two asks in one request. Shipping 5.8 is accepted with one prerequisite: F-014 means the 5.8 line's documented one-command install fails by default, so promoting it as it stands would contradict G-3, NFR-005 and the first success criterion. US-020 clears it. US-018 and US-019 were recorded done on the strength of the 5.7 line and are not true of the 5.8 line, so both drop to ready and US-018 now depends on US-020. The maintenance-line half is held, not accepted: 'minor fixes only' is not a bar the planner can apply at intake without an operational definition, and writing one is a new project principle, which P-03 makes a CR of type principle. 'Plan the big changes for 5.9' waits on the same CR and on 5.9 existing: P-14 already prescribes the switch, and the backlog has no engine-line dimension in which a 5.9-targeted story could sit.
- Outcome: US-020; plus a CR of type principle for the maintenance-line policy, not raised here
- Changes: backlog.md, architecture.md

Accepted in part. The shippable half becomes US-020 and two status corrections; the
maintenance-line policy is held for a change request.

What the CR of type `principle` must settle, because the planner cannot apply the bar
without it:

1. An operational definition of "minor fix" that decides at intake. A workable one:
   touches no contract, no module boundary, no ADR and no dependency; changes no
   Blueprint-facing signature, so it cannot invalidate a user's compiled assets;
   estimate S or M. Any definition is better than none -- an undefined bar is one the
   planner cannot reject against, so in practice it would admit everything.
2. Whether the open major findings are admitted on 5.8 as minor fixes. This is the
   decision with teeth. F-008 is two UPROPERTY specifiers, so it passes any size bar,
   but it changes where a secret is read from and existing assets must be re-saved.
   F-011's fix changes C-neo4j, so a contract-touching bar excludes it -- which parks a
   live defect that sends Neo4j credentials in the clear whenever a user sets UseSSL
   against a bolt:// endpoint. Deferring that one to 5.9 should be said out loud, not
   fall out of a definition.
3. How P-14 reads while a preview exists. P-14 says main carries the newest supported
   engine; the request treats 5.8 as the shipped line and 5.9 as the target. Name the
   trigger for branching 5.8 and moving main to 5.9 (5.9 release? 5.9 preview? the first
   5.9-only feature?), so the switch is not a judgement call each time.
4. Where 5.9-targeted work accumulates before 5.9 exists. The backlog has no engine-line
   dimension: a story is `draft`, `ready` or `done`, with nothing to say which line it
   targets. Either stories carry a target-line field, or 5.9 work waits unlogged, which
   loses it.

Two questions the lead should put to the user, neither of which changes the verdict:

- Are the NVIDIA ACE 5.8 bundles already built locally and merely unpublished? If so
  US-020 collapses from a re-port to an upload, and its estimate drops from L to S. The
  story is written to be agnostic about which.
- Nothing in F-014's text is in doubt, but its severity is: `major` as logged, when its
  effect is that no story it names can be `done`, which is this method's own definition
  of a blocker. Left as logged, since the status corrections below carry the same fact
  where the planner has authority, and since the one mechanical consequence of `blocker`
  -- the checker's "done but blocker open" error -- cannot fire now that neither story
  is done. If the lead disagrees, re-log it with `Corrects=J-0032`.

Status corrections in backlog.md, which the planner owns:

- US-018 `done` -> `ready`, `Depends on: none` -> `US-020`. Its acceptance says the
  default run fetches the NVIDIA ACE bundle and places the plugins next to FANTASIA.
  F-014 makes that false on the live line, so `done` was a statement about 5.7.
- US-019 `done` -> `ready`. A Blueprint-only user who follows the documented install on
  5.8 does not arrive at a working Audio2Face half, and FANTASIAACE's own 5.8 prebuilt
  state is unverified here. Narrower than US-018's case, and flagged as a judgement:
  core FANTASIA does load, so a reviewer could defend leaving this one `done`.

No story is written for the promotion itself. P-13 makes it the user's cherry-pick or
fast-forward, and the readiness criteria are already US-018, US-019 and the second
success criterion.

## J-0037 | 2026-11-12 | REQ-002 | planner
- Event: evaluated
- Evaluation: needs-info
- Serves: G-5 as a candidate (usable and citable research infrastructure); not established until the questions are answered
- Principles: P-01 n/a; P-02 engaged -- no module's paths cover a voice-over script, a shot list or published media, so if the artifact were tracked here no coder could own it; P-03 ok (no design changes yet); P-04 ok; P-05 n/a; P-06 n/a -- recording uses tools already on the machine (ffmpeg with gdigrab and h264_nvenc, OBS with obs-websocket) and adds nothing to the plugin; a connector would be a session tool, not a plugin dependency; P-07 ok; P-08 ok; P-09 ok; P-10 ok -- tutorials for a public academic channel, with no commercial justification; P-11 ok -- narration calls a TTS service, or FANTASIA's own component, and trains nothing; P-12 n/a; P-13 n/a; P-14 engaged only in that a tutorial must show the engine version the line ships; P-15 engaged -- if narration runs through UElevenLabsTTSComponent the key stays in DefaultGame.ini and must reach neither the repository nor the recording; P-16 n/a; P-17 engaged -- a rendered video is a heavy artifact and cannot be committed; P-18 ok.
- Reason: Capture is feasible on this machine (ffmpeg with gdigrab and h264_nvenc, OBS with obs-websocket, desktop control to drive the editor), so tooling is not what blocks this. What is missing is a testable target: 'the tutorials' names no set, 'updated' names no state, and the deliverable has no decided home -- the precedent for a documentation channel here is the wiki, which is managed outside this repository. No Given/When/Then can be written that decides pass or fail. Narration is unsettled too: the registry's ElevenLabs connector advertises voice-agent tools, so synthesis with the cloned voice is unverified, though FANTASIA's own UElevenLabsTTSComponent is a fallback that would dogfood US-006 and US-007. Five questions in the body. If the answer to the scope question is 'tracked in this repository', this returns as requires-change, because P-02 then needs a module that owns tutorial artifacts.
- Outcome: none -- no story until the questions are answered
- Changes: none

The questions, in the order that unblocks the most:

1. Which tutorials, and what makes one done? Name the list. "The tutorials" is not a set
   the acceptance can be written against. For each, say whether it is a re-record of an
   existing video against 5.8 or a new one. Note what changed under the tutorials since
   they were made: the provider-specific OpenAI, Groq and Ollama components are gone in
   favour of one `UOpenAICompatibleComponent` (ADR-0012), Neo4j now speaks plain TCP by
   default (ADR-0003), Audio2Face lives in a separate plugin (ADR-0005), and ACE TTS
   needs an explicit End Turn. Any tutorial showing the old components is wrong, not
   merely dated.
2. Where does the tracked artifact live? The voice-over script and the shot list are
   text that should outlive the video. Either they are tracked in this repository -- in
   which case this comes back as `requires-change`, because no module's paths cover
   tutorial artifacts and P-02 needs one owner -- or tutorials are managed outside it,
   as the wiki is, and this request leaves the ATeam backlog with no story. A rendered
   video cannot be committed either way (P-17).
3. Narration. Three options, and the choice is the user's: (a) connect an ElevenLabs
   connector that exposes synthesis with a chosen voice id -- the one in the registry
   advertises voice-*agent* tools, so this is unverified; (b) generate the narration
   through FANTASIA's own `UElevenLabsTTSComponent` in the editor, which needs only the
   key in DefaultGame.ini and dogfoods US-006 and US-007; (c) the user records it. Option
   (b) is the recommendation: it needs no new capability and it exercises the product.
4. Publishing. Who uploads, and is channel copy (titles, descriptions, links) part of the
   deliverable? Nothing will be published to a public channel without a go-ahead per
   upload.
5. What is recorded? A scene built for the recording, or the demo project -- which is
   REQ-003's separate repository, and would couple this request to that one.

Capture tooling is available and was confirmed on the machine: ffmpeg with the gdigrab
demuxer and the h264_nvenc encoder (the RTX 4060's encoder block is separate from its
CUDA cores, so recording does not compete with the editor for the 8 GB of VRAM), OBS
Studio with obs-websocket for scriptable scene switching and recording, and desktop
control to drive the editor. Recording is not what blocks this request.

## J-0038 | 2026-11-12 | REQ-003 | planner
- Event: evaluated
- Evaluation: requires-change
- Serves: G-1 and G-5 (the demo is the walking skeleton), if the CR brings it into scope
- Principles: P-01 n/a; P-02 engaged -- the demo lives in another repository, so no module's paths cover it and a story could name no file a coder owns; P-03 engaged -- giving it a home, or recording it as out of scope, changes architecture.md; P-04 ok; P-05 n/a; P-06 n/a; P-07 ok; P-08 ok, and in its favour -- the demo is exactly where turn-taking policy belongs, which is why it is not in the plugin; P-09 ok; P-10 ok; P-11 ok; P-12 n/a; P-13 n/a (that repository has its own history); P-14 engaged -- the demo must track the engine line it demonstrates; P-15 ok; P-16 n/a; P-17 n/a; P-18 ok.
- Reason: Serves G-1 and G-5 and crosses no non-goal. It cannot be planned inside the current architecture. FANTASIA-LangGraphDemo is a separate repository at codeberg.org/antori/FANTASIA-LangGraphDemo, not checked out here; the adoption scope is every first-party source, script and manifest file in this repository; and architecture.md already names the demo as the walking skeleton while owning none of it. So a load-bearing artifact has no module, no owner and no contract, and P-02 leaves a story nowhere to land. Closing that gap is the CR. What 'update' means is unspecified as well, and no estimate is honest without it.
- Outcome: a CR of type architecture, not raised here; the request waits for it
- Changes: none

What the CR must settle, in one decision with two branches:

- In scope. The demo repository gets a module of its own with its paths, an owner, and a
  statement of how a second repository is cloned, verified and kept in step with the
  plugin line it demonstrates. That is new ground for this architecture: every current
  module's paths are inside this repository, the checker resolves them against it, and
  P-02's one-owner rule assumes a single tree. The CR has to say how a cross-repository
  module is represented, not merely that there is one.
- Out of scope. The demo is recorded as managed separately, as the wiki is, and demo work
  is routed to its own project. architecture.md then says so where it names the demo as
  the walking skeleton, so the next reader does not take the reference for ownership.

Either way the architecture stops naming an artifact it does not own. Today the Walking
skeleton section says "The FANTASIA-LangGraphDemo project is that path wired up end to
end", which makes the demo load-bearing evidence for the skeleton while no module, owner
or contract covers it.

Second thing to settle, which is not architecture and will need a request of its own or a
sentence from the user: what "update" means. Candidates, from what changed in the plugin
since the demo was built -- the engine line (5.7 to 5.8), the removal of the
provider-specific OpenAI, Groq and Ollama components in favour of one
`UOpenAICompatibleComponent` (ADR-0012), the Neo4j plain-TCP default (ADR-0003), the
split of Audio2Face into FANTASIAACE (ADR-0005) and the End Turn requirement, and
aGrUM's upgrade. Which of those the demo must absorb decides whether this is an
afternoon or a sprint, and no estimate is honest without it.

## J-0039 | 2026-11-12 | REQ-004 | planner
- Event: evaluated
- Evaluation: requires-change
- Serves: G-4 (cover the interactive pipeline out of the box, Bayesian and influence reasoning included)
- Principles: P-01 engaged -- C-probabilistic's invariants name aGrUM 2.x and the ID, BN, BASE link order, and C-build carries the link list, so both contracts change; P-02 ok; P-03 engaged -- ADR-0009 and two contracts cannot change silently; P-04 engaged -- 3.x changes results, not only the build (LIMID optimal policy, MEU variance, evidenceProbability, the aggregators), so the expectations pinned by BayesianNetworkTests and InfluenceDiagTests are part of the change, not fallout from it; P-05 ok; P-06 n/a as a new dependency, but ADR-0009 is the ADR a major version bump amends; P-07 ok; P-08 engaged and decisive -- exposing decision models as components is in scope; deciding when or what the agent says is not (NG-1), and 'new ways of conducting decisions' is one step from a dialogue manager; P-09 ok (Win64 static libs); P-10 ok; P-11 ok, as a judgement I am flagging -- structure learning (PC, FCI) and KTBN order learning estimate a graphical model from data, which is the same shape as US-015's in-plugin SVM training and not the model training, hosting or weight shipping NG-4 denies; P-12 engaged -- relinking the module changes the shipped DLL; P-13 n/a; P-14 n/a; P-15 ok; P-16 engaged -- aGrUM 3.x's licence text must be refreshed in THIRD_PARTY_LICENSES.md; P-17 ok -- the static libs stay release assets; P-18 ok.
- Reason: Serves G-4 and crosses no non-goal, but neither half can be planned as it stands. The upgrade: C-probabilistic's invariant names aGrUM 2.x and ADR-0009 fixes the link order ID, BN, BASE, while 3.1.0 merged BN with BNLEARNING, made PRM independent and added CM, so the static library target names change -- unconfirmed upstream -- and the C-build link list with them. The API breaks are mechanical; the behaviour changes are not, and they move results the tests pin. The expansion: which model families to expose depends on what the request means, and the phrase 'raison d'exprimer'-like AI could not be resolved here. Since it governs the choice between causal models, dynamic networks, credal networks and structure learning -- four different capabilities -- that half is needs-info, and the question is in the body.
- Outcome: a CR of type architecture, not raised here; plus one question to the user before the model-family half can be scoped
- Changes: none

The question that has to be answered before the second half can be planned at all:

> Which kinds of decision do you want the agent to be able to make that it cannot make
> with a Bayesian network and an influence diagram today? aGrUM 3.x makes four distinct
> things available in C++, and they are not variants of each other:
>
> - **Causal reasoning and counterfactuals** (`CM`, new in 3.0.0): a `CausalModel` over
>   the network, do-calculus with backdoor and frontdoor set enumeration, ID/IDC
>   identification, `CausalImpact::impact()`, and counterfactual queries. This answers
>   "what would have happened if the agent had said something else", which an influence
>   diagram cannot.
> - **Dialogue state over time** (KTBN, k-order dynamic Bayesian networks, new in C++ in
>   3.2.0): inference over a time window, filtering and smoothing, and automatic learning
>   of the memory order K. This answers "what is going on now given the last k turns".
> - **Imprecise probabilities** (credal networks, `CredalNet`): decisions under
>   probabilities that are intervals rather than numbers, for when the CPTs are guesses.
> - **Structure learned from interaction data** (`PC`, `FCI` with a `PAG`,
>   `GreedyThickThinning`, now in C++): the model's shape discovered from logged
>   interactions instead of authored by hand.
>
> One of these, or more than one? The phrase "raison d'exprimer"-like AI in the request
> governs the answer and could not be resolved here, so this is a question and not a
> guess.

The second thing to settle with it, and the one that keeps the answer inside the brief:
each of these must arrive as a component the application drives, not as a policy that
decides what the agent says. That is NG-1 and P-08, and it is a real risk here -- "new
ways of conducting decisions" is one short step from a dialogue manager, which this
plugin deliberately is not.

What the CR of type `architecture` must cover for the upgrade half:

1. The target version, pinned. 3.0.0 was released 18 July 2026 and the master changelog's
   top entry is 3.3.2; the release dates and stable status of 3.1 to 3.3 are unconfirmed
   here, as is whether 2.3.2 is the last of the 2.x line. The CR picks one version and
   says why -- 3.3.2 if the newest, but note that 3.3.0 removed `DEFAULT_EPSILON` and
   friends in favour of `gum::Configuration` and 3.3.2 removed `SimpleMiic`.
2. ADR-0009, amended. It fixes the link order as ID, then BN, then BASE. 3.1.0 merged BN
   with BNLEARNING, made PRM an independent CMake module and added CM as a new one; the
   leaf modules are CM, CN, ID and PRM. The static library target names for 3.x are
   unconfirmed, so the CR must establish them by building before it asserts a link list.
   C-build changes with it.
3. C-probabilistic, version 2. Its invariant names aGrUM 2.x explicitly. If new families
   are exposed, decide whether they extend this contract or get their own -- M-probabilistic's
   responsibility line is "discrete probabilistic models and influence diagrams", which
   does not stretch to a causal model or a learner, and a new module may be the cleaner
   answer. This contract is also the plugin's public Blueprint surface, so a change
   invalidates compiled assets in user projects.
4. The behaviour changes, treated as behaviour and not as a port. These move results the
   tests pin, so each needs a decision recorded before the port starts: in 3.3.0
   `ShaferShenoyLIMIDInference`'s optimal policy keeps its decision parents and `MEU()`'s
   variance now includes covariances between utilities; `evidenceProbability()` in
   `LazyPropagation` and `ShaferShenoyInference` previously ignored connected components
   without a target; and the aggregators (`min`, `max`, `median`, `amplitude`, `sum`) now
   work on values rather than parent indices, which changes results for any range not
   starting at 0. US-012 and US-013 are pinned by BayesianNetworkTests.cpp and
   InfluenceDiagTests.cpp, so the CR says which expectations are expected to move and
   which must not.
5. The mechanical API breaks, for the impact estimate. `DAGmodel::dag()` and
   `UGmodel::graph()` return value copies, so code holding `const auto&` must use
   `internalDag()` / `internalGraph()`; `BNWriter::write()` and subclasses take the BN by
   non-const reference (we call `writeBIF`); `IDReader::proceed()` returns `Size` rather
   than void (we call `writeBIFXML`); path functions return `std::optional`;
   `const std::string&` parameters became `std::string_view` across BayesNet, variables,
   readers and learning; `StructuralComparator` is now `StructuralMetrics`, `Signaler1/2`
   are a variadic `Signaler`, `HashTable::tryGet` returns `optional_ref`. Nothing we name
   in C-probabilistic is renamed -- `BayesNet`, `InfluenceDiagram`, `LazyPropagation`,
   `ShaferShenoyInference` and `ShaferShenoyLIMIDInference` all survive -- and no
   `Potential` rename is involved, since that was the 2.x upgrade.
6. Two side obligations that fall out of it: THIRD_PARTY_LICENSES.md carries aGrUM's
   licence text and must be refreshed for the new version (P-16), and relinking the
   module changes the shipped DLL, so the prebuilt refresh applies (P-12). F-005 is worth
   closing in the same pass: the probabilistic sources include aGrUM headers without
   honouring `WITH_AGRUM_BINDING`, and every include line is being touched anyway.

## J-0040 | 2026-11-12 | REQ-001 | planner
- Event: evaluated
- Evaluation: accepted
- Serves: G-3 and G-5, as in J-0036
- Principles: Re-checked against F-016; the J-0036 verdicts stand except these. P-12 CORRECTED -- J-0036 recorded it ok on the strength of commit 94e906a8, but that commit touched only FANTASIA/Binaries, so FANTASIAACE's committed binaries are still the 5.7 build and P-12 was in fact breached when the 5.8 source landed; the breach is open and US-021 is its repair. P-04 engaged -- nothing pins either plugin's prebuilt BuildId, which is how a refresh commit half-finished unnoticed; that check belongs in BootstrapContractTests. P-16 is now carried by US-020's acceptance alone. P-03 unchanged -- the maintenance-line policy is still held for a CR of type principle.
- Reason: The J-0035 clarification and F-016 change the plan, not the verdict. The ACE 5.8 re-port is real work and not a publish step: no 5.8 build of NV_ACE_Reference exists on this machine, so US-020 keeps its L estimate and is retitled to say what it is. F-016 adds a second prerequisite that US-020 does not cover -- FANTASIAACE's own committed binaries are the 5.7 build (BuildId 47537391 against core's 55116800), so a user who enables the companion on 5.8 is told it was built with a different engine version and lip-sync does not load. That is US-021, which depends on US-020 because FANTASIAACE links ACERuntime and ACECore and cannot be built before NV_ACE_Reference exists for 5.8. US-017 drops from done to ready with it, since F-016 names it and the companion does not load on the live line at all. US-019's demotion, flagged as a judgement in J-0036, is now evidenced rather than judged.
- Outcome: US-020, US-021; the maintenance-line policy still needs a CR of type principle, not raised here
- Changes: backlog.md, architecture.md

Same verdict, a second story, and one corrected principle verdict.

Sequencing, since the two prerequisites are not parallel: US-020 first (NV_ACE_Reference
and the three character plugins rebuilt against 5.8 and published under the names
bootstrap already asks for), then US-021 (FANTASIAACE's own Win64 Development and
DebugGame binaries rebuilt against that and committed as their own refresh commit).
FANTASIAACE links ACERuntime and ACECore, so there is no way to do the second first, and
F-016 records that no 5.8 build of NV_ACE_Reference exists anywhere on the machine --
the copies under Alice, FedericoII and the demo project are all 5.7.

Three stories now carry the gate between the 5.8 line and main: US-018 and US-019 from
F-014, and US-017 from F-016, all moved from `done` to `ready`. None of them was ever
false of 5.7; each was recorded `done` from the 5.7 line and inherited.

What the planner got wrong in J-0036, recorded rather than quietly fixed: P-12 was
marked ok because commit 94e906a8 refreshed the prebuilt DLLs for 5.8. It refreshed
core's and not the companion's, so the principle was breached when the 5.8 source landed
and the breach is still open. The structural point is worth keeping: P-12 says "the Win64
prebuilt binaries are rebuilt", and nothing in the repository knows that there are two
sets of them. A BootstrapContractTests check that every committed `.modules` BuildId in
both plugins equals the engine's would have caught this the day it happened, and is one
of the mechanizable-but-unmechanized checks principles.md already complains about.

Still held, unchanged: the "minor fixes only" bar and the 5.9 feature target, for a CR of
type principle. Nothing in the clarification touches that half.

## J-0041 | 2026-11-12 | REQ-003 | lead
- Event: received
- Source: user
- Text: Decision on the demo project's home: OUT OF SCOPE, managed separately as the wiki is. FANTASIA-LangGraphDemo is not brought into this architecture as a module; demo work is routed to its own project. architecture.md must stop implying ownership where it names the demo as the walking skeleton. The architecture CR therefore takes the second branch the planner described, and no cross-repository module representation is needed.
- Changes: none

## J-0042 | 2026-11-12 | REQ-004 | lead
- Event: received
- Source: user
- Text: Answer to the model-family question: ALL FOUR. Causal reasoning and counterfactuals (CM), dialogue state over time (KTBN k-order dynamic Bayesian networks), imprecise probabilities (credal networks), and structure learned from interaction data (PC, FCI with PAG, GreedyThickThinning). This resolves the needs-info half of REQ-004 and settles what 'raison d'exprimer'-like AI should reach for: all four capability families, each exposed as a component the application drives. The architecture CR for the 3.x upgrade still stands, and its scope is now four new capability surfaces rather than one, so it will need splitting into separately shippable stories.
- Changes: none

## J-0043 | 2026-11-12 | NOTE | lead
- Changes: brief.md, principles.md, backlog.md, architecture.md

The user approved the brief, principles, backlog and architecture at the adoption gate, which is when the ADOPT entry was written. The documents were left saying 'Approved: pending' and the architecture 'Status: draft', which was an omission in closing the adoption rather than a pending decision. They now record the approval that was given.

Design version stays 1. What the approval covers is the recovered design as adopted, plus the planner's subsequent edits within its own authority: US-020 and US-021 added for REQ-001, and US-017, US-018 and US-019 demoted from done to ready because each was recorded done from the 5.7 line and is not true of 5.8.

Three change requests are queued and will raise the design version when they land: a principle CR for the maintenance-line policy (REQ-001), an architecture CR recording the demo as managed separately (REQ-003, the user chose that branch), and an architecture CR for the aGrUM 3.x upgrade and the four model families (REQ-004). Approving version 1 is approving what is, not foreclosing those.

## J-0044 | 2026-11-12 | CR-001 | lead
- Event: proposed
- Type: principle
- Class: major
- Impact: New principle governing what the 5.8 line admits, plus a change to how P-14 is read. Stories: US-020 and US-021 are the accepted 5.8 work and are unaffected; US-017, US-018 and US-019 are ready and gate the promotion. Findings: F-008 and F-011 are the two open majors whose admission this decides, and F-013, F-014, F-015, F-016 are all M-dist or M-ace defects on the 5.8 line. Contracts: none change, but the proposed definition uses 'touches no contract' as its bar, so C-neo4j decides F-011's fate. Modules: none change. Backlog structure: a target-line dimension would touch every story. Agents: none exist yet (no team.json; staffing has not run), so there is no coder or reviewer to notify and no verification profile to refresh. Tasks in flight: none.
- Options: (A) Adopt the planner's definition -- touches no contract, no module boundary, no ADR, no dependency; changes no Blueprint-facing signature; estimate S or M -- and decide F-008 and F-011 explicitly on top of it. (B) Adopt the definition but carve out an exception for security findings, so F-011 is admitted despite touching C-neo4j. (C) Define the bar by risk rather than by surface: anything that cannot change a user's results or break their assets is minor, whatever it touches. (D) Don't change: ship 5.8 with no stated bar. Cost of (D): the planner cannot reject anything as too large, so the maintenance line admits everything and 'minor fixes only' becomes a statement of intent with no mechanism; 5.9-targeted work also has nowhere to live and will be lost, since the backlog has no engine-line dimension.
- Changes: none

Raised from REQ-001, whose policy half the planner held rather than accepted: an
undefined "minor fix" is a bar it cannot reject against at intake, so in practice it
would admit everything.

Four things this must settle.

**1. What "minor fix" means, operationally.** The planner proposes: touches no contract,
no module boundary, no ADR and no dependency; changes no Blueprint-facing signature, so
it cannot invalidate a user's compiled Blueprint assets; estimate S or M. Any definition
beats none.

**2. Whether the open majors are admitted on 5.8.** This is the decision with teeth.
F-008 is two missing `Config` UPROPERTY specifiers, so it passes any size bar -- though
it changes where a secret is read from and existing assets must be re-saved. F-011's fix
changes C-neo4j, so a contract-touching bar excludes it, and that parks a defect which
sends Neo4j credentials in the clear whenever a user sets `UseSSL` against a `bolt://`
endpoint. Deferring a live credential exposure to 5.9 should be decided out loud, not
fall out of the wording of (1).

**3. How P-14 reads while a preview exists.** P-14 says main carries the newest supported
engine version. The request treats 5.8 as the shipped line and 5.9 as the feature target.
Name the trigger for branching 5.8 and moving main to 5.9 -- the 5.9 release, the 5.9
preview, or the first 5.9-only feature -- so the switch is not a judgement call each
time.

**4. Where 5.9-targeted work accumulates before 5.9 exists.** The backlog has no
engine-line dimension: a story is `draft`, `ready`, `planned`, `in-progress`, `done` or
`dropped`, with nothing to say which line it targets. Either stories carry a target-line
field, or 5.9 work waits unlogged and is lost.

Needs the user's approval: a principle change is major by definition, and (2) in
particular is a risk decision that is not the planner's to make.

## J-0045 | 2026-11-12 | CR-002 | lead
- Event: proposed
- Type: architecture
- Class: minor
- Impact: architecture.md only: the Walking skeleton section, which today says 'The FANTASIA-LangGraphDemo project is that path wired up end to end' and so makes the demo load-bearing evidence while no module, owner or contract covers it. No module is added, split, merged or removed; no contract changes; no story changes; no test changes. Consequence for the backlog: REQ-003 leaves it with no story, and demo work routes to its own project. Agents: none exist yet. Design version bumps 1 -> 2.
- Options: (A) Record the demo as managed separately, as the wiki is, and rewrite the Walking skeleton section so it cites the demo as an illustration rather than as an owned artifact -- the branch the user chose. (B) Bring the demo into scope as a cross-repository module, which needs a representation this architecture does not have: every current module's paths are inside this tree and the checker resolves them against it. (C) Don't change: leave architecture.md naming the demo as the walking skeleton with nothing owning it. Cost of (C): the architecture keeps asserting a skeleton it cannot verify, P-02 leaves any demo story nowhere to land, and the next reader takes the reference for ownership -- which is exactly how REQ-003 arrived as requires-change.
- Changes: none

Raised from REQ-003, which the planner evaluated `requires-change` because the demo is a
separate repository with no module, owner or contract, while architecture.md's Walking
skeleton section relies on it.

The user has already chosen the branch, in answer to a question that described this
option as "recorded as managed separately; architecture.md stops implying ownership
where it names the demo as the walking skeleton": **out of scope, managed separately as
the wiki is.** So the substance is decided and this entry records it.

Classed minor: it adds, splits, merges and removes no module, changes no contract and no
story. It is a clarification of scope, which is why the planner's authority covers it --
but the decision behind it is the user's and was given explicitly.

What the apply step changes: the Walking skeleton section states that the skeleton ships
and is illustrated by FANTASIA-LangGraphDemo, which is maintained as its own project and
owned by no module here, in the same arrangement as the project wiki. Design version goes
to 2 and an ADR records the decision.

Context for whoever picks the demo up, which is not work for this repository: it is in
fact checked out locally at `D:\Git\FANTASIA-LangGraphDemo`, and it currently carries the
5.7 ACE plugins and James only, so it inherits the same 5.8 port dependency as US-020.

## J-0046 | 2026-11-12 | CR-003 | lead
- Event: proposed
- Type: architecture
- Class: major
- Impact: Contracts: C-probabilistic (its invariants name aGrUM 2.x and the ID/BN/BASE link order) and C-build (it carries the static library link list, which 3.1.0 changes by merging BN with BNLEARNING, making PRM independent and adding CM) -- both break, both bump Version. ADR-0009 is amended or superseded. Modules: M-probabilistic changes and four new capability surfaces must be placed, either in it or in new modules; M-dist changes with the link list; M-tests changes because BayesianNetworkTests and InfluenceDiagTests pin results that 3.3.0 moves. Stories: US-012 and US-013 are the affected done stories; four new story groups come from the four families. NFRs: none directly, but M-probabilistic sits behind no NFR measure. Findings: F-005 (the WITH_AGRUM_BINDING guard M-probabilistic ignores) is in the blast radius and should be fixed in the same pass. Licensing: P-16 requires THIRD_PARTY_LICENSES.md to carry aGrUM 3.x's text. Agents: none exist yet (staffing has not run), so nobody to notify; when it does, M-probabilistic is a Prolog-adjacent niche surface that the model policy puts on opus. Tasks in flight: none.
- Options: (A) Upgrade and expose all four families, split into separately shippable stories, one capability at a time behind the upgrade. (B) Upgrade only, deferring every new family: smallest change that gets off a superseded 2.x, and it still breaks two contracts and moves pinned results. (C) Upgrade and expose one family first -- KTBN is the strongest candidate for an embodied conversational agent, being dialogue state over time -- and treat the rest as follow-on requests once the migration has proven stable. (D) Don't change: stay on 2.3.2. Cost of (D): the project stays on a line upstream has left (3.0.0 shipped 18 July 2026), every later aGrUM fix and all four capability families stay unreachable, and the eventual migration grows; against that, 2.3.2 works today, 107-plus tests pass on it, and nothing is broken by waiting.
- Changes: none

Raised from REQ-004. The planner evaluated the upgrade half `requires-change` and the
model-family half `needs-info`; the user has now answered the second with **all four**
families, which makes this CR considerably larger than a version bump.

**(a) The upgrade, 2.3.2 to 3.x.** 3.0.0 released 18 July 2026; the master changelog's
top entry is 3.3.2, whose release date and stable status are unconfirmed, as is whether
2.3.2 is the last 2.x. The CR must pick a target version, and it must **build before
asserting a link list**: 3.1.0 merged BN with BNLEARNING, made PRM an independent module
and added CM, so the static library target names change and are not confirmed upstream.

The API breaks are mechanical and need porting, not deciding: `dag()`/`graph()` return
value copies so callers use `internalDag()`/`internalGraph()`; `BNWriter::write()` takes
a non-const reference; path functions return `std::optional`; `IDReader::proceed()`
returns `Size`; `const std::string&` became `std::string_view`; `StructuralComparator`
became `StructuralMetrics`; `Signaler1/2` became variadic; `DEFAULT_EPSILON` and friends
moved into `gum::Configuration`.

The **behaviour** changes are decisions, because they move results the tests pin:
`ShaferShenoyLIMIDInference`'s optimal policy now keeps its decision parents; `MEU()`'s
variance now includes covariances between utilities; `evidenceProbability()` previously
ignored connected components without a target; and the aggregators (`min`, `max`,
`median`, `amplitude`, `sum`) now work on values rather than parent indices, which
changes results for ranges not starting at zero. Each needs a verdict on whether the new
behaviour is the intended one, and the tests rewritten to pin it deliberately rather than
adjusted until they pass.

Reassuringly, nothing we call is renamed: `BayesNet`, `InfluenceDiagram`,
`LazyPropagation`, `ShaferShenoyInference` and `ShaferShenoyLIMIDInference` keep their
names, and `Potential` to `Tensor` was already done in the 2.x upgrade.

**(b) Four capability surfaces, not one.** Causal reasoning and counterfactuals (the CM
module: `CausalModel`, the do-calculus AST, `DoorCriteria` backdoor and frontdoor
enumeration, ID/IDC identification, `CausalImpact::impact()`, counterfactual queries);
dialogue state over time (KTBN k-order dynamic Bayesian networks, with filtering,
smoothing and automatic order learning); imprecise probabilities (credal networks,
`CredalNet`); and structure learned from interaction data (`PC`, `FCI` with `PAG`,
`GreedyThickThinning`). These are not variants of each other and should land as separate
stories behind the upgrade, not as one change.

**The live design risk is NG-1 and P-08.** Each family must arrive as a component the
application drives. "New ways of conducting decisions" is one step from a dialogue
manager, and FANTASIA explicitly is not one: it may offer the machinery to decide, never
the policy about when or what the agent says.

**One judgement for the user, not for the planner.** The planner reads structure learning
and KTBN order learning as *estimation* -- the same shape as the in-plugin SVM training
US-015 already ships -- and therefore not the model training, hosting or weight shipping
NG-4 denies. A stricter reading of NG-4 makes that fourth family a non-goal violation and
removes it from the answer. Worth a moment, since the user chose all four.

Needs the user's approval: breaking changes to two contracts, an amended ADR, and four
new capability surfaces.

## J-0047 | 2026-11-12 | CR-002 | lead
- Event: approved
- Decision: Approved by the user. Asked which way the demo should go, with this option described as 'recorded as managed separately; architecture.md stops implying ownership where it names the demo as the walking skeleton', the user chose it over bringing the demo in as a cross-repository module. The substance was therefore decided before the CR was raised; this records it.
- Changes: none

Option (A) of three. (B), a cross-repository module, was rejected because it would need a
representation this architecture does not have: every module's paths are inside this tree
and the checker resolves them against it, so a module pointing at another repository
would be unverifiable by the one mechanical check the project has. (C), leaving
architecture.md as it was, was rejected because it is the state that produced REQ-003's
`requires-change` in the first place -- an architecture asserting a skeleton it cannot
verify.

## J-0048 | 2026-11-12 | ADR-0014 | lead
- Status: accepted
- Title: The demo project is maintained separately and owned by no module here
- Changes: none

### Context
architecture.md's Walking skeleton section cited FANTASIA-LangGraphDemo as "that path
wired up end to end", which made the demo load-bearing evidence for the skeleton. The
demo is a separate repository at codeberg.org/antori/FANTASIA-LangGraphDemo. No module's
paths cover it, no contract describes it, and P-02's one-owner rule left any story about
it nowhere to land -- which is why REQ-003 could not be planned.

### Decision
The demo is maintained as its own project, in its own repository, owned by no module
here, in the same arrangement as the project wiki. architecture.md cites it as an
illustration and rests nothing on it. Work on the demo is routed to its own project and
is not planned from this backlog.

### Consequences
The architecture now claims only what it owns, and the walking skeleton is verifiable
from this repository alone -- every component in that path belongs to a module here.
REQ-003 leaves this backlog with no story. The cost is that the demo can drift from the
plugin without anything here noticing: it currently carries the 5.7 ACE plugins and
James only, so it inherits US-020's 5.8 port dependency, and nothing in this repository
will flag it when the plugin moves. Whoever maintains the demo owns that risk.

### Alternatives
A cross-repository module, with paths in another tree: rejected because the checker
resolves module paths against this repository, so such a module could not be verified
mechanically, and P-02 assumes a single tree. Leaving the citation as it stood: rejected
as the status quo that made REQ-003 unplannable.

## J-0049 | 2026-11-12 | CR-002 | lead
- Event: implemented
- Evidence: architecture.md's Walking skeleton section rewritten: the skeleton is stated to live entirely in this repository and to be verifiable from it alone, and the demo is cited as an illustration maintained as its own project, citing ADR-0014. Design version 1 -> 2. ADR-0014 logged. Checker and reflexion run after the change; no module, contract, story or test was touched, so nothing else needed updating. No agents exist yet (staffing has not run), so there was nobody to notify.
- Changes: architecture.md

## J-0050 | 2026-11-12 | CR-001 | lead
- Event: approved
- Decision: Approved by the user, option (A): the surface bar. A fix is minor on a shipped engine line when it touches no contract, no module boundary, no ADR and no dependency, changes no Blueprint-facing signature, and is estimated S or M. The user was shown, in the option itself, that this admits F-008 and excludes F-011, and chose it on that basis.
- Changes: none

Option (A) of four. (B), the same bar with a security carve-out, and (C), a risk-based
bar, were both rejected; (D), no bar, was rejected as the status quo that makes the
policy unenforceable.

**What this costs, stated plainly because the wording would otherwise hide it.** F-011
is excluded from the 5.8 line. Its fix changes C-neo4j, and the bar excludes anything
that touches a contract. So for the life of the 5.8 line, a user who sets `UseSSL = true`
against a `bolt://` endpoint connects in plaintext and sends their Neo4j password in the
clear, and the plugin tells them nothing. That is a decision the user made with the
consequence in front of them, not a side effect of the definition -- which was the whole
reason the planner refused to write the definition without asking.

F-008 is admitted: two missing `Config` UPROPERTY specifiers, no contract touched, and
without it API keys are written into Blueprint and actor asset defaults that users commit.

Two parts of this CR the user has not decided, which the lead is applying as stated
assumptions rather than leaving the CR half-done. Either can be corrected by appending to
this CR:

- **The trigger for moving main to the next engine line**: a *released* engine, not a
  preview, and only once the plugin builds clean and its tests pass against it. This is
  the reading the request itself implies -- 5.8 is being shipped precisely because 5.9 is
  only a preview -- and it keeps P-14 mechanical rather than a judgement each time.
- **Where next-line work accumulates**: stories carry a `Target line:` field naming the
  engine line they are for, defaulting to the shipped line. Without it, 5.9-targeted work
  has nowhere to live and is lost, which was the fourth thing this CR had to settle.

## J-0051 | 2026-11-12 | CR-003 | lead
- Event: approved
- Decision: Approved by the user, option (C): migrate to aGrUM 3.x and expose dialogue state over time (KTBN) first, as the family that most directly serves an embodied conversational agent. Causal reasoning and counterfactuals, credal networks and structure learning become follow-on requests once the migration has proven stable. Separately, the user confirmed the planner's reading of NG-4: fitting a graphical model to logged interactions is estimation, the same shape as the SVM training US-015 already ships, so structure learning stays admissible as a later family.
- Changes: none

Option (C) of four. (A), all four families at once, was rejected as sequencing risk, not
on scope: the user still wants all four and said so. (B), upgrade with no new capability,
was rejected because it pays the whole migration cost -- two broken contracts and moved
results -- with nothing to show. (D), staying on 2.3.2, was rejected.

So the approved scope is: the 3.x migration, then KTBN. The other three families are
wanted and are not planned here; they arrive as their own requests.

**The NG-4 reading is now settled and should be cited rather than re-argued.** Structure
learning and KTBN order learning estimate a graphical model from data. NG-4 denies
training, fine-tuning, hosting and shipping model weights. The user has confirmed the
first is not the second, which also leaves US-015's existing in-plugin SVM training
unquestioned.

**What the apply step may and may not do now.** C-probabilistic and C-build are
`Provenance: recovered` state documents: they describe aGrUM 2.3.2 because that is what
is vendored today. They must not be rewritten to describe 3.x before the code is on 3.x,
or they would describe a future. So this approval produces an ADR, the architecture's
Decisions line, and stories; the contract rewrites and the version bumps happen when the
migration lands, and this CR gets its `implemented` event then.

**The migration's own hazard, for whoever plans it.** The target version is not settled
upstream -- 3.0.0 shipped 18 July 2026, the changelog's top entry is 3.3.2, and the
release dates and stable status of 3.1 to 3.3 are unconfirmed, as is whether 2.3.2 is the
last 2.x. The static library target names change (3.1.0 merged BN with BNLEARNING, made
PRM independent, added CM) and must be confirmed by building, never asserted into
C-build. And four behaviour changes move results the tests pin: the LIMID optimal policy
keeping decision parents, `MEU()`'s variance including covariances, `evidenceProbability()`
no longer ignoring targetless components, and the aggregators working on values. Each
needs a verdict that the new behaviour is the intended one, with the tests rewritten to
pin it deliberately -- not adjusted until they pass.

## J-0052 | 2026-11-12 | CR-001 | planner
- Event: implemented
- Evidence: principles.md: P-19 added (surface bar, applicable at intake, Enforced by planner+reviewer, Provenance stated) and P-14's statement amended to carry the released-engine trigger and the builds-clean-and-tests-pass condition. backlog.md: preamble documents the Target line convention and states F-011's exclusion from the 5.8 line with its consequence; Target line: 5.8 on US-020 and US-021; a Known defect line on US-014 pointing at F-011. No contract, module or test changed, so nothing else needed updating. Checker run after the change: 0 errors. No agents exist yet (staffing has not run), so there was nobody to notify.
- Changes: principles.md, backlog.md

P-19 is the new principle, in the Project principles section, applicable at intake and
enforced by judgement like the rest of that section: planner and reviewer, not a checker
and not a gate, because nothing here is mechanized. Its statement carries the surface bar
as approved -- no contract, no module boundary, no ADR, no dependency; no Blueprint-facing
signature; estimate S or M -- and adds the one clause the approved CR implies but does not
spell out: bringing a line into being (the engine port, the prebuilt refresh) is the line's
own work and is not measured by this bar. Without that clause P-19 would read as excluding
US-020 (L) and US-021 from the 5.8 line, which the CR's own impact analysis says are
unaffected. That wording is a judgement and can be corrected by appending here.

P-14 now names the trigger in its statement rather than leaving it to be judged each time:
main moves to the next engine line only on a released engine, never a preview, and only
once the plugin builds clean against it and the suite passes on it. Its title changed with
it (`Newest released engine on main`); nothing else in the state documents cites P-14 by
title.

The backlog's preamble documents the `Target line:` convention: a story may name the engine
line it is for, a story without one targets the shipped line, and work for a line that has
not shipped yet is logged with that line named so it is not lost. The field is on the
stories where it carries information -- US-020 and US-021, both 5.8 -- and not on the
sixteen recovered stories that simply describe what the plugin does.

F-011's exclusion is recorded in three places outside the journal, none of them a new
story. The backlog preamble states it in full, in front of whoever is choosing what to work
on, with the consequence named: a plaintext password for the life of the 5.8 line. US-014
carries a `Known defect:` line pointing at it. P-19's own rationale cites it as the live
case, so the principle cannot be read without meeting its price. I deliberately did not
open a story for the fix: F-011 is an open major finding, the status fold already carries
it, and the fix's shape (remove the property, refuse to connect, or warn) is undecided, so
a story would invent scope the user has not chosen.

## J-0053 | 2026-11-12 | ADR-0015 | planner
- Status: accepted
- Amends: ADR-0009
- Title: aGrUM moves to the 3.x line, with KTBN exposed first
- Changes: none

### Context
FANTASIA vendors aGrUM 2.3.2 as per-module static libraries -- `agrumID`, `agrumBN`,
`agrumBASE`, linked in that order (ADR-0009) -- and `M-probabilistic` is written against
the 2.x API. Upstream has left that line: 3.0.0 released 18 July 2026 and the master
changelog's top entry is 3.3.2. REQ-004 asked both for the upgrade and for new model
families, and aGrUM 3.x makes four reachable from C++ that 2.3.2 does not: causal models
with counterfactuals (the new `CM` module), k-order dynamic Bayesian networks (KTBN),
credal networks, and structure learning. The user wants all four, and in CR-003 chose to
migrate first and expose one family first.

### Decision
Migrate `M-probabilistic` and the aGrUM link surface to the 3.x line, and expose **KTBN --
dialogue state over time** as the first new capability, as a Blueprint-facing component the
application drives, never as a policy that decides what the agent says (NG-1, P-08).

The target 3.x version **is not settled upstream and is fixed by building**, not by reading
a changelog. 3.0.0's release date is known; the release dates and stable status of 3.1
through 3.3.2 are not, nor is whether 2.3.2 is the last of the 2.x line. The version this
project targets is the one that has been built with the suite green against it, and it is
named in `C-build` and `C-probabilistic` only once that has happened.

The static library target names and their link order are **confirmed by building and never
asserted**. 3.1.0 merged BN with BNLEARNING, made PRM an independent module and added CM,
so ADR-0009's `agrumID`/`agrumBN`/`agrumBASE` list cannot be carried across. The rule
survives where the list does not: vendor only the modules FANTASIA uses, list each library
before its dependency with the leaf last, and build them `/MD` without OpenMP.

Four behaviour changes are decisions and not porting fallout, because they move results the
tests pin. Each needs a recorded verdict that the new behaviour is the intended one, with
the test rewritten to pin it deliberately rather than adjusted until it passes:
`ShaferShenoyLIMIDInference`'s optimal policy keeping its decision parents; `MEU()`'s
variance including covariances between utilities; `evidenceProbability()` no longer ignoring
connected components without a target; and the aggregators working on values rather than
parent indices.

Causal models, credal networks and structure learning are wanted and arrive as their own
requests, behind this migration. Structure learning and KTBN order learning estimate a
graphical model from data, which is not the training, fine-tuning, hosting or weight
shipping NG-4 denies -- it is the shape of the in-plugin SVM training US-015 already ships.
That reading is settled with the user at J-0051 and is cited, not re-argued.

This ADR **amends ADR-0009; it does not supersede it.** ADR-0009 is the decision behind the
aGrUM 2.3.2 that is vendored today, and its link order is still a live invariant in both
`C-probabilistic` and `C-build`. Superseding it now would leave two state documents citing
a superseded decision for a rule still in force. It is superseded when the migration lands
and those contracts are rewritten, in the same change; until then ADR-0009 describes the
present and this ADR describes the direction, and architecture.md cites both.

### Consequences
The migration is not minor under P-19 -- it touches two contracts, a dependency and an ADR
-- so it targets the 5.9 line and not the shipped 5.8 one. `C-probabilistic` is also the
plugin's public Blueprint surface, so its version bump invalidates compiled Blueprint assets
in user projects, which is a second reason it cannot ride a maintenance line.

`C-probabilistic` and `C-build` are **not** rewritten by this decision. They are recovered
documents describing what is vendored today; rewriting them to describe 3.x before the code
moves would make them describe a future. They are rewritten, and their Versions bumped, when
the migration lands -- which is also when CR-003 gets its `implemented` event and when
ADR-0009 is superseded and dropped from architecture.md's Decisions.

Two standing obligations come with it. P-16: THIRD_PARTY_LICENSES.md must carry aGrUM 3.x's
upstream licence text. P-12: relinking changes the shipped DLL, so the prebuilt refresh and
its dumpbin check apply when the migration lands on a branch users install from. F-005 is
folded into the migration story rather than left behind it: every include line in
`M-probabilistic` is being touched anyway, and the `WITH_AGRUM_BINDING` guard it ignores is
in those files.

Until the migration lands, the project stays on 2.3.2 and the three other families stay
unreachable. Nothing is broken by waiting; the cost is that each later aGrUM fix stays out
of reach and the migration grows.

### Alternatives
Upgrade and expose all four families at once: rejected as sequencing risk, not on scope --
the user still wants all four. Upgrade only, deferring every family: rejected because it
pays the whole migration cost, two broken contracts and moved results, with nothing to show.
Stay on 2.3.2: rejected, upstream has left the line. Supersede ADR-0009 now rather than
amend it: rejected for the reason above -- the supersession belongs to the landing, not to
the planning.

## J-0054 | 2026-11-12 | NOTE | planner
- Subject: CR-003: the planning half applied; the CR stays approved
- Changes: architecture.md, backlog.md

**CR-003 stays `approved`, deliberately, and this NOTE is what records the half that
landed.** The approval itself (J-0051) says the contract rewrites and the version bumps
happen when the migration lands and that the CR gets its `implemented` event then. Logging
`implemented` now -- even scoped to the planning artifacts -- would tell the status fold
that CR-003 is done while two contract rewrites, two Version bumps and a supersession are
still owed, and would contradict the approval entry a reader reaches first. So: approved,
with this NOTE.

**What landed.**

- ADR-0015, accepted: aGrUM moves to the 3.x line with KTBN exposed first. It states that
  the target 3.x version is unsettled upstream and is fixed by building, and that the
  static library target names and link order are confirmed by building and never asserted.
  It **amends ADR-0009 rather than superseding it**: ADR-0009 is the decision behind the
  2.3.2 that is vendored today, and its ID/BN/BASE link order is still a live invariant in
  both C-probabilistic and C-build, which may not be rewritten yet. A supersession now
  would leave two state documents citing a superseded decision for a rule still in force.
  ADR-0009 is superseded, and dropped from architecture.md's Decisions, in the same change
  that rewrites those contracts.
- architecture.md: a Decisions line citing ADR-0015 beside the ADR-0009 line it amends;
  Design version 2 -> 3; US-022 and US-023 added to M-probabilistic's Satisfies and US-022
  to M-dist's, since the link list is M-dist's.
- backlog.md: US-022, the 3.x migration (`ready`, Must, Target line 5.9) and US-023, KTBN
  (`draft`, Must, 5.9); plus the three deferred families as `draft` stories at Target line
  5.9 -- US-024 causal reasoning and counterfactuals, US-025 credal networks, US-026
  structure learning. The deferred three deliberately have no module: M-probabilistic's
  responsibility is discrete probabilistic models and influence diagrams and does not
  stretch to a causal model or a learner, so the placement belongs to each family's own
  request. The checker's three "not satisfied by any module" warnings are that fact, not a
  defect to silence.

**What is pending, and where it is written down.** US-022's acceptance carries the
obligations so they cannot be lost: the four behaviour changes each need a recorded verdict
that the new behaviour is the intended one with the test rewritten to pin it deliberately;
C-probabilistic and C-build are rewritten and their Versions bumped only once the port is
green; THIRD_PARTY_LICENSES.md gets aGrUM 3.x's text (P-16); the prebuilt refresh and its
dumpbin check apply (P-12). F-005 is folded in as an acceptance clause -- the honest call,
since every include line in M-probabilistic is being touched and the ignored
`WITH_AGRUM_BINDING` guard is in those files.

**One consequence worth stating out loud.** Under P-19, applied earlier today from CR-001,
the migration is not minor: it touches two contracts, a dependency and an ADR. So it cannot
ride the shipped 5.8 line, and every story here carries `Target line: 5.9`. That follows
from the two approvals rather than from a choice made here, but it is the first time the
two have been put together.

## J-0055 | 2026-11-12 | TEAM | lead
- Changes: team.json, team.md, brief.md, verification/core-dev.md, verification/speech-dev.md, verification/reasoning-dev.md, verification/build-dev.md, verification/test-dev.md

The team is derived and approved. Nine agents, mode `subagents`, ownership on in observe
mode because the project is adopted: the hook logs would-be violations to
`.ateam/ownership.jsonl` and lets the edit through.

**Roster.** `planner` (opus). Five coders: `core-dev` (M-core, M-llm, M-neo4j, M-demo),
`speech-dev` (M-asr, M-tts, M-ace), `reasoning-dev` (M-prolog, M-probabilistic, M-ml),
`build-dev` (M-dist, C#), all opus, and `test-dev` (M-tests, sonnet). Then `tester`
(sonnet), `security` (opus) and `reviewer` (opus). Five on opus, because every coder
cluster contains a contract provider with two or more consumer modules and a mistake here
ships as a broken prebuilt binary to users who cannot rebuild it.

**Mode.** `subagents`, not `team`. The checker reports a contract-first width of 12, and
acting on it would be the mistake it warns about: all twelve contracts are
`Provenance: recovered`, version 1, and not one has a contract test — several were
corrected during the adoption review, which is what an immature contract looks like. The
work in hand is sequential too (US-020 before US-021, because FANTASIAACE links ACERuntime
and cannot be rebuilt until NV_ACE_Reference exists for 5.8).

**The one place the rules could not all hold, decided by the user.** Five coders exceeds
the policy's cap of four. The checker requires every module to be assigned to an agent and
forbids a tester from owning one, so the automation suite needs a coder; M-dist needs a C#
coder of its own; and that leaves the nine C++ component modules for two coders if the cap
is to hold, which would mean one owner for seven modules spanning shared types, two HTTP
back ends, Bolt framing, Prolog threading, aGrUM and libsvm — the cluster the splitting
rule says to split. The alternative that holds the cap, folding M-tests into `core-dev`,
was put to the user with its cost (every behavioural change becomes a cross-coder request
to update the test that pins it) and rejected in favour of the fifth coder. The right end
state is neither: dissolve M-tests and fold its per-component test files into each
module's Paths, so every coder owns its own tests. That is an architecture change, not a
staffing decision, and it would bring the roster back to four coders.

**Deviations from the standard structure**, all recorded in team.md with their reasons:
the three module `CLAUDE.md` files are per source tree rather than per module and belong
to the planner, because nine modules share two directories and no single coder can own a
file covering several coders' modules; M-dist has no directory at all, so its state lives
in C-build and C-install; and `gates.task_completed` is `null` with `gates.quality` empty,
which is the honest state — no contract tests exist, the suite needs the editor, there is
no `.clang-format` and neither clang tool is installed, UBT emits no compilation database,
and the `Build.cs` files belong to no solution for `dotnet format` to act on. Filling
those in with commands that cannot run would make the clean-code promise prose.

**Rejected alternatives**, with reasons, in team.md: `team` mode, `single` mode, one C++
coder, one coder per module, a separate owner for M-core, cheaper coders on sonnet with
`advisorModel`, an interface and personas designer (the product has no screens; its
people-facing surface is a Blueprint API whose ergonomics live in the contracts), and no
security engineer.

**The security engineer is on the team** because the system has credentials in four
components, a network endpoint in each of them, Prolog goals and Cypher built from
strings, and a vendored tree of seven third-party projects — and because the two findings
that matter most right now, F-008 and F-011, are both credential exposures found by
accident rather than by anyone looking. Its first task, before the first story it reviews,
is `docs/ateam/threat-model.md`.

**The team is local to this machine, by the user's decision**, which closes the open
question the adoption deferred to staffing. `.gitignore` excludes `.claude/` and
`CLAUDE.md`, so the nine agent definitions, the root protocol block, the three module
state documents and the dashboard configuration are untracked by choice. `docs/ateam/`
is tracked, so the design travels with the repository and the team does not; a reclone
re-derives it from team.json. Recorded as a constraint in the brief.

**Settings were not applied.** There is no `.claude/settings.json` at the repository root
for the renderer to add allow rules to, and settings are proposed rather than applied.
`.ateam/` is already gitignored.

Still open after staffing: REQ-003 and REQ-004 show as awaiting evaluation, because the
user's answers were logged after the planner's verdicts and were handled through CR-002
and CR-003; the planner owes each a closing `evaluated` entry. Next is `/ateam:sprint`,
whose first sprint is US-020 then US-021, with the threat model as the security engineer's
first task.
