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
