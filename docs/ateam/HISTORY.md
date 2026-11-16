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

## J-0056 | 2026-11-15 | REQ-003 | planner
- Event: evaluated
- Evaluation: rejected
- Principles: P-02 decisive -- no module's paths cover another repository, so a story here could name no file a coder owns; P-03 satisfied by CR-002, which recorded the demo as managed separately; every other principle unchanged from J-0038.
- Reason: Out of scope for this repository, by P-02 and the user's own decision at J-0041. The demo is a separate repository (codeberg.org/antori/FANTASIA-LangGraphDemo); CR-002 settled that it is not brought into this architecture as a module, and architecture.md now names it as an illustration rather than an owned artifact (ADR-0014). The work is wanted and is routed to the demo's own project; it is not planned from this backlog.
- Outcome: no story in this backlog; demo work routes to the demo's own project (CR-002, ADR-0014)
- Changes: backlog.md

Closes the evaluation this request has been waiting on since J-0038, where it was
`requires-change`. The change it waited for is CR-002, approved and applied at J-0045:
architecture.md's Walking skeleton section now names FANTASIA-LangGraphDemo as an
illustration maintained as its own project, owned by no module here, in the same
arrangement as the project wiki (ADR-0014).

With that applied, the request has no landing place in this repository. The work is
wanted -- the demo is how a new user sees the pipeline wired up -- but every file it
touches lives in another repository, so no task here could do it and no story here can
hold it. That is a rejection of scope, not of the idea: it is routed to the demo's own
project, where it has an owner.

What the demo still owes, for whoever picks it up there: the engine line (5.7 to 5.8),
the single `UOpenAICompatibleComponent` in place of the provider-specific components
(ADR-0012), the Neo4j plain-TCP default (ADR-0003), the FANTASIAACE split and its End
Turn requirement (ADR-0005), and aGrUM's version. It is checked out at
`D:\Git\FANTASIA-LangGraphDemo` and carries the 5.7 ACE plugins and James only, so it
inherits US-020's 5.8 port dependency.

## J-0057 | 2026-11-15 | REQ-004 | planner
- Event: evaluated
- Evaluation: accepted
- Serves: G-4
- Principles: P-01, P-03 and P-04 engaged and discharged through CR-003 (C-probabilistic and C-build change, ADR-0009 is amended by ADR-0015, and the four moved results are decided per behaviour rather than ported); P-08 and NG-1 engaged and held -- each family is a component the application drives, stated in every story's acceptance; P-11 and NG-4 ok on the recorded reading -- estimating a graphical model from the project's own data is US-015's shape, not training, hosting or shipping weights; P-12 and P-16 engaged, both named in US-022's acceptance; P-19 decisive on timing -- every one of the five targets the 5.9 line; P-02, P-05, P-06, P-07, P-09, P-10, P-13, P-14, P-15, P-17, P-18 ok.
- Reason: Both halves of J-0039 are answered. The upgrade half's change arrived as CR-003, approved at J-0051 with ADR-0015; the model-family half's question was answered by the user at J-0042 with all four families. Serves G-4 and crosses no non-goal. Accepted and already planned as five stories on the 5.9 line.
- Outcome: US-022, US-023, US-024, US-025, US-026 (CR-003, ADR-0015)
- Changes: backlog.md

Closes the evaluation left half-open at J-0039, which was `requires-change` for the
upgrade half and `needs-info` for the model-family half. Both halves are now answered:
the user chose ALL FOUR families at J-0042, and CR-003 was approved at J-0051 with
ADR-0015.

So the request is accepted and already planned. Nothing in the answer crossed a
non-goal. The one reading that had to be settled is recorded rather than re-argued:
structure learning and KTBN order learning estimate a graphical model from the project's
own data, which is the shape of the SVM training US-015 already ships, and not the model
training, hosting or weight shipping NG-4 denies. Each family arrives as a component the
application drives, never as a policy that decides what the agent says (NG-1, P-08) --
which is why every one of the five stories carries that clause in its acceptance.

All five target the 5.9 line: the migration touches C-probabilistic, C-build and
ADR-0009, so P-19 keeps every one of them off the shipped 5.8 line. US-022 is ready;
US-023 to US-026 stay `draft` because no module satisfies them yet, deliberately --
M-probabilistic's responsibility is discrete probabilistic models and influence diagrams
and stretches to neither a causal model, a credal network nor a learner, and the
placement of each is part of its own request.

## J-0058 | 2026-11-15 | ADR-0016 | planner
- Event: decided
- Title: The gate brings its own host project
- Status: accepted
- Story: US-027
- Module: M-dist
- Changes: architecture.md

**Context.** P-04 says nothing is done until its tests pass, and the project has no
mechanism to decide that: `gates.task_completed` is `null`, `gates.quality` is empty,
there is no CI, and the 109-test automation suite needs the Unreal editor. This
repository is a plugin: it contains no `.uproject`, so the suite cannot be run from it
as it stands. The one project on the maintainer's machine that holds the plugin,
`D:\Perforce\FANTASIADevelopment\FANTASIADevelopment.uproject`, lives outside this
repository and is unusable as a gate host for the 5.8 line on three counts, none of them
this plugin's fault: it associates with UE 5.6, which is not installed; it enables
`LowEntryExtStdLib`, which has no 5.8 build; and it carries its own C++ modules, which
would have to be compiled for 5.8 before the editor would boot. None of the three can be
fixed from here.

**Decision.** The gate is a committed script that brings its own host project. It
resolves the engine from `FANTASIA_UE_ROOT`, defaulting to the documented install path,
and refuses to run when the engine's `Build.version` does not match the version the
`.uplugin` descriptors name. It generates, and then reuses, a minimal code project
outside this repository's working tree, which reaches the plugin through the
`.uproject`'s `AdditionalPluginDirectories` and enables FANTASIA only. It compiles the
`FANTASIA` and `FANTASIATests` modules for that project's editor target, boots
`UnrealEditor-Cmd.exe` headless on it, runs `Automation RunTests FANTASIA`, and derives
its exit code from the exported report rather than from the editor's own exit status.
Verification therefore depends on an installed engine and on nothing else outside this
repository.

**Consequences.**

- A reclone plus an installed engine reproduces the gate. One environment variable is
  the whole machine-specific surface, and it has a default.
- The harness enables FANTASIA only, so FANTASIAACE is outside the gate's reach until
  its 5.8 binaries and its NVIDIA dependency exist (F-014, F-016, US-020, US-021). The
  gate says so when it runs, rather than appearing to cover it.
- A gate run recompiles into `FANTASIA/Binaries/Win64/`, which is tracked. That is the
  same artifact P-12 governs, so a dirty working tree after a gate run is expected and
  the refresh-commit discipline is unchanged.
- One clause of the brief's fourth success criterion becomes mechanical for free: the
  gate compares the `.modules` BuildId it emits with the engine's `CompatibleChangelist`,
  which is C-build's last invariant and the error users actually hit.
- The script belongs to M-dist, beside bootstrap and `enable-plugins.ps1`, so `build-dev`
  owns it and `team.json`'s `build-dev.owns` gains the path. That file is the lead's.
- Whether one run is fast enough to gate every task is measured, not assumed: US-027
  records a cold and a warm number and the command follows from them.

**Rejected alternatives.**

- *Take the host project's path from an environment variable and gate on it.* The three
  obstacles above are properties of someone's project, not of this plugin, and a gate
  that fails for reasons outside its subject is a gate nobody runs. It would also make
  verification unreproducible anywhere else.
- *Run the suite against the committed prebuilt DLLs, with no compile step.* The editor
  loads a prebuilt module by BuildId, not by source timestamp, so a stale DLL whose
  BuildId still matches runs old code and reports green. A gate that can pass without
  compiling the change it is gating is worse than no gate, because it is believed.
- *`RunUAT BuildPlugin` alone.* It compiles, which is the hard half, but it runs no tests
  and packages to a directory the editor would not load.
- *Write the procedure in a document and run it by hand.* P-04 asks for a mechanism. A
  documented procedure is a claim, and the project already has those.

## J-0059 | 2026-11-15 | NOTE | planner
- Subject: S-01 sprint plan proposed to the lead
- Goal: Give the project a gate it can run, stop the credential leak, and pin the worst-tested real-time path, so that from this sprint on 'done' is something the project can check rather than claim.
- Stories: US-027, US-028, US-029
- Tasks: T-01 threat model [no-gate] (security); T-02 gate script and measurement (build-dev, US-027); T-03 secret-property test (test-dev, US-028); T-04 Config on UOpenAICompatibleComponent::Config (core-dev, US-028); T-05 Config on URESTTTSComponent::Headers (speech-dev, US-028); T-06 test seam (speech-dev, US-029); T-07 capture characterisation (test-dev, US-029); T-08 contracts and backlog in step [no-gate] (planner)
- Left out: US-020 and US-021 (the 5.8 ship, mostly the user's machine); F-011, F-012 and the Neo4j half of F-007 (P-19 excludes all three from the 5.8 line); gates.quality (no .clang-format exists and choosing one is its own request); measures for NFR-001 to NFR-003
- Changes: backlog.md, architecture.md, verification/build-dev.md, verification/core-dev.md, verification/speech-dev.md, verification/test-dev.md

The plan the lead puts to the user. Sprint length is one working session, so selection is
bounded by that and not by what the backlog wants.

**Proposed goal.** Give the project a gate it can run, stop the credential leak, and pin
the worst-tested real-time path — so that from this sprint on, "done" is something the
project can check rather than claim.

**Stories, in dispatch order.** US-027 (Must, M) the gate; US-028 (Must, S) F-008's fix;
US-029 (Should, M) the capture path pinned. All three are new and all three are `ready`;
the lead sets them `planned` when the user confirms. US-029 is the one to drop if the
session runs short, and its first task is XS, so the drop point is clean.

**Tasks.** T-01 `[no-gate]` threat model (security); T-02 the gate script and its
measurement (build-dev, US-027); T-03 the secret-property test, written failing
(test-dev, US-028); T-04 the `Config` specifier on `UOpenAICompatibleComponent::Config`
(core-dev, US-028); T-05 the same on `URESTTTSComponent::Headers` (speech-dev, US-028);
T-06 the one-line test seam in `WhisperCaptureComponent.h` (speech-dev, US-029); T-07 the
capture characterisation (test-dev, US-029); T-08 `[no-gate]` bring the contracts and the
backlog in step with what landed (planner). T-01, T-02, T-03 and T-06 start together;
T-04, T-05 and T-07 follow; T-08 last.

**How finding fixes attach.** As their own stories, never by reopening the `done` story
the defect lives in. US-005 and US-008 describe behaviour this fix does not change, and
they are true today; reopening them to carry F-008 would make the backlog say the system
does something it does not. So F-008's fix is US-028, F-008 keeps naming US-005 and
US-008 as where the defect sits, and the rule is now written into the backlog's preamble
so the next finding does not get argued again. The gate had no story at all, which is the
same question with no candidate: US-027.

**Three judgements, flagged rather than applied.**

1. *P-19 and the gate.* The bar measures fixes to the shipped line's product surface. A
   test runner changes no contract, no module boundary, no dependency and no
   Blueprint-facing signature, and no user runs it; it does add an ADR, which a literal
   reading of "touches no ADR" could be read to forbid. I read that clause as forbidding
   the amendment of a decision in force, not the recording of a new one about verification
   tooling, and admitted US-027 on the 5.8 line. If the user reads it the other way, the
   sprint has no gate and P-04 stays unenforceable, so this is worth one sentence at
   review.
2. *The test seam.* US-029 reaches private members through one `friend` declaration. The
   alternative was to test only the public surface, which pins the configuration defaults
   and the muted/buffer behaviour but leaves the resampler, the RMS, the high-pass
   coefficients and the noise gate — the real-time arithmetic, which is where a regression
   actually costs something — untested. The seam widens no exported surface and touches no
   contract, so P-19 admits it; a reviewer who dislikes test seams on principle should say
   so now rather than at review.
3. *Effort for the two one-line security fixes.* The method says `high` for a security
   fix. T-04 and T-05 are one specifier and one doc comment each, with the design decided
   here and the test already written by T-03, so `medium` is proposed. The subtlety in
   both is serialisation of a non-scalar property, which the test covers.

**Left out, deliberately.** The 5.8 ship (US-020, US-021): mostly the user's machine, not
agent work. F-011, F-012 and the Neo4j half of F-007: P-19 keeps all three off this line,
and what is exported there already has 36 tests, so a Neo4j characterisation story this
sprint would add almost nothing. `gates.quality`: C++ has no `.clang-format`, and choosing
a format for a tree written by many hands is a request of its own with a very large first
diff; the checker keeps warning until it is made. The nine other open findings. A measure
for NFR-001 to NFR-003: the gate has to exist before a latency budget can be checked by
anything.

## J-0060 | 2026-11-15 | REQ-004 | planner
- Event: evaluated
- Evaluation: accepted
- Serves: G-4
- Principles: Unchanged from J-0057.
- Reason: Correcting the Outcome field of J-0057, which carried a parenthetical the status fold read as a story ID. The verdict and everything else in that entry stand: accepted, serves G-4, planned as five stories on the 5.9 line through CR-003 and ADR-0015.
- Outcome: US-022; US-023; US-024; US-025; US-026
- Changes: none

The substance is at J-0057. This entry exists only so the fold reads clean: CR-003 and ADR-0015 are named in the reason rather than inside the Outcome list, where a comma made 'CR-003' look like a backlog ID.

## J-0061 | 2026-11-15 | S-01 | lead
- Event: opened
- Goal: Give the project a gate it can run, stop the credential leak, and pin the worst-tested real-time path, so that from this sprint on "done" is something the project can check rather than claim.
- Stories: US-027, US-028, US-029
- Changes: sprint.md, backlog.md

The method's order for an adopted project: conformance and characterisation first, the
in-flight work second. The user chose it over the 5.8 ship, which is mostly their machine
rather than the team's — multi-hour NVIDIA plugin builds against 5.8 and a ~5 GB release
upload — and which therefore makes a poor first sprint for a team that has never run.

Three stories, all new, because fixes for findings get their own story rather than
reopening a `done` one: US-005 and US-008 describe behaviour F-008's fix does not change
and are true today, so reopening them would make the backlog claim something false while
the fix is pending. That rule is now in the backlog preamble.

US-027 is the one that matters most. `gates.task_completed` is `null` and
`gates.quality` is empty, so P-04 — nothing is done until its tests pass — has no
mechanism at all, and every story in this project has been verified by hand or not at
all. ADR-0016 records how it is solved: the gate brings its own host project rather than
borrowing the maintainer's. That decision came from three obstacles the lead verified on
the machine, all of them in a project outside this repository's scope — the dev project
declares UE 5.6 while only 5.7 and 5.8 are installed, it enables a plugin with no 5.8
build, and it lists neither FANTASIA nor FANTASIAACE explicitly. Rather than work around
another project's configuration, the gate stops depending on it. Running the suite
against the committed prebuilt DLLs was considered and rejected: the editor loads a
prebuilt module by BuildId, not by source timestamp, so a stale DLL would report green on
code that was never compiled.

US-028 fixes F-008, a live credential leak: two missing `Config` specifiers mean API keys
are written into committed Blueprint and actor asset defaults. It is admissible on the
5.8 line by the user's own decision on CR-001, and the user settled the one ambiguity it
raised — P-19's "touches no ADR" clause forbids amending a decision in force, not
recording a new one about verification tooling, so US-027 is admissible too. Without that
reading the sprint would have had no gate and P-04 would have stayed unenforceable for
the whole 5.8 line.

US-029 pins the microphone capture path, F-007's larger half and the largest file in the
project at 1250 lines, untested. It is `Should` and it is the designated drop if the
session runs short; its first task is XS so the drop point is clean. The Neo4j half waits:
P-19 excludes F-012's fix from the 5.8 line, and without it the Bolt message layer cannot
even be linked from the test module.

No budget this sprint, by the user's choice: there is no prior data to size one from, and
the cost report afterwards will give real medians per agent and effort for S-02.

Ownership is in observe mode, so this sprint is also the observation window the adoption
named: one sprint with no entry in `.ateam/ownership.jsonl` is the evidence for the change
request that would switch `observe_only` off.

## J-0062 | 2026-11-15 | DEC-001 | speech-dev
- Module: M-asr
- Decision: UWhisperCaptureComponent declares 'friend struct FWhisperCaptureTestAccess;' at the head of its private section, as the only test seam for US-029.
- Reason: The real-time arithmetic worth pinning (resampler, RMS, high-pass coefficients, noise gate) is private and not exported; a friend declaration reaches it without adding reflected, exported or Blueprint-facing surface, so C-asr is unchanged and P-19 holds.
- Changes: none

Alternatives rejected: (a) public-surface-only tests, which would pin only config defaults and muted/buffer behaviour and leave the DSP untested; (b) a narrow internal accessor or FANTASIA_API on the DSP helpers, which widens the module's surface for testing alone and, on a Blueprint-facing component, is exactly what P-19 keeps off the 5.8 line.

## J-0063 | 2026-11-15 | DEC-002 | test-dev
- Module: M-tests
- Decision: SecretConfigTests: ConfigFlag test reads CPF_Config off a table of six class/property rows; round-trips write to in-memory GGameIni via a scoped RAII entry, call LoadConfig on a transient instance, restore/remove the key, never flush.
- Reason: Reflection reads the flag directly so the test cannot pass on a value that happened to be set; LoadConfig is what exercises the struct/map serialisation F-008 is about; no file write keeps DefaultGame.ini untouched (P-15).
- Changes: none

Alternatives rejected: checking only inner struct fields (passes today, proves nothing); writing to a temp ini file (extra I/O and path handling); expected-failure marker (UE simple tests have none, so the test fails plainly until T-04/T-05 land).

## J-0064 | 2026-11-15 | DEC-003 | core-dev
- Module: M-llm
- Decision: UOpenAICompatibleComponent::Config carries the Config UPROPERTY specifier; its doc comment states the value comes from DefaultGame.ini and that an asset that already holds a key must be re-saved.
- Reason: LoadConfig walks the class's own Config properties and never descends into a non-Config struct property, so the specifiers on FOpenAICompatibleConfig's inner fields never fired and APIKey was stored in the owning asset's defaults (F-008). Adding Config on the owning property matches every other secret-bearing property in the plugin and touches no contract, so it stays inside what P-19 admits on 5.8.
- Changes: none

Alternatives rejected: flattening the struct into individual Config properties on the component (retypes a Blueprint-facing property, invalidates compiled assets, excluded by P-19); clearing the key in PostLoad or on save (hidden behaviour, silently drops a value the user typed, and still leaves it in assets already committed). Name, type, Category and EditAnywhere/BlueprintReadWrite are unchanged. The specifier does not retroactively clean an asset already saved with a key, which is why the tooltip says so.

## J-0065 | 2026-11-15 | F-017 | test-dev
- Event: opened
- Kind: bug
- Severity: minor
- Story: US-029
- Module: M-asr
- Repro: ResampleAndMixToMono reads one frame past its input: 44100 Hz mono, 4410 frames per callback, phase 0. The loop condition is Phase < SrcEnd in double, but the index is FloorToInt(static_cast<float>(Phase)); Phase = 4409.99999999 rounds to 4410.0 in float, so Idx0 == NumFrames and InAudio[Idx0*NumChannels+ch] is out of bounds. The last output sample is that garbage (Frac is about -1e-12, so Lerp returns S0). Pinned by FANTASIA.WhisperCapture.Resample.ReadsOneFramePastInput.
- Changes: none

Found while characterising US-029. The over-read happens once per callback whenever the carried phase lands within one float ulp below NumFrames; with 441-frame callbacks (10 ms at 44.1 kHz) it does not occur, with 4410-frame callbacks it occurs on every callback (simulated 1000 of 1000). Effect is one wrong sample (a click) per hit, and undefined behaviour; a crash needs the buffer to end at a page boundary. Fix belongs to speech-dev: index with FloorToInt on the double Phase and clamp Idx0 to NumFrames-1. Not fixed here; the test pins the current output (the guard sentinel reaches the output) and must be updated with the fix. Found by simulation of the arithmetic and a sentinel guard frame in the test; not observed under a sanitizer.

## J-0066 | 2026-11-15 | F-018 | test-dev
- Event: opened
- Kind: quality
- Severity: minor
- Story: US-029
- Module: M-asr, M-tests
- Repro: US-029 asks for StartCapture to be driven with an unopenable device and to return false. No such input exists: OpenCaptureDevice treats any AudioDeviceIndex outside the enumerated range (negative or too large) as the system default and opens it (WhisperCaptureComponent.cpp, device resolution after enumeration). On a machine with a microphone StartCapture(index 9999) opens the real device and returns true; on one without it returns false.
- Changes: none

The acceptance criterion conflicts with the same story's constraint that the file opens no audio device. T-07 therefore does not call StartCapture against a device; it pins the already-capturing early return, which is the only false path reachable without opening hardware. The failure path on a real missing device is unpinned. Closing it needs an injectable capture factory in M-asr (a seam beyond the one friend declaration) or a story change; planner to decide.

## J-0067 | 2026-11-15 | F-019 | test-dev
- Event: opened
- Kind: quality
- Severity: minor
- Story: US-029
- Module: M-asr
- Repro: Two comments in M-asr disagree with the code. (1) WhisperCaptureComponent.h says the high-pass state is Direct Form II Transposed; ApplyNoiseSuppression implements Direct Form I (keeps X1,X2,Y1,Y2). (2) ResampleAndMixToMono is documented and named as mixing to mono, but it selects the loudest channel per tap (signed value of the larger magnitude) and interpolates S0 and S1 independently, so two channels of opposite sign are not summed and S0 and S1 may come from different channels. Also ComputeRMS accumulates in float: a steady 0.1 signal over 30 s (480000 samples) reads about 0.09987 in simulation, an error of 1.3e-4, above the 1e-4 US-029 allows for short buffers.
- Changes: none

All three are pinned as they are by the FANTASIA.WhisperCapture tests (Resample.LoudestChannelWins, Rms.LongBufferStaysWithinOneThousandth). The loudest-channel rule is intentional per the source comment, so only the name and header text are in question. The RMS figure is from a sequential float simulation; the compiler may vectorise the loop and do better, which is why the test allows 1e-3.

## J-0068 | 2026-11-15 | DEC-004 | speech-dev
- Module: M-tts
- Decision: URESTTTSComponent::Headers carries Config, and its doc comment states that the value is read from the host project's DefaultGame.ini and that an asset which already holds a key must be cleared and re-saved for the key to leave it.
- Reason: F-008/US-028: Headers is where an xi-api-key goes, and without Config the map is stored in the owning Blueprint or actor asset's defaults and committed with the user's project. The class is already Config = Game and the neighbouring URL and BodyTemplate already carry Config, so the specifier alone closes the leak for new configuration; it does not clean an already-saved asset, which is why the tooltip has to say so.
- Changes: none

Name, type, Category and EditAnywhere, BlueprintReadWrite are unchanged, so no compiled Blueprint asset is invalidated and the change stays inside what P-19 admits on the 5.8 line (C-rest-tts's surface is untouched). Alternatives rejected: adding a separate Config FString ApiKey property beside Headers (new Blueprint-facing surface, changes C-rest-tts, and leaves the leaking property in place); stripping the key from Headers at load time (hides the asset's contents instead of fixing where the value is read from); relying on the specifier alone without the tooltip note (reports the fix as complete while existing assets still carry the key).

## J-0069 | 2026-11-15 | F-020 | security
- Event: opened
- Story: US-014
- Module: M-neo4j
- Kind: security
- Severity: blocker
- Repro: git grep -n 'SSL_CTX_set_verify|default_verify_paths|load_verify|SSL_set1_host|SSL_get_verify_result' -- FANTASIA/Source FANTASIAACE/Source returns nothing. FANTASIA/Source/FANTASIA/Private/Neo4jComponent.cpp:322-355 builds the TLS chain (SSL_CTX_new(TLS_client_method()), BIO_new_ssl_connect, SSL_set_mode, SSL_set_tlsext_host_name) and :389 BIO_do_handshake; no verify mode, no trust store, no hostname check, and the handshake result is never inspected beyond <= 0.
- Changes: none

The TLS transport accepts any certificate from any server.

OpenSSL's client-side default for a fresh SSL_CTX is SSL_VERIFY_NONE, and hostname
verification is never automatic: it needs SSL_set1_host or an X509_VERIFY_PARAM. The
component sets neither, loads no trust store, and never calls SSL_get_verify_result. The
only TLS-related calls are SSL_MODE_AUTO_RETRY and the SNI hostname, which is sent to the
server and proves nothing about it.

So every connection the component believes is encrypted is open to an active
man-in-the-middle: an attacker who can answer for the host (DNS, ARP, a hostile network, a
rogue Aura-looking endpoint) completes the handshake with a self-signed certificate and
reads and rewrites the whole session. The first thing on that session is the LOGON message
carrying 'user' and 'password' in cleartext inside the TLS tunnel
(Neo4jComponent.cpp:663, :683), followed by every Cypher query and every row returned.

This defeats the distinction the Neo4j URL schemes exist to make: 'neo4j+s://' and
'bolt+s://' mean full certificate verification and '+ssc' means self-signed accepted, and
C-neo4j's invariant says the scheme decides TLS. Both schemes behave as the weaker one, so
a user who follows Neo4j's own documentation gets no authentication of the server at all.

This is a separate defect from F-011. F-011 is about choosing plaintext when the user asked
for TLS; this is about the TLS path itself providing no server authentication, so even a
correctly configured 'neo4j+s://' Aura connection is unprotected against an on-path
attacker.

The fix is three calls in the TLS branch -- SSL_CTX_set_default_verify_paths (or the
engine's CA bundle), SSL_CTX_set_verify(SSL_VERIFY_PEER) and SSL_set1_host(Hostname) -- plus
a refusal to connect when verification fails, with '+ssc' as the documented way to opt out.
That adds a property-visible behaviour (a connection that used to succeed will now fail
against a self-signed server), so it changes C-neo4j and P-19 keeps it off the 5.8 line
exactly as it keeps F-011 off. That exclusion is the user's to accept, not mine: it leaves a
credential-exposing path open for the life of the line.

Reproduction without a network: the grep above is the evidence that no verification is
configured. End to end: point 'endpoint' at 'bolt+s://localhost' with a stunnel or mitmproxy
holding a self-signed certificate in front of a Neo4j server; the component completes the
handshake, authenticates, and the proxy prints the password.

## J-0070 | 2026-11-15 | F-021 | security
- Event: opened
- Story: US-014
- Module: M-neo4j
- Kind: security
- Severity: blocker
- Repro: FANTASIA/Source/FANTASIA/Private/BoltPackStream.cpp:169-207. ReadByte does Data.GetData()[Cursor++], ReadUInt16BE/ReadInt32BE/ReadInt64BE index Data.GetData()+Cursor, and ReadStringOfLength(Len) reads Len bytes from Cursor -- none of them compares against Data.Num(). Reachable from the network through ParseBoltServerMessage (BoltProtocol.cpp:329-333), which hands the dechunked message straight to FBoltPackStreamReader::ReadValue. From the test module, which can link this header: FBoltPackStreamReader R(TArray<uint8>{0xD2,0x7F,0xFF,0xFF,0xFF}); R.ReadValue(); reads a 2 GB string from a 5-byte buffer.
- Changes: none

The PackStream reader trusts every length and count in a message that came off the socket.

Four distinct ways a server (or, given F-020, anyone on the path) turns a Bolt message into
memory unsafety or exhaustion inside the editor or game process:

1. **Out-of-bounds read, attacker-chosen length.** Markers 0xD0/0xD1/0xD2 (string) and
   0xCC/0xCD/0xCE (bytes) read an 8-, 16- or 32-bit length and then copy that many bytes
   from the cursor. Nothing clamps it to the remaining buffer. A 5-byte message
   {0xD2, 0x7F,0xFF,0xFF,0xFF} asks for a 2 GB string. The bytes read are whatever follows
   the message buffer on the heap, and they are returned as the value: for a RECORD field
   they end up in a UNeo4jResultCellSimple and are handed to Blueprint, so this is an
   information-disclosure primitive as well as a crash.
2. **Unbounded recursion.** ReadValue calls itself for every list, map and structure
   element. A message of N repeated 0x91 bytes is N nested one-element lists, one stack
   frame per byte, and messages are assembled across chunks so N is not bounded by the
   16 KB chunk size. The result is a stack overflow -- an unrecoverable crash on Windows.
3. **Unbounded allocation.** List8/16/32 and Map8/16/32 call Reserve(S) with S read from
   the message before a single element exists, so a 5-byte message reserves gigabytes.
   32-bit lengths are read as signed int32, so a negative count also reaches Reserve and
   Append.
4. **No cap on message size.** FBoltDechunker::ProcessBuffer (BoltProtocol.cpp:27-55)
   accumulates chunks into CurrentMessageChunks until a zero-length terminator arrives,
   with no limit, so a server that never terminates a message grows the buffer until the
   process dies.

C-neo4j states the opposite of (1): 'ReadValue past the end of the buffer returns a null
value; HasMore() is the caller's guard (pinned by Reader.HasMoreEmpty)'. That test
(BoltPackStreamTests.cpp:484-492) only calls HasMore() on an empty buffer; it never calls
ReadValue, and ReadValue's first act is an unchecked ReadByte. The contract asserts a
mitigation that does not exist, which is why 36 passing codec tests have not caught this:
every one of them feeds the reader bytes the writer produced.

The fix is a bounds check in ReadByte and the multi-byte readers, a remaining-bytes clamp in
ReadStringOfLength and the bytes case, a reject-not-reserve rule for counts larger than the
remaining buffer, a depth limit in ReadValue, and a maximum message size in the dechunker.
It changes C-neo4j's error behaviour, so P-19 keeps it off the 5.8 line unless the user
decides otherwise.

The exported half of this codec is reachable from FANTASIATests, so each of the four has a
cheap unit reproduction and belongs in FANTASIA/Source/FANTASIATests/Private/Security/.

## J-0071 | 2026-11-15 | F-022 | security
- Event: opened
- Story: US-001
- Module: M-asr
- Kind: security
- Severity: major
- Repro: FANTASIA/Source/FANTASIA/Private/WhisperSubsystem.cpp:623-712 (UWhisperSubsystem::LoadWavFile). DataSize is the data chunk's declared size (line 641) and is never compared with FileData.Num() - DataOffset; the sample loop at :683-690 reads DataSize/2 int16s from DataOffset. A 44-byte-plus-minimal WAV whose data chunk declares 0x7FFFFFF0 bytes makes it read 2 GB past the mapped file. NumChannels (line 657) is used as a divisor at :680 with no zero check, and SampleRate (line 658) as a divisor at :695. The chunk walk at :629-647 advances by Offset += 8 + ChunkSize with ChunkSize read as signed, so a chunk declaring size -8 leaves Offset unchanged and the loop never terminates.
- Changes: none

Every field of the WAV header is trusted, including three that are used as a length and two
as divisors.

TranscribeAudioFile is BlueprintCallable and takes any path, so the untrusted input is a
file: a recording a player supplied, a file a mod dropped in, anything an application lets
a user choose. Four failures from one malformed or hostile file:

- **Heap over-read of up to 2 GB.** The declared data-chunk size is used as the sample
  count with no clamp to the bytes actually present. The samples read past the end are
  converted to floats, resampled, and handed to whisper for transcription, so adjacent heap
  memory can come back as transcript text -- an information-disclosure path, not only a
  crash.
- **Division by zero** when NumChannels is 0 (NumFrames = NumSamplesTotal / NumChannels)
  and again when SampleRate is 0 (Ratio = SampleRate / 16000.0 is 0, OutCount = NumFrames /
  0.0 is inf, and the cast of inf to int32 is undefined before SetNumUninitialized sees it).
- **Infinite loop** in the chunk walk on a negative declared chunk size: the file is read on
  a background task, so the symptom is a worker thread spinning forever and a transcription
  that never completes, not an immediate crash.
- **Negative sizes** reaching SetNumUninitialized when the declared data size is negative.

None of this is caught by a test: C-asr's error contract says a failed transcription comes
back through FWhisperResult with bSuccess false, and these inputs do not take that path.
The fix is local to LoadWavFile -- clamp DataSize to the remaining bytes, reject
NumChannels == 0 and SampleRate == 0, require ChunkSize > 0 in the walk, and fail through
FWhisperResult as the contract says -- and it changes no signature, so by CR-001's bar it is
admissible on the 5.8 line.

Note that the same file is the one place M-asr parses untrusted bytes: the live microphone
path carries no attacker-chosen lengths, and F-017 (the one-frame over-read in the
resampler) is a separate, much smaller defect in the capture path.

## J-0072 | 2026-11-15 | F-023 | security
- Event: opened
- Story: US-005
- Module: M-tts
- Kind: security
- Severity: major
- Repro: FANTASIA/Source/FANTASIA/Private/RESTTTSComponent.cpp:458-499 (TryParseWavHeader) reads NumChannels at offset 22 and SampleRate at offset 24 out of the HTTP response body and returns them unvalidated; :105-112 passes both to ResampleTo16kHz, where :507-508 computes FrameSize = 2 * NumChannels and divides NumBytes by it (division by zero when a response declares 0 channels) and :515 divides by SourceRate (division by zero when a response declares rate 0 -- rate 0 passes the != 16000 test at :107). The chunk walk at :484-492 advances Pos += 8 + ((ChunkSize + 1) & ~1) with ChunkSize read as signed int32, so a chunk declaring size -8 leaves Pos unchanged and the loop never terminates, on the thread that handles the HTTP response.
- Changes: none

The TTS response body is parsed as a WAV container with no validation of the fields that
become divisors or loop increments.

The input is a synthesis response. For the shipped ElevenLabs path that is an HTTPS
response from api.elevenlabs.io, so the attacker has to be the provider or break TLS. But
URESTTTSComponent is the generic base and its URL is a Config string with no scheme
restriction, so an application pointed at a local or http:// TTS service -- which the
component exists to support -- takes these bytes from anyone on the path.

Three outcomes: a crash on 0 declared channels, a crash on a declared sample rate of 0, and
a hang on a negative declared chunk size. The hang is the worst of them, because it happens
in the HTTP completion path rather than on a worker that can be abandoned, and the editor or
game simply stops.

Also in the same function and worth fixing with it: the fmt fields are read with
reinterpret_cast at fixed offsets 22, 24 and 34 rather than from the fmt chunk the walk
locates, so a file whose fmt chunk is not first is misparsed; and the fallback when no data
chunk is found assumes the classic 44-byte header.

The fix is local -- reject NumChannels == 0, reject SampleRate <= 0, require ChunkSize > 0,
and treat a failed parse as 'not a WAV' which the function already has a return path for --
and changes no signature, so by CR-001's bar it is admissible on the 5.8 line. The same
numbers are what the PCM16 decoder tests already cover for the headerless path; this is the
header path, which has none.

## J-0073 | 2026-11-15 | F-024 | security
- Event: opened
- Story: US-010
- Module: M-prolog
- Kind: security
- Severity: major
- Repro: FANTASIA/Source/FANTASIA/Private/SWIPrologThread.cpp:331-357 (executeQueryFromString) parses the caller's string with PlCompound and runs it as a goal through PlQuery with no sandbox check; :429-440 (ConsultString) asserts the caller's string as a clause through assertz; :86-96 (openPrologFile_) consults a caller-supplied path, which executes that file's directives. sandbox:safe_goal appears nowhere in the tree (git grep -n safe_goal returns nothing). With the plugin's default SWI-Prolog runtime, USWIPrologComponent::SWIPLqueryString(TEXT("shell('calc.exe')")) starts a process; the conjunction form ",(shell('calc.exe'),true)" works the same way because the parsed functor and its arguments are passed straight to PlQuery.
- Changes: none

Three Blueprint-callable entry points take a string and run it as Prolog, which in
SWI-Prolog means arbitrary code and arbitrary processes: shell/1, process_create/3, open/3,
delete_file/1, use_module/1 and the rest of the standard library are all in scope. There is
no sandbox (sandbox:safe_goal), no predicate allow-list and no distinction between a
query and a side effect.

That is what an embedded Prolog engine is for, and I am not reporting that the feature
exists. The finding is that nothing in the project says so and nothing stands between those
entry points and untrusted text:

- C-prolog documents SWIPLqueryString, SWIPLfindAll, SWIPLconsultString and openPrologFile
  as ordinary calls. Its Errors section covers an unparseable goal and a missing file. It
  nowhere states that a string reaching any of them is executed with the engine's full
  privileges, so an integrator has no way to learn it from the contract.
- FANTASIA is a conversational-agent plugin. Its own walking skeleton pipes model output
  through the application, M-llm exists to deliver that output, and the natural next step --
  'let the agent query the knowledge base' -- turns a model's reply into a goal. Within this
  repository nothing wires the two together (FANTASIATestActor uses literals only, and the
  comment at FANTASIATestActor.cpp:400 shows the author knew the string is executed
  directly), so the chain is the host project's to create, which is exactly why the contract
  has to warn about it.
- openPrologFile is worse than it looks, because consulting a file runs its directives:
  a ':- initialization(shell(...))' in a .pl file executes on load. The path is not confined
  to any root and no extension is checked, so a path from a model, a downloaded mod or a
  user-chosen file is code execution.

What would mitigate it, in order of cost: state the contract (a string reaching these four
is executed; untrusted text must not); offer a safe form that checks sandbox:safe_goal
before running, and say in the doc comment which to use when; confine openPrologFile under
the working directory the engine already sets
(ProjectDir/Resources, SWIPrologThread.cpp:224-229).

All three change C-prolog, so P-19 keeps them off the 5.8 line; the documentation half
should not wait for 5.9, since it costs nothing and is what an integrator needs.

## J-0074 | 2026-11-15 | F-025 | security
- Event: opened
- Story: US-012
- Module: M-probabilistic
- Kind: security
- Severity: major
- Repro: FANTASIA/Source/FANTASIA/Private/BayesianNetwork.cpp:148-152 (UBayesianNetwork::setBN) constructs gum::BIFReader and calls reader.proceed() with no try/catch and without checking the returned error count; grep -n 'try\|catch' BayesianNetwork.cpp shows catches only for gum::NotFound, none around setBN. A BIF file with a syntax error makes aGrUM throw gum::SyntaxError (or gum::IOError for an unreadable file) out of a BlueprintCallable UFUNCTION; exceptions are enabled in this module and the engine installs no handler, so the process terminates.
- Changes: none

setBN is the one place M-probabilistic reads a file, and it is the one aGrUM call in the
module that is not wrapped.

C-probabilistic states the opposite: 'aGrUM exceptions are caught at the boundary and
logged; they do not propagate into Blueprint code. Exceptions are enabled in this module for
exactly that reason.' That is true of the inference and build calls, which catch
gum::NotFound. It is not true of the reader, and the reader is the only entry point whose
input is a file rather than a Blueprint value -- so the one untrusted-input path is the one
with no handler, and the contract asserts a mitigation that is absent there.

Consequences, with a model file as the input (a scenario file shipped with a game, a file a
user or a mod supplies, a path a model chooses):

- A malformed or truncated BIF file terminates the editor or the game rather than logging.
- gum::NotFound is not the only exception the reader raises, so catching the one type the
  rest of the module catches would not be enough; the catch has to be gum::Exception (and
  for safety '...'), with the node-rebuild loop that follows only running on success.
- proceed()'s return value is the parse error count and is dropped, so a file that parses
  with errors is silently treated as a loaded network and the rest of setBN walks a
  half-built bn.

The same applies in reverse for the writers: writeBIF (BayesianNetwork.cpp:124-127) and
writeBIFXML (InfluenceDiag.cpp:332-335) hand a caller-supplied path to aGrUM with no
try/catch, no confinement to any root and no extension check, so they are an
arbitrary-file-write primitive when the path is not a literal, and an unhandled throw when
the path is unwritable.

The fix is a try/catch and a return-value check inside setBN plus the same around the two
writers. It changes no signature and no stated behaviour except turning a crash into the
logged failure the contract already promises, so by CR-001's bar it is admissible on the 5.8
line. US-022 rewrites this module for aGrUM 3.x and must not be waited for: the migration
moves the same unguarded call.

## J-0075 | 2026-11-15 | DEC-005 | test-dev
- Module: M-tests
- Decision: Capture characterisation drives only the private DSP steps and the device-free controls through FWhisperCaptureTestAccess (forwarding only), never StartCapture against a device; unreachable-by-API states are limited to bIsCapturing=true, which a successful start produces.
- Reason: Out-of-range AudioDeviceIndex falls back to the OS default, so no input makes StartCapture fail without risking opening the real microphone (F-018); US-029 forbids opening a device. Over-reads of the resampler input are pinned with a sentinel guard frame so the test itself stays in bounds (F-017).
- Changes: none

Alternatives rejected: calling StartCapture with index 9999 (opens the real mic where one exists, passes or fails by machine); an expected-failure marker for F-017 (the test pins current output and passes, and is updated by the fix); computing expected DSP output with a copy of the algorithm in the test (production logic in a helper): literals from an independent float32 simulation are used instead, with 1e-6 to 2e-6 tolerance.

## J-0076 | 2026-11-15 | F-026 | security
- Event: opened
- Story: US-018
- Module: M-dist
- Kind: security
- Severity: major
- Repro: FANTASIA/bootstrap.bat:143,168,201,242,262,282,302 and FANTASIA/bootstrap.sh:86,108,133 fetch every asset with 'curl -L --fail' and extract it with tar; grep -n 'sha256|shasum|certutil|Get-FileHash' over both scripts returns nothing, so no digest is computed or compared. bootstrap.bat:212 extracts the build-deps archive with tar -xf into %SCRIPT_DIR%. (the plugin's own tracked directory) and :176 extracts the GPU archive into %BIN_DIR% (FANTASIA/Binaries/Win64), which is where WhisperBackend.cpp:100-103 then calls FPlatformProcess::GetDllHandle on fantasia_whisper_cuda.dll. bootstrap.bat:47 and bootstrap.sh:31 build RELEASE_BASE from https://github.com/antori82/FANTASIA while the repository is now antori/FANTASIA, so every download depends on a GitHub rename redirect for a username nobody holds.
- Changes: none

About 5 GB of archives are fetched and unpacked into the user's project with no integrity
check of any kind, and one of the destinations is a directory the editor loads a DLL from.

What the install trusts today: HTTPS to github.com, and a redirect. Nothing else. No digest,
no signature, no manifest of expected sizes, no verification that the asset came from the
release the script named. 'curl -L' follows redirects without restricting the protocol
(--proto '=https'), so a redirect to http:// would be followed too.

Why the destinations matter:

- The GPU archive lands in FANTASIA/Binaries/Win64/, and M-asr later calls GetDllHandle on
  fantasia_whisper_cuda.dll from exactly that directory. An attacker who can serve that one
  archive has code execution in the editor process, with no user action beyond 'Backend =
  GPU'.
- The build-deps archive is extracted over the plugin's own directory, so its entries can
  replace tracked files -- the bootstrap scripts themselves, enable-plugins.ps1 (which
  bootstrap then runs with -ExecutionPolicy Bypass), a .Build.cs, or a committed DLL under
  Binaries/. No path traversal is needed for this; the archive only has to name the files it
  wants to overwrite.
- The Whisper model (about 574 MB) is parsed by whisper.cpp/ggml, a binary-format parser in
  C, with no checksum in front of it.

The owner-name half is a smaller but real exposure: the URLs name 'antori82', the account
has been renamed to 'antori', and GitHub keeps the old path working only by redirect. If
'antori82' were ever registered by someone else, that account could publish a repository
named FANTASIA with a release tagged runtime-v3.0 and the redirect would stop applying,
which turns every future bootstrap run into a download from a stranger. This is the same
line F-014 is about, and the two should be fixed in one pass: F-014 says the assets named do
not exist, this says that even when they do, nothing proves what arrived.

The fix is a SHA-256 manifest committed beside the scripts and checked after each download
(certutil -hashfile on Windows, shasum -a 256 on the others), the URLs rebuilt from the
current owner name, --proto '=https' on curl, and extraction into a staging directory that
is checked before anything is moved into Binaries/ or the plugin tree. This is C-install's
and C-build's territory, so it is a contract change by CR-001's bar -- but the hash check
alone adds no flag, breaks no documented behaviour and keeps idempotence, so it is worth
asking the user whether it is admissible on the 5.8 line.

## J-0077 | 2026-11-15 | F-027 | security
- Event: opened
- Story: US-008
- Module: M-llm
- Kind: security
- Severity: minor
- Repro: FANTASIA/Source/FANTASIA/Private/OpenAICompatibleComponent.cpp:178-182: Choices[0]->AsObject()->GetObjectField(TEXT("message")) -- AsObject() returns an invalid TSharedPtr when choices[0] is not an object, and the member call then dereferences null. A response body of {"choices":[1]} with HTTP 200 crashes the process. Same shape at :243-245 for the streaming path, where Choices[0]->AsObject() feeds TryGetStringField. Reproducible against any local OpenAI-compatible stub (MakeOllamaConfig points at http://localhost:11434 by default) returning that body.
- Changes: none

The response of an OpenAI-compatible endpoint is parsed with the right tool -- the JSON
deserialiser, not a regex over the body -- and the request is built with the serialiser, so
there is no injection here. What is missing is shape validation before dereferencing.

GetArrayField and GetStringField on a missing or wrongly typed field are benign in UE
(they log and yield an empty value), which is why the parser mostly degrades gracefully. The
exception is Choices[0]->AsObject(), which returns an invalid shared pointer for a non-object
element and is then used without a check, in both the non-streaming and the streaming path.
A single malformed element crashes the game.

Who can send it: the configured endpoint. For api.openai.com over HTTPS that is the
provider. But three of the five presets (Ollama, LM Studio, vLLM) default to http:// on
localhost and the URL is a free string, so a misbehaving local server, a stale proxy or
anyone on the path of an http:// endpoint qualifies. C-llm already says a caller must not
assume a reply per request; it should also say that a reply is validated before use.

The fix is a null check on each AsObject() and an early return, in both paths. No signature
changes, so it is admissible on the 5.8 line by CR-001's bar.

Related and reported here rather than separately: the error path at :137-139 logs the entire
response body at Error level. Provider error bodies are not expected to echo credentials,
but they do carry the prompt's subject matter in some cases, and the log is what users paste
into bug reports.

## J-0078 | 2026-11-15 | F-028 | security
- Event: opened
- Story: US-010
- Module: M-prolog
- Kind: security
- Severity: minor
- Repro: FANTASIA/Source/FANTASIA/Private/SWIPrologThread.cpp:14 and :66-70 -- Runnable is a file-scope static and setup() returns the one instance to every caller, so every USWIPrologComponent in the process shares one engine, one knowledge base and one SolutionAvailable multicast. SWIPrologComponent.cpp:72-79 subscribes each component to that shared delegate and :48-50 broadcasts ThreadHandle->currentSolution, which is also shared, so a query submitted by component A fires component B's SolutionAvailable with A's bindings. SWIPrologThread::Shutdown() is declared at SWIPrologThread.h:231 and defined at SWIPrologThread.cpp:58-63 and called from nowhere (git grep -n 'SWIPrologThread::Shutdown|Shutdown()' over the source finds only the declaration and definition), so the engine and its clauses outlive EndPlay and persist across PIE sessions.
- Changes: none

One process-wide Prolog engine is a defensible design: SWI-Prolog is not re-entrant and
C-prolog says one engine per component runs on its own thread. The code does not implement
that sentence -- it implements one engine for all components -- and three consequences follow
that nothing in the contract prepares an integrator for.

- **Cross-component disclosure.** Every component receives every other component's
  solutions, because the subscription and currentSolution both live on the shared thread
  object. Two agents with separate knowledge bases are not separate: one NPC's query result
  is delivered to the other's Blueprint graph.
- **Cross-component tampering.** The knowledge base is shared, so a retract or an assert
  from any component (including one in a Blueprint the project author did not write, or
  another plugin that uses FANTASIA) changes what every other component proves.
- **State that outlives the session.** Shutdown() is never called, so clauses asserted in
  one PIE run are still asserted in the next and across level changes. In the editor that
  makes behaviour depend on what was run before, and it keeps whatever a session asserted --
  including anything derived from user input -- in memory for the editor's lifetime.

In a single-player game every Blueprint is equally trusted, so the practical severity is
low, and the primary face of this is correctness rather than attack. It is logged as
security because it is an information flow between instances that the contract says does not
exist, and because the fix (per-component engines are impossible; per-component result
routing and a real shutdown are not) is the kind of thing that gets harder the more
applications rely on the current behaviour.

Minimum fix without touching the contract's words: route solutions to the component that
submitted the query (tag each command with its submitter and broadcast only to it) and call
Shutdown from the module's ShutdownModule. Making the knowledge base per-component would be
a contract change and probably a module redesign, and C-prolog's sentence about one engine
per component should be corrected either way.

## J-0079 | 2026-11-15 | F-029 | security
- Event: opened
- Story: US-008
- Module: M-demo
- Kind: security
- Severity: major
- Repro: FANTASIA/Source/FANTASIA/Public/FANTASIATestActor.h:93-94 (LLMAPIKey), :109-110 (Neo4jPassword) and :123-124 (ElevenLabsKey) are declared UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=...) with no Config specifier -- compare UElevenLabsTTSComponent::Key and UNeo4jComponent::password, which carry it. FANTASIATestActor.cpp:357, :382 and :461 copy them into the live components (LLMComponent->Config.APIKey = LLMAPIKey, Neo4jComponent->password = Neo4jPassword, ElevenLabsComponent->Key = ElevenLabsKey). AFANTASIATestActor is compiled into the runtime module (F-004), so it is placeable in any user level, and a value typed into its details panel is serialised into the .umap or actor asset.
- Changes: none

F-008's failure mode, a third time, in a module its six-row table does not reach.

AFANTASIATestActor exists so that an integrator can exercise every component by hand from
one actor. To do that against OpenAI, ElevenLabs or Neo4j they have to type a real API key
and a real database password into the actor's details panel -- that is the actor's designed
use, not a misuse. None of the three properties carries Config, so the values are stored in
the level or actor asset's defaults and committed with the user's project. The actor ships
inside the runtime module, so it is present in every build, not only in a test
configuration.

This is worse than F-008's two properties in one respect: those two are on components a
careful integrator may never set by hand, whereas this actor cannot be used at all without
pasting live credentials into an asset.

Adding Config here is the obvious fix and probably the wrong one: a diagnostic actor reading
the project's real keys out of DefaultGame.ini is not clearly better, and the actor should
not be in the runtime module in the first place (F-004). The options, for the planner and
the user to choose between: move the actor out of the runtime module and keep the properties
as they are; keep the actor and mark the three Config like every other secret-bearing
property; or drop the three properties and have the actor read the components' own
configured values, which is what an exercise of the shipped path should do anyway.

Whichever is chosen, SecretConfigTests.cpp's table should grow to cover every
secret-bearing property in the plugin rather than the six in the two components: today it
names six, the plugin declares ten (those six, UACEElevenLabsTTSComponent::Key which does
carry Config, and these three which do not).

## J-0080 | 2026-11-15 | NOTE | security
- Subject: threat model
- Changes: threat-model.md

Threat model written (T-01), covering all twelve modules, from the architecture, the twelve contracts and the code.

Eight trust boundaries. The three that carry real exposure: the Neo4j server to M-neo4j (credentials out, unchecked bytes in, no server authentication); untrusted text to the plugin's string sinks, which no contract mediates because the integrator creates the flow; and the GitHub release assets to the user's project tree, which nothing authenticates.

Ten findings opened from this pass: F-020 and F-021 (blockers, M-neo4j: TLS verifies no certificate or hostname; the PackStream reader bounds-checks nothing on network bytes), F-022 (M-asr WAV loader), F-023 (M-tts WAV header in the HTTP response path), F-024 (M-prolog's string entry points are unsandboxed code execution and the contract does not say so), F-025 (M-probabilistic's BIF reader has no exception handler, contradicting C-probabilistic), F-026 (bootstrap verifies no checksum on ~5 GB extracted into the plugin tree and the DLL directory), F-029 (three credential fields on the demo actor, F-008's leak a third time), F-027 and F-028 (minor).

Three contracts assert a mitigation the code does not implement: C-neo4j on ReadValue past the end of the buffer, C-probabilistic on aGrUM exceptions at the boundary, and C-prolog on one engine per component. Those three sentences are the most dangerous text in the design, because a reviewer who reads them stops looking.

Nothing in this model is covered by a test. FANTASIA/Source/FANTASIATests/Private/Security/ is empty; the two blockers are reproducible as unit tests against the exported PackStream codec and are the first cases owed.

## J-0081 | 2026-11-15 | F-030 | tester
- Event: opened
- Story: US-005
- Module: M-tts
- Kind: bug
- Severity: major
- Repro: FANTASIA/Source/FANTASIATests/Private/Quality/SecretLeakageTests.cpp, test FANTASIA.Quality.Secrets.Crash.RESTTTSPostWithoutContentType: run UnrealEditor-Cmd.exe <host.uproject> -ExecCmds="Automation RunTests FANTASIA.Quality.Secrets.Crash" -QualityRunCrashRepro -nullrhi -unattended; the process exits 3 with 'Assertion failed: !GetHeader(Content-Type).IsEmpty() || RequestPayload->IsURLEncoded() [CurlHttp.cpp:945]' on HttpManagerThread. Without the flag the test is skipped, so the gate stays green.
- Changes: none

URESTTTSComponent::IssueHttpRequest sends a POST whenever BodyTemplate is non-empty and sets only the headers the user configured. A user who configures just the key, as the new Headers tooltip invites (Headers=(("xi-api-key","..."))), plus a BodyTemplate, sends a POST body with no Content-Type. The engine's curl layer asserts, on the HTTP thread, which ends the editor or a Development game build (Shipping strips check()). Found while writing the log-leak test for US-028: my first REST request crashed the whole automation run. Not a regression of US-028 and not in its acceptance, but it is the first thing a user who follows the new Headers tooltip meets. A local fix (default Content-Type: application/json for a POST that has none) changes no signature; whether that is admissible on 5.8 under P-19 is the planner's call. Observed in UE 5.8 on Windows only.

## J-0082 | 2026-11-15 | F-031 | tester
- Event: opened
- Story: US-028
- Module: M-llm, M-tts
- Kind: security
- Severity: major
- Repro: FANTASIA/Source/FANTASIATests/Private/Quality/SecretConfigRobustnessTests.cpp, test FANTASIA.Quality.Secrets.Asset.TypedKeyIsSerialized: a key set on an OpenAI component or a REST Headers map is present in the bytes of the object's serialized form (FMemoryWriter, persistent), Config or not; a key arriving through the ini into the CDO is not.
- Changes: none

The Config specifier gives a property a second source at load time. It does not make the property transient, so a value typed into the Details panel of a Blueprint or placed actor is still saved into the asset, exactly as before. US-028's story sentence ('using a FANTASIA component cannot put my API key into a committed asset') is therefore true only for a user who puts the key in DefaultGame.ini and never types it into an editable field, and both properties remain EditAnywhere, BlueprintReadWrite, which invites exactly that. The OpenAI tooltip says the ini value 'keeps the API key out of any asset', which over-claims for the same reason; the REST tooltip is accurate. What does hold, and is pinned by the same test: with the key in the ini, the instance inherits it from the CDO and nothing is serialized. Residual risk, pre-existing, not a regression, not fixable inside P-19 (removing EditAnywhere or making the value transient changes the Blueprint surface); the user should decide whether to accept it. The same applies to F-029's three properties: adding Config to them would not stop a typed value being saved either.

## J-0083 | 2026-11-15 | F-032 | tester
- Event: opened
- Story: US-028
- Module: M-llm
- Kind: quality
- Severity: minor
- Repro: FANTASIA/Source/FANTASIATests/Private/Quality/SecretConfigRobustnessTests.cpp, test FANTASIA.Quality.Secrets.Preset.ApplyPresetDiscardsConfiguredKey
- Changes: none

ApplyPreset assigns Config = MakeXConfig(), as C-llm documents ('overwrites Config wholesale including APIKey'). With the key now read from DefaultGame.ini, calling Apply Provider Preset (the documented way to choose OpenAI, Groq, Ollama, LM Studio or vLLM) erases it; the next GetChatCompletion logs 'API key is required but empty'. A project that wants a preset and a key must set the key again after the preset from Blueprint, which is a literal in a graph, which is committed: the fix is defeated on the most common path. ApplyPreset(Custom) leaves the key. Not a regression (the same call erased an asset-typed key), but the fix's purpose is lost for these users. Preserving APIKey across ApplyPreset changes a documented behaviour of C-llm, so P-19 excludes it from 5.8; the planner may want to note it for 5.9, and the tooltip could say so now.

## J-0084 | 2026-11-15 | F-033 | tester
- Event: opened
- Story: US-028
- Module: M-tts
- Kind: quality
- Severity: minor
- Repro: FANTASIA/Source/FANTASIATests/Private/Quality/SecretConfigRobustnessTests.cpp, test FANTASIA.Quality.Secrets.Section.DerivedClass
- Changes: none

LoadConfig reads the ini section of the instance's own class. URESTTTSComponent::Headers is inherited by UElevenLabsTTSComponent and UACERESTTTSComponent (and by any Blueprint subclass), and a Headers line under [/Script/FANTASIA.RESTTTSComponent] reaches only a plain URESTTTSComponent: on a UElevenLabsTTSComponent instance the map loads empty (pinned by the test). The new tooltip says the value is 'read from the host project's DefaultGame.ini' and names no section, so a user of ACERESTTTSComponent who follows it by copying the base-class section gets no key, no error, and an unauthenticated request. UElevenLabsTTSComponent does not use Headers (BuildRequest takes Key), so its own exposure is nil; the ACE REST class is the one that matters. Same behaviour applies to every other inherited Config property in the plugin. A tooltip sentence naming the section rule is within what 5.8 admits.

## J-0085 | 2026-11-15 | DEC-006 | tester
- Event: decided
- Module: M-tests
- Decision: Quality secret tests are characterisation tests: failures that document an open finding are pinned as the observed behaviour (or tracked in an allow-list that fails when the finding is fixed), because UE simple tests have no expected-failure marker. A process-ending repro (F-030) runs only with -QualityRunCrashRepro so the gate stays green. Leak tests drive the real request path at a closed loopback port (127.0.0.1:1) with a log-capture device and a positive control; source is located from __FILE__ because the test module does not depend on Projects.
- Reason: A hand-maintained table of six cannot find a seventh; a no-leak assertion that cannot see the log is vacuous; the gate must not be broken by a test that documents a crash.
- Changes: none

Files: Private/Quality/SecretTestSupport.h, SecretConfigRobustnessTests.cpp, SecretLeakageTests.cpp. 15 tests, 150 in the suite, gate PASSED. Run the gate alone: two concurrent runs share one harness project and delete each other's report.

## J-0086 | 2026-11-15 | DEC-007 | build-dev
- Module: M-dist
- Story: US-027
- Decision: The gate's harness reaches the plugin through a directory junction at <harness>\Plugins\FANTASIA, not through the .uproject's AdditionalPluginDirectories as ADR-0016 words it.
- Reason: Measured, not reasoned: with AdditionalPluginDirectories the plugin is an external plugin, so UnrealBuildTool links its modules into the HOST PROJECT's Binaries (FantasiaGateEditor.target records $(ProjectDir)/Binaries/Win64/UnrealEditor-FANTASIATests.dll) while the editor's plugin loader still loads them from the PLUGIN's own Binaries\Win64, whose committed prebuilt DLLs carry a matching BuildId. The editor log proved it: InternalLoadLibrary: 'FANTASIATests' ('D:/.../FANTASIA/Binaries/Win64/UnrealEditor-FANTASIATests.dll'), a file older than the compile that had just run, and a deliberately failing test added to the source never appeared in the report. That is precisely the stale-prebuilt-DLL trap ADR-0016 rejected, reintroduced by the mechanism the ADR named. As an ordinary project plugin the compile's output and the editor's load path are the same file by construction. The junction also names the FANTASIA directory alone, so FANTASIAACE is unreachable from the harness rather than merely disabled.
- Changes: none

Alternatives rejected: (a) keep AdditionalPluginDirectories and copy the freshly built DLLs over the tracked ones before booting the editor - hand-rolled staging that overwrites P-12 artifacts on every run; (b) keep it and assert freshness - the assertion would simply always fail, since the editor would always load the committed DLL; (c) empty the plugin's Binaries\Win64 for the duration of a run - destructive to tracked files and racy with other agents.

A belt-and-braces guard stays in the script regardless of mechanism: after the editor run it reads the log's InternalLoadLibrary line for FANTASIATests and exits 10 if the module was loaded from anywhere other than the directory this run compiled into. The consequence ADR-0016 anticipated holds under the junction and not under the ADR's own mechanism: a compiling run rewrites FANTASIA/Binaries/Win64, and the script says so before it exits (P-12).

## J-0087 | 2026-11-15 | DEC-008 | build-dev
- Module: M-dist
- Story: US-027
- Decision: run-tests.ps1 deletes UnrealBuildTool's cached Makefile.bin for the harness target before every compile.
- Reason: Observed twice: with the makefile cached, UBT reported 'Target is up to date' although a new .cpp had been added to FANTASIATests/Private with a current timestamp, so the gate ran the suite without the new file and exited zero. For a project whose sprint consists of adding test files, that is the gate reporting green on code it never compiled. Discarding the makefile forces source rediscovery while object files are still reused incrementally, so a warm run stays at about 32 seconds.
- Changes: none

Alternatives rejected: -Rebuild (a full rebuild every run, 120 s instead of 32 s); trusting UBT's directory-timestamp invalidation (it is what failed); touching the module's .Build.cs to invalidate (a write into a tracked file on every gate run).

Evidence: before the change, 'Target is up to date' / 'Total execution time: 1.06 seconds' with ZZGateSelfCheckTests.cpp present and the report naming no such test. After it, 'Creating makefile for FantasiaGateEditor (no existing makefile)' / '[1/4] Compile [x64] ZZGateSelfCheckTests.cpp' and the gate exiting 5 with 'Fail: FANTASIA.GateSelfCheck.DeliberateFailure' in its output.

## J-0088 | 2026-11-15 | DEC-009 | build-dev
- Module: M-dist
- Story: US-027
- Decision: Scripts in this module are written in ASCII only, and the gate's verdict is derived from the exported report with a distinct exit code per way of failing (1 engine, 2 compile, 3 no report, 4 zero tests, 5 failures, 6 too few tests, 7 BuildId, 8 bad argument, 9 Intermediate not writable, 10 wrong module loaded).
- Reason: Two traps, both found by running the thing. Windows PowerShell 5.1 reads a UTF-8 file without a BOM as the ANSI codepage, where the third byte of an em-dash becomes U+201D, which PowerShell accepts as a string delimiter: two em-dashes inside one string silently turned the rest of the line into code and the script stopped parsing. And UnrealEditor-Cmd.exe exits zero in cases that are not successes and non-zero in cases that are, so its exit status cannot be the verdict; a numbered reason, printed with the path of the evidence, is what makes 'this run verified nothing' distinguishable from 'this run found a bug'.
- Changes: none

The tenth code exists because the gate's one unacceptable failure is a green run that compiled nothing. Exit 9 was added after speech-dev reported that a build launched from a sandboxed shell fails inside cl.exe with 'D8022: cannot open ...obj.rsp' and a missing .gen.cpp, which reads like a source error: the script now probes the plugin's Intermediate directory for writability first and names the sandbox.

## J-0089 | 2026-11-15 | DEC-010 | build-dev
- Module: M-dist
- Story: US-027
- Decision: The command proposed for gates.task_completed is the whole script, unrestricted: powershell -NoProfile -ExecutionPolicy Bypass -File FANTASIA/run-tests.ps1
- Reason: Measured on the reference machine (RTX 4060 laptop, i9-14900HX, 24 physical cores, UE 5.8.4, CompatibleChangelist 55116800). Cold - no harness project, no plugin Intermediate - 120.6 s, 70 compile actions of 77, green on 144 tests. Warm with no source change 31.8 s and 33.2 s on two consecutive runs, green on 150 tests; of that about 1-5 s is the no-op compile plus makefile rediscovery and about 27 s is the editor booting headless, while the 150 tests themselves take 2.3 s. Warm is well under US-027's 240 s threshold, so the first branch of its rule applies: no test-prefix restriction is needed, the full suite is the task gate, and nothing is left over to propose as a quality gate instead.
- Changes: none

A restricted form exists and was measured too: -Tests FANTASIA.BoltPackStream -MinTests 36 runs green in 19.2 s. It is not proposed, because the editor boot dominates, so restricting the prefix buys roughly twelve seconds while giving up nine tenths of the suite.

The suite's own duration is negligible against the editor's startup, so the gate's cost will not grow noticeably as the suite grows: it grew from 109 to 150 tests during this task's own measurements with no measurable change in wall-clock time.

## J-0090 | 2026-11-15 | NOTE | lead
- Changes: none

The gate caught a stale-binary state that the lead created, within minutes of existing.

On T-02's recommendation the tracked DLLs under FANTASIA/Binaries/Win64 were reverted with 'git checkout --', because they had been rewritten incidentally by other agents' builds and were not a deliberate P-12 refresh. That advice is right in general and wrong while the source is ahead of the committed binaries: 'git checkout' restores the old DLL with a *fresh* timestamp, so UBT judges the link output newer than its objects and skips the relink. The editor then loads a binary that predates the source.

The gate refused it: 'FAILED (6): the report names only 107 tests, fewer than the 109 expected. A suite that shrank silently is not a pass.' Nothing failed; the suite had simply lost every test added this sprint, because the loaded DLL knew nothing of them. US-027's minimum-count criterion, which existed to stop a run that verified nothing from exiting zero, caught a case nobody had in mind when it was written.

Deleting the two DLLs rather than restoring them forces the relink. The gate then reported PASSED: 150 tests, 0 failures in 30.4 s, and its load-path guard confirmed the editor read the module from the directory the run had compiled into.

Two lessons worth keeping. A tracked build artifact cannot be reverted independently of its source: restore it only together with the commit it belongs to, or delete it and let the build regenerate it. And the gate's value is already demonstrated beyond its acceptance criteria: this failure mode was invisible to every other check the project has, including three agents who had run the suite green minutes earlier.

## J-0091 | 2026-11-15 | DEC-011 | core-dev
- Module: M-demo, M-llm
- Decision: Mark AFANTASIATestActor's LLMAPIKey, Neo4jPassword and ElevenLabsKey Config, add Config = Game to the actor's UCLASS so the ini source exists, and rewrite all four secret tooltips to state the residual risk instead of claiming the key stays out of assets.
- Reason: F-029 is F-008's defect a third time, in the shipped runtime module; Config is the fix P-19 admits on the 5.8 line. F-031 established that Config only adds a load-time source: the properties stay EditAnywhere, so a key typed into the Details panel is still serialised into the Blueprint or .umap (pinned by FANTASIA.Quality.Secrets.Asset.TypedKeyIsSerialized). The tooltip T-04 wrote said the key 'stays out of any asset', which invites exactly the behaviour it appears to forbid; the tooltip is the only place a researcher learns where their key goes, so an over-claiming one is worse than none.
- Changes: none

Alternatives rejected: leaving the three properties as they are until F-004 removes the actor from the runtime module - rejected because the actor ships in every build today and P-19 keeps the module-boundary change off the 5.8 line, so the leak would stand unmitigated for the whole line. Dropping the three properties and reading the components' own configured values (the finding's third option) - rejected as a Blueprint-surface change (P-03) outside this task, and worth a change request rather than a silent widening. Making the properties transient or removing EditAnywhere, which would actually close the residual risk - rejected for the same reason F-031 gives: it changes the Blueprint surface. Config = Game on the UCLASS is not optional cosmetics: without a class config name the properties resolve against DefaultEngine.ini, and the tooltip's DefaultGame.ini sentence would be false.

## J-0092 | 2026-11-15 | CR-004 | lead
- Event: proposed
- Type: principle
- Class: major
- Requested by: user
- Impact: Principles: P-19's statement gains an exception and its rationale loses the F-011 paragraph that cites the refusal as an accepted price. Contracts: C-neo4j breaks and bumps Version three times over, once per admitted fix (F-020 changes what a bolt+s:// connection guarantees, F-021 changes what the reader accepts from the wire, F-011 changes the transport selection C-neo4j describes). Modules: M-neo4j only; BoltProtocol.cpp and BoltPackStream.cpp. Stories: US-014 is the affected done story and is the subject of the checker's two standing ERRORs, since it is marked done with two open blockers against it; the admitted fixes need a story of their own or US-014 reopened. US-020 and US-021, the 5.8 ship, now wait on whatever that story is. Findings: F-011, F-020, F-021 become admissible; F-012 (minor, quality, same module) does not and stays deferred. Tests: the Bolt layer's 36 existing codec tests are evidence for F-021's bounds checks without new infrastructure; F-020 needs a new test asserting verification is on, which is the security engineer's to write. Agents: core-dev owns all three files and must be told the bar moved; the planner and the reviewer enforce P-19 and both re-read it. Sprint: sprint.md's 'Not in this sprint' section states that P-19 excludes F-011, F-012 and the Neo4j half of F-007, and that sentence becomes false. Tasks in flight: none against M-neo4j.
- Options: (A) Admit a finding logged Kind=security by the security engineer at blocker or major, whatever its surface, with the contract edit limited to describing the new behaviour. Decidable from the journal at intake, cannot be claimed by whoever wants their change admitted, and preserves P-19's reason for existing -- no severity judgement, just provenance. Admits F-011, F-020, F-021 and nothing else today. (B) Admit any security finding regardless of severity. Also admits F-022 through F-028 -- seven more, five major -- across M-asr, M-tts, M-prolog, M-probabilistic and M-dist, which is most of this sprint's security work and makes the 5.8 line a security release rather than a ship. (C) Name the three findings as explicit exceptions and leave P-19 otherwise intact. Smallest edit, but it is a list rather than a principle, and the next security finding re-litigates from scratch. (D) Don't change. Cost: for the life of the 5.8 line a user who sets UseSSL = true against a bolt:// endpoint sends their Neo4j password in the clear with no warning, bolt+s:// is encrypted but unauthenticated so a machine-in-the-middle reads the credentials, and a hostile or faulty server drives an out-of-range read in the PackStream reader.
- Changes: none

P-19's rationale names F-011 as the case it deliberately refuses: "its fix changes
C-neo4j, so the 5.8 line does not admit it, and for the life of that line a user who sets
`UseSSL = true` against a `bolt://` endpoint sends their Neo4j password in the clear with
no warning." That was written as an accepted price. S-01's security pass then found two
more in the same module, both rated blocker:

- **F-020** — the TLS path sets `SSL_VERIFY_NONE`, so a `bolt+s://` connection is
  encrypted but unauthenticated and a machine-in-the-middle reads the credentials.
- **F-021** — the PackStream reader takes its lengths from the wire without bounding them
  against the buffer, so a hostile or faulty server drives an out-of-range read.

Three credential-or-memory defects in one module, all refused by the same clause, is no
longer a price. It is the bar selecting against the findings it most needs to admit, and
the reason is mechanical rather than deliberate: P-19 measures surface, and a security fix
in a module whose behaviour is described by a contract almost always touches that contract
precisely because it changes what crosses the trust boundary. The clause therefore filters
by where a defect lives rather than by how much it costs the user.

The user's direction, given in this sprint: carve security out. A security finding is
admissible on the shipped line even when its fix touches a contract.

**What the amendment must preserve.** P-19 exists because "minor fixes only" with no
stated bar admits everything, and the surface test was chosen so that intake needs no
severity judgement. An exception that reads "unless it is important" restores exactly the
judgement call P-19 was written to remove. So the carve-out needs its own decidable test,
and the obvious one is provenance rather than opinion: a finding logged `Kind=security` by
the security engineer, at `blocker` or `major`. That is a fact in the journal, checkable at
intake, and it cannot be claimed by whoever wants their change admitted.

**What it must not quietly widen.** The admitted item is the security fix, not the
refactor it would be convenient to carry with it, and the contract edit it forces is the
minimum that describes the new behaviour. P-07 already denies scope creep; this is where
it will be tested, because a contract that is open for editing invites more than the fix.

**Impact to be analysed, not assumed.** The amendment admits F-011, F-020 and F-021 to the
5.8 line. Each changes C-neo4j, so each needs the contract edit the amendment permits; the
Bolt layer's 36 existing codec tests are the evidence F-021's bounds checks can be verified
against without new infrastructure. Whether the three are one story or three, and whether
the 5.8 ship (US-020, US-021) now waits on them, is the planner's to work out and the
user's to accept. The security engineer's recommended order is F-021 first, then F-020,
then F-011.

## J-0093 | 2026-11-15 | F-029 | lead
- Event: fixed
- Evidence: T-09 (DEC-011, J-0091) added Config = Game to AFANTASIATestActor's UCLASS and the Config specifier to LLMAPIKey, Neo4jPassword and ElevenLabsKey; T-10 widened SecretConfigTests.cpp's reflection table from six properties to nine; T-11 retired the KnownOpen allow-list that tracked the three. Verified by the lead in the source at FANTASIATestActor.h:34, 104, 130, 153, and by running the gate: PASSED, 150 tests, 0 failures, 39.9 s.
- Changes: none

The finding as written is closed: the three properties were editable and unconfigured, and they are now configurable from DefaultGame.ini. The class-level Config = Game was load-bearing and was not in the task description -- without it the properties would have resolved against DefaultEngine.ini and the new tooltips' DefaultGame.ini sentence would have been false. core-dev found that itself; test-dev independently flagged the missing class specifier from the other side.

What did NOT close, and is tracked by F-031 rather than here: all three properties remain EditAnywhere, so a key typed into the Details panel still serialises into the level or Blueprint asset. Config added an ini route and an accurate warning; it did not close that hole, and the reflection sweep cannot detect it. core-dev's view, which the lead shares, is that the real fix is F-029's third option -- drop the three properties and have the test actor exercise the components' own configured values -- because it removes three secret-bearing properties from the runtime module instead of annotating them, and makes the harness test the shipped path rather than a parallel one. That is a Blueprint-surface change (P-03) and needs a change request; it was not acted on.

## J-0094 | 2026-11-15 | NOTE | lead
- Changes: none

The gate's one environmental failure mode, diagnosed so the next person does not have to. T-11's first gate run failed to compile with MSVC C3859 ('the paging file is too small') and C1076, and the gate correctly reported 'FAILED (2): the compile failed ... No test ran, so this run verified nothing' -- it did not report green on uncompiled code, which is the behaviour ADR-0016 asked for. The retry passed with the same source.

The cause is the commit limit, not RAM. This machine has 63.7 GB of physical memory and a Windows-managed pagefile of 8 GB, so the commit limit is about 72 GB. At the time of the failure 19 python3.12 processes (the session's MCP servers) were holding 23.3 GB of commit between them while 29.9 GB of physical memory sat free, leaving 3.7 GB of free virtual memory. UBT runs up to 24 cl.exe processes in parallel on this box (24 physical cores), and each reserves enough address space that the burst exhausts commit before it exhausts RAM.

Two consequences worth acting on separately. First, the machine's pagefile is undersized for a 24-way C++ compile alongside the editor and the MCP servers; raising it is a system setting and the user's to make. Second, the gate could name this cause the way build-dev's exit-9 guard names the sandbox: C3859, C1076 and 'paging file is too small' in the compiler output are a distinctive signature, and an exit code that says 'the compile ran out of commit, not out of correctness' would save the diagnosis. That is a small follow-up against US-027, not a defect in what landed.

## J-0095 | 2026-11-15 | NOTE | lead
- Changes: team.json

gates.task_completed is set. It was null, and US-027 existed to fill it. T-02 measured the gate and the lead verified the measurement by running it: 'powershell -NoProfile -ExecutionPolicy Bypass -File FANTASIA/run-tests.ps1', warm 30-45 s, cold 120.6 s. US-027's rule was to take the whole script if the warm run is at or under 240 s and otherwise to restrict it to a test prefix; the warm run is well inside that, so the whole suite is the task gate and gates.quality stays empty for now. From this entry on, sprint.md's Definition of Done clause 'the quality gate passes' means this command, not a coder running the story's own tests.

Two properties of the gate are worth recording because they were not free. It reaches the plugin by a directory junction into a generated harness project rather than by AdditionalPluginDirectories, because that mechanism -- which ADR-0016 itself proposed -- made the plugin external, so UBT linked into the host project while the editor loaded the committed prebuilt DLL from the plugin's own Binaries: it reported PASSED on a run with a deliberately failing test compiled in. And it deletes Makefile.bin before compiling, because UBT's cached makefile does not notice added source files and reports 'Target is up to date', which in a sprint whose work is adding test files would have been green on code never compiled. ADR-0016 therefore needs amending to describe the junction; that is T-08's.

## J-0096 | 2026-11-15 | F-008 | lead
- Event: fixed
- Evidence: T-04 (DEC-003) added Config to UOpenAICompatibleComponent::Config, the owning struct property rather than its inner fields; T-05 (DEC-004) added Config to URESTTTSComponent::Headers. T-03 (DEC-002) wrote SecretConfigTests.cpp, whose FANTASIA.Secrets.ConfigFlag reflection table and two round-trip tests (RoundTrip.OpenAICompatibleKey, RoundTrip.RESTTTSHeaders) fail before the fix and pass after it. Verified by the lead at OpenAICompatibleComponent.h:95 and RESTTTSComponent.h:322, and by running the gate: PASSED, 150 tests, 0 failures.
- Changes: none

The mechanism the finding identified is closed. UObject::LoadConfig walks the class's own Config properties and does not descend into a struct property that is not itself Config, which is why the inner fields' specifiers did nothing; the specifier is now on the owning property, and the round-trip test proves the ini form actually loads rather than merely that the flag is set. Both classes already carried config = Game, so no class specifier was needed here -- unlike AFANTASIATestActor under F-029, which needed both.

The residual risk is F-031 and stays open: both properties remain EditAnywhere, so a key typed into the Details panel still serialises into the Blueprint or level asset. Config added an ini route and, after T-09, an accurate warning that says so. Only the user can accept that residual.

## J-0097 | 2026-11-15 | CR-005 | planner
- Event: proposed
- Type: architecture
- Class: minor
- Impact: Decisions: ADR-0016's mechanism clause is wrong and is restated by a new ADR-0017 that amends it; architecture.md's Decisions section and Design version change. Modules: M-dist only, and its Paths line gains FANTASIA/run-tests.ps1. Contracts: none break; C-build gains a sentence that its BuildId invariant is now checked mechanically, carried by a separate CR. Stories: US-027's acceptance criteria name AdditionalPluginDirectories in the harness clause and that clause becomes unsatisfiable, so it is rewritten to the junction, the post-run load-path assertion and the makefile discard. Team: team.json's design_version and build-dev.owns both need the lead's edit, raised separately. Tasks in flight: none against M-dist; T-02 is complete. Findings: none opened or closed.
- Options: (A) Restate the decision whole as ADR-0017 amending ADR-0016, keeping ADR-0016's context reachable, and rewrite US-027's mechanism clause. (B) Supersede ADR-0016 outright, which loses the context paragraph that is still the live reason the gate brings its own host project. (C) Leave the ADR and let the script differ from it, which is the drift this method exists to prevent and which would send the next reader to AdditionalPluginDirectories. Cost of not changing: the one document stating how the gate works describes a mechanism that reports PASSED on a deliberately failing test, and the two traps that cost T-02 a day are recorded nowhere a reader will look.
- Changes: none

ADR-0016 states the mechanism by which the gate reaches the plugin, and that mechanism
defeats the decision the ADR exists to make. The ADR's own rejected-alternatives section
refuses "run the suite against the committed prebuilt DLLs, with no compile step", on the
grounds that "a gate that can pass without compiling the change it is gating is worse than
no gate, because it is believed". `AdditionalPluginDirectories` reintroduces exactly that:
it makes the plugin external to the host project, so UnrealBuildTool links the freshly
compiled modules into the **host project's** `Binaries` while the editor's plugin loader
still loads them from the **plugin's own** `Binaries\Win64`, where the committed prebuilt
DLLs carry a matching BuildId. build-dev measured it during T-02 (DEC-007, J-0086): the
editor log read `InternalLoadLibrary: 'FANTASIATests' ('.../FANTASIA/Binaries/Win64/
UnrealEditor-FANTASIATests.dll')`, a file older than the compile that had just run, and a
deliberately failing test compiled into the source never appeared in the report. The gate
said PASSED.

This is the whole value of the amendment. ADR-0016 is the decision that forbids gating on
a stale prebuilt DLL, and the mechanism it proposed for itself was a route to that stale
DLL. The ADR was reasoned; the mechanism was not measured. Recording the rejection beside
the decision is what stops the next person reaching for `AdditionalPluginDirectories`
because it reads like the tidier option.

A second trap belongs with it, UBT's rather than the ADR's. With a cached `Makefile.bin`,
UnrealBuildTool reports "Target is up to date" although a new `.cpp` with a current
timestamp has been added to `FANTASIATests/Private`, so the gate ran the suite without the
new file and exited zero (DEC-008, J-0087). In a sprint whose work consists of adding test
files, that is the gate reporting green on code it never compiled. Both traps have the same
shape and the same remedy: assert, do not assume, that the thing you compiled is the thing
that ran.

**Scope of this CR.** The decision in force is restated whole as ADR-0017 so that a reader
needs only one entry; ADR-0016 keeps its context, which is still the reason the gate brings
its own host project at all, and architecture.md's Decisions line cites ADR-0017 amending
ADR-0016, as it already does for ADR-0015 amending ADR-0009.

US-027's acceptance criteria name `AdditionalPluginDirectories` in the clause describing
the harness, so that clause is now unsatisfiable by a gate that works. It is rewritten to
the junction, to the post-run assertion on the editor's load path, and to the makefile
discard. This is a criterion the user accepted at sprint planning and I am changing it, so
it is called out at review rather than left to be noticed: the story's intent is untouched,
the mechanism clause was mine and was wrong, and the evidence that it was wrong is a
measured run that reported PASSED on a deliberately failing test.

Classified minor: no module boundary moves, no contract breaks, no story is added or
dropped, and the mechanism is already built, measured and verified by the lead, who set
`gates.task_completed` from it in J-0095. The planner approves it. What the user is asked
to confirm at review is the edited acceptance criterion, not the mechanism.

## J-0098 | 2026-11-15 | CR-004 | lead
- Event: approved
- Decision: Approved by the user, option (A): a finding logged Kind=security by the security engineer at blocker or major is admissible on a shipped engine line whatever its surface, with the contract edit limited to describing the new behaviour. Options (B) any security finding regardless of severity, (C) naming the three findings as explicit exceptions, and (D) no change were not taken.
- Changes: none

The user chose the shape whose test is provenance rather than opinion. Three properties of that choice should be cited rather than re-argued later.

It keeps the reason P-19 exists. P-19 measures surface precisely so that intake needs no severity judgement, and an exception reading 'unless it is important' would restore exactly the judgement call the principle was written to remove. 'Logged Kind=security by the security engineer at blocker or major' is a fact in the journal, checkable at intake, and it cannot be claimed by whoever wants their own change admitted -- the coder who wants the exception cannot grant it to themselves.

It admits three findings today and no more. F-011 (UseSSL = true against a bolt:// endpoint sends the password in the clear with no warning), F-020 (SSL_VERIFY_NONE, so bolt+s:// is encrypted but unauthenticated) and F-021 (PackStream lengths taken off the wire unbounded). F-012 is minor and quality, in the same module, and stays deferred -- which is the test doing its job, since the same module is not the criterion.

Option (B) was declined knowingly, not overlooked. It would also have admitted F-022 through F-028, seven more findings across five modules, five of them major, turning the 5.8 line into a security release rather than a ship. The user's intent is a shipped line that does not carry known credential-or-memory defects, not a line that absorbs the whole security backlog.

The scope discipline is where this will be tested. The admitted item is the security fix, not the refactor that would be convenient to carry with it, and the contract edit is the minimum that describes the new behaviour. P-07 already denies scope creep; the reviewer should expect to apply it here, because a contract that is open for editing invites more than the fix.

## J-0099 | 2026-11-15 | ADR-0017 | planner
- Event: decided
- Status: accepted
- Amends: ADR-0016
- Title: The gate reaches the plugin as an ordinary project plugin, by directory junction
- Story: US-027
- Module: M-dist
- Changes: none

### Context

ADR-0016 decided that verification brings its own host project, and that decision stands:
this repository is a plugin, it contains no `.uproject`, the automation suite needs the
editor, and the one project on the maintainer's machine that holds the plugin is unusable
as a gate host for the 5.8 line for three reasons that are properties of that project
rather than of this plugin. ADR-0016's context paragraph carries that reasoning and is
still the reason the gate generates a harness at all.

What ADR-0016 got wrong is the sentence describing how the harness reaches the plugin. It
said `AdditionalPluginDirectories`, reasoned rather than measured, and T-02 measured it.

### Decision

The gate reaches the plugin as an **ordinary project plugin**: `run-tests.ps1` creates a
directory junction at `<harness>\Plugins\FANTASIA` pointing at this repository's `FANTASIA`
directory. Everything else ADR-0016 decided is unchanged — the engine is resolved from
`FANTASIA_UE_ROOT` with a documented default, the script refuses to run when the engine's
`Build.version` does not match the version the `.uplugin` descriptors name, it compiles the
`FANTASIA` and `FANTASIATests` modules for the harness project's editor target, boots
`UnrealEditor-Cmd.exe` headless, runs `Automation RunTests FANTASIA`, and derives its exit
code from the exported report rather than from the editor's exit status.

Two assertions make the mechanism self-checking, and they are part of the decision rather
than implementation detail:

1. **The module that ran is the module that was compiled.** After the editor run the script
   reads the log's `InternalLoadLibrary` line for `FANTASIATests` and exits 10 if the module
   was loaded from any directory other than the one this run compiled into.
2. **The makefile is discarded before every compile.** The script deletes
   UnrealBuildTool's cached `Makefile.bin` for the harness target, which forces source
   rediscovery while object files are still reused incrementally.

### Consequences

- As an ordinary project plugin, the compile's output path and the editor's load path are
  the same file by construction, so the stale-prebuilt-DLL trap is closed structurally and
  assertion (1) is a belt-and-braces guard rather than the only line of defence.
- The junction names the `FANTASIA` directory alone, so FANTASIAACE is not merely disabled
  in the harness but unreachable from it. The gate says so when it runs.
- Discarding the makefile costs roughly one to five seconds of rediscovery on a warm run.
  Measured warm total 31.8 s and 33.2 s, cold 120.6 s, of which about 27 s is the editor
  booting headless and 2.3 s is the 150 tests themselves (DEC-010, J-0089). The gate's cost
  will not grow noticeably as the suite grows.
- A gate run recompiles into `FANTASIA/Binaries/Win64/`, which is tracked. That is the
  artifact P-12 governs, so a dirty working tree after a gate run is expected and the
  script says so before it exits.
- The gate compares the `.modules` BuildId it emits with the engine's `CompatibleChangelist`
  and exits non-zero on a mismatch, which makes C-build's last invariant mechanical and is
  the error users actually hit.
- A reclone plus an installed engine reproduces the gate. One environment variable is the
  whole machine-specific surface and it has a default.
- `run-tests.ps1` belongs to M-dist, beside bootstrap and `enable-plugins.ps1`, so
  `build-dev` owns it and `team.json`'s `build-dev.owns` gains the path. That file is the
  lead's.

### Rejected alternatives

- ***`AdditionalPluginDirectories`, which ADR-0016 itself proposed.*** Rejected on evidence,
  and this is the rejection worth keeping. The entry makes the plugin external to the host
  project, so UnrealBuildTool links the compiled modules into the host project's `Binaries`
  (`FantasiaGateEditor.target` records `$(ProjectDir)/Binaries/Win64/
  UnrealEditor-FANTASIATests.dll`) while the editor's plugin loader still loads them from
  the plugin's own `Binaries\Win64`, whose committed prebuilt DLLs carry a matching BuildId.
  The editor log proved it: `InternalLoadLibrary: 'FANTASIATests'
  ('D:/.../FANTASIA/Binaries/Win64/UnrealEditor-FANTASIATests.dll')`, a file older than the
  compile that had just run, and a deliberately failing test compiled into the source never
  appeared in the report. The gate reported PASSED. That is precisely the stale-prebuilt-DLL
  trap ADR-0016 rejected in its own alternatives, reintroduced by the mechanism ADR-0016
  named. A gate that can pass without running the code it compiled is worse than no gate,
  because it is believed.
- ***Keep `AdditionalPluginDirectories` and copy the freshly built DLLs over the tracked
  ones before booting the editor.*** Hand-rolled staging that overwrites P-12 artifacts on
  every run.
- ***Keep it and assert freshness.*** The assertion would always fail, since the editor
  would always load the committed DLL.
- ***Empty the plugin's `Binaries\Win64` for the duration of a run.*** Destructive to
  tracked files and racy with other agents.
- ***Trust UBT's directory-timestamp invalidation instead of discarding the makefile.***
  That is what failed: with the makefile cached, UBT reported "Target is up to date" and
  "Total execution time: 1.06 seconds" with a new `.cpp` present and the report naming no
  such test. After the discard: "Creating makefile for FantasiaGateEditor (no existing
  makefile)", `[1/4] Compile [x64] ZZGateSelfCheckTests.cpp`, and the gate exiting 5 with
  `Fail: FANTASIA.GateSelfCheck.DeliberateFailure` in its output.
- ***`-Rebuild` on every run.*** Correct but 120 s instead of 32 s, which is the difference
  between a gate run per task and a gate run per sprint.
- ***Touch the module's `.Build.cs` to invalidate the makefile.*** A write into a tracked
  file on every gate run.

### The lesson this ADR records

Both traps are the same mistake in two places: assuming that the artifact you built is the
artifact that ran. One came from a reasoned ADR, the other from a build tool's cache, and
each produced a green gate over code that was never executed. A gate's first duty is to be
able to fail, so the gate asserts what it compiled and what was loaded, and the one
unacceptable outcome is a green run that verified nothing — which is why the script has a
distinct exit code per way of failing rather than a single non-zero (DEC-009, J-0088).

## J-0100 | 2026-11-15 | CR-006 | planner
- Event: proposed
- Type: contract
- Class: minor
- Impact: Contracts: C-neo4j, C-probabilistic and C-prolog each retract one asserted mitigation and go to Version 2. No interface moves and no behaviour changes; the code is unchanged. Consumers: M-demo and M-tests consume all three, so core-dev, reasoning-dev and test-dev re-read. Modules: none change. Stories: US-014 (C-neo4j), US-012 and US-013 (C-probabilistic), US-010 and US-011 (C-prolog) are the affected done stories and none is edited by this CR. Findings: F-021, F-025 and F-028 are the three claims; F-034 is opened for the unguarded aGrUM entry points F-025 does not name. Tests: BoltPackStreamTests Reader.HasMoreEmpty and BayesianNetworkTests EraseNode are both cited as pins for claims they do not pin, so both citations are removed. Tasks in flight: none against M-neo4j, M-probabilistic or M-prolog.
- Options: (A) Correct all three to describe the code as it is, bump each to Version 2, and leave the remediation of the underlying defects to their own stories. (B) Correct them and schedule the fixes in the same change, which widens a documentation correction into three module changes across two coders and buries the correction in the work. (C) Soften to point releases because the edits are one line each, which weakens the only signal a consumer has that a guarantee they coded against is gone. (D) Don't change. Cost: three contracts keep promising guards that do not exist in code, so a consumer who reads them omits a bounds check, a try/catch and a per-component result filter respectively, and the omission is invisible because the contract says it is unnecessary.
- Changes: none

Three contracts assert a mitigation the code does not implement. In each case the contract
is the more trustworthy-looking of the two documents, which is what makes it dangerous: a
consumer reads the sentence, omits the guard it promises, and the omission is invisible
because the contract says it is unnecessary.

Each was verified against the source for this CR rather than taken from the finding text,
because correcting a document that was wrong twice is the failure mode to avoid.

**C-neo4j — `ReadValue` past the end of the buffer.** The contract says it "returns a null
value; `HasMore()` is the caller's guard (pinned by `Reader.HasMoreEmpty`)". Verified at
`BoltPackStream.cpp:171-214`: `ReadByte` is `Data.GetData()[Cursor++]` with no comparison
against `Data.Num()`; `ReadUInt16BE`, `ReadInt32BE` and `ReadInt64BE` index
`Data.GetData() + Cursor` and advance blindly; `ReadStringOfLength(Len)` converts `Len`
bytes from the cursor; the `0xCC/0xCD/0xCE` bytes case `Append`s `Len` bytes from the
cursor. `ReadValue`'s first act is that unchecked `ReadByte`, so on an empty buffer it
reads through a null `GetData()`. The cited test (`BoltPackStreamTests.cpp:484-492`) calls
`HasMore()` on an empty buffer and never calls `ReadValue`, so the pin does not pin the
claim. This is F-021, and it is why 36 passing codec tests never caught it: every one of
them feeds the reader bytes the writer produced.

**C-probabilistic — aGrUM exceptions at the boundary.** The contract says they "are caught
at the boundary and logged; they do not propagate into Blueprint code. Exceptions are
enabled in this module for exactly that reason." Partly true and in three places false.
Verified: the build and inference calls do wrap, but each catches `gum::NotFound` alone
(`BayesianNetwork.cpp` lines 86, 112, 232, 249, 259, 277; `InfluenceDiag.cpp` lines 77,
104, 125, 143, 175, 299, 312, 324), so any other aGrUM exception type propagates. Three
`BlueprintCallable` entry points have no handler at all:

- `setBN` (`BayesianNetwork.cpp:148`) constructs `gum::BIFReader` and calls
  `reader.proceed()` bare, and drops the return value, which is the parse error count — so
  a malformed BIF file terminates the process and a file that parses *with* errors is
  treated as loaded while the rest of `setBN` walks a half-built network. This is the one
  entry point whose input is a file rather than a Blueprint value, so the only
  untrusted-input path is the one with no handler (F-025).
- `writeBIF` (`BayesianNetwork.cpp:124`) and `writeBIFXML` (`InfluenceDiag.cpp:332`) hand
  a caller-supplied path to aGrUM bare (F-025).
- `erase` on both classes (`BayesianNetwork.cpp:129`, `InfluenceDiag.cpp:339`) calls
  `bn.erase(name)` / `id.erase(name)`, and `UInfluenceDiag::idFromName` calls
  `id.idFromName(name)`, all bare. The vendored header documents the throw:
  `variableNodeMap.h:176` — "@throw NotFound if no such name exists in the graph" — and
  `BayesNet.h:304` is `void erase(const std::string& name) { erase(idFromName(name)); }`.
  So the contract's neighbouring claim that "`erase` on an absent variable is a no-op
  (pinned by `EraseNode`)" is false twice over: it terminates the process, and the cited
  test (`BayesianNetworkTests.cpp:133-145`) only erases a variable that is present.
  `UBayesianNetwork::idFromName` *is* guarded, by a `nodeNameSet` pre-check returning -1;
  `UInfluenceDiag::idFromName` is not, so the contract's single sentence about negative ids
  is true of one class and false of the other. F-025 does not name these three, so they are
  opened as F-034 rather than quietly folded into it.

**C-prolog — one engine per component.** The contract's first invariant says "One Prolog
engine per component, running on its own thread." Verified: `SWIPrologThread.h:99` declares
`static SWIPrologThread* Runnable` and `SWIPrologThread.cpp:65-69` is
`if (!Runnable) Runnable = new SWIPrologThread(); return Runnable;` — one engine for the
whole process. `SWIPrologComponent.cpp:73-79` subscribes every component to that one
object's multicast `SolutionAvailable`, `AllSolutionsReady` and `QueryError`, and the
component's handler does not filter, so a query submitted by one component fires every
other component's Blueprint events with the first one's bindings. The knowledge base and
the single open `myQuery` are shared too, and the header's own note already says what the
contract denies: "Only one Prolog engine can exist per process." This is F-028, including
the detail that `Shutdown()` is called from nowhere, so asserted clauses outlive `EndPlay`
and persist across PIE sessions.

**Class and route.** Logged minor, and the reasoning matters because the Version bumps say
"breaking". Nothing about the system changes here: the code behaves today exactly as it
behaved before this CR, and no interface moves. There is therefore no decision for the user
to take — the only alternative is a document that knowingly lies — which is why the planner
approves it rather than routing it. The Version goes to the next integer on each of the
three all the same, because from a consumer's point of view the guarantee they coded
against has been withdrawn, and that is precisely what a breaking bump exists to signal.
Weakening the signal to a point release to match the size of the edit would be the same
mistake these three sentences already made: letting the document's tidiness stand in for
the code's behaviour.

What is *not* in this CR, and stays with the user: what to do about the three real defects.
F-021 is admitted to the 5.8 line by CR-004 and carried by US-030, so C-neo4j's sentence
will be rewritten a second time when the bounds checks land — it is corrected here to
describe today's code and deliberately does not describe the fix. F-025 and F-034 need a
story in M-probabilistic; F-028 needs one in M-prolog, and its fix is per-component result
routing rather than per-component engines, which are impossible. None of the three is
planned by this CR.

## J-0101 | 2026-11-15 | CR-007 | planner
- Event: proposed
- Type: contract
- Class: minor
- Impact: Contracts: C-llm and C-rest-tts stop recording F-008 as present fact and state the ini route plus F-031's residual; C-asr gains the capture tests as evidence and names F-017, F-018 and F-019; C-build records that its BuildId invariant is now decided by the gate. All four go to Version 1.1 \u2014 nothing breaks. Consumers: M-demo and M-tests for all four, M-ace for C-rest-tts and C-build, M-asr and M-probabilistic for C-build. Modules: none change. Stories: US-005, US-008, US-028 (C-llm, C-rest-tts), US-001 to US-004 and US-029 (C-asr), US-018 to US-022 and US-027 (C-build); none is edited by this CR. Findings: F-008 closed before this CR and F-029 with it; F-031, F-032, F-033, F-017, F-018, F-019 stay open and are named where a reader of the contract will meet them. Tests: SecretConfigTests.cpp and Quality/SecretConfigRobustnessTests.cpp are the new evidence for the first pair, WhisperCaptureComponentTests.cpp for C-asr, FANTASIA/run-tests.ps1 for C-build. Tasks in flight: none.
- Options: (A) Follow all four to the state that landed, point releases, naming the residual findings where a consumer meets them. (B) Follow only the two F-008 contracts and leave C-asr and C-build stating that nothing is tested, which is the drift this task exists to clear. (C) Replace F-008's sentences with 'all secret-bearing properties carry Config', which is true and misleading, because it reads as 'your key cannot reach an asset' and F-031 says it still can. (D) Don't change. Cost: two contracts keep describing a fixed defect as live, so a reader distrusts the fix or re-fixes it; C-asr keeps telling a maintainer the capture path has no tests, which is the single best reason not to touch it carefully; and C-build's BuildId invariant keeps reading as a promise when it is now a check.
- Changes: none

Four contracts describe the state of things before this sprint. Each follows the work that
landed, and none of the four interfaces moves, so each takes a point release rather than an
integer: a consumer who coded against version 1 is still correct, and gains something.

**C-llm and C-rest-tts: F-008 is fixed, and the honest replacement is not "all properties
carry `Config` now".** Both contracts record the defect as present fact —
`UOpenAICompatibleComponent::Config` is "**not** marked Config" so "`APIKey` is stored in
the owning asset's defaults and is committed with it", and `URESTTTSComponent::Headers` is
"**not** marked Config even though it is where an API key goes". Verified fixed in the
source: `OpenAICompatibleComponent.h:95` and `RESTTTSComponent.h:322` both now read
`UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, ...)`, and the specifier is on the
owning property rather than on the struct's inner fields, which is the whole point —
`UObject::LoadConfig` walks the class's own `Config` properties and does not descend into a
struct property that is not itself `Config`, which is why the inner specifiers never fired
(F-008, J-0096).

What replaces the defect sentence is F-031, which is open and is a different claim. `Config`
adds a load-time source; it does not make a property transient. Both properties remain
`EditAnywhere, BlueprintReadWrite`, so a key typed into the Details panel is still
serialised into the Blueprint or level asset, pinned by
`FANTASIA.Quality.Secrets.Asset.TypedKeyIsSerialized`. What does hold, and the same test
pins it: with the key in `DefaultGame.ini` the instance inherits it from the CDO and nothing
is serialised. So the contracts gain the ini route as a guarantee and keep a warning, and
C-llm's flat statement that `APIKey` "is **not** safe from being committed, contrary to
P-15, until F-008 is fixed" becomes conditional on how the key arrives rather than
unconditional.

**C-asr: the capture component is no longer untested.** Its last invariant says
"`UWhisperCaptureComponent` and the backend dispatcher have no tests at adoption (F-007).
Every invariant above that no test name is cited for is read from the code, not pinned."
US-029 landed 25 tests in `WhisperCaptureComponentTests.cpp`, so the component's defaults,
its device-free controls and its four DSP steps are pinned; the backend dispatcher still is
not. The contract's own two stated capture invariants — `AudioDeviceIndex` of -1 and both
filters defaulting true — now cite the test that pins them. Three defects the
characterisation found are named rather than left for the next reader to rediscover (F-017,
F-019), including that the high-pass is documented as Direct Form II Transposed and
implemented as Direct Form I, and that `ResampleAndMixToMono` selects the loudest channel
per tap rather than mixing.

The `StartCapture` error sentence is reworded but not weakened. It says the call "returns
false when the audio device cannot be opened, and also when capture is already running".
The second half is pinned. The first is true only of a machine that enumerates no input
device at all: verified at `WhisperCaptureComponent.cpp:366-380`, an `AudioDeviceIndex`
outside the enumerated range — negative or too large — logs a warning and opens the system
default, so no value of the property reaches the failure path (F-018). That distinction is
the difference between a guarantee a caller can rely on and one they cannot provoke, so the
contract states it.

**C-build: one of its invariants is now checked mechanically.** "The `.uplugin`
`EngineVersion` and the committed `.modules` BuildId describe the same engine. They move
together or users are locked out (NFR-005)." The gate compares them on every run and exits
non-zero on a mismatch (ADR-0017, US-027), which makes this the first clause of C-build
that a command decides rather than a person. Its last invariant, "No test covers any of this
at adoption", is correspondingly narrowed rather than deleted: the BuildId clause is
covered, the defines, the link order and the import cleanliness still are not, and
`BootstrapContractTests.cpp` is still where they belong.

**Class and route.** Minor on all four. Nothing breaks: two contracts gain a configuration
route that did not exist, one gains evidence for invariants it already stated, one gains a
mechanical check for an invariant it already stated. The planner approves it. The residual
risk in the first pair — F-031 — is the user's to accept and is not accepted here; it is
recorded on US-028 and in both contracts as the warning a consumer needs.

## J-0102 | 2026-11-15 | F-034 | planner
- Event: opened
- Kind: bug
- Severity: major
- Story: US-012, US-013
- Module: M-probabilistic
- Repro: FANTASIA/Source/FANTASIA/Private/BayesianNetwork.cpp:129 (UBayesianNetwork::erase) calls bn.erase(nodeName) with no try/catch; FANTASIA/ThirdParty/aGrUM/Includes/agrum/BN/BayesNet.h:304 defines erase(const std::string&) as erase(idFromName(name)) and agrum/base/graphicalModels/variableNodeMap.h:176 documents idFromName as throwing gum::NotFound when the name does not exist. Same in InfluenceDiag.cpp:339. InfluenceDiag.cpp:357 (UInfluenceDiag::idFromName) is a bare return id.idFromName(...) with no name-set guard, where BayesianNetwork.cpp:241 has one and returns -1. From Blueprint or the test module: construct a network, call erase on a name that was never added; the process terminates rather than logging. The test cited for the no-op claim, FANTASIA.BayesianNetwork.EraseNode at BayesianNetworkTests.cpp:133, only erases a variable that is present.
- Changes: none

Found while correcting C-probabilistic's claim that "aGrUM exceptions are caught at the
boundary and logged; they do not propagate into Blueprint code" (CR-006). F-025 established
that the claim is false for the reader and the two writers. It is false in two more places
that F-025 does not name, and they are opened here rather than folded into it because
F-025's fix is scoped to `setBN`, `writeBIF` and `writeBIFXML` and would leave these
standing.

**1. `erase` on an absent variable terminates the process.** The contract said it "is a
no-op ... (pinned by `EraseNode`)". Neither half was true. `UBayesianNetwork::erase`
(`BayesianNetwork.cpp:129-135`) calls `bn.erase(nodeName)` with no `try`/`catch`, and the
vendored `BayesNet.h:304` is `void erase(const std::string& name) { erase(idFromName(name)); }`
while `variableNodeMap.h:176` documents `idFromName` as "@throw NotFound if no such name
exists in the graph". `UInfluenceDiag::erase` (`InfluenceDiag.cpp:339-353`) is the same, and
worse in one respect: it strips the variable from `arcs`, `arcSet`, `nodeNames`,
`nodeNameSet` and `nodeDescriptions` either side of the throwing call, so a failed erase
leaves the component's own bookkeeping half-updated and out of step with the model even if
the exception were caught. The cited test
(`BayesianNetworkTests.cpp:133-145`, `FANTASIA.BayesianNetwork.EraseNode`) erases
`Sprinkler` from a network that contains it and never erases an absent one, so it does not
pin the claim it was cited for.

**2. `UInfluenceDiag::idFromName` on an unknown name throws instead of returning -1.** The
contract stated one sentence for both classes. `UBayesianNetwork::idFromName`
(`BayesianNetwork.cpp:241-246`) guards with `nodeNameSet.Contains(variable)`, logs and
returns -1. `UInfluenceDiag::idFromName` (`InfluenceDiag.cpp:357-359`) is a bare
`return id.idFromName(TCHAR_TO_UTF8(*variable));`. Two classes with the same method name,
the same contract sentence and opposite failure behaviour is the kind of asymmetry a caller
has no way to discover.

Both are `BlueprintCallable`. Exceptions are enabled in this module, the engine installs no
handler for them, and an unhandled C++ exception out of a reflected call terminates the
editor or the game. A Blueprint graph that erases a node it is not certain exists — or that
asks for an influence-diagram node id by a name assembled at runtime — is an ordinary thing
to write and a plausible crash.

A third thing to fix in the same pass, cheaper than either: every wrapped call in this
module catches `gum::NotFound` alone (`BayesianNetwork.cpp` 86, 112, 232, 249, 259, 277;
`InfluenceDiag.cpp` 77, 104, 125, 143, 175, 299, 312, 324), so `gum::OperationNotAllowed`,
`gum::SizeError`, `gum::DuplicateElement` and the rest propagate from calls that look
guarded. The handler should be `gum::Exception`, with the narrower catch kept only where the
log message depends on it.

Severity major rather than blocker: the trigger is a Blueprint author's own mistake rather
than untrusted input, and the module is not on the walking skeleton's critical path. Kind
`bug` rather than `drift`: the document half is corrected by CR-006, so what remains is a
crash.

The fix adds `try`/`catch` and a pre-check; it changes no signature, and it turns a
termination into the logged failure C-probabilistic already promises everywhere else, so
it is admissible on the 5.8 line under P-19's ordinary bar without needing the security
carve-out. It wants one story with F-025, since both are the same omission in the same two
files, and that story is not written here. US-022 rewrites this module for aGrUM 3.x and
must not be waited for: the migration moves the same unguarded calls.

## J-0103 | 2026-11-15 | DEC-012 | planner
- Event: decided
- Module: M-neo4j
- Story: US-014
- Decision: US-014 moves from done to ready with a Blocked by line naming F-020 and F-021, its behaviour description kept and all three Bolt defects listed as Known defect lines; the fixes go to US-030 and not into US-014. The backlog's preamble gains the general rule: a done story that acquires an open blocker finding leaves done, and the fix still arrives as its own story.
- Reason: The Definition of Done requires no open blocker finding, so a story carrying one does not meet it whatever its description says; done was carrying two separable claims (this is what the system does, and the project is finished with this) and the checker was right to refuse the overload. Correct under either CR-004 outcome, which is how it was decided before the answer was known.
- Changes: none

The checker's two standing ERRORs say US-014 is `done` while F-020 and F-021, both
`blocker`, are open against it. They are right and the backlog was wrong, but not in the way
it first looks, so the resolution had to separate two rules that had been collapsed into one.

**The rule that holds.** A finding's fix arrives as its own story, never by reopening the
`done` story the defect lives in. Its reason is sound: a `done` story describes what the
system does today, and reopening it makes that description false while the fix is pending.
F-008's fix was US-028, and F-008 kept naming US-005 and US-008 as where the defect sits.
That rule is unchanged, and the three admitted Bolt fixes go to US-030, not into US-014.

**The rule that was missing.** Nothing said what happens to a `done` story that acquires an
open blocker. The backlog read as though the first rule settled it — keep the story `done`,
put the fix elsewhere — and that is what produced the ERROR, because the Definition of Done
in `sprint.md` requires a tester report with no open blocker finding. A story carrying one
does not meet it, whatever its description says. The two claims are separable: *this is what
the system does* and *the project is finished with this* are different statements, and
`done` was carrying both.

So US-014 moves to `ready` with `Blocked by: F-020, F-021`, its behaviour description kept
and extended with all three defects as `Known defect:` lines, and the new rule written into
the backlog's preamble so the next case does not need re-deciding. `ready` is earned rather
than assigned: the story passes INVEST, its acceptance is in Given/When/Then, its `Tests:`
path exists with 36 passing tests, and M-neo4j lists it in `Satisfies`, which is the
Definition of Ready in full.

**Alternatives rejected.** Re-pointing F-020's and F-021's `Story:` fields at the fix story
would clear the ERROR and lose what the field is for — where the defect sits — which is
exactly the information F-008 was careful to keep. Downgrading the severities is not the
planner's to do and would be dishonest besides; both findings are credential- or
memory-safety defects in a network parser. Leaving the ERROR standing and annotating it as
known would train the team to read the checker's output as advisory, which is worse than any
status.

**Why this call is right under either CR-004 outcome,** which is how it was made before the
user's answer was known. If the carve-out is approved, the two blockers are admitted, get a
story, close, and US-014 returns to `done` — the status change is temporary and accurate
throughout. If it is declined, the blockers stay open for the life of the 5.8 line and
US-014 stays `ready`, which keeps the cost of that decision visible in the backlog instead
of hidden behind a story that reads as finished. The user then chose (A), so the first path
is the one being taken; the decision did not depend on it.

## J-0104 | 2026-11-15 | DEC-013 | planner
- Event: decided
- Module: M-asr, M-dist, M-llm, M-tests, M-tts
- Story: US-027, US-028, US-029
- Decision: US-027, US-028 and US-029 go to in-progress, not done: the Definition of Done ends with the user accepting the story at review and no review has happened. US-029 is additionally not whole, and its unmet criterion is written up as three options with costs for the user to choose at review rather than amended to match the code.
- Reason: Marking a story done before the review would assert the user's acceptance on their behalf, which is the claim this sprint existed to replace with a check; in-progress is the only status in the vocabulary that is true of built, gate green, awaiting acceptance. US-029's criterion is a promise the user accepted and only the user withdraws it (F-018, DEC-005).
- Changes: none

Three stories' work landed this sprint and none of them becomes `done` here.

**US-027 and US-029 are complete in substance.** US-027's gate is green at 150 tests, 0
failures, warm 31.8 s, verified by the lead running it; US-028's nine properties carry
`Config`, the ini round-trip is pinned, and both tooltips state the residual. Every
criterion either story stated is met. They are still not `done`, because `sprint.md`'s
Definition of Done ends with "the user accepted the story at review", and the review has not
happened: no reviewer verdict and no tester report exists for any of the three in the
journal. `in-progress` is the only status in the vocabulary that is true of "built, gate
green, awaiting acceptance", and the transition to `done` belongs to the review, where the
user makes it. A planner writing `done` into the backlog the hour before the review would be
asserting the user's acceptance on their behalf, which is the precise claim this sprint
existed to replace with a check. Each story's `Open:` field says it is complete and what it
waits for, so nothing is lost by the honest status.

**US-029 is not complete, and that is recorded as a choice rather than a status.** Its second
acceptance criterion asks that `StartCapture` be driven with an unopenable device and return
false. No such input exists: `OpenCaptureDevice` treats any `AudioDeviceIndex` outside the
enumerated range as the system default and opens it, so index 9999 opens the real microphone
and returns true, which would also break the story's own last criterion that the file opens
no audio device. test-dev found this (F-018), declined to fake it, and pinned the
already-capturing early return instead, which is the only false path reachable without
hardware (DEC-005). That was the right call and it leaves the story short of its own text.

Three ways out are written onto the story for the user to choose at review, with costs: an
injectable capture seam in M-asr as its own story, which closes the criterion properly and
also makes the rest of the device-failure handling testable, but is a module change this
story's seventh criterion forbids; amending the criterion to the path that exists, which
costs nothing and leaves the field failure unpinned for the life of the line; or accepting
the gap. The planner recommends the first with the second applied to US-029 so it can close
honestly, and recommends against the third — not for its testing cost but because a story
whose acceptance criteria are knowingly unsatisfiable teaches the team that acceptance
criteria are aspirational.

What was deliberately not done: amending the criterion to match the code. It would have made
the story closeable today and the checker quiet, and it would have been the same move the
three contracts corrected this sprint made — letting the document follow the code without
saying that the promise changed. The criterion is a promise the user accepted; if it is to be
withdrawn, the user withdraws it.

US-027's second criterion *was* amended, and the asymmetry is deliberate rather than
convenient. That clause named `AdditionalPluginDirectories`, a mechanism measurement showed
makes the gate report PASSED on a deliberately failing test, so the criterion as written
could only be met by a gate that does not work. US-029's criterion describes an outcome that
is worth having and cannot be reached by the current code; US-027's described a means that
defeats its own end. The first is the user's to decide, the second is a correction. Both are
on the stories and both are reported.

## J-0105 | 2026-11-15 | CR-005 | planner
- Event: approved
- Decision: Approved by the planner as a minor architecture change. No module boundary moves, no contract breaks, no story is added or dropped, and the mechanism was built, measured and verified before this CR: the lead set gates.task_completed from it in J-0095. ADR-0017 amends ADR-0016; architecture.md's Decisions section cites ADR-0017 amending ADR-0016, following the existing ADR-0015/ADR-0009 convention in the same file. What goes to the user at review is the one edited acceptance criterion on US-027, not the mechanism.
- Changes: none

## J-0106 | 2026-11-15 | CR-005 | planner
- Event: implemented
- Evidence: architecture.md's ADR-0016 Decisions bullet rewritten to the junction and the load-path assertion, citing ADR-0017 amending ADR-0016; Design version 3 to 4; M-dist Paths gains FANTASIA/run-tests.ps1. US-027's second acceptance criterion replaces AdditionalPluginDirectories with the junction at <harness>/Plugins/FANTASIA and the Makefile.bin discard, and a new criterion requires a non-zero exit when the editor log's InternalLoadLibrary line for FANTASIATests names any directory other than the one the run compiled into. ADR-0017 recorded at J-0099 with both traps and their measured evidence in its rejected alternatives.
- Changes: architecture.md, backlog.md

## J-0107 | 2026-11-15 | CR-006 | planner
- Event: approved
- Decision: Approved by the planner as a minor contract change, with each contract going to the next integer. The code is unchanged and no interface moves, so there is no decision for the user to take: the only alternative to correcting the three sentences is three documents that knowingly lie. The integer bump is nonetheless right, because from a consumer's point of view a guarantee they coded against has been withdrawn, and softening it to a point release to match the size of the edit would repeat the mistake these sentences made. Remediation of the three underlying defects stays with the user: F-021 via US-030 (CR-004), F-025 and F-034 need a story in M-probabilistic, F-028 one in M-prolog.
- Changes: none

## J-0108 | 2026-11-15 | CR-006 | planner
- Event: implemented
- Evidence: C-neo4j Version 2: the ReadValue sentence now states that neither ReadByte nor the multi-byte readers nor the length-driven cases compare against Data.Num(), that HasMore() is the only guard there is and does not make a malformed message safe, and that the sentence is rewritten again when US-030 lands the bounds checks. C-probabilistic Version 2: the exception sentence now says the handlers catch gum::NotFound alone and names the three unguarded groups (setBN plus its dropped proceed() return value, the two writers, erase on both classes with UInfluenceDiag::idFromName); the erase no-op claim and the single idFromName sentence are replaced by what each class actually does, and the EraseNode pin citation is removed because that test only erases a present variable. C-prolog Version 2: the first invariant now states one engine per process shared by every component, with the four consequences that follow (cross-component events with another's bindings, a shared knowledge base, one shared open query, and clauses outliving EndPlay because Shutdown is never called), and the specification note says there is one instance per process. Every claim verified in the source for this CR: BoltPackStream.cpp:171-214 and 216-311, BayesianNetwork.cpp:105-175 and 226-246, InfluenceDiag.cpp:339-359, BayesNet.h:304, variableNodeMap.h:176-177, SWIPrologThread.h:92-99, SWIPrologThread.cpp:65-77, SWIPrologComponent.cpp:63-89. F-034 opened at J-0102 for the two unguarded paths F-025 does not name.
- Changes: contracts/C-neo4j.md, contracts/C-probabilistic.md, contracts/C-prolog.md

## J-0109 | 2026-11-15 | CR-007 | planner
- Event: approved
- Decision: Approved by the planner as a minor contract change, point releases on all four. Nothing breaks: C-llm and C-rest-tts gain a configuration route that did not exist, C-asr gains evidence for invariants it already stated, C-build gains a mechanical check for an invariant it already stated. Option (C) was explicitly not taken: replacing F-008's sentences with all secret-bearing properties carry Config would be true and misleading, because a reader takes it to mean their key cannot reach an asset and F-031 says it still can. The residual F-031 is the user's to accept and is not accepted here.
- Changes: none

## J-0110 | 2026-11-15 | CR-007 | planner
- Event: implemented
- Evidence: C-llm 1.1: the Config property is described as a Config UPROPERTY with the reason the specifier must sit on the owning property, and the P-15 invariant is split into the ini route that is safe and the Details-panel route that is not (F-031), with the reflection sweep and round-trip tests cited. C-rest-tts 1.1: Headers is Config, and the same split. Both gain SecretConfigTests.cpp on their Tests line. Verified fixed in the source at OpenAICompatibleComponent.h:95 and RESTTTSComponent.h:322, both UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, ...), tooltips stating the residual. C-asr 1.1: the capture component's 25 tests replace the no-tests-at-adoption sentence with the backend dispatcher still named; the two stated capture invariants cite Defaults.ContractInvariants; F-017 and F-019 are named where a reader meets them; the StartCapture error sentence separates the pinned already-capturing path from the device-open path no value of AudioDeviceIndex provokes (F-018), verified at WhisperCaptureComponent.cpp:330-380 and 525-566. C-build 1.1: the BuildId invariant records that run-tests.ps1 checks both the engine-version refusal and the .modules-against-CompatibleChangelist comparison, and the no-tests invariant is narrowed to the clauses still unchecked rather than deleted.
- Changes: contracts/C-llm.md, contracts/C-rest-tts.md, contracts/C-asr.md, contracts/C-build.md

## J-0111 | 2026-11-15 | CR-008 | planner
- Event: proposed
- Type: team
- Class: major
- Impact: team.json only, and the lead's to edit. Two fields. (1) build-dev.owns gains FANTASIA/run-tests.ps1. The file exists, it is 18.9 KB of PowerShell, it is the project's only gate, and no agent owns it: with ownership in observe mode nothing is blocked today, but the first agent to edit the gate would do so unowned, and enforcement cannot be switched on over a gap. ADR-0016 already said the path would be added and ADR-0017 repeats it. (2) design_version goes from 3 to 4, following architecture.md, which CR-005 bumped. The checker warns on the mismatch until it is done, which is the mechanical nag that should get it done rather than a defect. Modules: M-dist's Paths line already names the file (CR-005). Agents: build-dev owns bootstrap and enable-plugins.ps1 in the same module and is the obvious owner; test-dev, tester and security own paths under FANTASIA/Source/FANTASIATests/Private/ and none of them should own the gate, which compiles and boots the editor rather than asserting anything. No other agent's owns list changes. Nothing else in team.json needs editing: the agent roster, modes and module assignments are unaffected.
- Options: (A) Add the path to build-dev.owns and set design_version to 4. (B) Add the path and leave design_version, which keeps a standing checker warning and makes the next team change start from an inconsistent file. (C) Leave the gate unowned, which means the project's one mechanical check is the one file no agent is responsible for. Cost of not changing: ownership cannot leave observe mode while a file this load-bearing is unowned, and the checker's design-version warning stays on, which trains the team to read warnings as background noise.
- Changes: none

Raised by T-08 rather than acted on, because team.json is the lead's file and a team or ownership change is major and needs the user's approval. Both edits are mechanical and neither is a judgement call: the file exists and belongs to the module build-dev owns, and the design version follows architecture.md, which CR-005 moved to 4. This is recorded as its own CR rather than mentioned in a report so that it cannot be lost between sprints -- it is the second time the path has been owed, ADR-0016 having named it already.

## J-0112 | 2026-11-15 | CR-004 | planner
- Event: implemented
- Evidence: principles.md P-19 amended: the title names the carve-out, the statement adds the admissibility test (a journal finding with Kind=security, opened by the security engineer, at blocker or major) with the near-misses that do not qualify and the scope limit that the admitted item is the fix alone, and the rationale's F-011 sentence is replaced by why the carve-out exists, with F-012 as the non-security case the bar still refuses. backlog.md: US-030 (F-021, ready), US-031 (F-020, ready) and US-032 (F-011, draft) written in the security engineer's recommended order and chained by Depends on; the preamble states the carve-out and that F-012 stays deferred; US-014 moves to ready with Blocked by F-020, F-021 (DEC-012, J-0103). architecture.md: M-neo4j and M-tests Satisfies gain US-030, US-031, US-032. contracts/C-neo4j.md: corrected under CR-006 to describe the unbounded reader as it is today and marked as being rewritten again when US-030 lands, so it never describes a future. Checker: the two standing ERRORs about US-014 and its open blockers are cleared.
- Outcome: US-030, US-031, US-032
- Changes: principles.md, backlog.md, architecture.md

**P-19.** The title now reads "admits only minor fixes and security fixes, and minor is
measured by surface", so the carve-out is visible before the statement is read. The
statement keeps the surface test unchanged for everything else and adds the exception with
its test spelled out — a finding recorded in the journal with `Kind=security`, opened by the
security engineer, at `blocker` or `major` — followed by what does *not* qualify, because the
near-misses are where this will be argued: a minor security finding, a security-adjacent
fix, and a finding any other author rated. The scope limit is in the statement rather than
left to P-07: the admitted item is the fix and nothing travelling with it, and the contract
edit is the minimum that describes the new behaviour.

**The rationale's last sentence is replaced, not deleted.** It cited F-011 as the price the
bar deliberately paid, and that is now false. In its place the rationale says why the
carve-out exists: a security fix in a module whose behaviour a contract describes almost
always touches that contract *precisely because* it changes what crosses a trust boundary,
so measuring surface refused these fixes by where the defect lived rather than by what it
cost the user; three credential-or-memory defects in one module refused by one clause is a
mechanism selecting against the findings it most needs to admit, not a price. It also records
why the test is provenance rather than opinion — an exception reading "unless it is
important" would restore the severity judgement the principle was written to remove, and a
coder cannot grant themselves an author, a kind or a severity.

**The principle keeps its teeth, and the example is now F-012.** Nothing in `BoltProtocol.h`
carries `FANTASIA_API`, so the Bolt message layer cannot be linked from the test module at
all; exporting it widens C-neo4j's surface, so for the life of the 5.8 line the handshake,
HELLO, LOGON, RUN, PULL and the dechunker are not merely untested but untestable. That is a
real price, currently paid, and charged deliberately. F-012 was chosen over the other
refusals on purpose: it is minor and quality and sits in the *same module* as all three
admitted findings, which demonstrates in one example that the module is not the criterion
and that the new test refuses as readily as it admits.

**Three stories, not one.** US-030 (F-021, bounds on the PackStream reader and the
dechunker), US-031 (F-020, server authentication on the TLS path), US-032 (F-011, transport
selection), in the security engineer's recommended order and chained by `Depends on` because
all three touch `Neo4jComponent.cpp` or `BoltPackStream.cpp`. They are separate because each
is independently shippable and because two of them carry a user-visible behaviour change
that deserves its own decision: US-031 breaks a `bolt+s://` connection to a self-signed
server, whose migration is one character (`bolt+ssc://`) but whose first report will be "the
update broke my database"; and US-032's fix has three shapes, one of which deletes a `Config`
UPROPERTY. Bundling them would put three such decisions into one review, and the one thing
J-0098 flagged as certain to be tested is scope discipline.

US-030 and US-031 are `ready`: INVEST holds, the acceptance criteria decide pass or fail
without interpretation, the `Tests:` paths are under
`FANTASIA/Source/FANTASIATests/Private/Security/`, which the security engineer owns, and
M-neo4j and M-tests now list all three in `Satisfies`. US-032 is `draft`, because its shape
is a decision the user takes: remove `UseSSL` (what every official Neo4j driver does, and a
Blueprint-surface change the carve-out now permits), refuse to connect on the mismatch, or
warn and continue. The planner recommends removal with refusal as the fallback, and says so
on the story rather than choosing.

Three decisions are left open on the stories on purpose, because each is a number or a choice
that should be stated in the contract rather than settled in code: US-030's recursion depth
limit and maximum message size, which have to be justified against a legitimately large
`PULL` response; and US-031's trust store, where the engine's bundle and OpenSSL's
`SSL_CTX_set_default_verify_paths` give different answers on a Windows machine with a
corporate root.

**What this does not do.** It does not select or sequence a sprint — the user accepts that at
review. It does not touch `sprint.md`, whose "Not in this sprint" sentence about P-19 is now
false and whose replacement wording goes to the lead. US-020 and US-021, the 5.8 ship, now
wait on US-030 and US-031 at least; whether they wait on US-032 depends on which shape it
takes, and that is part of the same acceptance. And it does not pre-write the fixed behaviour
into C-neo4j: the contract was corrected under CR-006 to describe the unbounded reader as it
is today, and US-030 rewrites that clause when the bounds checks land, so the contract never
describes a future.

## J-0113 | 2026-11-15 | NOTE | planner
- Changes: FANTASIA/Source/FANTASIA/CLAUDE.md

The runtime module's own state document said 'Two properties currently break this and it is a live finding (F-008)'. F-008 is fixed (J-0096) and so is F-029 (J-0093), but the honest replacement is not 'all properties carry Config now' -- that reads as 'your key cannot reach an asset', and F-031 says it still can. The secrets invariant is rewritten to state both halves in the order a coder needs them: all nine secret-bearing properties in the tree carry the specifier and a reflection sweep fails if any loses it; the specifier must sit on the owning property because LoadConfig does not descend into a non-Config struct property; a class needs Config = Game or its properties resolve against DefaultEngine.ini; and Config does not make a property transient, so every one of those nine is still EditAnywhere and a key typed into the Details panel is serialised into the asset exactly as before (F-031, open, and only the user can accept it). The invariant now reads: configuration is the route that is safe, not the only route. The tooltip rule is kept beside it, because an over-claiming tooltip invites the behaviour it appears to forbid -- the lesson of DEC-011, where T-04's 'stays out of any asset' had to be rewritten -- and OpenAICompatibleComponent.h and RESTTTSComponent.h are named as the models to copy, since a coder adding a tenth secret property will look for one.

## J-0114 | 2026-11-15 | F-035 | reviewer
- Event: opened
- Story: US-027
- Module: M-dist
- Kind: bug
- Severity: major
- Repro: FANTASIA/run-tests.ps1: the harness root, the report directory and the editor log are fixed paths under %LOCALAPPDATA%\FANTASIA-Gate\FantasiaGate, and the report directory and log are deleted at the start of every run, so two overlapping gate runs share one harness, one report and one log. A run whose editor writes no report can read the other run's report and exit 0.
- Changes: none

The gate is the project's one mechanical verification and `gates.task_completed`, so every
agent runs it at the end of every task. Its harness path, its report and its editor log are
all fixed, single locations: `$env:LOCALAPPDATA\FANTASIA-Gate\FantasiaGate`, with
`Saved\GateReport` and `Saved\Logs\gate.log` deleted unconditionally at the start of each
run (run-tests.ps1, the layout and "Both the report and the log are evidence about THIS
run" blocks). Two runs that overlap therefore share one project, one report and one log.

DEC-006 (J-0085) already records the hazard as advice -- "Run the gate alone: two
concurrent runs share one harness project and delete each other's report" -- but nothing in
the script detects or refuses the overlap, and the team's own dispatch runs coders in
parallel (S-01 ran T-04, T-05 and T-07 together, each owing the gate at task end).

The benign outcome is a false failure: run B deletes run A's report, A exits 3 "the editor
wrote no report" although its editor ran. The outcome that matters is the opposite one.
Two UnrealEditor-Cmd processes on the same .uproject do not coexist well; if B's editor
exits without writing a report while A's writes one into the shared directory, B reads A's
report, sees 150 passing tests and exits 0 -- a green run whose editor verified nothing,
which is the single failure mode ADR-0016 and build-dev's profile name as making a gate
worse than none. The load-path guard does not catch it: both runs read the same shared
`gate.log`, and the line they find names the same directory either way.

Fix, cheapest first: take an exclusive lock (a lock file, or a named mutex) around the
compile-and-editor phase and refuse with its own exit code when it is held, so a concurrent
run is named rather than silently mixed; or give each run its own report directory and log
path (PID or GUID) while keeping the shared Intermediate, which removes the cross-talk but
not the two-editors-one-project problem. Not a defect in any acceptance criterion of
US-027 -- the criteria say nothing about concurrency -- but it is a defect in the gate as
the project now uses it.

## J-0115 | 2026-11-15 | F-036 | reviewer
- Event: opened
- Story: US-027
- Module: M-dist
- Kind: quality
- Severity: minor
- Repro: FANTASIA/run-tests.ps1 param block: -MinTests defaults to 109 while the suite the gate runs reports 150 (verified: PASSED, 150 tests, 0 failures, 34.5 s). The shrink guard therefore tolerates the silent loss of 41 tests. 134 is the define-safe floor (150 minus the 16 tests behind WITH_AGRUM_BINDING).
- Changes: none

`-MinTests` defaults to 109, which is what US-027's acceptance names, so the script
conforms to the criterion as written. The number has stopped doing its work. The suite the
gate now runs reports 150 tests, so the guard whose message reads "A suite that shrank
silently is not a pass" accepts the silent loss of 41 of them -- which is every test this
sprint added, plus the two the gate itself was built to prove. The failure mode is not
hypothetical: J-0090 is a run where a stale binary cost the suite 43 tests and the
minimum-count check is what caught it, at a time when the gap happened to cross 109.

A floor cannot simply be set to 150, because 16 of the tests sit behind
`WITH_AGRUM_BINDING` (BayesianNetworkTests.cpp and InfluenceDiagTests.cpp, entirely inside
one `#if`), so a build without the aGrUM binding legitimately reports 134. 134 is therefore
the define-safe floor today, and the number should move with the suite whenever a sprint
adds tests, which is a line in the sprint's close rather than a mechanism.

Related and worth deciding at the same time: exit 4 ("the report names zero tests") appears
to be unreachable by the obvious input. Running the gate with a prefix that matches nothing
(`-Tests FANTASIA.ZzzNoSuchArea -MinTests 1`) produces exit 3, because the editor writes no
report at all rather than an empty one -- verified, 19.7 s, "FAILED (3): the editor wrote
no report". The gate still refuses the run and still says which of the three happened, so
the acceptance criterion holds; but the exit-4 branch has never been seen to fire and may
be dead.

## J-0116 | 2026-11-15 | F-037 | reviewer
- Event: opened
- Story: US-027
- Module: M-tests
- Kind: bug
- Severity: minor
- Repro: 152 tests are declared in FANTASIA/Source/FANTASIATests/Private and the gate's report names 150. The missing two are FANTASIA.ElevenLabs.WordsFromCharacters (ElevenLabsProtocolTests.cpp:31) and FANTASIA.ElevenLabs.ApplyPronunciations (:66): each full path is also an internal node of the automation tree because later tests add children under it, so the base case never runs. Verified by diffing the exported report's fullTestPath set against the names declared in the source.
- Changes: none

The suite declares 152 tests and runs 150. Comparing the gate's exported report against
the names declared in the source identifies the two that never run:

- `FANTASIA.ElevenLabs.WordsFromCharacters` (ElevenLabsProtocolTests.cpp:31)
- `FANTASIA.ElevenLabs.ApplyPronunciations` (ElevenLabsProtocolTests.cpp:66)

Both are full test paths that are also internal nodes of the automation tree, because
later tests in the same file add children under them -- `...WordsFromCharacters.Empty`,
`.SingleChar`, `.MultipleSpaces`, `.TrailingSpace` (lines 214-260) and
`...ApplyPronunciations.MultipleKeys`, `.EmptyText` (lines 374-389). The framework keeps
the node, not the test, so the two base cases are registered and never executed or
reported.

Three consequences. The base case of each function is untested while appearing tested:
`WordsFromCharacters` on ordinary input and `ApplyPronunciations` on a normal
substitution are exactly the cases the children do not cover. Every documented test count
in the project is two too high -- `Source/FANTASIATests/CLAUDE.md` and
`docs/ateam/verification/test-dev.md` both say 109, the real pre-sprint figure is 107
(the eleven tracked files declare 109 at HEAD; the gate reported 107 against the
pre-sprint binary in J-0090, which was read there as the stale DLL alone). And the
gate's `-MinTests` default was taken from the documented number rather than from a run
(F-036).

Pre-existing, not this sprint's work, and none of this sprint's 43 new names collides:
checked by diffing the report against the declared names, where the only two missing are
these.

Fix: rename the two parents to a leaf name (`...WordsFromCharacters.Basic`,
`...ApplyPronunciations.Basic`) and correct the counts in the two state documents. A
cheap guard also exists against the class of defect: no test's full path may be a prefix
of another's, which is checkable in a test over `TObjectIterator` or in the gate itself.

## J-0117 | 2026-11-15 | F-038 | reviewer
- Event: opened
- Story: US-028
- Module: M-tts
- Kind: security
- Severity: minor
- Repro: FANTASIA/Source/FANTASIA/Public/RESTTTSComponent.h:311-320 -- the Headers tooltip warns only that an asset which ALREADY holds a key keeps it ('this specifier does not clean it retroactively') and never states that a key typed into the Details panel today is still serialised into the asset, which the other four rewritten secret tooltips all state. docs/ateam/contracts/C-rest-tts.md (1.1) asserts 'The Headers tooltip states this' of exactly that residual.
- Changes: none

T-09 rewrote four secret tooltips after F-031 (DEC-011) and they read accurately: each one
says the value is read from `DefaultGame.ini`, that a key typed into the Details panel is
*still* saved into the Blueprint or level asset because `Config` adds an ini source and
does not make the property transient, and that an asset which already holds a key keeps it
until the value is cleared and the asset re-saved.

`URESTTTSComponent::Headers` was not among the four, because F-031 called it "accurate"
when it was written (J-0082). Read beside the other four it is the weakest of the five.
What it says is `RESTTTSComponent.h:311-320`: the value "is read from the host project's
`DefaultGame.ini` and must be set there, not typed into an asset", and then, in a note,
that "a project that **already** typed a key into a Blueprint or actor asset still has that
key stored in the asset: this specifier does not clean it retroactively". The live residual
-- that typing a key into the Details panel *today*, after the fix, still writes it into
the asset -- is the one thing it does not say, and the retroactive framing invites the
reader to conclude that from now on the property is handled. That is a softer form of the
over-claim F-031 opened, in the one place an integrator reads about their key.

C-rest-tts (version 1.1) states the residual and then says "The `Headers` tooltip states
this." It does not. C-llm's "The property's tooltip states all of this" is near enough to
true for the OpenAI property, except that the contract also names the ini section
`[/Script/FANTASIA.OpenAICompatibleComponent]` and no tooltip names a section (F-033,
still open, says why that matters for the ACE REST subclass).

Fix: one sentence in the `Headers` doc comment, matching the other four -- "A key typed
into the Details panel is still saved into the Blueprint or level asset: this specifier
adds an ini source, it does not make the property transient." That is cheaper than editing
the contract, and it makes the contract's sentence true rather than removing it.

## J-0118 | 2026-11-15 | F-039 | reviewer
- Event: opened
- Story: US-029
- Module: M-asr
- Kind: quality
- Severity: minor
- Repro: docs/ateam/contracts/C-asr.md (1.1), Invariants: 'AudioDeviceIndex defaults to -1 and means the operating system's default input device; so does any index outside the enumerated range (pinned by Defaults.ContractInvariants)'. The cited test (WhisperCaptureComponentTests.cpp:128-136) asserts only the -1 default and the two filter booleans; the out-of-range behaviour is unpinned and unpinnable while US-029 forbids opening a device (F-018).
- Changes: none

C-asr 1.1's invariant now reads: "`AudioDeviceIndex` defaults to -1 and means the operating
system's default input device; so does any index outside the enumerated range (pinned by
`Defaults.ContractInvariants`)."

`FANTASIA.WhisperCapture.Defaults.ContractInvariants`
(WhisperCaptureComponentTests.cpp:128-136) asserts three things: `AudioDeviceIndex == -1`,
`bEnableHighPassFilter` true and `bEnableNoiseGate` true. It says nothing about an index
outside the enumerated range, and by F-018 no test can: reaching that behaviour means
calling `StartCapture`, which opens the real microphone, and US-029 forbids opening a
device. The parenthetical attaches the pin to both halves of the sentence, so the one half
that is read from the code rather than pinned -- and that is the subject of an open finding
-- reads as covered by a test.

This is the sentence C-asr's own closing invariant warns about ("every invariant above
that no test name is cited for is read from the code, not pinned"): a test name cited
beside an unpinned claim is worse than no citation, because the closing sentence then
certifies it.

Fix: move the citation to the first clause only -- "`AudioDeviceIndex` defaults to -1
(pinned by `Defaults.ContractInvariants`) and means the operating system's default input
device; so does any index outside the enumerated range, which is read from the code and
not pinned (F-018)."

## J-0119 | 2026-11-15 | F-040 | reviewer
- Event: opened
- Story: US-029
- Module: M-tests
- Kind: quality
- Severity: minor
- Repro: FANTASIA/Source/FANTASIATests/Private/WhisperCaptureComponentTests.cpp:76-78 concatenates 'FANTASIA.WhisperCapture.' with each case name inside the WHISPER_CAPTURE_TEST macro, so none of the 25 full test paths appears as a literal in the tree: grepping the name the gate prints on failure, or the report's fullTestPath, finds nothing, and a grep for IMPLEMENT_*_AUTOMATION_TEST counts this file as 1 test rather than 25.
- Changes: none

The 25 capture tests are declared through a local wrapper
(WhisperCaptureComponentTests.cpp:76-78):

    #define WHISPER_CAPTURE_TEST(Id, Name) \
        IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhisperCapture##Id, "FANTASIA.WhisperCapture." Name, CaptureTestFlags) \
        bool FWhisperCapture##Id::RunTest(const FString& Parameters)

so no test's full path exists as a literal anywhere in the tree. The project's other 125
tests do carry theirs. The cost is specific rather than aesthetic: the gate prints the full
path when a test fails ("[gate] Fail: FANTASIA.WhisperCapture.Resample.LinearInterpolation"),
and the first thing anyone does with that string is grep for it, which now returns nothing.
The same is true of the `fullTestPath` in the exported report, and of any count or
cross-check script that greps for `IMPLEMENT_*_AUTOMATION_TEST` -- this file answers 1 for
25, which is how the 152-versus-150 discrepancy in F-037 stayed invisible.

The macro is not overengineering: it is used 25 times and it keeps the area prefix from
drifting. The defect is only the concatenation of the prefix, and it is a one-line fix that
keeps every benefit:

    #define WHISPER_CAPTURE_TEST(Id, Name) \
        IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhisperCapture##Id, Name, CaptureTestFlags) \
        bool FWhisperCapture##Id::RunTest(const FString& Parameters)

with each call site naming its full path, `WHISPER_CAPTURE_TEST(DefaultsConfig,
"FANTASIA.WhisperCapture.Defaults.Config")`. Since a strictly better variant costs one
line, the choice is not a matter of taste, which is why this is logged rather than left as
a reviewer's remark.

## J-0120 | 2026-11-15 | F-038 | lead
- Event: fixed
- Evidence: speech-dev rewrote the @brief of URESTTTSComponent::Headers (RESTTTSComponent.h:311-323) to core-dev's T-09 wording, keeping the REST-specific xi-api-key example. Verified by the lead: the comment now states the ini route, that a key typed into the Details panel is still saved into the Blueprint or level asset, and that the specifier adds an ini source without making the property transient; the @note's retroactive clause is unchanged and the UPROPERTY line, name and type are byte-identical. Gate: PASSED, 150 tests, 0 failures, one run, no retry.
- Changes: none

The phrase that had to go was 'must be set there, not typed into an asset'. As an instruction it was fine; sitting where a reader looks for what the engine guarantees, it read as a guarantee, which is the F-031 over-claim in a milder form. It is replaced by 'should be set there' plus the mechanism clause, so a reader who wonders whether Config protects them gets an answer instead of a rule.

This also makes C-rest-tts version 1.1 true as written. That contract asserts the Headers tooltip states the residual; until this edit it did not, which would have made it the fourth contract in one sprint claiming a mitigation that does not exist. Correcting the comment was cheaper and more honest than weakening the contract to match it.

Two notes from speech-dev worth keeping for whoever picks up the neighbouring findings. F-032 does not apply to this property at all: Headers is a top-level TMap on the component, not a field of a preset-replaced FOpenAICompatibleConfig, and URESTTTSComponent has no preset function. And F-033 is now sharper rather than unchanged -- the new text endorses exactly one safe route and still does not say how to spell its ini section, which is an argument for fixing F-033 rather than a regression from this edit.

## J-0121 | 2026-11-15 | DEC-014 | build-dev
- Module: M-dist
- Decision: The gate holds an exclusive file-handle lock on its harness for the whole run, refusing a concurrent run with exit 11, and proves the report's provenance by requiring the pid in the report's device instanceName to be the editor process it started itself.
- Reason: F-035: a false pass was possible when a second run's editor wrote no report while a first run's report sat in the shared report directory. A lock alone enforces serialisation but cannot make a report trustworthy (a killed run or a hand-started editor can leave one); a per-run path alone hides the deeper hazard, since two runs also share the plugin's Intermediate, the harness Intermediate and the tracked Binaries the link step rewrites. The pid check is direct provenance, the same standard as the InternalLoadLibrary load-path guard applied to the report. FApp::GetInstanceName is ComputerName-ProcessId and is not overridable, so the pid is the only token available; Start-Process -PassThru is used in place of the call operator to obtain it.
- Changes: none

Alternatives rejected: (a) a lock alone, as DEC-006 advised in prose - serialises but leaves a stale or foreign report readable; (b) per-run report, log and harness paths alone - avoids collision rather than proving provenance, and multiplies harness directories that something must then collect; (c) stamping a run token into the report via -InstanceName= - the engine hard-codes the instance name in FApp::GetInstanceName (Engine/Source/Runtime/Core/Public/Misc/App.h:276) and parses no such argument; (d) comparing the report's write time against the editor launch time - a concurrent run writes a report newer than the second run's launch, so it does not discriminate. Report and log paths still carry a per-run id, as hygiene so runs cannot land in each other's evidence, but no verdict rests on that separation. Evidence from earlier runs is deleted wholesale at the start of a run rather than aged out, because holding the lock means no other gate run exists and all of it is dead. The lock is a handle opened for writing and shared for reading, so the holder's identity can be read and quoted in the refusal while a second acquisition is denied, and nothing can leave the lock stale: the operating system closes the handle when the process ends, crash or Ctrl-C included. Demonstrated: two runs started three seconds apart gave exit 11 in 0.6 s for the second (naming the holder) and exit 0 for the first; a genuine 150-test zero-failure report from another editor, planted at a live run's own report path while that run's editor wrote none, gave exit 3 rather than the green verdict the old script would have printed.

## J-0122 | 2026-11-15 | DEC-015 | build-dev
- Module: M-dist
- Decision: -MinTests defaults to 134, and exit code 4 is retired as unreachable rather than kept in the documented list.
- Reason: F-036: the old default of 109 tolerated losing 41 of the 150 tests the suite now runs. 134 is the floor the suite cannot fall below in any supported configuration: 150 run today, of which 16 are compiled only with WITH_AGRUM_BINDING (all of BayesianNetworkTests.cpp and InfluenceDiagTests.cpp, the only conditional compilation in the test tree). Exit 4 claimed to catch a report naming zero tests; measured, a prefix matching no test makes the editor exit without exporting any report, which is exit 3, and -MinTests is already required to be at least 1 so a zero-test report would be exit 6.
- Changes: none

Counts verified in the tree, not taken from the documents: 127 plain IMPLEMENT_SIMPLE_AUTOMATION_TEST declarations plus 25 invocations of the WHISPER_CAPTURE_TEST wrapper macro in WhisperCaptureComponentTests.cpp = 152 declared; two of those (FANTASIA.ElevenLabs.WordsFromCharacters at ElevenLabsProtocolTests.cpp:31 and FANTASIA.ElevenLabs.ApplyPronunciations at :66) are also internal nodes of the automation tree and do not run (F-037), so 150 run, as the report confirms. The floor makes no assumption that F-037 stays unfixed: fixing it raises the suite to 152 and a floor is only a lower bound, so 134 holds either way. The one thing 134 tolerates is a build without the aGrUM binding, which is a supported configuration and must not fail the gate. Exit 4's number is left vacant rather than reused, and the other codes keep their numbers, because they are quoted outside the script; the header says so. Measured: -Tests FANTASIA.NoSuchGroupZZZ exits 3 with no report written at all; -MinTests 151 exits 6 against the 150 that ran.

## J-0123 | 2026-11-15 | F-035 | lead
- Event: fixed
- Evidence: build-dev implemented both halves (DEC-014, J-0121): an exclusive lock on <harness>/gate.lock held for the whole run, opened share-read so a refuser can quote the holder, exit 11 in 0.6 s; and a direct provenance check -- UE writes each device's instanceName as <computer>-<pid> (FApp::GetInstanceName), and the gate now requires that pid to be the editor process it started itself, obtained via Start-Process -PassThru. Verified by the lead with an independent run: 'report stamped by this run's own editor (pid 67840)', PASSED, 150 tests, 0 failures, 46.4 s.
- Changes: none

The proof that matters is not the refusal but the reproduction. build-dev planted a genuine 150-test zero-failure report from a different editor instance (ACER-31992) into the report directory of a run whose own editor wrote none, which is F-035's bad case exactly. The old script would have printed PASSED. The new one exits 3: 'the report ... was written by instance(s) ACER-31992, none of which is the editor this run started (pid 4076). It is someone else's evidence, so this run concludes nothing from it.' The InternalLoadLibrary load-path guard passed on that input, as expected -- it proves which DLL was loaded, not which editor wrote the verdict, so the pid check is what caught it.

The lock was kept alongside the provenance check rather than replaced by per-run paths, and the reasoning is worth preserving: isolating the report would have hidden the deeper hazard instead of removing it, because two concurrent runs also share the plugin's Intermediate, the harness Intermediate, and the tracked Binaries/Win64 that the link step rewrites. An OS file handle also cannot go stale -- it closes on crash or Ctrl-C. Per-run report and log paths were added as hygiene only; no verdict rests on that separation, so nothing has to collect them: holding the lock means no other run exists, and a run deletes all earlier evidence at its start.

Rejected, and recorded: stamping a token through -InstanceName= (the engine parses no such argument), and comparing the report's write time against the editor launch -- a concurrent run's report is newer than the second run's launch, so it does not discriminate and would not have caught this finding.

## J-0124 | 2026-11-15 | F-036 | lead
- Event: fixed
- Evidence: The -MinTests floor is 134 (DEC-015, J-0122), and build-dev verified the arithmetic in the tree rather than taking it from the lead or the documents: 152 declared (127 plain IMPLEMENT_SIMPLE_AUTOMATION_TEST plus 25 WHISPER_CAPTURE_TEST wrapper invocations), 2 shadowed per F-037, so 150 run -- matching the report's succeeded 148 + succeededWithWarnings 2 + failed 0 + notRun 0; and 16 behind WITH_AGRUM_BINDING, being all 9 of BayesianNetworkTests.cpp and all 7 of InfluenceDiagTests.cpp as whole-file #if blocks, with no other conditional compilation anywhere in the test tree. Floor bites: -MinTests 151 gives FAILED (6).
- Changes: none

The floor's assumption is stated rather than left implicit: 134 does not assume F-037 stays unfixed. Fixing the two shadowed tests raises the suite to 152, and a floor is only a lower bound, so 134 holds either way. The one thing it deliberately tolerates is a build without the aGrUM binding, which is a supported configuration the gate must not fail.

Exit code 4 is retired, and it was measured unreachable rather than reasoned so. With -Tests naming a group that does not exist, the editor exports no report at all -- Saved/GateReport does not exist afterwards -- so that run is exit 3, which is what the lead's own earlier probe also produced. Since -MinTests is already required to be at least 1, a zero-test report would be exit 6 if the engine ever wrote one. The other codes keep their numbers because they are quoted outside the script, and the header records that 4 is vacant and why.

Note for anyone counting tests from a grep: WhisperCaptureComponentTests.cpp declares one wrapper macro and 25 tests through it, so a naive grep for IMPLEMENT_*_AUTOMATION_TEST answers 1 for that file and 128 for the tree. That is how F-037's two shadowed tests stayed invisible, and it is the substance of F-040.

## J-0125 | 2026-11-15 | CR-009 | lead
- Event: proposed
- Type: story
- Class: major
- Impact: Stories: US-029's second acceptance criterion is rewritten to the already-capturing guard (both its When and its Then), its option (A)/(B)/(C) notes collapse to the decision taken, and a new story US-033 carries the injectable capture seam in M-asr; no other story moves. Modules: M-asr gains a private, non-exported indirection under US-033, not under this CR; M-tests gains the failure tests with it. Contracts: none change - the seam adds no exported symbol, so C-asr's interface is untouched, and its F-018 sentence (Version 1.1, CR-007) stays true until US-033 lands. Findings: F-018 stays open and is carried by US-033; F-017, F-019, F-039 and F-040 are unaffected. Tests: FANTASIA/Source/FANTASIATests/Private/WhisperCaptureComponentTests.cpp already pins the guard, so the amendment adds no test; US-033 adds the device-open and stream-start failure tests to the same file. Tasks in flight: none.
- Options: (A) The seam as its own story AND this story's criterion amended to the path that exists - the device-failure path gets pinned, but by US-033 and not by claiming US-029 already pins it. (B) Amend the criterion only: no work, US-029 closes today, and the device-failure path stays unpinned for the life of the line. (C) Accept the gap: leave the criterion unmet and recorded by F-018, which teaches the team that acceptance criteria are aspirational - the one thing this sprint existed to stop. (D) Don't change. Cost: the story's own acceptance stays self-contradictory in both directions, and the next reader cannot tell which half is wrong.
- Changes: none

US-029's second acceptance criterion cannot be satisfied, and F-018 names only half of why.
`StartCapture` has exactly three `return false` paths
(`FANTASIA/Source/FANTASIA/Private/WhisperCaptureComponent.cpp:531`, `:566`, `:581`), and
the criterion — "`StartCapture` returns false with `IsCapturing()` still false" — was
written against the two that need hardware:

- **:566**, `!OpenCaptureDevice()`. `bIsCapturing` is never set, so the criterion's *Then*
  holds. Unreachable: an `AudioDeviceIndex` outside the enumerated range falls back to the
  OS default and opens it, so no input makes this fire on a machine with a microphone
  (F-018).
- **:581**, `!AudioCapture->StartStream()`. Resets `bIsCapturing` to false, disables tick
  and closes the device, so the *Then* holds. Also needs a device that opens and a stream
  that then fails.
- **:531**, the already-capturing guard. The only path reachable without hardware, and the
  one T-07 pins (DEC-005) — **and it is the one path on which `IsCapturing()` is `true`
  afterwards**, necessarily, because `bIsCapturing` being true is its entry condition.

So the criterion's *When* excludes the only reachable path and its *Then* contradicts it.
Replacing the *When* alone, which is how option (B) was drafted, would leave a second
unsatisfiable criterion behind — differently worded and no more true. Both clauses move or
neither does.

**The user's decision at review: option (A) with option (B) applied — the seam as its own
story, and this story's criterion amended to the path that exists.** Taken together these
say that the device-failure path will be pinned, but not by US-029 and not by claiming it
already is.

**What the amendment must do.** State the already-capturing guard as what it is: a second
`StartCapture` on a running session returns false *while* `IsCapturing()` stays true, and
leaves the session undisturbed. That last half is worth more than the return value and is
free to assert: the guard returns before `++CaptureSessionId` and before the state reset at
`:538-559`, so the running session's buffer, session id and resample phase are untouched —
an invariant a future refactor could plausibly break. The phrase "with an unopenable
device" is deleted rather than reinterpreted, so no later reader believes that path is
covered, and the criterion says in its own words that the device-failure path is US-033's.

**What the amendment must not do.** It does not touch the story's other six criteria, all
of which are met; it does not reopen the seventh, which limits the `WhisperCaptureComponent.h`
edit to one `friend` declaration and is the reason the seam cannot live here; and it adds no
test, since `FANTASIA.Whisper.Capture.*` already pins the guard.

**The new story (US-033) is the larger half of the value.** Scoped to the one criterion it
would buy little. The seam makes both hardware paths testable — :566's device-open failure
*and* :581's stream-start failure, including that path's cleanup (flag reset, tick disabled,
`CloseCaptureDevice` called) — which is the failure a researcher with a busy or absent
microphone actually hits. Constraints: no exported symbol, no Blueprint-facing signature, so
it stays inside what P-19 admits on the 5.8 line; the indirection sits on a per-session path,
not a per-buffer one, so NFR-001 is untouched. F-018 closes when US-033 lands, not with this
amendment.

## J-0126 | 2026-11-15 | CR-009 | lead
- Event: approved
- Decision: Option (A): the injectable capture seam becomes its own story, and US-029's second acceptance criterion is amended to the already-capturing guard so the story closes on what it actually pins.
- Approver: user
- Changes: none

The user chose at sprint review, in these words: "Seam story + amend". Both halves of the criterion move, not only the When: the guard returns false precisely when IsCapturing() is true, so a rewrite that kept the Then would have replaced one unsatisfiable criterion with another. The amendment is the planner's to apply to docs/ateam/backlog.md; the seam itself is not in S-01 and is not scheduled by this approval.

## J-0127 | 2026-11-15 | DEC-016 | planner
- Event: decided
- Module: M-asr, M-tests
- Story: US-029, US-033
- Decision: US-029's amended StartCapture criterion names only the clauses the existing test asserts - the return value, IsCapturing() staying true, one warning, and the buffer's duration unchanged; the guard's other invariant (CaptureSessionId and the resample phase untouched) is recorded as a note on the story, not as a clause of the criterion. The old criterion is split in two rather than rewritten in place, so the SetMuted/ClearBuffer half keeps its text.
- Reason: A criterion clause with no assertion behind it is the defect the amendment exists to remove; FWhisperCaptureTestAccess has no CaptureSessionId accessor and the test's fast-path append leaves the resample phase zero on both sides, so asserting either would need a test-module edit CR-009 forbids or would pass vacuously. The two cases also stopped sharing a precondition: the guard's Given is a session that is running.
- Changes: backlog.md

CR-009's text offers three things as "free to assert" of the already-capturing guard — that
it leaves the running session's buffer, `CaptureSessionId` and resample phase untouched — and
only the first of the three is asserted today.
`FANTASIA.WhisperCapture.Controls.StartCaptureWhileCapturing`
(WhisperCaptureComponentTests.cpp:212-225) appends 8000 samples at 16 kHz, forces
`bIsCapturing`, expects one warning containing "already capturing", and then asserts
`StartCapture()` false, `IsCapturing()` true and `GetBufferDuration()` still 0.5 s. It does
not look at the session counter: `FWhisperCaptureTestAccess` has no accessor for
`CaptureSessionId`. It does not look at the phase either, and an assertion there would be
vacuous — `AppendToBuffer` goes through the resampler's 16 kHz fast path, where
`ResamplePhase` is zero before the call and zero after it, so the test would pass whether the
guard returned early or not.

So writing all three into the criterion would have cost one of two things: an edit to the
test module, which CR-009 forbids in the same breath ("adds no test") and which would reopen
a story the review had just closed to add a task for `test-dev`; or a criterion with a clause
no test keeps, which is the exact defect this amendment exists to remove. The clause is worth
keeping as an invariant — the guard returns before `++CaptureSessionId` and before the state
reset at `:538-559`, and a refactor that moved the guard below either would break it silently
— so it is recorded as an `Open:` note on US-029, where it reads as an invariant known and
unpinned rather than as acceptance the story claims. US-033 adds the session-counter accessor
for its own two failure paths; asserting the guard's half then costs one line, and US-033's
notes say so.

The second choice is the split. The old criterion bundled two unrelated cases under one
*Given* ("no capture is running"): the `StartCapture` refusal and the `SetMuted`/`ClearBuffer`
sequence. They no longer share a precondition — the guard's *Given* is a session that **is**
running — so they are two criteria, and the mute-and-clear half keeps exactly the text it
could always keep. Rewriting them as one would have forced a *Given* that contradicts itself.

## J-0128 | 2026-11-15 | NOTE | planner
- Finding: F-018
- Story: US-033, US-029
- Module: M-asr, M-tests
- Changes: none

This finding is carried by US-033, not by US-029, and stays open until US-033 lands.

US-029's acceptance no longer asks for the input this finding says does not exist: its
`StartCapture` criterion states the already-capturing guard, which is the one false path a
test can drive without hardware, and names the two hardware paths as US-033's (CR-009,
J-0126; DEC-016). So the conflict this finding reported — an acceptance criterion against a
story constraint that forbids opening a device — is gone, and what remains is the thing
itself: no value of `AudioDeviceIndex` reaches `OpenCaptureDevice`'s failure path on a
machine with a microphone, so the component's behaviour when the device will not open is
read from the code and not pinned.

US-033 closes it by indirecting the two calls rather than by choosing a property value:
`StartCapture`'s call to `OpenCaptureDevice`, and its call to `AudioCapture->StartStream()`.
Both hooks are private, settable only through `FWhisperCaptureTestAccess`, and consulted
once per capture session, so nothing about the shipped component changes — which is why the
sentence C-asr 1.1 states about this finding stays true after US-033 and gains the two tests
as its evidence.

## J-0129 | 2026-11-15 | CR-009 | planner
- Event: implemented
- Evidence: backlog.md: US-029's StartCapture criterion is split from the SetMuted/ClearBuffer case and states the already-capturing guard - returns false while IsCapturing() stays true, warns exactly once, leaves the buffered audio unchanged in duration - every clause of which FANTASIA.WhisperCapture.Controls.StartCaptureWhileCapturing (WhisperCaptureComponentTests.cpp:212-225) asserts today; the phrase 'with an unopenable device' is deleted; the three option notes collapse to the decision taken and to the guard's unpinned CaptureSessionId/phase invariant (DEC-016); Status stays in-progress (DEC-013). US-033 'Pin what the capture path does when the microphone will not open' is ready, Should, S, 5.8, with seven criteria covering both hardware paths (:566 device-open, :581 stream-start with its cleanup proven by DeviceSampleRate and DeviceNumChannels back to zero), the no-exported-symbol, no-Blueprint-surface and not-per-callback constraints as criteria rather than prose, and the 25 existing tests passing unchanged. architecture.md: US-033 joins M-asr's and M-tests' Satisfies lines, which the Definition of Ready requires. F-018 stays open and is carried by US-033 (J-0128). Checker before and after: 1 error, 17 warnings, and nothing about US-029 or US-033.
- Changes: backlog.md, architecture.md

Both halves of the user's decision are in the backlog: US-029's `StartCapture` criterion
states the path that exists, and US-033 carries the path that does not yet.

**The amendment.** The old criterion bundled the `StartCapture` refusal with the
`SetMuted`/`ClearBuffer` sequence under one *Given* ("no capture is running"), so it is split
rather than rewritten in place (DEC-016): the mute-and-clear half keeps its own text, and the
guard gets a *Given* of its own, a session that is running. Every clause of the new criterion
is asserted by a test that exists today —
`FANTASIA.WhisperCapture.Controls.StartCaptureWhileCapturing`, WhisperCaptureComponentTests.cpp:212-225
— and no clause is asserted by nothing. What the criterion does **not** say is that the
guard leaves `CaptureSessionId` and the resample phase untouched: `FWhisperCaptureTestAccess`
has no session-counter accessor and the test's fast-path append leaves the phase zero on
both sides, so those two clauses would have been a promise no test keeps, which is the defect
being removed rather than a second instance of it. They are recorded as an invariant on the
story instead, with the line numbers that make them true.

**US-033** is scoped to what the seam unlocks and not to the one criterion it replaces: both
hardware paths, `:566`'s device-open failure and `:581`'s stream-start failure including its
cleanup, each with its own observable consequence. The stream-start case is the one worth the
story — a refused `StartStream` must reset `bIsCapturing`, disable the tick and run
`CloseCaptureDevice`, and the test decides that mechanically by reading `DeviceSampleRate`
and `DeviceNumChannels` back to zero after a fake opener set them. The constraints are
criteria and not prose: no `FANTASIA_API`, no `UPROPERTY`, `UFUNCTION`, delegate or enum
change, hooks private and settable only through the `friend` the header already declares
(C-asr unchanged, P-19 satisfied on the 5.8 line), and nothing consulted per callback or per
sample, which the diff decides. Two hooks are the minimum, because the two failures sit on
different objects: one is the component's own method, the other a call on
`TUniquePtr<Audio::FAudioCapture>`, a concrete non-virtual type that would be far costlier to
fake than to indirect.

US-029 stays `in-progress`: the status change belongs to the review (DEC-013). F-018 stays
open and now says US-033 carries it. architecture.md is in this change for one reason only —
the Definition of Ready requires the modules a story touches to list it, so US-033 joins
M-asr's and M-tests' Satisfies lines. No module's responsibility, paths or contracts move,
and no contract changes: the seam adds no exported symbol.

## J-0130 | 2026-11-15 | F-017 | security
- Event: opened
- Corrects: J-0065
- Kind: security
- Severity: major
- Module: M-asr
- Story: US-029
- Repro: FANTASIA.WhisperCapture.Resample.ReadsOneFramePastInput; reachability simulated against the exact arithmetic: 44100/4410 every callback, 44100/2048 and 22050/1024 one per 20.6 s, none at 44100/1024 and none on 48000 or 32000 devices
- Changes: none

Severity ruling: **major**, not minor, and it is a security finding (memory safety). Not a
blocker. The rating is argued from reachability and consequence, not from the module.

**Reachability — nothing untrusted controls it, and nothing in the plugin chooses it.** The
over-read depends only on `SrcRate`, `NumFrames` and the carried `ResamplePhase`
(`WhisperCaptureComponent.cpp:1018-1056`); the sample *values* never enter the index
arithmetic. So the one input an attacker can influence — what the microphone hears — cannot
provoke it. `SrcRate` is the device's `PreferredSampleRate` (`:388`) and `NumFrames` is
whatever the backend delivers; the plugin only *requests* 1024 (`:471`). Both come from the
OS audio stack, i.e. from the trusted operator's own configuration. Simulating the exact
arithmetic (repeated `Phase += SrcToDst` in double, `float` rounding at the midpoint below
`NumFrames`):

| device | frames/callback | over-reads |
| :--- | :--- | :--- |
| 44100 | 4410 | every callback (~10/s) |
| 44100 | 2048 | 1 per 20.6 s of capture |
| 22050 | 1024 | 1 per 20.6 s of capture |
| 44100 | 1024, 512, 480, 441, 256, 128 | none in 19 min |
| 48000 / 32000 | 441, 480, 1024 | none — the ratio is an exact integer, so the phase is
  exactly representable and never lands below an integer boundary |

So: unreachable on the 48 kHz family (most Windows mics), reachable continuously on a 44.1
kHz device with large callbacks, reachable rarely on two configurations in between. No
attacker, no unusual setup, and no setting the plugin or the user can turn off.

**Consequence — it is not one click.** The read is bounded (`Phase < SrcEnd` in double, so
float rounding lifts `Idx0` to at most exactly `NumFrames`): at most `NumChannels` floats,
4-32 bytes, past the end of a backend-owned buffer, never a write. A fault needs that buffer
to end on a page boundary, which an 8-35 KB heap allocation does not; under Page Heap or
ASan it is a hard editor crash, which is why this bug blocks sanitising the capture path at
all. What lifts it above minor is that the value does not stay in the sample it corrupts:

1. `Frac` is about -1e-4, so `Lerp(S0,S1,Frac)` returns essentially the out-of-bounds value
   itself, which enters `AudioBuffer`.
2. `:436-444` takes the raw peak of the new samples and `SetRecentPeak` does
   `RecentPeakFixed.Set(FMath::RoundToInt(Value * 10000.f))` (`:986`) with no clamp. Any
   adjacent-heap bit pattern above ~2.1e5 — about 40% of exponents — overflows the
   conversion: a second undefined behaviour, which on x86-64 yields INT_MIN, so
   `GetRecentPeak()` returns about -214748. `RecentPeak` is the *only* input to both VAD
   branches (`:207`, `:277`: `bIsSpeaking = Peak > EffectiveThreshold`), so that window
   reports silence. NaN cannot reach it (`NaN > ExistingPeak` is false), so this half is
   about large finite garbage.
3. `:466` then runs `ApplyNoiseSuppression`, whose high-pass is an IIR with *persistent*
   state (`HpfX1/X2/Y1/Y2`, `:1109-1113`), cleared only in `StartCapture` (`:550`) and at
   `:943`. A non-finite value latches there for the rest of the session, and
   `NoiseGateEnvelope` with it. `TranscribeBuffer` zeroes NaN and Inf (`:755-766`) — but
   downstream of the filter, so a latched filter sends whisper pure silence with no error.

And the byte read is the same address on every callback, so a machine either never sees this
or sees it continuously: not a transient glitch but a per-deployment coin flip that then
stays flipped. On a 44.1 kHz device with 100 ms callbacks the realistic failure is "speech
detection never fires, or ASR silently returns nothing", with no log line and nothing in the
behaviour to suggest an out-of-bounds read. That is the gap between the behavioural lens
(one wrong sample, a click) and the memory-safety one.

**Why not a blocker.** No trust boundary is crossed. The buffer goes only to the in-process
`UWhisperSubsystem::TranscribeAudioData` (`:794`); nothing on this path writes the audio to
a file or sends it to a network service, so the 4 bytes of adjacent heap never leave the
process — and inside it they are perturbed by `Lerp`, mixed by a recursive filter and then
transcribed, a lossy non-invertible chain, so disclosure of adjacent memory is not
achievable. No untrusted input reaches the defect and no secret is exposed. Major, not
blocker: US-029 can be accepted with this open; the 5.8 line should not ship with it.

**Admissible on the 5.8 line.** The fix is confined to a private `.cpp` body — index from
the double (`static_cast<int32>(FMath::FloorToDouble(Phase))`) and clamp `Idx0` to
`[0, NumFrames-1]`; the peak conversion is the separate finding below. No contract, no
module boundary, no ADR, no dependency, no Blueprint-facing signature, so it passes CR-001's
bar. Owner M-asr (speech-dev); not fixed here. The pinning test
`FANTASIA.WhisperCapture.Resample.ReadsOneFramePastInput` asserts current output and must be
updated by the fix.

## J-0131 | 2026-11-15 | F-041 | security
- Event: opened
- Story: US-029
- Module: M-asr
- Kind: security
- Severity: minor
- Repro: FWhisperCaptureTestAccess::ApplyNoise with a single NaN sample latches HpfY1/HpfY2 and makes every later output NaN (read back with FWhisperCaptureTestAccess::Filter); FMath::RoundToInt(215000.f * 10000.f) is INT_MIN, so SetRecentPeak(215000.f) makes GetRecentPeak() return about -214748
- Changes: none

M-asr validates captured audio **downstream** of the two places that cannot survive bad
values, so a single non-finite or out-of-range sample from any source — a driver glitch, a
denormal, or F-017's out-of-bounds read — is latched in state that no later validation can
reach.

The order in the audio callback (`WhisperCaptureComponent.cpp`) is: resample (`:422`), take
the raw peak and publish it (`:436-444`), filter (`:466`). The only validation in the module
is in `TranscribeBuffer` (`:755-766`), which zeroes NaN and Inf, zeroes denormals and
clamps to [-1,1] — on the game thread, after all three, on a *copy*. So:

1. **Persistent IIR state has no guard.** `ApplyNoiseSuppression`'s high-pass is Direct
   Form I over member state `HpfX1/X2/Y1/Y2` (`:1109-1113`). One NaN input makes `HpfY1`
   and `HpfY2` NaN, and from then on every output sample is NaN for the rest of the
   session: the state is cleared only in `StartCapture` (`:550`) and at `:943`.
   `NoiseGateEnvelope` (`:1148`) latches the same way. `TranscribeBuffer` then zeroes the
   whole buffer, so whisper is handed pure silence and ASR stops returning anything, with
   no error and no log line. The existence of the NaN/Inf/denormal counters at `:755-766`
   is the module's own evidence that such samples do occur.
2. **A narrowing conversion with no clamp.** `SetRecentPeak` is
   `RecentPeakFixed.Set(FMath::RoundToInt(Value * 10000.f))` (`:986`), called at `:441-444`
   with the raw peak. Any `|Value|` above about 2.1e5 makes `Value * 10000.f` exceed
   INT32_MAX: an out-of-range float-to-int conversion, undefined behaviour, which on x86-64
   yields INT_MIN, so `GetRecentPeak()` returns about -214748. That atomic is the only input
   to both VAD branches (`:207` and `:277`, `bIsSpeaking = Peak > EffectiveThreshold`), so
   the affected 200 ms window reports silence whatever the microphone heard. NaN cannot
   arrive here (`CallbackPeak > ExistingPeak` is false for NaN), so this half concerns large
   finite values only.

Repro, through the seam that already exists and with no device: `FAccess::ApplyNoise(C, {NaN})`,
then `FAccess::Filter(C).Y1` is NaN; call `ApplyNoise` again with any clean buffer and every
output sample is NaN. For the second half, `FMath::RoundToInt(215000.f * 10000.f)` is
INT_MIN on this toolchain, so `SetRecentPeak(215000.f)` followed by `GetRecentPeak()` yields
a negative peak.

Fix (M-asr, speech-dev): sanitise each sample where it enters — at the end of the resample
step or at the head of `ApplyNoiseSuppression` — instead of in `TranscribeBuffer`, and clamp
in `SetRecentPeak` before the conversion. Both are private `.cpp` bodies: no contract, no
boundary, no ADR, no dependency, no Blueprint-facing signature, so the fix passes CR-001's
bar for the 5.8 line. Minor on its own: no untrusted input reaches it and nothing crosses a
trust boundary — it is the amplifier that makes F-017 major rather than a defect an attacker
can steer. C-asr says nothing about either guarantee; a reader of the contract would assume
the validation it does document protects the filter, and it does not.

## J-0132 | 2026-11-15 | F-042 | security
- Event: opened
- Story: US-018
- Module: M-dist
- Kind: security
- Severity: major
- Repro: git check-ignore -v FANTASIA/ThirdParty/aGrUM/Libraries/agrumBASE.lib exits 1 (not ignored); du -sh that directory is 249M; git status --porcelain lists it as untracked; .gitignore:35 names the aGrUM 1.x filename agrum.x64.lib
- Changes: none

P-17 says heavy runtime artifacts "are gitignored, and bootstrap is the single place that
knows where they are". For aGrUM that stopped being true at the 2.x upgrade and nobody
noticed, because `.gitignore:35` names the 1.x filename:

    FANTASIA/ThirdParty/aGrUM/Libraries/agrum.x64.lib

aGrUM 2.3.2 ships per-module libraries, so the directory now holds `agrumBASE.lib`,
`agrumBN.lib` and `agrumID.lib` — 249 MB that no pattern matches.
`git check-ignore -v FANTASIA/ThirdParty/aGrUM/Libraries/agrumBASE.lib` exits 1, and
`git status --porcelain` lists the directory as untracked **right now**, in a working tree
that holds an entire uncommitted sprint. The next `git add -A` commits a quarter-gigabyte of
third-party build output into a repository that users clone, and `.gitattributes` does not
route `*.lib` to LFS, so it lands in the object store permanently: unremovable without a
history rewrite of a repository with three live branches and a force-push-protected `main`.

The same stale name is in `THIRD_PARTY_LICENSES.md:25`, which tells a redistributor that
FANTASIA links `aGrUM/Libraries/aGrUM.x64.lib` — a file that does not exist — and the
`.gitignore` comment block and `THIRD_PARTY_LICENSES.md:59` both still say the assets come
from `runtime-v2.0`, while both bootstrap scripts set `RELEASE_TAG=runtime-v3.0`
(`bootstrap.bat:46`, `bootstrap.sh:30`). So the provenance documentation for the dependency
tree describes a release the build no longer fetches.

Why this is security and not only hygiene: these libraries are not ours and are not built
here. `bootstrap.bat:191` uses `agrumBASE.lib` as the marker for the
`fantasia-build-deps-win64.tar.gz` asset, which is downloaded with no integrity check at all
(F-026). Committing them promotes unverified third-party binaries into the artifact users
receive, and does it permanently, which is precisely the outcome P-17 and the bootstrap
design exist to prevent. The two sibling directories are still protected
(`.gitignore:36-37`, gRPC and OpenSSL release libs, both untracked and both ignored), so
this is one stale line, not a design gap.

Fix (M-dist, build-dev): ignore the directory rather than a filename —
`FANTASIA/ThirdParty/aGrUM/Libraries/` with the headers under `Includes/` staying tracked —
and correct the two stale statements in `THIRD_PARTY_LICENSES.md` and the `.gitignore`
comments. No source, no contract, no dependency change; admissible on the 5.8 line under
CR-001. Until it lands, nobody should run `git add -A` in this tree.

## J-0133 | 2026-11-15 | F-042 | security
- Event: opened
- Corrects: J-0132
- Story: US-018
- Module: M-dist
- Kind: security
- Severity: major
- Repro: git check-ignore -v FANTASIA/ThirdParty/aGrUM/Libraries/agrumBASE.lib exits 1 (not ignored); du -sh that directory is 249M; git status --porcelain lists it as untracked; .gitignore:35 names the aGrUM 1.x filename agrum.x64.lib; .gitattributes:2 routes *.lib to LFS
- Changes: none

Correction to J-0132, on one point of consequence. I wrote that `.gitattributes` does not
route `*.lib` to LFS. It does: `.gitattributes:2` is
`*.lib filter=lfs diff=lfs merge=lfs -text`. So a `git add -A` would push the 249 MB as LFS
objects, not into the git object store.

What that changes: the artifacts would consume the account's LFS storage and bandwidth, and
every default clone would fetch them — which is the exact cost the `.gitignore` comment at
lines 31-34 says the release-asset design exists to avoid ("~330 MB uncompressed ...
Blueprint-only users don't strictly need them"). Undoing it still needs a history rewrite on
a force-push-protected `main` plus an LFS purge, because the pointer files stay in history,
but it is not the "unremovable" case I described.

What it does not change: `.gitignore:35` still names the aGrUM 1.x filename, the directory is
still unignored and untracked in a tree holding an uncommitted sprint, the libraries still
arrive from an unverified download (F-026), and P-17's stated mechanism still does not cover
them. Severity stays major; the fix is unchanged.

## J-0134 | 2026-11-15 | NOTE | security
- Changes: threat-model.md

The sprint-level dependency and secrets pass for S-01, and the two findings it produced.

**Dependencies.** Every version in the table is now read from the file named beside it and
the file is cited, because the previous table was partly wrong where it had taken a document
at its word. Three corrections matter. OpenSSL 1.1.1t is the **plugin's own vendored copy**,
not the engine's: `FANTASIA.Build.cs:44` calls its variable `EngineThirdPartyPath` and
assigns `ModuleDirectory/../../ThirdParty/`, which is the plugin's tree and the same path as
`ThirdPartyPath`, and `:54` adds every `.lib` under it explicitly. So the open question of
whether Epic backports into the engine's copy is moot — the libraries we link are ours, they
are pinned at an end-of-life version, and they are fetched by bootstrap with no integrity
check, which puts the library protecting the Neo4j password inside F-026. libsvm's version
*is* stated (`Source/FANTASIA/Public/svm.h:6`, `LIBSVM_VERSION 335` = 3.35, current), where
the table said it was stated nowhere. nlohmann/json is 3.7.3 from 2019, and gRPC 1.51.1,
protobuf 3.21.6 and abseil LTS 20220623.0 are a single late-2022 stack that nothing calls.
Added: ggml 0.9.8, and that the staged build defines `WHISPER_VERSION="0.0.0"` and
`GGML_COMMIT="unknown"`, so a shipped binary cannot report which whisper.cpp it contains.
The audit read versions and provenance; no CVE feed was queried, and the project has no
manifest that would let one be, which the section now says in place of implying otherwise.

**Secrets.** Scanned against `HEAD`, not by commit, because the entire sprint is
uncommitted: the text diff of 24 modified tracked files, six untracked source and script
files, and the printable strings of both rebuilt DLLs, against sixteen credential patterns.
One match, benign: `APIKey="injected"`, the ini-syntax fixture from
`Quality/SecretConfigRobustnessTests.cpp:217`. In a sprint whose subject was credentials,
the test support is the thing worth recording — one distinctive fake key, scoped in-memory
GConfig entries restored in a destructor, and no `GConfig->Flush` anywhere, so no test can
write a key into a host project's ini.

**Findings.** F-042 (major): `.gitignore:35` names aGrUM 1.x's `agrum.x64.lib`, so 249 MB of
2.x libraries sit unignored and untracked in a tree holding an uncommitted sprint, and P-17's
claim that these artifacts are gitignored is false for them. F-041 (minor): M-asr's audio
validation runs downstream of the peak publication and the filter, so one non-finite sample
latches the high-pass state for the session and `SetRecentPeak` narrows an unclamped float to
`int32` on the VAD's only input. F-017 is raised to major as a security finding in J-0130,
and the M-asr section carries the reasoning.

The gate changes two standing statements. US-027 means a security test placed in the suite is
now run by something, so "nothing is checked by a machine" is too strong and has become "no
CI, and a gate a person has to remember to run"; and the test count is 150, of which none is
a security test, with `Private/Security/` still empty.

## J-0135 | 2026-11-15 | F-043 | lead
- Event: opened
- Kind: quality
- Severity: minor
- Story: US-018, US-019
- Module: M-dist
- Repro: THIRD_PARTY_LICENSES.md:25 tells a redistributor the build links 'aGrUM/Libraries/aGrUM.x64.lib'; no build file references that name and FANTASIA/Source/FANTASIA/FANTASIA.Build.cs:24-26 links agrumID.lib, agrumBN.lib and agrumBASE.lib instead. THIRD_PARTY_LICENSES.md:59 heads its fetched-at-runtime section 'runtime-v2.0 Release' and .gitignore says runtime-v2.0 at lines 8, 12, 31 and 40, while FANTASIA/bootstrap.bat:46 and FANTASIA/bootstrap.sh:30 both set runtime-v3.0 (db2dfbd6, which pointed the 5.8 line at the new release and did not follow the prose).
- Changes: none

Surfaced by the security engineer's S-01 dependency audit, which could not own it: THIRD_PARTY_LICENSES.md and .gitignore are M-dist's.

This is a distributed document, and it is the one a redistributor reads to learn what the shipped binary links. It currently cannot answer that. The LGPL notice still names aGrUM as a component, so compliance is not broken by the stale filename, but the index points at a library that exists nowhere in the tree or the build, and it names the wrong release for everything bootstrap fetches.

Related and separate: F-042 is the stale .gitignore line 35 (agrum.x64.lib, the 1.x filename), which leaves 249 MB untracked and unignored. Same owner, same two files, and the four runtime-v2.0 comment lines sit in one of them, so the three are one edit rather than three.

## J-0136 | 2026-11-15 | F-044 | security
- Event: opened
- Story: US-019
- Module: M-dist
- Kind: security
- Severity: minor
- Repro: FANTASIA.Build.cs:48 declares the engine OpenSSL module while :54 and :56 add the plugin's own copy of the same libraries; sha256 of the plugin's libssl.lib (23c1434c79...) and libcrypto.lib (a818a7a542...) equal UE 5.8's Engine/Source/ThirdParty/OpenSSL/1.1.1t copies byte for byte; the engine's OpenSSL.Build.cs:51-52 already adds both
- Changes: none

The plugin links OpenSSL twice, and the second copy is hidden behind a variable name that
says it is the engine's.

`FANTASIA.Build.cs:48` already declares `"OpenSSL"` in `PublicDependencyModuleNames`, and the
engine module supplies exactly what the plugin needs: `OpenSSL.Build.cs:51-52` adds
`libssl.lib` and `libcrypto.lib`, `:46` adds the include path. On top of that, `:54` globs
every `*.lib` out of the plugin's own
`ThirdParty/OpenSSL/1.1.1t/lib/Win64/VS2015/Release` into `PublicAdditionalLibraries` and
`:56` adds the plugin's include path — both reached through
`string EngineThirdPartyPath = Path.Combine(ModuleDirectory, "../../ThirdParty/")` (`:44`),
which despite its name is the plugin's tree and is the same path as the `ThirdPartyPath`
parameter the method already receives. All of it sits inside `LoadGrpc`, the loader for the
dependency nothing uses (F-002), so the one TLS decision in the project is made in the body
of a function about gRPC.

The two copies are the same bytes. sha256 of the plugin's
`ThirdParty/OpenSSL/1.1.1t/lib/Win64/VS2015/Release/`:

    libssl.lib     23c1434c794e77028cfede8f12ee91b5...
    libcrypto.lib  a818a7a54286fb7dbba3a82c92a5e29e...

and the same two files under
`C:/Program Files/Epic Games/UE_5.8/Engine/Source/ThirdParty/OpenSSL/1.1.1t/lib/Win64/VS2015/Release/`
hash identically. The plugin's copy is a duplicate of the engine's, fetched by bootstrap into
`.gitignore:37` with no integrity check (F-026) — 23 MB of download to obtain bytes the
engine already has on disk.

Why this is security and not only hygiene: it is the reason nobody could say, until now, which
OpenSSL the Neo4j TLS path actually links. The threat model asserted the engine's copy because
the variable is named `EngineThirdPartyPath`; I corrected that to the plugin's copy in J-0134
by reading the assignment; both readings were half right, because the module links **both**.
A duplicate static-library link of a crypto library is also a standing hazard in its own
right: the moment the two copies diverge — a bootstrap release built against a different
OpenSSL, or an engine patch that bumps it — the module links two versions of the same symbols
and which one wins is a link-order accident, in the code that negotiates TLS.

Fix (M-dist, build-dev): delete `:54` and `:56` and rely on the `"OpenSSL"` module
dependency at `:48`, which makes the engine the single source of the version; drop the OpenSSL
libraries from the bootstrap deps asset and from `.gitignore:37`; and rename
`EngineThirdPartyPath`, or delete it, since `ThirdPartyPath` is the same path. Needs a build to
confirm the engine module's include path satisfies the Bolt TLS sources, which I cannot run.
No contract, no boundary, no Blueprint-facing signature, and it *removes* a dependency copy
rather than adding one, so it passes CR-001's bar.

**This finding is deliberately not about the version.** See the ruling in Open risks: the
engine hardcodes 1.1.1t (`OpenSSL.Build.cs:12`), UE 5.8 ships no other version, and our bytes
are its bytes, so no action available to this project changes which OpenSSL is linked.

## J-0137 | 2026-11-15 | NOTE | security
- Changes: threat-model.md

The OpenSSL question the lead raised, ruled and recorded where it will be read.

Measuring instead of arguing settled it. The plugin's vendored `libssl.lib` and
`libcrypto.lib` are **byte-identical** to UE 5.8's own 1.1.1t copies (sha256
`23c1434c79…` and `a818a7a542…`), the engine hardcodes the version at
`OpenSSL.Build.cs:12`, UE 5.8 ships no other, and the module links **both** copies —
the engine's `OpenSSL` module dependency at `FANTASIA.Build.cs:48` plus the plugin's own
globs at `:54` and `:56`. So the project has no version to choose, and my own correction in
J-0134 was half right in the same way the sentence it replaced was: the answer is "both",
not "ours" and not "the engine's".

**Ruled: the end-of-life version is not a finding; the duplicate link is (F-044, minor).**
The version is unfixable here, and the confidentiality consequence the EOL pin appears to
carry is already owned by F-020 — a path that authenticates no server does not need a library
defect to leak the password, so patching OpenSSL first would be weight on an open door. The
advisories after 1.1.1t are denial of service and certificate-policy correctness. A finding
naming a condition nobody here can close would stay open for the life of the line, so Open
risks now carries the ruling with the two conditions that would make it actionable — Epic
shipping a newer OpenSSL in a targeted engine, or the project deciding to carry its own TLS
for Bolt, which is the user's design decision under P-06 and not a coder's defect.

There is also a reason of consequence not to open it: CR-004 admits a security finding onto
the 5.8 line whatever its surface, so opening this one would authorise the single riskiest
available action — a second, divergent OpenSSL in a process whose HTTP module links the
engine's. The carve-out's provenance rule exists so that this is weighed, and it is.

The Dependencies row now states that both copies are linked and that they are the same
bytes, which is what a reader needs to know before touching either.

## J-0138 | 2026-11-15 | NOTE | lead
- Finding: F-044
- Module: M-dist
- Changes: none

The three-line deletion is a four-line edit, and the missing line is what makes it compile. Verified against FANTASIA/Source/FANTASIA/FANTASIA.Build.cs with grep -n:

- :54  string[] libraryPaths = Directory.GetFiles(... OpenSSL/1.1.1t/lib ...);
- :55  PublicAdditionalLibraries.AddRange(libraryPaths);
- :56  PublicIncludePaths.Add(... OpenSSL/1.1.1t/include ...);
- :58  libraryPaths = Directory.GetFiles(Path.Combine(ThirdPartyPath, "grpc", ...));

Line 54 carries the `string[]` declaration, and :58 only assigns. So deleting :54 to :56 leaves :58 assigning to an undeclared variable and the module stops compiling. The fix is to delete :54 to :56 and give :58 the declaration, which also reads better: the gRPC glob is the only one left, so the variable is declared where it is used.

Independently verified for the ruling in the security engineer's message: the plugin's and the engine's OpenSSL libraries are byte-identical under SHA-256 - libssl.lib 23C1434C794E77028CFEDE8F12EE91B5..., libcrypto.lib A818A7A54286FB7DBBA3A82C92A5E29E..., same on both sides. That is what makes the duplicate link harmless today and makes F-044 minor rather than major, and it is also the measurement that retires the EOL-OpenSSL question: there is no version here that is ours to choose.

## J-0139 | 2026-11-15 | CR-008 | lead
- Event: approved
- Decision: Option (A): build-dev.owns gains FANTASIA/run-tests.ps1 and design_version goes from 3 to 4.
- Approver: user
- Changes: none

The user approved at sprint review, in one word. Both edits are mechanical and neither was a judgement call: architecture.md:196 already names the file in M-dist's Paths (CR-005), and the design version follows architecture.md, which CR-005 moved to 4.

This clears the checker's only ERROR, which matters beyond tidiness: the Definition of Done requires a clean checker, so no story in S-01 could be accepted while the gate was unowned.

## J-0140 | 2026-11-15 | CR-008 | lead
- Event: implemented
- Evidence: team.json: build-dev.owns gains FANTASIA/run-tests.ps1, inserted after FANTASIA/enable-plugins.ps1 to follow architecture.md:196's Paths order; design_version 3 -> 4. File re-parsed as JSON after the edit and the ownership assertion re-read from the parsed document rather than from the diff. ateam_check.py --root . now reports 0 errors, 17 warnings, down from 1 error.
- Changes: team.json

The gate is owned. This was the project's one checker ERROR, and it mattered more than tidiness: the Definition of Done requires a clean checker, so no S-01 story could be accepted while the file holding the gate belonged to no agent.

It also removes the obstacle to leaving observe mode. .ateam/ownership.jsonl still holds 0 would-be violations since S-01 opened, and enforcement could not have been switched on over a gap in the ownership map - the one file every agent's work is judged by was the one file no agent was responsible for.

## J-0141 | 2026-11-15 | NOTE | reviewer
- Event: verdict
- Story: US-027
- Verdict: meets request
- Open findings: F-037 (minor, M-tests) compatible; F-035 and F-036 verified fixed
- Evidence: FANTASIA/run-tests.ps1:231-267, :460-523, :525-545 re-read; lead's run 150 tests, 0 failures, 46.4 s (J-0123); script not re-run by the reviewer
- Changes: none

**Verdict: meets request.** This is a verdict against the gate as it stands now, not as I
first reviewed it. My original pass was against the pre-F-035 script: fixed harness, report
and log paths, `-MinTests 109`, exit 4 live, and no proof of the report's provenance. Two of
those are what F-035 and F-036 were, and both are fixed, so the verdict is re-reached rather
than carried over. I re-read the script at the lock (`FANTASIA/run-tests.ps1:231-267`), the
editor launch and provenance check (`:460-523`) and the verdict (`:525-545`); I did not re-run
it, by the lead's instruction, so the measurement standing behind this verdict is the lead's
own run: 150 tests, 0 failures, 46.4 s, provenance line naming its own pid.

**The load-bearing reason.** The gate can no longer exit zero on evidence it did not produce,
which is the one failure mode ADR-0016 and build-dev's profile both name as making a gate
worse than none. The lock serialises runs (exit 11), and the pid check proves the report came
from the editor this run started. What makes this a fix rather than a claim is that the bad
case was reproduced before it was closed: a genuine 150-test zero-failure report from another
instance, planted at a live run's own report path while its editor wrote none, printed PASSED
under the old script and exits 3 under this one. The detail worth keeping is that the
pre-existing `InternalLoadLibrary` guard **passed** on that input. It proves which DLL was
loaded, never which editor wrote the verdict, so the two guards are not redundant and neither
substitutes for the other.

**The two fixes break no criterion.** `-MinTests 134` is stricter than the criterion's "at
least 109", and a necessary condition met more tightly is still met; the floor's derivation
(150 run, 16 behind `WITH_AGRUM_BINDING`) is in the script's own header, and it holds whether
or not F-037 is fixed, because a floor is a lower bound. Retiring exit 4 leaves the criterion
"the compile fails, or the editor writes no report, or the report names zero tests ... says
which of the three happened" satisfied: a prefix matching nothing exits 3 with no report
written at all, and a zero-test report, if the engine ever wrote one, exits 6 naming the count
it found. It was measured unreachable, not argued so, and the vacated number is recorded as
vacant rather than reused. The remaining criteria I re-checked in place: the junction naming
the plugin directory alone, the `Makefile.bin` discard (`:422`), the BuildId check against
`CompatibleChangelist` (`:443-452`), the P-12 notice when a run rewrote the tracked binaries
(`:433-435`), and the elapsed line (`:544`).

**F-037 is compatible with this verdict.** It is minor, it sits in M-tests rather than in the
gate, and no US-027 criterion mentions the declared-versus-run count. Its only contact with
the gate was the floor taken from a document instead of a run, and that is what F-036 closed.

**One thing US-027's work made stale and no finding carries.**
`FANTASIA/Source/FANTASIATests/CLAUDE.md:73-75` still says the suite needs the editor "which
is why `gates.task_completed` in `docs/ateam/team.json` is `null`: there is no fast command to
gate a task on. Producing one is first-sprint work." `team.json` now holds the command, so the
module's own state document tells a reader the project has no gate. Line 29 of the same file
says "109 tests, all `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, across 11 files"; the tree holds 15
test files, 150 tests run, and 25 of them go through a wrapper macro. F-037's fix note carries
the count half for two documents; the gate sentence is carried by nothing. Reported to the
lead as a minor documentation finding for test-dev, and it changes no verdict.

## J-0142 | 2026-11-15 | NOTE | reviewer
- Event: verdict
- Story: US-028
- Verdict: meets request (criteria met; the story's own sentence holds only on the ini route, which is F-031 and the user's to accept)
- Open findings: F-031 (major, security) compatible and disclosed; F-032, F-033 (minor) bear on no criterion; F-038 verified fixed
- Evidence: RESTTTSComponent.h:311-323 and OpenAICompatibleComponent.h:84-96 read today; SecretConfigTests.cpp:40-53, :92-106, :108-141
- Changes: none

**Verdict: meets request**, with the story's own one-sentence promise qualified below. My
original pass was against the Headers tooltip as F-038 found it, which said the key "must be
set there, not typed into an asset" and warned only about assets that *already* held one. That
was the one place the fix was still reported as complete on the strength of the specifier
alone. It is fixed, so this verdict is against the present text, which I read today.

**The load-bearing reason.** The third criterion asks that the doc comment — the tooltip, which
is where an integrator learns where their key goes — state both the ini route and the re-save
obligation, explicitly "so the fix is not reported as complete on the strength of the specifier
alone". `RESTTTSComponent.h:311-323` now states the ini route, that a key typed into the Details
panel is still saved into the Blueprint or level asset, and that the specifier adds an ini source
without making the property transient; the `@note` keeps the retroactive clause. It now reads as
`OpenAICompatibleComponent.h:84-96` does, which is the model the module's CLAUDE.md names. With
that, every place the fix speaks to the user says both halves, and C-rest-tts 1.1's assertion
that "the `Headers` tooltip states this" is true as written rather than the fourth contract in
one sprint claiming a mitigation that does not exist.

**The other criteria, re-checked at present state.** `SecretConfigTests.cpp:40-53` sweeps nine
properties, the criterion's six among them, and `FANTASIA.Secrets.ConfigFlag` fails if any loses
`CPF_Config` (`:92-106`); the two round-trips (`:108-141`) prove a `Config` specifier on a struct
and on a map property actually serialises, which is the criterion's second clause and the reason
the specifiers on `FOpenAICompatibleConfig`'s inner fields never fired. Both fixed properties keep
their name, type, `Category` and `EditAnywhere, BlueprintReadWrite`, so no compiled Blueprint
asset is invalidated. Nine properties carrying the flag where the criterion names six is a
superset, not scope creep: the three on `AFANTASIATestActor` arrived with F-029, which is a
separate finding with its own decision (DEC-011).

**What "meets request" does not mean here, and the lead should put this to the user in these
terms.** The criteria are met; the story's narrative sentence — "using a FANTASIA component
cannot put my API key into a committed asset" — is true only of the ini route. F-031 is exactly
that gap and is open at major: all nine properties stay `EditAnywhere`, so a key typed into the
Details panel still serialises, pinned by
`FANTASIA.Quality.Secrets.Asset.TypedKeyIsSerialized`. Closing it means dropping `EditAnywhere`
or making the value transient, which is a Blueprint-surface change (P-03, P-19) that only the
user can accept. Compatible with this verdict because the Definition of Done bars an open
blocker and not an open major, because the residual is pre-existing rather than introduced, and
because the story now discloses it in three places — the backlog note, both contracts, and both
tooltips — instead of claiming it away. F-032 and F-033 are minor, are the same sentence owed by
two more tooltips and by the ini-section rule for the ACE REST subclass, and neither bears on a
criterion.

## J-0143 | 2026-11-15 | NOTE | reviewer
- Event: verdict
- Story: US-029
- Verdict: meets request, against the criterion as amended by CR-009 (J-0129), which satisfies me
- Open findings: F-017 (major, security), F-041 (minor, security, not in the brief), F-019, F-039, F-040 all compatible; F-018 is US-033's per CR-009 though the status listing still files it under US-029
- Evidence: WhisperCaptureComponentTests.cpp:212-225 checked clause by clause against the amended criterion; FWhisperCaptureTestAccess has no CaptureSessionId accessor and the test's append takes the 16 kHz fast path, so DEC-016's two exclusions hold in the source
- Changes: none

**Verdict: meets request, and the amended criterion satisfies me.** This is the story whose text
moved rather than whose code moved, so it is the one where the distinction matters. What I
reviewed then was the pre-CR-009 second criterion, which asked for `StartCapture` returning false
"with `IsCapturing()` still false" on an unopenable device. I withdraw nothing from that pass: my
objection was that the criterion was unsatisfiable in **both** clauses, because the one path a
test can drive without hardware is the already-capturing guard, whose entry condition is
`bIsCapturing == true`, so replacing only the *When* would have left a second unsatisfiable
criterion behind. CR-009 moved both clauses, and the amendment is the shape I would have asked
for.

**The load-bearing reason.** Every clause of the amended criterion is asserted today, and the
clauses no test keeps were deliberately left out instead of promised.
`FANTASIA.WhisperCapture.Controls.StartCaptureWhileCapturing`
(`WhisperCaptureComponentTests.cpp:212-225`) buffers 0.5 s at 16 kHz, forces `bIsCapturing`,
expects exactly one warning containing "already capturing", and asserts `StartCapture()` false,
`IsCapturing()` true and `GetBufferDuration()` still 0.5 s — the return value, the flag, the one
warning and the undisturbed buffer, which is the criterion clause for clause. DEC-016's two
exclusions check out in the source rather than only in its reasoning: `FWhisperCaptureTestAccess`
exposes the filter and the phase but no `CaptureSessionId` accessor, and the test appends through
the resampler's 16 kHz fast path, where the phase is zero on both sides, so an assertion there
would have passed whether the guard returned early or not. A criterion clause with no assertion
behind it is the defect this amendment existed to remove, so writing them in would have defeated
it; recording them as an `Open:` invariant instead is the right place for something a refactor
could break silently.

**The one place the amended text runs ahead of the pin, and why it is not a finding.** "It leaves
the running session undisturbed" is broader than what is asserted, and it is the pattern F-039
objects to elsewhere — a claim standing next to a pin that covers part of it. Three things keep
it on the right side here: the apposition narrows it in the same breath ("the buffered audio
unchanged in duration"), the unpinned half is stated as unpinned in the story's own note with
DEC-016 cited, and the criterion names the device-failure paths as US-033's rather than leaving a
reader to assume coverage. That is disclosure, which is what F-039 asks for and does not get in
C-asr.

**The open findings are compatible, and one of them is not in my brief.** F-017 is now major with
`Kind=security` (J-0130); the security engineer's own entry says US-029 can be accepted with it
open and that the 5.8 line should not ship with it, and that is the correct split, because the
story asked for the path to be *pinned as it is* and finding three defects while pinning them is
the story working rather than failing. F-041 (minor, security, J-0131) was opened against this
story after the brief I was given and belongs to the same class: unguarded persistent IIR state
and an unclamped narrowing conversion, both characterised through the seam this story built, both
M-asr's to fix. Compatible on the same reasoning. F-019, F-039 and F-040 are minor; F-039 is a
contract sentence and F-040 a one-line macro change, and neither bears on a criterion. F-018
stays open and is carried by US-033, which is what CR-009 decided; the status listing still shows
it under US-029, which the lead may want to retarget so the story's open set matches what the
story is answerable for.

## J-0144 | 2026-11-15 | F-045 | lead
- Event: opened
- Kind: quality
- Severity: minor
- Story: US-027
- Module: M-tests
- Repro: FANTASIA/Source/FANTASIATests/CLAUDE.md:73-75 states that the suite needs the editor to run, 'which is why gates.task_completed in docs/ateam/team.json is null: there is no fast command to gate a task on. Producing one is first-sprint work.' team.json now holds that command (J-0095), so M-tests' own state document tells a reader the project has no gate. Line 29 of the same file states '109 tests, all IMPLEMENT_SIMPLE_AUTOMATION_TEST, across 11 files'; the tree holds 15 test files and the gate runs 150 tests, 25 of them declared through a wrapper macro.
- Changes: none

Surfaced by the reviewer in T-16. The count half is already carried by F-037's fix note for two other documents; the gate sentence is carried by nothing, which is why this exists.

Worth more than its severity suggests: the gate is S-01's main deliverable, and the state document a coder reads before touching the test tree currently says it does not exist. P-18 puts the story of how we got here in the journal and the present tense in the state document; this sentence is the opposite of both.

## J-0145 | 2026-11-15 | F-018 | lead
- Event: opened
- Kind: quality
- Severity: minor
- Story: US-033
- Module: M-asr, M-tests
- Corrects: J-0066
- Repro: Unchanged from J-0066: no value of AudioDeviceIndex makes StartCapture fail, because an out-of-range index falls back to the OS default and opens it.
- Changes: none

Record correction, not a re-ruling: severity, kind and repro all stand. CR-009 moved this finding's subject from US-029 to US-033, which now carries the injectable capture seam that makes the device-failure path reachable, but status --open still filed it under US-029 and so overstated what that story was answerable for at its own review. Re-pointed by the same mechanism the security engineer used for F-017's re-ruling (an opened entry carrying Corrects=), because a finding's event vocabulary is only accepted, fixed and opened.

## J-0146 | 2026-11-15 | NOTE | lead
- Story: US-027
- Finding: F-035
- Changes: none

The clause to tighten first if the gate ever runs more than one device, recorded so it is not rediscovered. Observed by the reviewer in T-16 and deliberately not a finding.

FANTASIA/run-tests.ps1 accepts the report when ANY device in it carries this run's editor pid, rather than requiring every device to. With the per-run GUID report directory and the harness lock both in place, the only way to reach a report whose devices disagree is deliberate tampering, which is outside what F-035 was about. It becomes reachable by accident the moment the gate starts more than one device.

## J-0147 | 2026-11-15 | NOTE | lead
- Story: US-027, US-028, US-029
- Decision: The user accepted all three stories at the S-01 review.
- Approver: user
- Changes: none

Accepted in these words: "Accept all three", against the three verdicts logged at J-0141, J-0142 and J-0143.

All seven clauses of the Definition of Done hold for each: the gate passes (150 tests, 0 failures, 46.4 s, provenance stamped by its own editor); no open blocker finding stands against any of the three (the project's two blockers, F-020 and F-021, belong to US-014 and are carried forward by US-030 and US-031); the reviewer's verdict is 'meets request' with no blocker, issued against each story's present state rather than the state it first reviewed; the implementation decisions are DEC entries; ateam_check.py reports 0 errors; .ateam/ownership.jsonl holds no violation since S-01 opened; and the user has now accepted.

What the acceptance of US-028 explicitly includes, because it was put to the user in these terms: F-031 stays open and is the story's residual. Every one of the nine secret-bearing properties remains EditAnywhere, so a key typed into the Details panel still serialises into the Blueprint or level asset. The ini route is what the story made safe; it did not make the asset route impossible. Closing that is a Blueprint-surface change and needs a change request nobody has raised.

## J-0148 | 2026-11-15 | NOTE | planner
- Story: US-027, US-028, US-029
- Decision: The three stories accepted at the S-01 review are recorded done in backlog.md; two sentences that restated the superseded status are rewritten.
- Changes: backlog.md

The three stories the user accepted at the S-01 review are `done` in backlog.md. This entry
records the status change and restamps the file; the substance is already in the journal and is
not repeated here: the reviewer's three verdicts at J-0141, J-0142 and J-0143, and the user's
acceptance at J-0147.

US-027 `in-progress` to `done`. US-028 `in-progress` to `done`. US-029 `in-progress` to `done`.
They were the file's only `in-progress` statuses, so backlog.md now holds none, and the thirty-
three stories are eighteen `done`, three `ready` (US-030, US-031, US-033), one `draft` (US-032)
and the rest unstarted.

Two sentences in the same file said what the status itself now says, and both are rewritten
rather than left to contradict it: US-027's and US-028's closing `Open:` notes each ended
"Status stays `in-progress` until the user accepts the story at review", which was true when it
was written and is the clause J-0147 discharged. US-027's other `Open:` note said the user "is
asked at review to confirm the edited criterion" (CR-005's junction clause); the user accepted
the story with that criterion as its present text. Nothing else in either story moves, and no
criterion, source, estimate or finding changes anywhere in the file.

What follows from accepting stories while the sprint is still open: ateam_check.py now reports
three warnings of the form "S-01 lists US-027, which is already done", one per story. They are
correct and they are not a defect in either file. sprint.md lists the sprint's stories and
backlog.md holds their status, so between the review and the sprint's close the two documents
disagree by design; the lead clears all three by closing S-01. The checker is otherwise at
0 errors, and the Definition of Done's clean-checker clause is read against that.

## J-0149 | 2026-11-15 | REQ-003 | lead
- Event: decided
- User decision: confirmed
- Changes: none

Confirmed at the S-01 review, in these words: "Confirm rejection".

The method presents each request the planner rejected for the user to confirm or overturn. This one is near-formal and was put to the user as such: the rejection records the user's own explicit instruction (J-0041, 'OUT OF SCOPE, managed separately as the wiki is'), and the planner's rejected evaluation at J-0056 predates S-01 opening at J-0061. There was no request rejected on a principle during this sprint, because no request arrived during it - all four predate it.

Verified rather than assumed while presenting it: the user's instruction that architecture.md stop implying ownership did land. CR-002 rewrote the Walking skeleton section, and architecture.md:258 now reads that FANTASIA-LangGraphDemo 'illustrates the path wired into a running application' rather than being it. So the demo is cited as illustration and owned by no module here, in the same arrangement as the project wiki.

## J-0150 | 2026-11-15 | NOTE | lead
- Story: US-027
- Change: CR-005
- Decision: The user confirmed US-027's rewritten second acceptance criterion, and the new load-path criterion with it.
- Approver: user
- Changes: none

Confirmed in these words: "Confirm the rewrite".

CR-005 stated twice that the one thing going to the user at review was the edited acceptance criterion on US-027, not the mechanism. When I presented the story at review the user accepted it (J-0147) without that edit being named, so the acceptance stood on criteria the user had not been shown had changed since they approved the sprint plan. This entry closes that gap rather than letting J-0147 carry weight it had not earned.

What was put to the user, and what they confirmed: the criterion they approved at planning named AdditionalPluginDirectories as the harness mechanism; that clause became unsatisfiable, because making the plugin external means UBT links into the host project while the editor loads the committed prebuilt DLL from the plugin's own Binaries - the same discovery behind F-035, which is why the gate once reported PASSED on a run with a deliberately failing test compiled in. The criterion now names the junction at <harness>/Plugins/FANTASIA, the post-run load-path assertion and the Makefile.bin discard, and a new criterion requires a non-zero exit when the editor log's InternalLoadLibrary line for FANTASIATests names any directory other than the one the run compiled into.

The rewrite is stronger than what it replaced: it asserts mechanically what the original clause could only assume. The user was told that, and told that declining would send the story back to ready with no loss, since the criterion describes what the script already does.

With this, no edit to an accepted story's criteria in S-01 is unconfirmed.

## J-0151 | 2026-11-15 | F-045 | test-dev
- Event: fixed
- Evidence: CLAUDE.md now states 152 declared / 150 run / 25 via wrapper (grep recount: 127 direct + 25 wrapper over 15 files) and names gates.task_completed as FANTASIA/run-tests.ps1; the 'null' and 'first-sprint work' sentences are gone. Not gate-run in this pass, by instruction.
- Changes: FANTASIA/Source/FANTASIATests/CLAUDE.md

Also corrected the coverage table and the F-007 paragraph for US-028 and US-029, and replaced the stale 'Ten Tests: paths' count with a pointer to the backlog markers.

## J-0152 | 2026-11-15 | DEC-017 | tester
- Event: decided
- Module: M-tests
- Decision: In the Quality secret tests the source scanner strips comments and string contents once per file (ScanSource returns both findings), the secret-word test is its own NamesSecret predicate, and the sweep's offence is one named predicate IsEditableSecretWithoutConfig; FLogCapture::Num and the redundant key== clause are removed. The GConfig discipline is untouched: one fake key, in-memory entries restored or removed in a destructor, no flush, and the single APIKey injection literal kept as it was.
- Reason: S-01 retrospective cleanup (T-21): StripCommentsAndStrings ran twice per scanned file, the Key and SecretWords test lived inline in the scan loop, and the sweep's offence was a three-term expression a reader had to decode. Behaviour is unchanged; no test name or count changes; the S-01 secrets scan credited the GConfig discipline, so none of its three properties moved.
- Changes: none

Files: Private/Quality/SecretTestSupport.h, SecretLeakageTests.cpp, SecretConfigRobustnessTests.cpp. Alternatives rejected: a helper for the repeated key-only Config literal (four spellings) - rejected because it would move a literal the security scan reads; merging the two FindFilesRecursive globs - rejected as four directory walks against thousands of file reads. Not compiled or run here: the gate is run once by the lead after all cleanup passes land.

## J-0153 | 2026-11-15 | DEC-018 | build-dev
- Module: M-dist
- Decision: The S-01 cleanup pass over FANTASIA/run-tests.ps1 changes no behaviour: it hoists the two repeated harness path prefixes into $logDir and $sourceDir, names the provenance match ($ownInstances) and tests it by count rather than by pipeline truthiness, and corrects the header's exit-0 sentence to include the report's provenance. The exit-code contract, the invocation, the lock and the pid check are untouched.
- Reason: The file went 430 to 545 lines over two rounds and the second was under pressure to close F-035, so it was audited against the six cleanup aspects. Only duplication, naming and header drift were found; the pass verified the rest rather than changing it. Path equality was proved for all six hoisted constructions and the provenance predicate was compared against the old one over eight device lists, including the empty, single-match and no-match cases; the script was parse-checked with the 5.1 parser and left ASCII only, since a non-ASCII byte read as the ANSI codepage is how this module has broken a script before.
- Changes: FANTASIA/run-tests.ps1

Alternatives rejected: folding the three engine-file existence guards into a table-driven loop (it would add machinery to save nine lines and would flatten the one message that distinguishes 'no engine at all' from 'an engine missing a tool'); replacing the devices projection with member enumeration $report.devices.instanceName (shorter, but a null devices array would then print as a one-element list of nothing in the exit-3 message, changing the diagnostic); deleting the retired-exit-4 paragraph from the header (DEC-015 requires the header to record that 4 is vacant and why, because the other numbers are quoted outside the script and 4 must not be reused); reverse-reading the editor log instead of Select-String (the whole pass is a fraction of a 30-45 s warm run dominated by UnrealBuildTool and editor startup, and the hot-path rule asks for a measurement before trading readability for speed).

## J-0154 | 2026-11-15 | NOTE | lead
- Story: US-027, US-028, US-029
- Gate: PASSED, 150 tests, 0 failures, 84.8 s, exit 0
- Changes: none

S-01's retrospective cleanup pass is complete across its three scopes (T-19 build-dev, T-20 test-dev, T-21 tester; DEC-018, DEC-017) and the gate is green after it.

The gate run is the drift check, and it carries more weight than planned. The method's post-cleanup step is that the reviewer reads the diff for behaviour drift, and that cannot be done here: every file S-01 produced is untracked - Private/Quality/, both new test files and run-tests.ps1 - so no baseline exists to diff against. That is a direct consequence of the lead's decision to defer the commit until after cleanup in order to rebuild the prebuilt DLLs once rather than twice. The method puts the commit at review, before the retrospective, and this is what the deviation cost. Recorded so the trade is visible rather than discovered later.

What stands in its place. The gate compiled 7 actions, of which exactly 4 were compiles - SecretConfigTests.cpp, SecretConfigRobustnessTests.cpp, WhisperCaptureComponentTests.cpp, SecretLeakageTests.cpp - which is precisely the set of files the three passes touched and nothing else; 150 of 150 tests passed, including the tester's 15 Quality tests and ScannerControl's 13 assertions, which exist to pin the scanner the tester refactored; the editor loaded FANTASIATests from the directory this run compiled into; and the report was stamped by this run's own editor (pid 78684).

Five edits the suite cannot vouch for were verified by the lead against the source directly, because each could have changed behaviour while leaving every test green:
1. MakeRamp computes Out[i] = i * Step, multiplication rather than accumulation, so the DSP inputs behind literals pinned at 1e-5 and 1e-4 are bit-identical to the loops it replaced. An accumulating ramp would have drifted and the tests would have passed while measuring something else.
2. The FScopedGameConfigEntry parameter rename from Key to KeyName is consistent across the initialiser, the body and the destructor, which must use the member.
3. ScanSource strips once at :184 and hands the same stripped text to both finders, and the ScannerControl lambdas call ScanSource itself rather than a parallel copy.
4. NamesSecret's early return replaces a boolean accumulation over side-effect-free predicates; the Hits accumulation it feeds lives in the caller.
5. LooksSecret's removed 'L == "key" ||' is covered by its trailing return L.EndsWith(TEXT("key")). This is the one the gate provably could not catch: the only property named exactly Key is UElevenLabsTTSComponent::Key, which already carries Config, so it never appears as an offender and a regression here would have passed 150/150 in silence.

build-dev proved its own two refactors rather than arguing them: the six hoisted path constructions compared -ceq identical old against new, and the provenance predicate was tested equivalent over eight device lists including null, empty string and a bare pid. It also declined the one cleanup that would have broken a recorded decision - DEC-015 requires the header to record that exit code 4 is vacant and why, because the other codes are quoted outside the script.

## J-0155 | 2026-11-15 | NOTE | lead
- Finding: F-017, F-041
- Module: M-asr
- Story: US-029
- Changes: none

F-017 and F-041 share a fix site and should be scheduled as one patch, not two. Found by speech-dev during the S-01 memory tidy-up, reading the journal rather than the briefing it was given.

Both findings pass through WhisperCaptureComponent.cpp:986, SetRecentPeak's RoundToInt(Value * 10000.f) with no clamp. F-017 supplies the out-of-range float by reading one frame past the resampler's input; F-041's second half is that the narrowing is unclamped at all. Whoever schedules them should expect one patch to close both halves - the clamp - plus F-041's own first half, which is separate: the NaN/Inf guard sits in TranscribeBuffer, downstream of both the peak publication and the high-pass, so one non-finite sample latches the IIR for the session.

Two operational consequences worth having before the fix is planned:

- The existing test Resample.ReadsOneFramePastInput passes *because* the over-read reaches the output. Fixing F-017 therefore requires updating that test, which is M-tests' file and not M-asr's, so the fix needs speech-dev and test-dev together rather than one task.
- F-017 makes the capture path uninspectable under Page Heap or ASan - a hard editor crash - so it blocks the one tool that would find the rest of F-007's gap. The security engineer reached the same conclusion independently in J-0130.

Also recorded as a correction to my own briefing practice: the brief I gave speech-dev named four open findings against its modules where thirteen stand open (F-003, F-007, F-009, F-016, F-017, F-019, F-022, F-023, F-030, F-031, F-033, F-039, F-041). It declined to copy the short list into memory and pointed at 'ateam_log.py status --open' as the authority instead, naming the briefing as the thing not to trust. That is the correct resolution and the same rule the ADR-0016 and CR-008 episodes produced, now applied to the lead.

## J-0156 | 2026-11-15 | F-046 | lead
- Event: opened
- Kind: quality
- Severity: major
- Story: US-018, US-019
- Module: M-dist
- Repro: .gitignore:93 is the single lowercase pattern 'claude.md'. Git's ignore matching is case-insensitive on this NTFS checkout, so it matches every CLAUDE.md at every depth: git check-ignore -v reports '.gitignore:93:claude.md' for CLAUDE.md, FANTASIA/Source/FANTASIA/CLAUDE.md and FANTASIA/Source/FANTASIATests/CLAUDE.md, and git ls-files --error-unmatch fails for all three. None is tracked.
- Changes: none

Found while staging S-01's commit, which is the only reason it surfaced: the files were edited all sprint and never appeared in git status.

Why this is major rather than cosmetic. The project's own protocol document says 'State: docs/ateam/ (except HISTORY.md) and the CLAUDE.md in each module directory say how things are now.' So module CLAUDE.md files are designated state documents, the planner and test-dev maintain them as such, and the ATeam checker counts them. None of them is version-controlled, so a reclone of this repository has no module state documents and no protocol document at all. docs/ateam/ is tracked, which is far more process-heavy material, so the inconsistency points at an accident rather than a distribution decision.

Probable cause, worth stating because it decides the fix: the pattern is lowercase. On a case-sensitive filesystem it would match only a literal lowercase claude.md, which is plausibly what someone meant to exclude. On Windows it takes every CLAUDE.md with it. The fix is therefore not simply 'delete line 93' - it needs to say what was meant.

Immediate consequence for the record: F-045 was logged fixed at J-0151, and its fix is test-dev's rewrite of FANTASIA/Source/FANTASIATests/CLAUDE.md. That file is ignored, so the fix exists only in this working copy and cannot be committed. F-045 is fixed locally and not in the repository, and the same is true of the planner's rewrite of FANTASIA/Source/FANTASIA/CLAUDE.md at J-0113.

Whether these documents should ship is a distribution question for build-dev and the user, not something to settle inside a commit, so S-01's source commit proceeds without them and this finding carries the decision.

## J-0157 | 2026-11-15 | F-042 | build-dev
- Event: fixed
- Module: M-dist
- Evidence: git check-ignore -v reports .gitignore:38 FANTASIA/ThirdParty/aGrUM/Libraries/ for agrumBASE.lib, agrumBN.lib and agrumID.lib; the gRPC and OpenSSL release libs still report .gitignore:39 and :40 unchanged; git status --porcelain no longer lists FANTASIA/ThirdParty/aGrUM/Libraries/ and nothing else in that list changed
- Changes: .gitignore

The stale aGrUM 1.x filename at .gitignore:35 is replaced by the library DIRECTORY, matching how the two sibling third-party library directories (gRPC, OpenSSL) were already ignored:

    FANTASIA/ThirdParty/aGrUM/Libraries/

A directory pattern is version-proof: the next aGrUM bump can rename or re-split the libs
without reopening this hole. A comment above the three entries records that reason.

Nothing under that directory was ever tracked: git ls-files on it returns 0 paths, and
b6d1ea36 (the 2.3.2 upgrade) touched only headers and Build.cs there. So ignoring the
directory removes nothing from the repository.

Comment-block half done here: the four 'runtime-v2.0' references in .gitignore now say
runtime-v3.0, matching bootstrap.bat:46 and bootstrap.sh:30, and the size estimate for the
three library directories is corrected from ~330 MB to the measured ~420 MB (aGrUM 249 +
gRPC 144 + OpenSSL 23). THIRD_PARTY_LICENSES.md:25 and :59 are untouched and remain F-043.

One correction to the opening entry: .gitattributes:2 DOES route *.lib to LFS, so a stray
'git add -A' would have pushed 249 MB as LFS objects that every default clone then fetches -
worse than plain objects, not better.

Nothing staged or committed; the .gitignore edit sits in the working tree.

## J-0158 | 2026-11-15 | F-046 | lead
- Event: opened
- Kind: quality
- Severity: major
- Story: US-018, US-019
- Module: M-dist
- Corrects: J-0156
- Repro: The ignore block is four lines, not one: '.claude/', '.claude', 'CLAUDE.md' and 'claude.md' (93-96 after T-22's edit), under the comment 'Tools - keep Claude Code dev artifacts out of the repo'. CLAUDE.md is ignored by an explicit uppercase pattern of its own; git check-ignore -v names only the lowercase line because git reports the last matching pattern. Four files are affected, not three: ./CLAUDE.md, FANTASIA/Source/FANTASIA/CLAUDE.md, FANTASIA/Source/FANTASIATests/CLAUDE.md and FANTASIAACE/Source/FANTASIAACE/CLAUDE.md.
- Changes: none

Record correction. Severity, kind, module and consequence all stand; my stated cause did not, and build-dev caught it in T-22.

I wrote J-0156 as an accident of case - a lowercase pattern that would match nothing on Linux taking every CLAUDE.md on Windows. That is wrong. The block carries an explicit 'CLAUDE.md' line, so these files are ignored deliberately and not by accident, and un-ignoring means removing two patterns rather than one. The lowercase line is redundant belt-and-braces beside it.

What survives of the diagnosis is staleness rather than case: the block's comment is about keeping Claude Code dev artifacts out of the repository, it predates the ATeam adoption (4bb82896), and at that time the only such file was the root one, which genuinely was local dev notes. It was never revisited when module CLAUDE.md files became designated state documents. So the same failure mode as F-042 - a pattern that was right when written and was not followed when the thing it described changed.

build-dev's recommendation, which is better than the one implied by my own entry and is the user's to accept: narrow the ignore to '/CLAUDE.md', root-anchored, so the three module state documents become tracked while the protocol file stays local. Two reasons not to simply un-ignore all four. The root file instructs a reader to run /ateam:request, /ateam:change and the .claude/ agents, none of which a cloner receives, since .claude/ is gitignored by deliberate choice - so it would ship instructions that cannot be followed. And the root file and FANTASIA/Source/FANTASIA/CLAUDE.md both hard-code 'C:/Users/ACER/.claude/skills/ateam/scripts/...', an absolute path carrying the owner's account name, which should not be published in a plugin users clone. Those paths want making relative before any of this ships, whatever the user decides about tracking.

Unchanged: F-045's fix and the planner's J-0113 rewrite both live in ignored files and cannot be committed, so neither is fixed in the repository.

## J-0159 | 2026-11-15 | S-01 | lead
- Event: closed
- Done: US-027, US-028, US-029
- Review: The user accepted all three stories (J-0147), US-028 explicitly including F-031 as its disclosed residual; confirmed REQ-003's rejection (J-0149) and US-027's CR-005-rewritten acceptance criterion (J-0150). No story was sent back. The user also approved CR-004 (P-19's security carve-out), CR-008 (the gate's ownership, which cleared the checker's last error) and CR-009 (US-029's criterion plus US-033).
- Retrospective: What slowed it: nothing was blocked, but three defects in the sprint's own output were found by the reviewer rather than by the gate (F-035, F-036, F-038), and two acceptance criteria proved unsatisfiable as written - US-027's harness clause (CR-005) and US-029's StartCapture clause in both its When and its Then (CR-009). Findings by kind: 16 security, 9 quality, 4 bug. The security count is a first measurement rather than a process failure, because T-01 created the threat model where none existed; it becomes a failure if the next sprint produces another 16. Ownership: .ateam/ownership.jsonl is empty, 0 would-be violations since S-01 opened, which is the evidence for a team CR leaving observe mode. Requests: none arrived during S-01, so the checker's intake statistics say nothing about this sprint's brief or principles. Quality tools: gates.quality is still empty for cpp and csharp and there is no .clang-format. CRs raised: CR-004 principle, CR-005 architecture, CR-007 contract, CR-008 team, CR-009 story. Cleanup: three of five coders ran a pass; core-dev's and speech-dev's diffs were one specifier and one doc comment, and the skips are recorded rather than omitted.
- Cost: $184.65 at the 2026-11-13 list prices (lead 43%, planner 15%, security 14%); cold-cache rewrites $15.76; flags: 4
- Commit: 13b91fce
- Changes: backlog.md, sprint.md

Verification summary.

Tests: the suite runs 150 tests across 15 files, up from 107 at adoption; 152 are declared and two are shadowed by being internal nodes of their own automation path (F-037). Added this sprint: 25 capture characterisation tests over the defaults, the device-free controls and the four DSP steps; the secret-config ini round-trip and the nine-property reflection sweep; and the Quality secret-leakage suite with its source scanner. The gate's own last run: 150 tests, 0 failures, 84.8 s, the module loaded from the directory the run compiled into and the report stamped by the run's own editor.

Findings: 29 opened (2 blocker, 12 major, 15 minor), 5 fixed - F-008, F-029, F-035, F-036 and F-038. Three entries were re-rulings or record corrections rather than new defects: F-017 from minor to major with Kind=security, F-018 re-pointed from US-029 to US-033, and F-042's consequence corrected once *.lib was found to route to LFS.

Stress measures against NFRs: none, and this is the honest gap. NFR-001 to NFR-003 still have no measure; nothing could check a latency budget before a gate existed, and writing those measures is the first thing the gate makes possible. The one timing fact recorded is a risk rather than a measure: the capture suite runs in 0.98 s against US-029's 'under a second', mostly per-test framework overhead, and it will exceed that budget on a slower machine.

Security pass: the dependency audit priced ten dependencies from the tree, each with the file and line its version was read from, three of them pinned to 2022-era or end-of-life code, and stated that no CVE feed was queried because the project has no mechanism permitting one. The secrets scan covered the working tree against HEAD, the printable strings of both rebuilt DLLs and 16 credential patterns; it found one match, the test suite's own ini fixture. It scanned the working tree rather than commits because S-01 had none, which is also why the method's post-cleanup diff read was impossible (J-0154).

## J-0160 | 2026-11-15 | CR-010 | lead
- Event: proposed
- Type: contract
- Class: major
- Impact: Contracts: C-neo4j loses UseSSL from its configurable surface (line 19) and its TLS clause is rewritten so the scheme is the only thing that decides encryption; lines 81-83, which currently describe the silent downgrade as present fact, go with it. This is an interface removal, so C-neo4j takes an integer version rather than a point release. Modules: M-neo4j, core-dev's. Blueprint surface: UseSSL is a reflected property on UNeo4jComponent (Neo4jComponent.h:124), so removing it invalidates any compiled Blueprint that pins it - P-03 engaged independently of P-19, which is why this CR exists even though CR-004 already admits the fix to the 5.8 line. Consumers: M-demo and M-tests; no test references UseSSL, so the suite is unaffected. Stories: US-032 moves from draft to ready with this shape. Findings: F-011 closes when US-032 lands; F-020 and F-021 are separate and sequenced before it.
- Options: (A) Remove UseSSL so the scheme alone decides TLS, as Neo4j's own drivers do - the misleading state becomes unrepresentable rather than merely reported. (B) Keep UseSSL and refuse to connect when it is true against a non-TLS scheme: no Blueprint surface moves, and nothing silently downgrades. (C) Keep both property and behaviour and log loudly on the downgrade: smallest change, and the password still crosses the network in clear for anyone who misses the log line. (D) Don't change: F-011 stays open and a user who sets UseSSL against bolt:// is told nothing.
- Changes: none

F-011 is that setting UseSSL = true against a bolt:// endpoint sends the Neo4j password in clear with no warning. P-19 refused the fix for the life of the 5.8 line until CR-004's security carve-out admitted it; the shape was left to the user and this CR records their choice.

The user chose (A): remove UseSSL. The property's only honest value is the one the scheme already implies, so the fix is to stop offering a knob that cannot be obeyed.

One thing US-032 must settle rather than assume, flagged here because it was noticed while writing this entry and not verified: Neo4jComponent.cpp:117-122 forces UseSSL to match the scheme and only respects the configured value for a scheme it does not recognise, while the comment at :184 says it honours UseSSL even where the scheme did not imply TLS, describing an upgrade. Those two may not agree, and if they do not, the two transports promise different things - which is the same shape as the gap US-031 closes between the plain-TCP and WebSocket TLS paths. The story should establish which is true before it removes anything.

P-07 applies with force here: the admitted item is F-011's fix and the contract edit that describes the new behaviour, not the refactor it would be convenient to carry while C-neo4j is open.

## J-0161 | 2026-11-15 | CR-010 | lead
- Event: approved
- Decision: Option (A): remove UseSSL, so the endpoint scheme is the only thing that decides whether the connection is encrypted.
- Approver: user
- Changes: none

Chosen by the user at S-02 planning, from the three shapes the backlog had drafted. The planner's recommendation and the user's choice agree.

## J-0162 | 2026-11-15 | F-046 | lead
- Event: accepted
- Reason: The user chose to leave every CLAUDE.md ignored. The module state documents stay local by decision rather than by accident, which is what this finding existed to establish.
- Changes: none

Accepted, not fixed, and the consequences are accepted with it: a reclone of this repository gets no module state documents and no protocol document; F-045's fix and the runtime module's J-0113 rewrite remain in this working copy only, so neither is fixed in the repository; and the ATeam scripts' hard-coded C:/Users/ACER/... paths are not published, which is the one benefit of the decision.

The finding's value was never the fix. It was that three files the project's own protocol designates as state were untracked without anyone having decided that. That is now decided.

## J-0163 | 2026-11-15 | NOTE | lead
- Story: US-031
- Finding: F-020
- Decision: The user confirmed that US-031 may break a connection that works today.
- Approver: user
- Changes: none

US-031 makes bolt+s:// and neo4j+s:// verify the server's certificate against a trust store and check its hostname, which stops a self-signed server connecting where it connects today. The sprint method asks the user to confirm that trade before the story is planned, and they did, choosing it over warning for the 5.8 line and enforcing in 5.9.

What the story therefore owes, and the reason the alternative was offered at all: the migration is one character - bolt+ssc:// accepts a self-signed certificate and is the documented way to opt out - and that must appear in the failure's log message and in C-neo4j, or the first report will be 'the update broke my database' rather than 'I need to change my URL'.

The rejected option is recorded because it was not unreasonable: warning and connecting breaks nothing on update. It was declined because it leaves F-020's exposure open for the life of the line, and F-020 is the finding that makes the password readable to anyone who can answer for the host.

## J-0164 | 2026-11-15 | DEC-019 | planner
- Module: M-tests
- Decision: The security engineer writes US-030's and US-031's acceptance tests in the Security/ directory it owns, and does not review the implementation tasks whose tests it wrote; the reviewer reviews implementation and test together and the tester runs the independent pass on the landed fix.
- Reason: The batteries are adversarial encodings and an OpenSSL-context assertion, which is the security role's competence; both findings are its own, so the test is its repro turned into an assertion; and moving the paths out of Security/ would need a team change, which is the user's.
- Changes: none

`team.json` gives `FANTASIA/Source/FANTASIATests/Private/Security/` to the security
engineer and the rest of `Private/` to test-dev, and the most specific path wins. US-030's
and US-031's `Tests:` paths are in that directory, so the acceptance tests of two stories
are in their own auditor's hands. That is a different arrangement from the method's default
flow — the coder implements, the tester tests, the security engineer reviews — and it should
be chosen rather than inherited from a path.

It is chosen, for three reasons. The tests are adversarial batteries and a TLS-context
assertion, which is the security role's competence and not a sonnet coder's: a weak battery
written by test-dev would be rewritten by the security engineer anyway, at the cost of two
runs and a review cycle. The repro is already the security engineer's — both findings are
its own, with byte sequences and the four missing OpenSSL calls named — so the test is its
repro turned into an assertion rather than a fresh act of invention. And moving the paths out
of `Security/` to keep the roles apart would either leave the security engineer's own
directory empty of the tests that belong in it or require a team change, which is the user's.

What it costs, and the two things that pay for it. The cost is that nobody independent checks
whether the battery asserts the right invariant. It is paid first by the authoring rule on
the two test tasks: T-25 is written from F-021's repro and US-030's acceptance criteria and
**not** from T-24's diff, which is feasible because `ReadValue`'s signature does not change;
T-27's test reads only the declarations of the seam it calls, not its bodies. A test that
mirrors the implementation's structure proves the implementation is self-consistent and
nothing else. It is paid second by the reviewer, whose first object on both test tasks is
whether every criterion has an assertion and whether at least one assertion would fail
against the pre-fix code, and by the tester's independent pass on the landed fix — which is
where independence was demonstrated in S-01, when the tester opened F-031, F-032 and F-033
against US-028's fix rather than confirming it.

The corollary, and the reason this is written down: the security engineer does not also
review the two implementation tasks whose tests it wrote. The reviewer reviews implementation
and test together. If the lead's dispatch requires a security review on every task carrying a
`security:` line, then this arrangement inverts — test-dev writes the batteries in `Private/`
from a specification the security engineer writes — and the planner's recommendation is the
arrangement above, not the inversion.

## J-0165 | 2026-11-15 | NOTE | planner
- Stories: US-030, US-031, US-034
- Held: US-032, US-033
- Decision: DEC-019
- Changes: backlog.md, architecture.md, verification/core-dev.md, verification/build-dev.md, verification/speech-dev.md, verification/test-dev.md

The goal the user set is to make the Neo4j connection safe to point at a real server. Two
stories reach it and a third does not, which is the whole of the recommendation below.

**Two stories, not three.** US-030 (F-021, the reader bounds) then US-031 (F-020, server
verification). US-032 (F-011, `UseSSL`) is refined to `ready` with CR-010's shape and held
for S-03. Four reasons, in the order they decided it. US-014's `Blocked by:` names F-020 and
F-021 and not F-011, so closing exactly these two is what returns US-014 to `done` and
unblocks US-020 and US-021, the 5.8 ship; F-011 is on US-014's `Known defect:` line and on no
critical path. US-031 is bigger than its `S` said, because the decision it has to assert is
not reachable from any test today and the story now carries a seam as well as a fix, so it is
re-estimated `M`. US-032 depends on US-031's seam, so holding it costs no rework and buys the
one thing a one-coder critical path cannot buy itself: slack. And US-031 already breaks a
working configuration (a self-signed certificate on a `bolt+s://` URL); shipping `UseSSL`'s
removal in the same release puts two breaking migrations of the same property of the same
component into one release note, where two releases with one each are the same work and a
better explanation.

**US-033 is out.** It is `ready` and it closes F-018, and it is M-asr and does not serve this
goal. The argument for it is real and is recorded here so that the user can overrule knowing
it: speech-dev and test-dev are otherwise idle while core-dev serialises, the story is two
small tasks on files nothing else in the sprint touches, and it would cost about three
dollars. The argument against is that it makes the sprint's answer to "what was S-02 about"
two sentences instead of one, and S-01's own retrospective named scope discipline as the
thing most likely to be tested. It stays at the head of the S-03 candidates with US-032.

**One new story, US-034**, for the two tooltip sentences the user asked to ride into S-02:
F-032 (M-llm, `Apply Provider Preset` erases the configured key) and F-033 (M-tts,
`LoadConfig` reads the ini section of the instance's own class). It is off-goal and included
because the user asked. It is a new story rather than two tasks citing US-028 because a task
names a story and US-028 is `done`: citing it would attach work to a story the project is
finished with, and reopening it would be worse, since `done` says what the system does today
and only an open blocker takes that away (US-014 is the live case). US-034 is also two tasks
and not one, because the two properties are in different modules and different owners' files
— `OpenAICompatibleComponent.h` is core-dev's and `RESTTTSComponent.h` is speech-dev's — plus
a third for the assertion in `SecretConfigTests.cpp`, which is test-dev's.

**Nine tasks, T-24 to T-32.** T-24 (US-030, M-neo4j, core-dev) → T-25 (US-030's battery,
security) → T-27 (US-031, M-neo4j, core-dev) → T-28 (US-031's TLS test, security) → T-32
(C-neo4j, planner) is the serial chain and the sprint's length. T-26 (US-031, M-dist,
build-dev) adds OpenSSL to `FANTASIATests.Build.cs` and runs first, in parallel, because
without it T-28 can assert the TLS *decision* and not the TLS *context* — and F-020 is a
defect in the applying, not in the deciding, so a test that cannot see the context does not
pin it. T-29, T-30 and T-31 (US-034) are off the chain entirely. Who writes the two security
batteries, and what that costs, is DEC-019.

**The open question CR-010 left to US-032 is answered: they do not disagree.**
`InitiateConnection` is called from `BeginPlay` with `ExtractHostname(endpoint)` and from
`ReconnectToBackendAddress` with a bare `host`, so the full-URL branch at
`Neo4jComponent.cpp:172-181` never runs and the `ws://`-to-`wss://` upgrade at `:186` can
never fire — when `bActiveTls` is true the `else` branch has already written `wss://`, which
does not start with `ws://`. Both transports take encryption from the single
`bActiveTls = UseSSL` at `:147` and promise the same thing; the comment at `:184` describes a
dead path. Two consequences for US-032, both now criteria on the story. The property is
honoured today only for `neo4j://` and for an endpoint with no recognised scheme, because
`BeginPlay` forces it true for the six `+s`, `+ssc`, `https` and `wss` schemes and false for
`bolt://`, `http://` and `ws://` and `neo4j://` is in neither list — so those are the only
configurations the removal changes, and for them it changes encrypted to plaintext, which is
a silent downgrade introduced by the fix for a silent downgrade unless the component says so.
It must say so, naming `neo4j+s://`, which is the same one-character migration US-031 needs.
And the dead upgrade at `:184-186` goes with the property, because it exists only to honour
it; the dead branch above it stays, under P-07, and is a quality finding for the tester or
the security engineer to open.

**Two things found while planning that belong to other owners.** The threat model's § M-neo4j
says F-011 is excluded from the 5.8 line by CR-001, where CR-004 admitted it and CR-010 chose
its shape, and it lists `UseSSL` among M-neo4j's entry points, which US-032 removes; that file
is the security engineer's. And `FBoltDechunker` carries no `FANTASIA_API` and gains none in
US-030, because F-012 stays deferred and widening C-neo4j's surface is not what CR-004 admits:
the message cap is therefore enforced on `UNeo4jComponent`'s side of the dechunker, where the
class's own `FANTASIA_API` already exports the private members a test needs, and if M-neo4j
concludes it can only live inside the dechunker the task stops for a CR rather than exporting
a class in passing.

**A budget of $160, against an expected $145.** Built from S-01's medians times the planned
runs, with two corrections S-01's numbers do not carry. core-dev's $1.53 median came from a
one-property edit and will not hold for a bounds pass over a wire codec or for a TLS fix with
a seam: $12 and $15 are the honest figures, with $2 for the tooltip. The security engineer's
two test tasks are writing rather than auditing, so $10 and $12 rather than $12.32 twice.
Then build-dev $4, speech-dev $2, test-dev $1, the planner $12 for the contract and a
contingency, five reviewer passes at $5, two tester passes at $4, and the lead's own session
at about $40 — S-01's single run cost $67.45 across 23 tasks and this sprint has nine. The
lead's line is the largest and the least predictable, and the two ways the figure breaks are a
second core-dev attempt on T-27 if the seam is wrong, and this planning run itself if the cost
hook bills it to S-02 rather than to the sprint that is closed.

## J-0166 | 2026-11-15 | NOTE | lead
- Decision: DEC-019's arrangement stands: the security engineer writes the batteries under Private/Security/, and no security review runs on the two implementation tasks whose tests it wrote.
- Reference: DEC-019
- Changes: none

The planner asked for the lead's dispatch ruling, because requiring a security review on every task carrying a security: line would invert the arrangement and hand the batteries to test-dev instead. The arrangement stands, for one reason that is evidence rather than preference: the backstop against a battery asserting the wrong invariant is the tester's independent pass on the landed fix, and that is exactly where independence showed in S-01 - the tester opened F-031, F-032 and F-033 against US-028's fix after the coders and the gate were satisfied with it. The reviewer reads implementation and test together, which catches a test written to the code rather than to the criteria.

The three costs the planner named are accepted as named: T-25 is written from F-021's repro and US-030's criteria rather than from T-24's diff; T-28 reads only the seam's declarations; and the security engineer does not review T-24 or T-27.

## J-0167 | 2026-11-15 | F-047 | lead
- Event: opened
- Kind: quality
- Severity: minor
- Story: US-014
- Module: M-neo4j
- Repro: Neo4jComponent.cpp: InitiateConnection(const FString& Hostname, int32) is called only from :132 with the extracted hostname and from :1135 with a bare BackendHost, so the full-URL scheme-rewriting branch at :166-178 - which tests WsUrl.StartsWith("neo4j+s://") and five more - can never match its argument. The else at :180 always runs. The guarded upgrade at :186-187, 'if (bActiveTls && WsUrl.StartsWith(TEXT("ws://")))', is dead for a second and independent reason: when bActiveTls is true the else has already written 'wss://', and "wss://host".StartsWith("ws://") is false.
- Changes: none

Found by the planner while resolving CR-010's open question, and verified by the lead against the source and both call sites before logging.

Two dead regions, not one, and they fail differently. The scheme-rewriting branch is unreachable because of what its caller passes; the ws-to-wss upgrade is unreachable because of what the branch above it already wrote. Each would survive the other's removal.

Why it is only minor: nothing is wrong with the behaviour. Both transports take encryption from the single bActiveTls = UseSSL at :147, which is correct, and the dead code cannot produce a wrong answer because it cannot run. The cost is that the comment at :184 documents a capability the component does not have - 'Honor UseSSL even when the scheme didn't already imply TLS' - which is exactly the kind of sentence someone later defends in a review.

Scheduling: US-032 removes the upgrade at :186-187 along with the property it refers to, so that half closes with F-011. The unreachable branch at :166-178 is not US-032's and must not be swept into it - P-07 - so this finding is what carries it. It is M-neo4j's, so core-dev's, and it wants no CR: deleting unreachable private code moves no contract and no surface.

## J-0168 | 2026-11-15 | NOTE | planner
- Stories: US-030, US-031, US-034
- Decision: DEC-019
- Changes: backlog.md

US-030, US-031 and US-034 move from `ready` to `planned` for S-02, on the lead's acceptance of the plan in J-0165. US-032 stays `ready` and is held for S-03 with the `neo4j://` fact going back to the user before it is planned; US-033 stays `ready` and is out of S-02. No criterion and no Tests path moves with the status: the three stories are planned as they were refined.

## J-0169 | 2026-11-15 | S-02 | lead
- Event: opened
- Goal: Close the two blocker findings that make the Bolt connection unsafe against a real server - an unbounded message reader (F-021) and a TLS path that authenticates nobody (F-020) - so that US-014 returns to done and the 5.8 ship stops waiting on them.
- Stories: US-030, US-031, US-034
- Budget: 160 USD at list prices, expected 143
- Changes: sprint.md

The user confirmed the goal before planning. Three things about this sprint's shape are deliberate and were argued rather than assumed.

Two stories, not three. US-014's Blocked by line names F-020 and F-021 and not F-011, so closing exactly those two returns US-014 to done and unblocks US-020 and US-021; F-011 is on its Known defect line and on no critical path. US-032 is refined to ready and held for S-03 because it depends on the seam US-031 builds, so holding it costs no rework, and because shipping UseSSL's removal alongside US-031's verification change would put two breaking migrations of the same property of the same component into one release note.

One coder owns the critical path. All of M-neo4j is core-dev's, so T-24 to T-27 run in sequence and the sprint is that chain. T-26 is parallel and exists so T-28 can assert the TLS context rather than the TLS decision - F-020 is a defect in the applying, not the deciding. The three US-034 tasks are off the chain.

US-031 grew from S to M during planning, for a reason that is now a standing rule: its acceptance test cannot be written against today's code, because ShouldUseSSL, ExtractHostname, TcpConnect and bActiveTls are all private and non-static, TcpConnect opens a socket, and the only friend is FNeo4jRecvRunnable. So the story carries a seam as well as a fix. The rule is CR-009's, established for M-asr and applied here: a criterion is met by a test that exists, so a story whose test cannot be written owes the seam that lets it be.

US-034 is off-goal. F-032's and F-033's tooltips do not serve a Neo4j sprint and are in it because the user asked for them in these words at planning: ride into S-02. Recorded so that the sprint's scope reads as a decision rather than as drift.

## J-0170 | 2026-11-15 | DEC-020 | core-dev
- Module: M-neo4j
- Decision: FBoltPackStreamReader refuses rather than clamps, through one Require(N) gate called before every read, Reserve and Append, and one funnel that turns any refusal into a null value; nesting is bounded at 64 containers by a depth parameter rather than a member counter.
- Reason: F-021's four unsafe paths share one arithmetic question -- are N more bytes available -- so they share one checked function instead of four inline comparisons. Require(N) refuses a negative N as well, which is what closes the signed int32 length and count cases. A declared element count is bounded by the same test because every element costs at least one byte, so a count larger than the bytes remaining is rejected before Reserve is reached, never after it. ReadValue(Depth) calls ReadMarkedValue and returns null whenever bRefused is set, so a refusal at any level of the recursion propagates outwards and a partially decoded value is never returned; Refuse() sets Cursor to Data.Num() so HasMore() reports exhausted and the next ReadValue refuses at once, and because Require short-circuits on bRefused only the first refusal logs. Depth is a parameter, not a member: there is no decrement for a return path to forget, which is the failure mode the verification profile asks to be checked for. 64 containers is eight times the deepest shape a Neo4j server builds from stored data (RECORD -> field list -> Path -> node list -> Node -> property map -> list), leaves room for a query returning its own nested literals, and bounds the reader's stack to a few kilobytes.
- Changes: none

Alternatives rejected: clamping a length to the bytes remaining, which decodes a truncated message as if it were whole and hands the caller a value it cannot distinguish from a real one -- refusal is the only answer that keeps the contract honest. A member depth counter incremented and decremented around the recursive calls, rejected because every early return becomes a place to leak the count. Separate bounds checks written inline at each of the four call sites, rejected as four chances to put the check after the act. Tightening the map bound to 2*Count (a pair is two values), rejected because 2*Count overflows int32 for a hostile count while Count alone is already a sufficient necessary condition. The 36 existing codec tests pass unchanged: every one feeds the reader bytes the writer produced, and Require's bound on a declared count is a necessary condition for well-formed input, so it never refuses valid data -- checked against the tightest cases in the suite, an empty list header, a 15-element tiny list and a structure whose fields end the buffer.

## J-0171 | 2026-11-15 | DEC-021 | core-dev
- Module: M-neo4j
- Decision: The Bolt message cap is 16 MiB, enforced in UNeo4jComponent::OnDataReceived before the bytes reach the dechunker, counted as bytes handed over since the dechunker last produced a complete message (UnresolvedMessageBytes); on a refusal the partial message is discarded, the dechunker is reset and the connection is left standing.
- Reason: FBoltDechunker carries no FANTASIA_API and gains none (F-012 stays deferred, and widening C-neo4j's surface is not what CR-004 admits), so the cap goes on the component's side, where UNeo4jComponent's class-level FANTASIA_API already exports the private members and a friend struct FNeo4jTestAccess reaches them. The counter is the component's honest view of an in-flight message: it is never less than the partial message the dechunker holds, so testing it before Accumulate bounds that message without reaching inside the dechunker. 16 MiB bounds ONE message, not one result set -- a PULL answers with one RECORD message per row plus a terminating SUCCESS, so a large result set is many bounded messages and never one big one, which is the objection the story raises against a cap. 16 MiB is 1024 maximum-size (16 KiB) Bolt chunks and three orders of magnitude above any single row the component can usefully convert into UNeo4jResultCell objects for Blueprint.
- Changes: none

What the component enforces is the cap plus at most one transport read, because the counter is zeroed when a message completes and a read that completes a message may carry the first bytes of the next one. The TCP recv buffer is 16384 bytes (Neo4jComponent.cpp:423), so the slack is 16 KiB against a 16 MiB cap. An exact bound would require the dechunker to report its partial size, which is a change to a class this task must not widen. Alternatives rejected: enforcing the cap inside FBoltDechunker, which is the natural home for it and is refused because the test module cannot link the class -- the sprint's standing decision is that this stops for a CR rather than exporting a class in passing. Tearing the connection down on a cap refusal, which is what a framing violation deserves and is NOT done here: the story asks only that the partial message be discarded with a logged reason, the tester's plan asserts the connection survives the refusal, and a teardown is behaviour P-07 keeps out of this task. The residual is that after a refusal the byte stream cannot be resynchronised -- subsequent bytes are read as chunk headers at an arbitrary offset -- so the connection reports itself healthy while decoding nothing. That is now bounded and memory-safe but it is a defect, and it is reported to the lead rather than fixed here. Also moved the dechunker's construction from InitiateConnection to the constructor, so the receive path is well defined before any connection and a test can drive OnDataReceived; InitiateConnection resets the dechunker instead of replacing it, which is the same per-connection state and one creation site instead of two.

## J-0172 | 2026-11-15 | F-048 | lead
- Event: opened
- Kind: bug
- Severity: major
- Story: US-030
- Module: M-neo4j
- Repro: After UNeo4jComponent::OnDataReceived refuses a message for exceeding MaxBoltMessageBytes it calls Dechunker->Reset(), so the next bytes off the socket are read as a chunk header from an arbitrary offset in the middle of the server's framing. The Bolt byte stream is then unresynchronisable: the connection reports itself healthy and decodes nothing but unknown markers, with no error delivered to the component's delegates.
- Changes: none

Raised by the lead from core-dev's T-24 report, where it is recorded only as a rejected alternative inside DEC-021. Opening it because a defect that lives in a DEC's rejected-alternatives section will never be scheduled, which is the same reason the EOL-OpenSSL risk was not left in the threat model's prose.

This is the residual of F-021's fix rather than a defect in it. F-021 asked that a hostile or faulty server not drive an out-of-range read, and it no longer can: the reader is bounded and memory-safe and the cap is enforced before accumulation. What the fix does not do is recover the framing, and core-dev was right to keep that out under P-07 - the honest repair is to tear the connection down on a framing violation, which US-030 does not ask for and which the tester's stress plan explicitly expects not to happen ('the connection survives the refusal').

Why major rather than minor: the user-visible result is a component that reports itself connected while answering nothing, with no diagnostic. For a researcher whose agent depends on Neo4j that is indistinguishable from a hang, and the log line naming the cap is the only trace. Why not blocker: the trigger is a message exceeding 16 MiB, so it takes a hostile or badly faulty server to reach, and reaching it no longer corrupts memory.

Kind is recorded as bug, deliberately not security. The trigger is attacker-controlled and a server able to force one refusal can keep the connection useless indefinitely, which is a denial of service and arguably makes this a security finding - but under CR-004 that classification decides what the 5.8 line admits, and only the security engineer's ruling carries the provenance the carve-out tests for. T-25's brief asks it to rule.

## J-0173 | 2026-11-15 | NOTE | security
- Event: rewritten
- Story: US-030
- Module: M-neo4j
- Changes: threat-model.md

F-021's threat is now mitigated with a test battery as the evidence, and F-048 is ruled a bug rather than a security finding.

T-25 put 18 tests under FANTASIA/Source/FANTASIATests/Private/Security/BoltPackStreamBoundsTests.cpp, with the FNeo4jTestAccess seam in a sibling header so that US-031's file can share one definition of the struct rather than two. They are written from US-030's five acceptance criteria and F-021's reproduction, not from T-24's diff (DEC-019): every criterion has an assertion, each refusal's log is counted with an exact occurrence number so a reader that logged per recursion level or per later call fails, and two tests assert the duals -- nesting within the limit decodes, and a value ending on the buffer's last byte decodes while the same value one byte short is refused. Without those duals the whole battery would also pass against a reader that refused everything, which is the one failure mode the arrangement's missing independent check cannot otherwise catch. The gate reports 168 tests, 0 failures.

F-048, ruled: Kind stays bug. It passes the STRIDE test on denial of service -- the trigger is attacker-controlled across TB-4, one 16 MiB message suffices, the effect outlasts the attacker, and the component reports itself healthy throughout -- so on the letter of my own brief it is mine. The reason it is not is the same reason the EOL OpenSSL is an Open risk rather than a finding (J-0137): under CR-004, Kind=security is not a neutral act of record-keeping, it licenses the fix onto the shipped 5.8 line whatever surface it touches. The only fix available for an unresynchronisable byte stream is a connection teardown on a framing violation, which changes what C-neo4j promises about a connection, fires on a merely faulty server as readily as on a hostile one, and lands in the disconnect path beside a live receive thread. Taking that unbarred is a larger risk than the one it answers; a defect whose admitted fix I would not let anyone act on is not a security finding. The threat model records three conditions that flip the ruling, the sharpest being a cap refusal reachable by a message a legitimate server sends. Checked while ruling: Dechunker->Reset() has exactly two call sites, per-connection at Neo4jComponent.cpp:141 and the refusal at :579, so the mid-stream desync is reachable only past 16 MiB.

Also corrected in the M-neo4j section: F-011 was recorded as excluded from the 5.8 line by CR-001, when CR-004 admitted it and CR-010 chose its shape. UseSSL stays in the entry-point list, because it is still in the header -- US-032 is ready, not done, and a state document that describes a tree which does not exist is worse than one a story will update. The line now says what is true today instead: BeginPlay forces the property for nine of the ten schemes (Neo4jComponent.cpp:119-125), so it is honoured only for neo4j:// and for a schemeless endpoint.

## J-0174 | 2026-11-15 | NOTE | lead
- Finding: F-048
- Reference: CR-004
- For: S-02 retrospective
- Changes: none

A side effect of CR-004 worth examining at the retrospective, noticed because the security engineer has now declined the security label twice for the same reason.

CR-004 made Kind=security an authorisation and not only a classification: a finding logged security by the security engineer at blocker or major is admitted to the shipped line whatever its surface. The security engineer has now twice declined to apply the label because of what it would license rather than because of what the defect is - the EOL OpenSSL pin (J-0137) and F-048 (J-0173). In both cases its reasoning is sound and it offered the ordinary route instead, a story on the next line or a CR to the user, so nothing is hidden and nothing is lost.

But the carve-out was written to admit security fixes, and its effect here is to make the project's security engineer withhold the label to avoid admitting a fix it judges worse than the defect. That is the bar working in one sense and inverted in another: classification now requires weighing the remedy, which is a harder judgement than describing the defect and one the classifier has an incentive to get wrong in the conservative direction.

No change proposed. Three things to weigh at the retrospective: whether a finding's kind should carry authorisation at all, or whether the carve-out should test severity and provenance while admission stays a separate step; whether 'Open risk with its trigger conditions written down' deserves to be a first-class state rather than the absence of a finding; and whether this has cost anything yet - so far it has not, because both defects are recorded with their flip conditions.

## J-0175 | 2026-11-15 | NOTE | lead
- Finding: F-048
- Module: M-neo4j
- Changes: none

The security engineer's ruling (J-0173) stands: F-048 is Kind=bug, routed the ordinary way - a story on the 5.9 line, or a change request if the user wants it on 5.8. Its three flip conditions are in the threat model, and it verified the second while ruling: Dechunker->Reset() has exactly two call sites, Neo4jComponent.cpp:141 per connection and :579 on the refusal, so the mid-stream desync is reachable only past 16 MiB.

Worth scheduling separately from the ruling, because it is the better half of the fix and needs none of the risk: the damaging part of F-048 is 'reports itself healthy', not 'cannot resynchronise'. Having the component stop reporting itself ready after a cap refusal, and log on each subsequent read, addresses the half that makes the defect indistinguishable from a hang. It is not a teardown, so it does not fire on a merely faulty server, does not contradict the tester's stress plan that the connection survives the refusal, and does not touch the disconnect path beside the live receive thread where a half-done teardown is a use-after-free - which is why the full fix was rejected. M-neo4j's to judge.

## J-0176 | 2026-11-15 | NOTE | lead
- For: T-32
- Story: US-030
- Changes: none

C-neo4j must not state the nesting limit as '64 containers'. Verified in the source: BoltPackStream.cpp:178 declares MaxBoltNestingDepth = 64 and :294 tests 'if (Depth >= MaxBoltNestingDepth)' against a zero-based depth, so depths 0 to 63 are entered and the element inside the 64th container is refused. The effective maximum a server can send is therefore 63 nested containers with a payload inside, which is what FANTASIA.Security.Bolt.Reader.NestingAtDepthLimitDecodes pins.

Found by the security engineer while writing the battery and confirmed by the lead. It is not a defect - the constant is named 64 and the refusal message says 64, so code and log agree - but a contract that says 64 would be wrong by one in the direction that matters, since it is the number a server author would build to. T-32 should state what a message may contain rather than what the counter reaches, and the wording is core-dev's to approve.

## J-0177 | 2026-11-15 | F-049 | lead
- Event: opened
- Kind: quality
- Severity: minor
- Story: US-034
- Module: M-llm
- Repro: OpenAICompatibleComponent.h:18, the ELLMProvider enum's own doc comment, directs the reader to 'UOpenAICompatibleComponent::ApplyPreset' - the C++ symbol. The Blueprint node is 'Apply Provider Preset' (meta = (DisplayName = ...) at :144), which is the only name a user of the enum in a graph ever sees.
- Changes: none

Surfaced by core-dev during T-29 and deliberately left: the task said one doc comment is the whole diff, and it held to that rather than widening. The right call, and the reason this entry exists - a one-word fix nobody logs never happens.

Same defect class as F-032 at one remove. F-032 was that the APIKey tooltip did not warn what Apply Provider Preset does to a configured key; this is that a sibling comment in the same file names a symbol the user cannot type. Minor rather than major because the consequence is a reader who cannot find the node, not a credential that goes somewhere unexpected.

Scheduling: it wants no task of its own. It rides into whichever core-dev task next opens this file, and the fix is replacing one symbol with the display name read from the UFUNCTION below it, the way T-29 verified all three names it used.

## J-0178 | 2026-11-15 | DEC-022 | build-dev
- Module: M-dist
- Decision: FANTASIATests links the ENGINE OpenSSL module via AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL"), so T-28 can read SSL_CTX verification state; no ADR is raised.
- Reason: A test-module dependency is not a shipped dependency: FANTASIATests is not among the plugin's shipped modules, so nothing a user links changes and P-06 does not engage. F-020 is a defect in the applying, not the deciding -- the code chooses TLS correctly and then builds a context that verifies nobody -- so a test that cannot inspect SSL_CTX could only re-assert the decision, which was never wrong.
- Changes: none

Alternatives rejected: (1) adding the plugin's own OpenSSL 1.1.1t include/lib paths as FANTASIA.Build.cs:54,56 do -- the test module has no reason to inherit the duplicate copy (F-044, open: the engine module at :48 and the bundled copy are byte-identical under SHA-256), and using the engine module in the new place means F-044's fix does not have to come back and fix this file too; (2) a public rather than private dependency -- nothing downstream of FANTASIATests exists, so private is the smaller surface; (3) asserting the TLS decision only, with no OpenSSL dependency -- that is the assertion F-020 shows to be insufficient. Gate: 168 tests, 0 failures, both before and after; the diff is three lines, one of them code.

## J-0179 | 2026-11-15 | F-050 | test-dev
- Event: opened
- Story: US-034
- Module: M-llm
- Kind: bug
- Severity: minor
- Repro: FANTASIA.Secrets.Tooltip.ApplyPreset (SecretConfigTests.cpp): read UFunction ApplyPreset ToolTip metadata
- Changes: none

The F-032 warning in the ApplyPreset doc comment never reaches the Blueprint node. UHT keeps only the @brief line and the @param line as the function's ToolTip metadata (observed text: '@brief Overwrites Config with the built-in preset for the given provider.' newline '@param Provider ...'); the paragraph that names 'Apply Provider Preset', says APIKey is discarded and that DefaultGame.ini keys are lost is dropped. A user hovering the node in the editor sees none of it; it exists only in the header. The property comment on Headers (F-033) does survive in full. Fix is core-dev's, in OpenAICompatibleComponent.h: put the warning in the brief sentence, or in a tooltip UFUNCTION meta key, so it appears in the reflected ToolTip. The test is marked expected-failure on this finding; when it is fixed the expected-error line fails and is to be deleted.

## J-0180 | 2026-11-15 | DEC-023 | core-dev
- Module: M-neo4j
- Story: US-031
- Task: T-27
- Decision: The chain is verified against the engine's own certificate bundle, found the way the engine's TLS stack finds it: the host project's Content/Certificates/cacert.pem if it exists, otherwise Engine/Content/Certificates/ThirdParty/cacert.pem, loaded with SSL_CTX_load_verify_locations and refused when the load does not return 1.
- Reason: SSL_CTX_set_default_verify_paths is not the alternative it appears to be on this platform. It resolves to the OPENSSLDIR compiled into the engine's static OpenSSL 1.1.1t build, which nothing populates, and it does not read the Windows certificate store at all - so it would load no anchors and every verified connection would fail, everywhere, including the Aura case the feature exists for. The engine bundle makes the guarantee statable: a server the engine's own HTTPS trusts is a server this component trusts, so a project has one trust store rather than two, and the project-content path is the documented place a site adds a private or corporate root. It also gives the explicit load return the story asks for, which the alternative route through the engine's SSL module cannot: ISslCertificateManager::AddCertificatesToSslContext returns void.
- Changes: FANTASIA/Source/FANTASIA/Private/Neo4jComponent.cpp

Alternatives rejected: (1) SSL_CTX_set_default_verify_paths, for the reason above - on Windows it is a silent no-op, which is F-020 with more code in it. (2) The engine's SSL module (FSslModule::Get().GetCertificateManager()->AddCertificatesToSslContext). It is the most faithful answer to 'what does the engine trust', since it honours the SSL section's OverrideCertificateBundlePath and the per-platform bundle as well, but it returns void, so a failed load is unobservable and the story's 'load return value is tested' criterion cannot be met without counting X509_STORE objects afterwards; and it needs the SSL module added to FANTASIA.Build.cs, which is build-dev's file and a second round trip on the critical path. (3) The Windows system store via CertOpenSystemStore, which would serve the corporate-proxy user directly but is a platform-specific store walk this plugin has no other use for, and leaves the engine and the component disagreeing about what is trusted. The one case the chosen store does not cover is a machine whose only anchor for the server is in the Windows store: for that user the remedy is in the refusal message - put the root in the project's bundle, or use +ssc deliberately. The per-platform candidate the engine also searches (Certificates/Windows/cacert.pem) is omitted because no such bundle ships for Win64 and this plugin is Win64-only; adding it would be a path that can never match.

## J-0181 | 2026-11-15 | DEC-024 | core-dev
- Module: M-neo4j
- Story: US-031
- Task: T-27
- Decision: The seam splits the connection's security into a decision and its application, and carries the host with the decision. DecideTransportSecurity(SchemeUrl, TargetHost, Transport, UseSSL) returns an FNeo4jTlsDecision as data; ApplyTlsVerification(void* SSL_CTX, Decision, OutFailure) configures a context the caller owns; TcpConnect takes the decision instead of (Hostname, Port, bUseTls); ExtractHostname, ShouldUseSSL and SchemeRequiresRouting become static; bActiveTls and ActiveHostname are replaced by one ActiveSecurity member. Six readers and two forwarders are added to FANTASIATestAccess in Private/Security/Neo4jTestAccess.h. No FANTASIA_API, no UPROPERTY, UFUNCTION, delegate or enum change.
- Reason: F-020 is a defect in the applying, so a seam that exposes only the decision would not pin it. Making the applier take an SSL_CTX by void* is what lets a test own the context and read SSL_CTX_get_verify_mode off it, and void* is the convention the component already uses for SslCtx so no OpenSSL header enters a public header. Folding Hostname into the decision is the structural answer to the routing question: there is one string, used for BIO_set_conn_hostname, for SNI and for the X509 verification parameter, so the host connected to and the name verified cannot drift apart.
- Changes: FANTASIA/Source/FANTASIA/Public/Neo4jComponent.h

Alternatives rejected: (1) Keeping TcpConnect's (Hostname, InPort, bUseTls) signature and verifying inside it. The verification would then be unreachable from a test without a server, and - the worse half - the host connected to and the host verified would be two values a future edit could let differ, which is exactly how the routing redirect stays open while looking closed. (2) Passing the SSL_CTX as a typed SSL_CTX*, which would put openssl/ssl.h into a public header the whole plugin and the test module include. (3) A decision function taking only the endpoint, as the story's first criterion reads. It cannot express the reconnect: ReconnectToBackendAddress targets a host the server named through the routing table, so the scheme's promise and the host it applies to arrive from different places. The function therefore takes both, and the rule it encodes is that the scheme comes from the configured endpoint, always, and the host from the connection being made. (4) Branching on Transport inside the decision. Both framings make the same promise per scheme, so the parameter is named and discarded; the difference that is real - the WebSocket client verifies regardless and cannot honour +ssc - is logged as a warning at connect time and is C-neo4j's to state (T-32). (5) Leaving ActiveHostname beside ActiveSecurity.Hostname. They are the same value on every live path, and two copies of the verified host is the bug class this story exists to close.

## J-0182 | 2026-11-15 | DEC-025 | core-dev
- Module: M-neo4j
- Story: US-031
- Task: T-27
- Decision: The verification is applied to the context before the BIO exists, so a refusal costs no socket; the handshake-failure branch, not a later check, is where the certificate's rejection is explained, because SSL_VERIFY_PEER aborts the handshake; SSL_get_verify_result is then inspected again after a successful handshake as an independent gate; and a verifying decision with an empty hostname is refused outright.
- Reason: With SSL_VERIFY_PEER a self-signed certificate never reaches a post-handshake check - BIO_do_handshake returns <= 0 - so a message written only after the handshake would never be the one a user sees. SSL_get_verify_result is set on the aborted handshake as well, which is what lets the refusal name the actual check (an untrusted issuer and a name mismatch have different remedies) rather than print an opaque ERR string. The post-handshake inspection is not redundant: it is the only thing that fails a context which completed a handshake without verifying, which is precisely the state F-020 shipped, so it tests the applying rather than trusting it. Applying to the SSL_CTX before BIO_new_ssl_connect is required, not stylistic: SSL_new inherits the verify mode and the X509_VERIFY_PARAM at creation.
- Changes: none

Every refusal sets ConnectionState to Failed and returns from TcpConnect before SendBoltHandshake, so HELLO and the LOGON that carries user and password are unreachable by construction and not by a check. Both messages name bolt+ssc:// and neo4j+ssc:// as the migration, and the certificate message also prints the trust store path that was used, because a user whose anchor is in the Windows store needs to know which file to add it to.\n\nThe empty-hostname guard exists because verification against no name is verification against anything: X509_VERIFY_PARAM_set1_host with an empty string clears the host list and returns success, so without the guard an endpoint that parsed to nothing would verify the chain and silently skip the name. It is also the one deterministic, offline refusal the suite has, which is how T-28 can show LOGON is unreachable without a server.\n\nOne limit on what T-28 can assert, found while choosing this shape and reported to the security engineer: the engine ships OpenSSL 1.1.1t, where X509_VERIFY_PARAM is opaque and X509_VERIFY_PARAM_get0_host does not exist (it arrives in 3.0). The hostname set on the context cannot be read back as a string. What is readable on a context the test owns is SSL_CTX_get_verify_mode == SSL_VERIFY_PEER, and X509_VERIFY_PARAM_get_hostflags == X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS, which is a non-default value nothing else in the process sets, so it witnesses that the component wrote the host parameters on that exact context. Partial wildcards are off as a hardening choice in its own right - RFC 6125 discourages them and no Neo4j deployment needs them - and not as a test hook.

## J-0183 | 2026-11-15 | NOTE | core-dev
- Task: T-27
- Story: US-031
- Decision: DEC-023, DEC-024, DEC-025
- Changes: FANTASIA/Source/FANTASIA/CLAUDE.md

M-neo4j's tree-level state document gains the invariant the three decisions above establish: an encrypted connection in this tree is an authenticated one, by four steps each of which is present and asserted. Recorded there because the next coder to open a TLS connection in this tree reads that file and not this entry, and because three of the four steps is the defect it closes.

## J-0184 | 2026-11-15 | F-051 | test-dev
- Event: fixed
- Finding: F-050
- Evidence: Withdrawn: first runs read a stale compiled header; after a rebuild the ApplyPreset ToolTip carries the whole comment (FANTASIA.Secrets.Tooltip.ApplyPreset green)
- Changes: none

Not a defect.

## J-0185 | 2026-11-15 | F-050 | test-dev
- Event: fixed
- Evidence: Withdrawn: stale compiled header in the first runs; ToolTip carries the whole comment after rebuild
- Changes: none

Not a defect.

## J-0186 | 2026-11-15 | F-051 | test-dev
- Event: fixed
- Evidence: Mistaken entry made while closing F-050; no defect
- Changes: none

Not a defect.

## J-0187 | 2026-11-15 | NOTE | lead
- Finding: F-050, F-051
- Status: void - neither is a defect and neither is a fix
- Changes: none

Both entries are void and must not be counted among S-02's fixed findings. Recorded here rather than left to be ignored, because 'ignore these' is not a thing a journal can carry and the next reader will meet the entries before any instruction about them.

F-050 (J-0179 opened, J-0185 fixed) was opened by test-dev in good faith: the ApplyPreset tooltip read back as only its @brief and @param lines, which looks exactly like a UHT truncation. It was a stale compiled copy. A later run, after the harness recompiled, showed the full comment, and test-dev found this itself, corrected its test and closed the entry. There was never a defect, so there was never a fix.

F-051 is a phantom: it has two 'fixed' events (J-0184, J-0186) and no 'opened' event at all, created by the first attempt to close F-050. ateam_check.py reports 0 errors against it, so the tooling tolerates a finding that was resolved without ever being raised.

The underlying cause is the F-entry vocabulary, and this is the third distinct consequence of it in two sprints. --event accepts only accepted, fixed and opened, so there is no way to annotate a finding (J-0128 went under NOTE), no way to re-point one (F-018 needed an 'opened' entry carrying Corrects= at J-0145), and no way to withdraw one that was opened in error - which leaves 'fixed' as the only exit and makes a mistaken finding indistinguishable in the record from a repaired defect. Worth raising at the retrospective as a change to ATeam rather than to this project: a 'withdrawn' event, and a check that a finding has an 'opened' entry before it can be closed.

## J-0188 | 2026-11-15 | NOTE | lead
- For: S-02 retrospective
- Reference: US-027
- Changes: none

The harness lock serialises gate runs. It does not serialise edits, and that is a dispatch problem rather than a gate defect.

T-31 reported several gate runs failing to compile Neo4jComponent.cpp with ActiveHostname and bActiveTls undeclared. That was T-27 mid-edit, not T-31's code, and it cleared by itself. Nothing was lost, but the runs were wasted and the diagnosis cost test-dev real turns: it had to work out that a compile error in a file it had never touched was not its problem. It also means T-31's own 170/0 is a result for the tree as it stood at that moment and not for the tree now, since T-27 has continued editing since.

This is my error, not the gate's. The lock answers 'two runs must not overlap', which was F-035's question. It cannot answer 'this run must not compile a tree someone is halfway through changing', because a run has no way to know another agent is editing.

The rule it produces, for the next sprint's dispatch: two tasks may run in parallel only when neither compiles the other's files. Different owners and non-overlapping paths are not sufficient - T-31 owned SecretConfigTests.cpp and T-27 owned Neo4jComponent.cpp, which do not overlap at all, and the gate compiles both because the test module links the runtime module. Parallel is safe for tasks that touch no code (documents, memory, the journal) and for one code task beside any number of non-code ones. Two code tasks in one module's compile unit go in sequence, whatever their ownership says.

Worth weighing at the retrospective against the obvious alternative, which I do not recommend: having the gate take a tree snapshot. That would make each run self-consistent and would also hide exactly this collision, so the wasted runs would become silently stale passes - the failure mode F-035 existed to remove.

## J-0189 | 2026-11-15 | F-052 | lead
- Event: opened
- Kind: quality
- Severity: major
- Story: none
- Module: none
- Repro: Two cross-ownership edits happened in S-02 and .ateam/ownership.jsonl is empty, so neither was recorded. (1) core-dev added six members to FANTASIA/Source/FANTASIATests/Private/Security/Neo4jTestAccess.h in T-27 - verified present at lines 78, 91, 97 - and team.json gives that directory to security. (2) test-dev rewrote FANTASIA/Source/FANTASIATests/CLAUDE.md in T-20 and again in T-31, and team.json gives that exact file to planner. ateam_check.py reports '0 would-be violation(s) logged since S-02 opened, 0 in all'.
- Changes: none

This matters because of what the empty log is being used for. The adoption named S-01 the observation window and the checker states the rule: 'enforcement needs a sprint that logs none'. The project is therefore on course to switch ownership out of observe mode on the evidence that nothing was logged - and that evidence is void, because at least two boundary crossings occurred and were not logged. An empty log means the hook did not see them, not that nobody crossed.

I caused both edits and neither agent is at fault: T-27's brief told core-dev to extend the existing seam header rather than duplicate it, and T-31's brief told test-dev that FANTASIATests/CLAUDE.md was its file. core-dev flagged its own crossing in its report and asked that it be confirmed with security rather than left implicit, which is the behaviour the method wants and the only reason this was noticed at all.

What I did not establish, and what someone should: why the hook recorded nothing. The candidates are that it does not fire for subagent edits, that its path matching misses these cases, or that observe mode records nothing rather than recording would-be violations - which would make the mode useless for its stated purpose. That is ATeam tooling rather than this project's code, so it is a change to ATeam, and the finding stays here because the decision it corrupts is this project's.

Until it is resolved, the honest position is that this project has no measurement of ownership violations, and the team change that would switch enforcement on has no evidence behind it.

## J-0190 | 2026-11-15 | F-053 | lead
- Event: opened
- Kind: quality
- Severity: minor
- Story: none
- Module: M-tests
- Repro: docs/ateam/team.json lists FANTASIA/Source/FANTASIATests/CLAUDE.md under planner's owns, while that file's own header reads 'Owner: test-dev. Inside it, Private/Quality/ and Private/Stress/ are the tester's and Private/Security/ is the security engineer's'. The two state documents name different owners for the same file.
- Changes: none

Found while checking F-052. It is the reason I briefed test-dev twice to edit a file team.json does not give it - I read the owner from the file's own header, which is where a coder would look.

Minor because nothing broke: ownership is in observe mode, both edits were correct in substance, and the planner has in practice let test-dev maintain that document. But it is a state inconsistency of exactly the kind the two-document rule exists to prevent, and it will matter the moment enforcement is switched on, because then one of the two documents starts refusing edits the other invites.

Either answer is defensible and the choice is the user's, since ownership is a team change: the planner owns every docs-shaped file including module state documents, or a module's CLAUDE.md belongs to the module's coder as its header says. What cannot stand is both.

## J-0191 | 2026-11-15 | F-054 | lead
- Event: opened
- Kind: quality
- Severity: minor
- Story: US-032
- Module: M-neo4j
- Repro: Neo4jComponent.cpp:824-827, OnWebSocketConnectionError's diagnostic, advises the user to 'try Use SSL=true' and hardcodes 'URL=ws://%s:%d' regardless of the transport actually attempted. US-032 removes the UseSSL property (CR-010, approved), after which this message names a property that does not exist.
- Changes: none

Noticed by core-dev during T-27 and left alone deliberately under P-07, which was right - it is not US-031's. Logging it so the removal does not leave the advice behind.

Two separate defects in one message. The advice will become false when CR-010 lands, and the right replacement is the scheme, since after US-032 the scheme is the only thing that decides encryption: a user whose server requires TLS changes bolt:// to bolt+s://, not a checkbox. The hardcoded ws:// is wrong already for a wss:// attempt, which is a smaller version of the same confusion.

Scheduling: this belongs with US-032 rather than on its own, because the correct new wording depends on what US-032 decides the user should do instead. Its Story field says US-032 for that reason.

## J-0192 | 2026-11-15 | F-055 | security
- Event: opened
- Story: US-031
- Module: M-neo4j
- Kind: security
- Severity: major
- Repro: grep -rn 'bDisableCertValidation|lws-insecure' FANTASIA/Source FANTASIAACE/Source returns nothing, against LwsWebSocketsManager.cpp:93,97 and LwsWebSocket.cpp:554-566,741,746-749 in the engine; and FANTASIA/Source/FANTASIA/Private/Neo4jComponent.cpp:310-329 maps eight schemes but not wss+insecure://, reachable from :1351-1368 with a routing record the server supplies. End to end: set [LwsWebSocket] bDisableCertValidation=true in DefaultEngine.ini, point a neo4j+s:// endpoint with Transport=WebSocket at a server behind a self-signed certificate, and the connection authenticates.
- Changes: none

On the WebSocket transport the component's security decision does not govern the
connection. `DecideTransportSecurity` returns the same answer for both framings, as it
should, but only `TcpConnect` applies it: the WebSocket branch builds a URL string and
hands it to the engine's WebSockets module, and from there two inputs the decision never
sees determine whether the server is authenticated.

**1. An engine-wide switch the plugin cannot observe.** `FLwsWebSocketsManager` reads
`[LwsWebSocket] bDisableCertValidation` from `GEngineIni`
(`LwsWebSocketsManager.cpp:93`) and also accepts `-lws-insecure=true` on the command line
(`:97`). When it is set, the certificate callback forces `PreverifyOk = 1` and skips the
chain check and `VerifySslCertificates` entirely (`LwsWebSocket.cpp:554-566`), and the
client flags become `LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED` for every `wss://`
connection in the process (`:741`). An integrator who sets that key for an unrelated
development endpoint, or ships a shortcut carrying that switch, silently removes server
authentication from the Neo4j connection as well. `neo4j+s://` over WebSocket then encrypts
without authenticating and `LOGON` carries `user` and `password` to whoever answers, which
is F-020's consequence by another route. The plugin reads neither the key nor the switch
(grep over `FANTASIA/Source` and `FANTASIAACE/Source` returns nothing), so it cannot warn,
and C-neo4j is about to state the guarantee unconditionally.

**2. A scheme the server can choose.** `InitiateConnection` builds the WebSocket URL from
its raw `Hostname` argument, not from `ActiveSecurity.Hostname`. When that argument contains
`://` it is mapped by eight `StartsWith` branches (`Neo4jComponent.cpp:310-322`) which do not
include `wss+insecure://` — a scheme the engine's client accepts and which means
`LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED` (`LwsWebSocket.cpp:746-749`). The following
upgrade (`:329`) tests `StartsWith("ws://")`, which that string does not match, so it passes
through verbatim. The one caller that can supply such a string is the server:
`ReconnectToBackendAddress` splits the routing record on its last colon and passes the rest
as the host (`Neo4jComponent.cpp:1351-1368`). So a server reached over a verified
`neo4j+s://` WebSocket connection can answer the routing query with
`wss+insecure://evil.example.com:7687` and have the client reconnect, unverified, to a host
of its choosing and authenticate there. The planner's note on US-032 that this branch is
dead because both call sites pass a bare hostname holds for a well-behaved server and not
for a hostile one. On the plain TCP transport the same record is harmless, because
`DecideTransportSecurity` runs `ExtractHostname` over it and the scheme is stripped before
`BIO_set_conn_hostname` sees it.

Why this is not F-020 and not covered by US-031's tests: F-020 was the applying of the
decision on the transport that applies it, and T-27 scoped the fix to that transport
deliberately (DEC-024's fifth alternative). This is the transport where there is nothing to
apply it to, which is a different claim and one C-neo4j is about to make wrongly.

**The fix, and what it costs.** Route (2) closes inside M-neo4j in a few lines and fails
closed: build the WebSocket URL from `ActiveSecurity.bEncrypt` and `ActiveSecurity.Hostname`
only, which deletes the eight-branch block US-032 already plans to remove, and gives that
deletion a security reason rather than a tidiness one. Route (1) cannot be closed inside the
plugin, only reported: either C-neo4j scopes the guarantee to the plain TCP transport and
says that on WebSocket the engine's stack owns verification and a project-level switch can
disable it, or the component reads the same key through `GConfig` at connect time and warns
once. The documentation half is the minimum and costs nothing; it belongs with T-32, which
already owes C-neo4j the statement that `+ssc` cannot be honoured over WebSocket. Both
halves are admissible on the 5.8 line under CR-004, and neither is the kind of fix that is
riskier than the risk it answers — which is why this is a finding and F-048 is not.

## J-0193 | 2026-11-15 | NOTE | security
- Task: T-28
- Story: US-031
- Module: M-neo4j, M-tests
- Decision: DEC-023, DEC-024, DEC-025
- Changes: threat-model.md

US-031's battery, eleven tests in
`FANTASIA/Source/FANTASIATests/Private/Security/Neo4jTlsVerificationTests.cpp`, and the
threat model rewritten around what it now proves. The gate reports 181 tests, 0 failures,
up from 170.

**Every assertion about verification is read off an `SSL_CTX` this file creates and the
component configures.** That is the point of the shape T-27 chose, and it is what separates
this battery from one that watches a connection fail: F-020 was a defect in the applying,
so the application is what is read. `Decision.*` calls `DecideTransportSecurity` with no
component in existence — ten schemes on both framings with both values of `UseSSL`, forty
decisions, all four fields of each — and `Context.*` hands the decision to
`ApplyTlsVerification` on a context the test owns and reads back
`SSL_CTX_get_verify_mode`, the host flags and the trust store's object count.

**The duals, which are the discipline this battery exists under.** Every "verification is
configured" assertion would also pass against code that verified *everything*, including the
`+ssc` case where the user opted out, so each side is pinned against its opposite:
`Context.FreshContextVerifiesNothing` establishes what an unconfigured context reports;
`Context.SelfSignedSchemeVerifiesNothing` requires `+ssc` to still report exactly that;
`Context.VerifiedNameIsTheOneDecided` pairs a certificate carrying the decided name with one
carrying another; and `Refusal.EmptyHostnameWithVerificationIsRefused` pairs the refusal with
the same nameless endpoint under `+ssc`, which is accepted. Without its dual each of the four
is satisfied by code that is wrong in the opposite direction.

**The hostname is read by exercising it, not by reading it.** DEC-025 is right that OpenSSL
1.1.1t has no `X509_VERIFY_PARAM_get0_host` — confirmed in the engine's own header, which
declares `set1_host` and `get_hostflags` and no getter for the host. The host flags are a
witness that the parameters were written on that context, and the battery verifies rather
than trusts that: the fresh-context test proves `X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS` is
not the default. But a witness is not the name. So the test builds a 2048-bit key and two
self-signed certificates in memory, one naming the decided host and one naming
`attacker.example.com`, copies the context's verification parameters onto an
`X509_STORE_CTX` with the certificate trusted and `X509_V_FLAG_PARTIAL_CHAIN` set so that
nothing but the name can fail, and verifies each. The matching name gives `X509_V_OK` and
the other `X509_V_ERR_HOSTNAME_MISMATCH` — and on an unconfigured context both give
`X509_V_OK`, which is what makes the pair evidence. That closes the gap DEC-025 recorded as
unclosable: the name cannot be printed, but it can be asked.

**What fails against the pre-fix code.** `Context.VerifyingSchemeConfiguresTheContext`, on
all three of its context assertions. At `HEAD` the TLS branch runs `SSL_CTX_new` straight
into `BIO_new_ssl_connect` with no verify mode, no trust store and no verification
parameter — the finding's own reproduction, `git grep` over the two source trees for
`SSL_CTX_set_verify|load_verify_locations|default_verify_paths|SSL_set1_host|X509_VERIFY_PARAM|SSL_get_verify_result`,
still returns nothing at `HEAD` and five call sites in the tree. So an `SSL_CTX` the pre-fix
component hands to the BIO is indistinguishable from one nobody touched, which is exactly
what `Context.FreshContextVerifiesNothing` asserts and passes on today: the difference
between those two tests is the whole content of the fix. Two others fail there as well —
`Context.VerifiedNameIsTheOneDecided`, because with no host set the attacker's certificate
raises no name objection, and `Refusal.RefusedVerificationNeverSendsLogon`, because the
refusal it expects does not exist and `HEAD` instead assigns `TcpBio` and goes to the
network with an empty host.

**What no offline test reaches, stated so it is not mistaken for covered.** The two
certificate refusals in `TcpConnect` — the aborted handshake naming
`SSL_get_verify_result`, and the independent post-handshake gate — need a server holding a
certificate. What is asserted is the structure they share: a refusal from
`ApplyTlsVerification` returns before a socket exists, leaving `ConnectionState` at `Failed`
with no transport, so `SendBoltHandshake` and the `LOGON` behind it are unreachable by
construction. The refusal's message is asserted as one contiguous substring of one log line,
so a message that drops the migration hint fails the test.

**The threat model.** M-neo4j's spoofing threat becomes mitigated with the four steps and the
evidence; the routing redirect becomes safe on the plain TCP transport, because the scheme
comes from the endpoint and the verified name from the host being connected to, in one field
that cannot drift; TB-4 and the Neo4j actor are corrected; the WebSocket gap is a threat of
its own (F-055); M-tests gains what F-020's battery adds to that module's own surface, which
is OpenSSL and an in-memory key pair; and the end-of-life OpenSSL ruling is rewritten, because
it leaned on F-020 being open and now has to stand on what it always actually rested on — the
version is Epic's, the bytes are the engine's, and the only fix a finding would license is a
second divergent OpenSSL in the process.

## J-0194 | 2026-11-15 | NOTE | lead
- Finding: F-047, F-055
- Corrects: J-0167
- Module: M-neo4j
- Changes: none

F-047's first half is wrong and I am the one who got it wrong. The WebSocket scheme-rewriting branch is NOT unreachable, and F-055 (J-0192) is the proof.

What J-0167 claimed, from the planner's CR-010 investigation and with my own confirmation attached: the branch is unreachable 'because of what its caller passes', since InitiateConnection is called only with a hostname. I checked both call sites and saw Hostname and BackendHost, and stopped there. What I did not ask was where BackendHost comes from. It comes from the server: ReconnectToBackendAddress takes a routing record, splits it on the LAST colon only to recover a port, and passes the remainder as the host - so 'wss+insecure://evil:7687' yields BackendHost = 'wss+insecure://evil' with the scheme intact. Verified in the source at :1351-1368. Checking an argument's spelling is not checking its provenance.

The chain is confirmed end to end, including in the engine. The WS branch takes the raw Hostname, matches none of its eight schemes, and 'wss+insecure://...' does not start with 'ws://' so the later upgrade leaves it alone - it reaches the engine verbatim, while ActiveSecurity is consulted only in the else. And UE_5.8's LwsWebSocket.cpp:747-749 accepts 'wss+insecure' and sets LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED; :759 names the three protocols it takes as ws, wss and wss+insecure. So a server reached over a verified neo4j+s:// WebSocket connection can redirect the client to an unverified connection anywhere and receive its LOGON.

Two consequences that change how the code should be treated.

The branch must not be removed as dead code. Deleting it does happen to fix F-055's second half - 'wss+insecure://evil' would fall to the else and become the unconnectable 'wss://wss+insecure://evil' - so F-047 and F-055 converge on the same deletion. But the reason is now security rather than tidiness, and the reason is what decides admission: under CR-004 a security finding at major is admissible on the 5.8 line whatever its surface, where F-047 as a minor quality finding was not. Anyone who deletes that block as cleanup will have closed a vulnerability without knowing it, and will not state it in the contract.

F-047's second half still stands: the ws-to-wss upgrade is unreachable for the independent reason that the else has already written wss:// when bEncrypt is true. That half is genuinely dead and genuinely only tidiness.

## J-0195 | 2026-11-15 | CR-011 | planner
- Event: proposed
- Type: contract
- Class: minor
- Impact: C-neo4j goes from Version 2 to Version 2.1 for US-030. Nothing breaks and no interface moves: the unbounded-reader clause is replaced by the four refusals, the message cap and its residual are stated, and two numbers are stated as a server author must read them. Consumers: M-demo and M-tests, so core-dev and test-dev re-read. Modules: none change. Stories: US-030 is the source and is not edited by this CR; US-032 is named in the contract as the case that still follows UseSSL. Findings: F-021 is the defect the clause described and F-048 its residual, both stated rather than closed here. Tests: BoltPackStreamBoundsTests.cpp joins the Tests line. Tasks in flight: none against M-neo4j.
- Options: (A) Rewrite the clause to the four refusals, state both limits in the form a server author reads, and state F-048 beside the cap; point release. (B) State the limits as the constants read (64 containers, 16 MiB), which is what the code and the log say and is wrong by one in the direction a server author would build to, and wrong by one transport read on the cap. (C) State the refusals and leave F-048 to its own story, which gives a consumer a mitigation with no residual and the belief that a refused message costs one message. (D) Do not change. Cost: the contract keeps telling consumers to treat the reader as trusting its input entirely, which is now false and costs them a guarantee they could rely on.
- Changes: none

C-neo4j describes an unbounded reader because that is what the code was. US-030 landed the
bounds checks, so the sentence the contract was told to rewrite when they landed is now
false in the safe direction: it warns a consumer about a danger that no longer exists and
tells them to treat the reader as trusting its input entirely, which is advice that costs
them a guard they could rely on.

Nothing breaks. A consumer coded against Version 2 wrote their own caution around a reader
that had none; that caution is now redundant rather than wrong, and every call they make
returns what it returned before for every message a well-behaved server sends — which is
what the 36 unchanged codec tests say, since each of them feeds the reader bytes the writer
produced and none is near a limit. So this is a point release: guarantees are added and none
is withdrawn.

Two numbers come with it, and both are easy to state wrongly in the direction a server
author would build to.

**The nesting limit is not 64 containers.** `MaxBoltNestingDepth = 64` is tested as
`Depth >= 64` against a zero-based depth, so the 64th container is entered and the value
inside it is refused. The constant and the refusal message both say 64 and agree with each
other, so this is wording and not a defect — but a contract that said "64 containers" would
be wrong by one, and wrong in the direction that matters. The number stated is therefore
what a message may contain: a value nested inside at most 63 containers. That form is exact
rather than merely cautious, because it also covers the case 64 would get wrong in the other
direction — an empty container at the limit is itself a value nested inside 63 and decodes.
Both sides are pinned, by `Bolt.Reader.NestingPastDepthLimit` and
`Bolt.Reader.NestingAtDepthLimitDecodes` (J-0176, DEC-020).

**The message cap is 16 MiB plus at most one transport delivery.** DEC-021 is explicit that
the counter is zeroed when a message completes, and the read that completed it may already
carry the first bytes of the next message, which the dechunker holds while the counter reads
zero. The round number is therefore not the guarantee. The slack is one receive buffer on
the plain TCP transport, 16384 bytes against a 16 MiB cap (`Neo4jComponent.cpp:632`), and on
WebSocket it is whatever the engine's client hands over in one binary-message callback,
which the plugin does not choose. An exact bound would need `FBoltDechunker` to report its
partial size, and F-012 makes that class unlinkable from a test, which is also why the cap
lives on the component's side of it.

F-048 goes in with the cap rather than being left for its own story to mention. The cap's
refusal resets the dechunker mid-stream, so the framing cannot be resynchronised and the
component reports itself connected while decoding nothing. A consumer who reads only the cap
will believe a refused message costs them one message; it costs them the connection, in a
way that looks like a hang. A contract that states a mitigation and not its residual is the
shape of document CR-006 was raised to correct.

## J-0196 | 2026-11-15 | CR-012 | planner
- Event: proposed
- Type: contract
- Class: major
- Impact: C-neo4j goes from Version 2.1 to Version 3 for US-031 — an integer, because a consumer breaks: a self-signed certificate on bolt+s:// or neo4j+s:// connected under Version 2 and is refused now. No signature moves and no Blueprint asset is invalidated; the break is behavioural. Consumers: M-demo and M-tests, so core-dev and test-dev re-read. Modules: none change. Stories: US-031 is the source and is not edited by this CR; US-032 is named as the case that still follows UseSSL. Findings: F-020 is the defect the new clauses answer and F-055 is why the guarantee is scoped to the plain TCP transport rather than stated flatly; F-011 keeps its clause because the property is still in the header. Tests: Neo4jTlsVerificationTests.cpp joins the Tests line. Tasks in flight: none against M-neo4j.
- Options: (A) State the guarantee scoped to the transport that makes it: plain TCP verifies and checks, WebSocket is stricter about +ssc and advisory about verification, with F-055's two routes named. (B) State one unqualified sentence that an encrypted connection is an authenticated one, which the plain TCP evidence supports and the WebSocket path does not, leaving a reader on the wrong transport believing a guarantee they do not have. (C) State the guarantee and mention F-055 only as an open finding elsewhere, which puts the exception in a document consumers do not read. (D) Do not change. Cost: +s keeps meaning what F-020 made it mean in the contract, the user's condition on J-0163 goes unmet, and the one-character migration exists only in a log line a user sees after their database stops working.
- Changes: none

A consumer breaks, so the version takes an integer. A user pointed at a server with a
self-signed certificate on `bolt+s://` or `neo4j+s://` connected under Version 2 and is
refused under Version 3. The user confirmed that trade at S-02 planning (J-0163) over the
alternative of warning on 5.8 and enforcing on 5.9, on one condition: that the migration —
one character, `bolt+ssc://` — appear in C-neo4j as well as in the failure message. It is in
the message, asserted as one contiguous substring of one log line by
`Neo4jTls.Refusal.RefusedVerificationNeverSendsLogon`; this CR is the other half of the
condition.

What the contract now states as the meaning of `+s`: `SSL_VERIFY_PEER`, a trust store whose
load return is tested, the endpoint's hostname on the X509 verification parameters, and
`SSL_get_verify_result` inspected after the handshake rather than a completed handshake read
as a verified one (DEC-023, DEC-024, DEC-025). Which of the four TLS schemes verify, and
that `+ssc` is the documented opt-out rather than an accident, is now stated per scheme:
today all four behave alike, and that is exactly what changes.

**The guarantee is not transport-uniform, and stating it flatly would be a lie.** US-031's
WebSocket criterion left two ways to satisfy it — the decision comes from the same function,
or the difference is stated in C-neo4j as a difference. The security engineer's verdict
(F-055, J-0192) is that the WebSocket branch takes its decision from the same function and
then does not apply it, so only the second is available. I verified the chain myself
(J-0194): `ReconnectToBackendAddress` splits a server-supplied routing record on its last
colon only, so a scheme survives into `InitiateConnection`; the WebSocket branch builds its
URL from the raw hostname, matches none of its eight schemes, and passes `wss+insecure://`
through verbatim, while `ActiveSecurity` is consulted only in the `else`; and UE 5.8's
`LwsWebSocket.cpp:747-749` accepts `wss+insecure` and sets
`LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED`. So a server reached over a verified
`neo4j+s://` WebSocket connection can redirect the client to an unverified connection
anywhere and receive its `LOGON`.

So the statement had to cover three things and not one. The plain TCP transport makes the
guarantee and is the only transport that honours `+ssc`. The WebSocket transport is stricter
in one respect — `+ssc` cannot be honoured at all, because the engine's client verifies the
chain itself with no way to relax it — and advisory in two others: two engine-level inputs
the plugin cannot observe can disable authentication process-wide
(`[LwsWebSocket] bDisableCertValidation` and `-lws-insecure=true`), and a routing redirect
can reach an unverified host. A reader choosing a transport has to be able to tell which
promise they get, so the contract scopes the guarantee to the transport that makes it and
says what the other one does instead.

What this CR deliberately does not do: remove `UseSSL`. US-032 is `ready` and not done, so
the property is still in the header and `BeginPlay` still overwrites it. A contract that
removed it now would describe a tree that does not exist, and the silent-downgrade sentence
goes when the property does. The security engineer made the same call for the threat model.

## J-0197 | 2026-11-15 | CR-011 | planner
- Event: approved
- Decision: Approved by the planner as a minor contract change, Version 2.1. Option (A). Nothing in the interface moves and no consumer that compiled against Version 2 stops compiling or stops behaving as it did: the four refusals and the cap are guarantees added, not withdrawn, which is what a point release is for. There is no decision here for the user to take — the alternative to rewriting the unbounded-reader clause is a contract that warns about a danger the code no longer has and advises a caution that now costs the consumer a guarantee. Options (B) and (C) were both refused for the same reason, that each states a number or a mitigation in the form that is easiest to read wrongly: 64 containers is wrong by one and the round 16 MiB is wrong by one transport read, and a cap with no residual invites a consumer to believe a refused message costs them one message rather than the connection.
- Changes: none

## J-0198 | 2026-11-15 | CR-011 | planner
- Event: implemented
- Evidence: C-neo4j Version 2.1. The unbounded-ReadValue clause is replaced by four refusals, each yielding a null value, leaving HasMore() false and logging once per message: a read at or past Data.Num() including ReadByte on an empty buffer and each multi-byte reader truncated mid-value; a declared length or count larger than the bytes remaining, refused before anything is reserved because every element costs at least one byte; a negative length or count; and a value nested inside more than 63 containers. A refusal propagates outwards so no partially decoded value is returned. The nesting limit is stated as what a message may contain and not as what the counter reaches: a value nested inside at most 63 containers decodes, the constant and the log both say 64 because the counter is zero-based, and 63 is the number to build to, pinned from both sides by Bolt.Reader.NestingPastDepthLimit and Bolt.Reader.NestingAtDepthLimitDecodes. The cap is stated as 16 MiB plus at most one transport delivery and not as the round number, with the slack named per transport: one 16384-byte receive buffer on plain TCP, and on WebSocket whatever the engine hands over in one binary-message callback. F-048 is stated beside the cap as the residual that it is. BoltPackStreamBoundsTests.cpp joins the Tests line and the Specification records that the cap lives on the component's side of FBoltDechunker because F-012 makes the class unlinkable from a test. Verified in the source: BoltPackStream.cpp:178, :184, :192, :250-294; Neo4jComponent.h:222; Neo4jComponent.cpp:632, :781-801, :844.
- Changes: contracts/C-neo4j.md

Every claim was verified in the source for this CR rather than taken from the task brief or
the findings, because a contract corrected from a second-hand description is where the next
wrong version comes from.

`BoltPackStream.cpp:178` declares the constant and `:294` tests `Depth >= MaxBoltNestingDepth`
against the `Depth` that `ReadValue()` seeds at 0, so containers are entered at depths 0
through 63 and the value inside the 64th is refused. The form the contract states — a value
nested inside at most 63 containers decodes, one nested deeper is refused — is exact in both
directions, which "63 nested containers" alone would not be: an *empty* container at the
limit is itself a value nested inside 63 and decodes, and the test pins exactly that shape
(`StatedNestingDepth - 1` lists with an int at the bottom, and no expected error declared at
all).

`Require(N)` at `:192` refuses `N < 0` and `N > Data.Num() - Cursor` in one comparison, which
is what makes the four refusals one gate rather than four; `Refuse()` at `:184` sets `Cursor`
to `Data.Num()` so `HasMore()` reports exhausted, and short-circuits so only the first
refusal logs. `ReadList`, `ReadMap` and `ReadStructure` call `Require` on the declared count
before `Reserve`, and `ReadStructure` requires `NumFields + 1` for the tag byte.

The cap is at `Neo4jComponent.h:222` and enforced at `Neo4jComponent.cpp:781`, before
`Accumulate`, in `int64` so a hostile `Size` cannot overflow the comparison; `:799` zeroes
the counter inside the `PopMessage` loop, which is the slack the contract states, and
`:632`'s `uint8 Buffer[16384]` is its size on the TCP path. The WebSocket path reaches the
same function through `OnWebSocketBinaryMessage` at `:844`, where the delivered size is the
engine's choice, so the slack is stated per transport rather than as one number.

## J-0199 | 2026-11-15 | CR-012 | planner
- Event: approved
- Decision: Approved on the user's decision at S-02 planning (J-0163), which is where the authority for this one sits: a consumer's working connection stops working, so it was never the planner's to take. Version 3, an integer, because from a consumer's point of view a connection that was made is now refused — the usual test for an integer is that a guarantee is withdrawn, and here it is the laxity that is withdrawn, which breaks them just the same. Option (A). The user's condition was that the one-character migration appear in the contract as well as in the failure message, and it does, in the Errors clause beside the refusal that states it. Option (B) is the one actually on offer and is refused: one unqualified sentence is what the plain TCP evidence supports and the WebSocket path does not, and F-055 is the proof that writing it would be a lie by omission rather than a simplification. Option (C) is refused because a consumer reads the contract and not the threat model, so an exception recorded only there is an exception nobody meets until it costs them a password.
- Changes: none

## J-0200 | 2026-11-15 | CR-012 | planner
- Event: implemented
- Evidence: C-neo4j Version 3. The scheme invariant now states the decision as data and per scheme: bolt+s, neo4j+s, https and wss encrypt and verify chain and hostname; bolt+ssc and neo4j+ssc encrypt and verify neither, as the documented opt-out; bolt, http and ws do not encrypt; neo4j and an unrecognised scheme follow UseSSL and are fully verified when encrypted, the case US-032 removes — pinned for all ten schemes on both framings and both values of UseSSL by Neo4jTls.Decision.SchemeMatrix. The verified name is the host being connected to, in the same field used for BIO_set_conn_hostname and SNI, so the host reached and the name checked cannot drift. A new Errors clause states that a TLS connection that cannot be verified is refused before any credential is sent, with the four refusals, what each message names, and the one-character migration to bolt+ssc:// or neo4j+ssc:// that the user made the condition of this change (J-0163). A new subsection states the guarantee scoped to the transport that makes it: on plain TCP the four steps of DEC-023, DEC-024 and DEC-025 with the trust store named and +ssc honoured; on WebSocket the decision is advisory in both directions, stricter because +ssc cannot be honoured at all and weaker because bDisableCertValidation, -lws-insecure=true and a scheme-bearing routing record can each remove authentication without the plugin being able to observe it (F-055). The routing invariant is scoped the same way. UseSSL keeps its clause and F-011 its sentence, because US-032 is ready and not done. Neo4jTlsVerificationTests.cpp joins the Tests line, and the seam is described as adding no exported symbol and no Blueprint-facing declaration. Verified in the source: Neo4jComponent.cpp:60, :126-180, :181-238, :283, :290-345, :530-600; LwsWebSocket.cpp:747-749 in the engine for the wss+insecure flags.
- Changes: contracts/C-neo4j.md

The whole transport-scoping statement was verified in the source for this CR, both halves of
it, because this is the clause a reader will act on when choosing a transport.

The plain TCP half, at `Neo4jComponent.cpp:181-238` and `:530-600`: `ApplyTlsVerification`
runs on the `SSL_CTX` before `BIO_new_ssl_connect`, refuses when the trust store is absent
or `SSL_CTX_load_verify_locations` does not return 1, refuses a verifying decision with an
empty hostname, sets `SSL_VERIFY_PEER`, and writes `X509_VERIFY_PARAM_set1_host` with
`X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS`. `ResolveTrustStorePath` at `:60` tries the project's
`Content/Certificates/cacert.pem` and then the engine's
`Content/Certificates/ThirdParty/cacert.pem`, in that order. `SSL_get_verify_result` is read
on the aborted handshake and again after a successful one, and each refusal sets
`ConnectionState` to `Failed` and returns before `SendBoltHandshake`, so the `LOGON` is
unreachable by construction. Both refusal messages name `bolt+ssc://` and `neo4j+ssc://`.

The WebSocket half, at `:290-345`: `DecideTransportSecurity` is called once for both framings
at `:283` and only `TcpConnect` is handed it. The branch builds `WsUrl` from the raw
`Hostname` argument; the eight `StartsWith` rewrites are `neo4j+s`, `neo4j+ssc`, `neo4j`,
`bolt+s`, `bolt+ssc`, `bolt`, `http` and `https`, so `wss+insecure://host` matches none, and
the following upgrade tests `StartsWith("ws://")`, which it does not satisfy either — it
reaches `CreateWebSocket` verbatim. `ActiveSecurity` is consulted only in the `else`, for a
bare hostname. The `+ssc` warning is at `:337`.

One wording choice worth recording. The WebSocket transport is described as *advisory in
both directions* rather than simply weaker, because it is genuinely stricter about `+ssc`:
the engine's client verifies the chain itself and offers no way to relax it, so a user who
asked for a self-signed certificate does not get one. A reader who takes "the decision is
not applied" to mean "nothing is checked" would conclude that `+ssc` works there, which is
the opposite of true, and would read a connection failure as the plugin's bug.

One observation, not a defect and not opened as a finding: the cap refusal's log prints
`UnresolvedMessageBytes`, the count *before* the rejected read, so the number it reports is
up to one transport delivery short of the bytes it is discarding. It is accurate as the size
of the partial message held and is within the slack the contract now states.

## J-0201 | 2026-11-15 | NOTE | planner
- For: CR-011, CR-012
- Contract: C-neo4j
- Changes: contracts/C-neo4j.md

Three sentences in the rewritten C-neo4j narrated the change instead of stating the state, and the checker caught one of them (P-18): the coverage invariant opened 'the connection is no longer untested', the ReadValue clause ended 'which is the reverse of Version 2', and the migration clause said a self-signed certificate 'connected before and does not now'. All three are rewritten in the present tense, and the version delta that a consumer actually needs is kept where it belongs - 'Version 3 refuses one connection that Version 2 makes', which is a statement about two versions rather than about the project's past. Worth recording because the pull is specific to a contract rewrite: the interesting thing while writing it is what moved, and the reader's question is what holds.

## J-0202 | 2026-11-15 | NOTE | lead
- Reference: CR-011, CR-012
- For: core-dev, test-dev
- Changes: none

C-neo4j's consumer notification is owed and undelivered. The planner tried to send it from T-32 and SendMessage refused - no coder was live - so M-demo's owner (core-dev) and M-tests' owner (test-dev) have not been told. Recorded here so it is not lost between spawns; the lead relays it at the next spawn of either, and at the sprint review.

What they need to know: C-neo4j is at Version 3 (CR-011 took it to 2.1, CR-012 to 3). What is new rather than reworded is the per-transport guarantee subsection at the end of Invariants, the four reader refusals and the message cap in Errors with F-048 stated beside the cap as its residual, and the TLS refusal clause with the bolt+ssc:// migration. UseSSL and F-011 keep their clauses, because US-032 is ready and not done. The reasoning and the verified source lines are in 'ateam_log.py show CR-011' and 'show CR-012'.

The one clause that changes what a consumer may assume: Version 3 refuses one connection Version 2 made - a self-signed certificate on bolt+s:// - and the migration is bolt+ssc://. Any demo or test fixture pointing at a self-signed server over bolt+s:// needs that one character.

## J-0203 | 2026-11-15 | F-056 | tester
- Event: opened
- Story: US-030
- Module: M-neo4j
- Kind: performance
- Severity: major
- Repro: FANTASIA/Source/FANTASIATests/Private/Stress/BoltStressTests.cpp, test FANTASIA.Stress.Bolt.Reader.DecodeAmplification (default run: 1 Mi elements; add -StressFullAmplification for the full 16 MiB message)
- Changes: none

The 16 MiB message cap bounds bytes on the wire, not what they decode into. A well-formed RECORD-shaped message holding a List32 of N one-byte integers is N+5 bytes and decodes into N heap-allocated FBoltValue objects: sizeof(FBoltValue) is 176 bytes, MakeShared makes it a 192-byte block, and the list holds a 16-byte shared pointer to it, so one wire byte costs about 208 bytes of heap. Measured at the cap (16777211 elements, 16777216 wire bytes, passes OnDataReceived and the reader's bounds because every count is satisfied by the bytes that follow): 3141 MiB resident (196 bytes per wire byte) and 1066 ms to decode; the 1 Mi element run measured 120 MiB and 46 ms, and the scaled figure underestimates the full one, so allocation cost is superlinear. OnDataReceived runs on the game thread (TcpRecvLoop hands every read to it by AsyncTask), so a server that sends one such message freezes the game for about a second and takes about 3 GiB of memory, repeatably, once per message. F-021 asked that a hostile or faulty server not be able to exhaust memory; the cap removes the unbounded growth and leaves this factor of about 200. It is not a regression (the old reader decoded the same message, with no cap), and no reader bound fixes it by itself: a bound on elements decoded per message, or a cheaper representation for a run of scalars, would. Not logged as Kind=security, deliberately: under CR-004 the label licenses the fix onto the 5.8 line, and that is the security engineer's ruling to make, not mine. The legitimate case it also touches: a collect() of a large result is one such list.

## J-0204 | 2026-11-15 | F-057 | tester
- Event: opened
- Story: US-030
- Module: M-neo4j
- Kind: bug
- Severity: minor
- Repro: FANTASIA/Source/FANTASIATests/Private/Quality/Neo4jReceivePathQualityTests.cpp, test FANTASIA.Quality.Neo4j.Cap.UnderCapMessageRefusedWhenReadStraddles
- Changes: none

The message cap is charged to bytes handed to the dechunker, and the check runs on the whole transport read, so a read that completes a message and also carries the first bytes of the next is charged to the first. A message of 16 MiB minus 5 bytes (under the cap) is delivered when the transport cuts the stream at multiples of 16384 from its start, and is refused (and then F-048's desynchronisation follows) when the very same bytes arrive with the first read one byte long. So the effective limit on one message is the cap minus up to one transport read, depending on where TCP happened to cut, 16 KiB below the 16 MiB the contract states. C-neo4j documents the guarantee as the cap plus at most one delivery, which is the permissive side only. Narrow: it takes a legitimate message within 16 KiB of 16 MiB. Minor, and worth saying only because the consequence of any refusal is F-048.

## J-0205 | 2026-11-15 | F-058 | tester
- Event: opened
- Story: US-031
- Module: M-neo4j
- Kind: bug
- Severity: minor
- Repro: FANTASIA/Source/FANTASIATests/Private/Quality/Neo4jTransportQualityTests.cpp, test FANTASIA.Quality.Neo4j.Hostname.EdgesOfTheName
- Changes: none

UNeo4jComponent::ExtractHostname cuts at the first colon, which for a bracketed IPv6 literal is inside the brackets: ExtractHostname('bolt+s://[::1]:7687') is '[', and '[2001:db8::1]' is '[2001'. That string is both the host the socket is asked to reach and, under US-031, the name the certificate is checked against (FNeo4jTlsDecision.Hostname), so an IPv6 literal endpoint cannot connect on any scheme. It fails closed: nothing named '[' answers and no certificate carries it, so no credential goes anywhere, which is why this is minor and not a security finding. Pre-existing and not introduced by US-031, but US-031 made the function's answer load-bearing for verification, and the security matrix pins only well-formed names. A userinfo prefix (bolt+s://someone@host) likewise stays in the host.

## J-0206 | 2026-11-15 | F-059 | tester
- Event: opened
- Story: US-034
- Module: M-llm
- Kind: quality
- Severity: minor
- Repro: FANTASIA/Source/FANTASIATests/Private/Quality/Neo4jTransportQualityTests.cpp, test FANTASIA.Quality.Tooltip.ConfigAndPreset
- Changes: none

US-034's first criterion is about the reflected metadata of UOpenAICompatibleComponent::Config: it must name Apply Provider Preset as a call that replaces the whole configuration including the key, and say the key must be set again after it. The sentence was written on the ApplyPreset function instead (OpenAICompatibleComponent.h:128-141) and the coders' test FANTASIA.Secrets.Tooltip.ApplyPreset reads it there. Config's own tooltip, the one a researcher reads in the Details panel when deciding where the key goes, still says only that the key belongs in DefaultGame.ini and names neither the node nor the erasure. The Blueprint node's hover text is a defensible place for the warning, arguably the better one, since it is where the act happens; but the criterion as written asks for Config, so either the criterion or the placement should move. A hypothesis about the reader's intent, not a measured usability result. Separately the coders' assertion is weak: it checks for the words Apply Provider Preset and APIKey, and APIKey would stay in the tooltip if the set-again obligation were removed; the tester's test additionally pins wholesale, again and Custom.

## J-0207 | 2026-11-15 | NOTE | lead
- Finding: F-048
- Corrects: J-0172, J-0175
- Reference: DEC-021
- Changes: none

The 'tester's stress plan' cited as a reason to leave F-048 open does not exist, and I propagated the error twice. Established by the tester at my request, and verified by me.

What is actually in the repository: docs/ateam/verification/core-dev.md:88, the Stress bullet of the PLANNER's verification profile FOR core-dev, added in the uncommitted S-02 planning edit. It reads '...confirming that the cap is what stops it and not an allocation failure, and that the connection survives the refusal'. There is no tester-authored stress plan anywhere; docs/ateam/verification/ holds coder profiles only and has no tester.md.

So the sentence exists but it is not the tester's, and it does not say what three entries claim. 'Survives' can only mean 'not torn down' - it predates F-048 (J-0172) and says nothing about the connection continuing to decode. DEC-021 says 'the tester's plan asserts the connection survives the refusal'; the security engineer's ruling says 'the tester's stress plan explicitly expects not to happen'; and J-0172 and J-0175 are mine, repeating it. core-dev's original phrasing was the loosest reading rather than an invention, and the security engineer and I then hardened it into a constraint nobody had written.

What this does and does not change. F-048 stays open and the ruling stands, on three reasons that survive intact: P-07 keeps a teardown out of a story that asked only for the partial message to be discarded; a teardown lands in the disconnect path beside a live receive thread where a half-done teardown is a use-after-free; and under CR-004 the Kind=security label would license exactly that fix. What is withdrawn is the fourth reason, that a plan expected the connection to keep working. No plan said so.

Two consequences. J-0175's half-fix - stop reporting ready after a refusal, log each later read - contradicts nothing in any plan, so the one objection that might have blocked it is gone. And the tester's new pin, Quality.Neo4j.Cap.RefusalLeavesStreamUnrecovered, measures what the sentence was being used to assume away: one 20 MiB message, then 0 of 3 following replies delivered against 3 of 3 on the control. It also corrected my wording - 'cannot resynchronise' overstates it, since realignment by luck is possible; with this payload it never happens.

## J-0208 | 2026-11-15 | NOTE | lead
- Finding: F-059
- Story: US-034
- For: the S-02 review
- Changes: none

F-059 is two defects, and the tester understated the second by calling the whole thing a hypothesis about reader intent. The first half is interpretation; the second is a flat conflict between a story's criterion and a project decision, and it is the review's to settle.

US-034's first criterion, read verbatim from backlog.md:514: 'Given UOpenAICompatibleComponent::Config, When its UHT-generated documentation metadata is read through reflection, Then that metadata names Apply Provider Preset as a call that replaces the whole configuration including the key, AND SAYS THE KEY MUST BE SET AGAIN AFTER IT'.

First half, a location mismatch and fixable: the sentence is on ApplyPreset's metadata, not on Config's, and Config's own comment is silent about the preset. Whether 'its metadata' permits the sibling function's is arguable; a reader in the Details panel looking at Config does not see it, which is the reading that matters.

Second half, and this is not arguable: the criterion requires the tooltip to advise setting the key again after the preset. T-29 deliberately refused to write that, and was right - setting the key again in a Blueprint graph makes it a literal in a committed asset, which defeats ADR-0001 exactly as a typed asset value does, and which is the trap F-032 exists to warn about. What it wrote instead is the honest alternative: skip the node and set the endpoint fields in the same DefaultGame.ini entry as the key.

So the criterion asks for the defect its own story was created to remove. This is the second time in two sprints that a criterion written before the work specified the wrong remedy - US-029's asked for an input that cannot exist (CR-009) - and the pattern is worth a retrospective line rather than being treated as two accidents.

US-034 cannot be accepted against this criterion as written. The amendment is a change request and the user's: move the requirement onto Config's own metadata, and replace the set-again clause with the ini route T-29 actually documented. I am not raising it before the reviewer has ruled, because whether the criterion is met is its call and I should not foreclose it.

## J-0209 | 2026-11-15 | F-056 | security
- Event: opened
- Corrects: J-0203
- Story: US-030
- Module: M-neo4j
- Kind: security
- Severity: major
- Repro: FANTASIA/Source/FANTASIATests/Private/Stress/BoltStressTests.cpp, test FANTASIA.Stress.Bolt.Reader.DecodeAmplification with -StressFullAmplification
- Changes: none

Ruled `Kind=security`, severity unchanged at major. The tester measured it, declined the
label, and deferred the call; this is the answer.

**Why security.** It is denial of service across TB-4, driven entirely by bytes from the
untrusted side of that boundary, and it is the surviving half of the threat F-021 was
opened for. Nothing in the code budgets what a message decodes into: the 3141 MiB is a
property of arithmetic on `sizeof(FBoltValue)` and not the outcome of any control, so
US-030's cap bounds the input and leaves the resource unbounded by anything checked. The
threat model's M-neo4j entry claimed exhaustion from a server message was mitigated; with
this measured it is not, and a finding is the only thing that keeps that sentence honest.

**Why the carve-out test passes this time, having failed twice.** I have twice declined
`Kind=security` because under CR-004 the label licenses the fix onto the shipped 5.8 line
whatever surface it touches, and the fix available was worse than the defect: a second
divergent OpenSSL in the process (J-0137), a connection teardown beside a live receive
thread (J-0173). The fix this label licenses is a running budget on values decoded per
message inside `FBoltPackStreamReader`, checked in the element loops of `ReadList`,
`ReadMap` and `ReadStructure` and refusing past it exactly as `Require` and
`MaxBoltNestingDepth` already do. It adds no exported symbol, no `UPROPERTY`, `UFUNCTION`,
delegate or enum, and no Blueprint-facing signature, so it clears even CR-001's narrower
bar; it is the same shape as F-021's own fix, which CR-004 admitted without controversy.

It also cannot cause F-048. By the time the reader runs, `Dechunker->PopMessage` has
already resolved the message boundary (`Neo4jComponent.cpp:795-801`), so a refusal inside
the reader costs one message and leaves the framing aligned. A refused read makes
`ParseBoltServerMessage` take its `Expected structure` branch and return a default
`FBoltServerMessage` (`BoltProtocol.cpp:334-338`), whose tag 0 matches no case in
`HandleServerMessage`'s `Streaming` state, so nothing is retained and the connection is
neither torn down nor desynchronised. The consequence is that one query never completes,
which is the gap J-0175's half-fix addresses and not a new one. I would be glad to see this
land, which is the test.

**Why major and not blocker.** My blocker criteria are a way across a trust boundary, an
exposed secret, or untrusted input reaching code or a privileged tool; this is none of
them. No memory is read out of bounds, no credential moves, nothing becomes code — F-021
was a blocker for its items 1 and 2, an out-of-bounds read handed to Blueprint and a stack
overflow, and this is arithmetic on legitimate allocations. The damage per message is
bounded and transient: about 3.1 GiB allocated and released and about 1 s of game thread,
survivable once on the 64 GB this project budgets for, and its leverage comes from
repetition, which is a flood no client-side bound stops. And US-030 strictly reduced the
exposure, from unbounded to a bounded factor of about 200; blocking the story that improved
it would hold F-020's and F-011's fixes behind it and leave the worse state in place
longer.

**So US-030 can be accepted, on one documentation condition.** Its acceptance criteria are
all about malformed messages and this message is well formed, so the criteria are met as
written. What over-claims is prose: the story's own narrative says a server 'cannot read my
process's memory or stop it', and C-neo4j and the threat model say exhaustion is mitigated.
Those three sentences have to be scoped to what the cap does before the story is accepted,
or acceptance records a guarantee the code does not provide. The threat model's half is
done here.

**One warning on the fix's shape.** The amplification is a property of `FBoltValue`: a
176-byte variant holding every alternative inline (`FString`, `TArray`, `TMap` and
`FBoltStructure` all resident at once), individually heap-allocated by `MakeShared` for
every scalar, so 192 bytes of block plus 16 of shared pointer per one-byte integer — which
is the tester's measured 196 bytes per wire byte. The structural fix is therefore a
narrower value representation, and `FBoltValue` carries `FANTASIA_API` and is named in
C-neo4j, so that fix is a contract version and is **not** admissible under CR-004 without a
change request. The budget is the admissible fix; nobody should reach for the
representation inside a security story.

**A note for whoever sizes the budget.** The one real cost of a budget is refusing a
legitimate result, and `BuildPullAllFramed` sends `PULL n=-1` (`BoltProtocol.cpp:297-301`),
so the client asks for every row and a large result is many bounded messages rather than one
big one (DEC-021). A budget per message therefore bites only a single RECORD holding one
enormous value, which is the hostile shape; the legitimate large-result case is F-060's
problem, not this one.

## J-0210 | 2026-11-15 | F-060 | security
- Event: opened
- Story: US-030
- Module: M-neo4j
- Kind: security
- Severity: major
- Repro: FANTASIA/Source/FANTASIA/Private/Neo4jComponent.cpp:1038 retains every RECORD's fields in CurrentRecords, emptied only at :1123 (query start) and :1429 (terminating SUCCESS); BoltProtocol.cpp:297-301 sends PULL n=-1. No test yet; the one owed is described in the body.
- Changes: none

Found while ruling on F-056, and it is the fact that decides F-056's severity, so it is
recorded apart from it: the amplification F-056 measures is bounded per message only
because something downstream is assumed to release it, and nothing does.

**The defect.** Decoded RECORD fields are retained across messages with no bound.
`HandleServerMessage` moves each RECORD's fields into `CurrentRecords`
(`Neo4jComponent.cpp:1038`), and `CurrentRecords` is emptied in exactly two places:
`DispatchCommand` at `:1123`, when a query starts, and
`ConvertBoltRecordsToResponse` at `:1429`, when the terminating SUCCESS arrives. Between
the two, every RECORD the server sends is accumulated and held. The client asks for all of
them — `BuildPullAllFramed` sends `PULL n=-1` (`BoltProtocol.cpp:297-301`) — so no
protocol field bounds the count, and a server that sends RECORDs and never the SUCCESS is
never made to stop. The 16 MiB cap does not reach this: `UnresolvedMessageBytes` is zeroed
every time a message completes (`Neo4jComponent.cpp:798`), so each RECORD is independently
under the cap and the number of them is unbounded.

**Why it matters.** This is the half of F-021's item 4 that US-030 did not close. That item
was 'a server that never terminates a message grows the buffer until the process dies';
US-030 capped the dechunker's accumulation, and the identical unbounded accumulation sits
one layer downstream, in the component, uncapped. The cost per wire byte is F-056's
amplification: about 208 bytes of heap per `FBoltValue`, so with F-056's shape about 300 MiB
of upload retains about 60 GiB and kills a 64 GB machine, and even without amplification a
flood of minimal RECORDs — about 11 wire bytes each, holding one 192-byte `FBoltValue`
block plus its 16-byte shared pointer — retains about 36 times what it costs to send. Both
end in process death rather than F-056's survivable spike. The two compose and the pair is
this project's strongest availability exposure: F-056's fix does not touch this one, because
the amplification is per value and a bound per message leaves a flood of small messages
untouched.

**Kind=security, and the carve-out test passes.** The fix the label licenses is a budget on
what one query may retain — a count of records, or an aggregate of values — in
`UNeo4jComponent`'s private state, with an action when it is hit. The terminal action
already exists in-path and needs no new behaviour: the FAILURE branches at `:1021-1029` and
`:1046-1057` set `ConnectionState = Failed` and send a framed RESET, on the game thread, with
no teardown and no interaction with the receive thread, and `IncomingResponse` then either
does not fire or fires with no rows, which is what C-neo4j already says of a FAILURE. So
unlike F-048 the fix is a reuse of a path that is already there, in the thread that already
owns that state. Private members only, no exported symbol, no Blueprint-facing signature:
admissible even under CR-001's narrower bar. I would be glad to see it land.

**Severity major, not blocker, and it does not bar US-030.** F-021's item 4 names
`FBoltDechunker::ProcessBuffer` specifically, and US-030's criteria ask for the dechunker's
accumulation to stop at a stated maximum; both are satisfied. This is a defect I found
beside the story, not a criterion the story fails. As for grade: it is unbounded and ends
in process death, which argues up, but the precondition is a server the user configured,
pointed at and holds credentials for, and the outcome is a crash of the user's own editor
or game with no disclosure and no persistence; on the plain TCP transport an attacker in
position to inject RECORDs is already inside F-011's blast radius, where the recorded
outcome is worse. Major, and the next M-neo4j story's work rather than this one's.

**Reproduction owed, and cheap.** No test exists. The one to write drives
`OnDataReceived` through `FNeo4jTestAccess` with the component in `Streaming` /
`WaitingPullRecords`, feeds N framed RECORD messages and no terminating SUCCESS, and reads
`CurrentRecords.Num()` back through the seam — the growth is the assertion, with the dual
that a terminating SUCCESS leaves it at zero. It needs no device, no network and no
database, and the existing receive-path quality tests already reach that state. Until it is
written this finding rests on the five line numbers above, which is the same standing
F-021 was opened on.

## J-0211 | 2026-11-15 | NOTE | security
- Finding: F-057, F-058
- Reference: F-056, F-060, F-048, J-0207
- Changes: threat-model.md

The threat model now says what the 16 MiB cap does and does not do, and F-057 and F-058 are
ruled not security — recorded because the lead asked rather than assumed, which is the whole
value of the question.

**F-057, not security.** Its error is strict and never permissive: charging the cap to the
whole transport read refuses up to one delivery early and cannot let more than the cap
through, so provoking it gains an attacker nothing a compliant message does not already get.
It fails closed. Its only security-adjacent consequence is that it widens F-048's trigger,
and that is assessed where it belongs, under F-048's flip conditions: F-057 does make a cap
refusal reachable by a message under the cap, which is the first condition's subject, but
the band is one Bolt message within 16 KiB of 16 MiB and it takes a result already sitting
at the edge of what the cap permits, so the desync is not reachable in ordinary operation and
the balance that decided the ruling does not move. A lowered cap, or a result shape that
routinely approaches it, would move it. One correction to the finding's wording: C-neo4j is
incomplete rather than wrong — it states the slack on the permissive side only, and the
strict side needs the same sentence. That is core-dev's text to add.

**F-058, not security**, and the reason is worth keeping because it is the general rule for
this class. A hostname parse bug becomes a security defect exactly when the connect target
and the verified name can diverge; here they cannot, and I checked rather than assumed.
`ExtractHostname` produces one value, `InitiateConnection` takes it as the host to reach and
passes that same argument into `DecideTransportSecurity` as the name to verify
(`Neo4jComponent.cpp:266`, `:285`), which is the stated mitigation for the routing redirect.
`[` resolves to nothing and no certificate carries it; a userinfo prefix left in the host
behaves the same way; an endpoint that parses to no host is already refused, with a dual
test on it. So the defect fails closed on both uses at once. The risk to watch is not this
defect but any later change that hands the socket a differently parsed host from the one
verified.

**What changed in the threat model.** M-neo4j's *Deserialises* bullet now says the three
bounds hold over wire bytes and that neither decoded size nor retention is bounded. The
single threat *Memory corruption and exhaustion from a server message* is split: memory
corruption stays mitigated with its 18 tests, and a new *Memory exhaustion from a server
message* carries F-056's measurement, F-060's retention, why they compose and why the
narrower `FBoltValue` is a contract version rather than a security fix. Open risks gain
F-056 and F-060 as the residue of F-021, and F-048's entry gains the tester's pin
`Quality.Neo4j.Cap.RefusalLeavesStreamUnrecovered`, the withdrawal of its fourth reason
(J-0207) stated as a withdrawal so nobody cites it again, and the F-057 assessment above.
The coverage list notes that F-056's test is the tester's rather than mine, that nothing is
owed under `Private/Security/` for it until there is a budget to pin, and that F-060's test
is the next case to write after F-022's and F-023's.

## J-0212 | 2026-11-15 | NOTE | reviewer
- Event: verdict
- Story: US-030
- Verdict: meets request
- Open findings: F-048, F-056, F-057, F-060 (all major or minor, none blocking); F-021's threat half open
- Evidence: gate report e093da91568a/index.json 198 tests 0 failures 11.149 s (read, not re-run); 18 tests FANTASIA.Security.Bolt.*; BoltPackStream.cpp:164-360; Neo4jComponent.cpp:772-802
- Changes: none

**Verdict: meets request.** Every one of US-030's six criteria is met by a test that exists,
and the two open majors ruled `Kind=security` today (F-056, F-060) do not bar it.

**Evidence.** The gate was not re-run, by the lead's instruction; the measurement behind this
verdict is the exported report of the 19:03:33 run, read directly:
`%LOCALAPPDATA%\FANTASIA-Gate\FantasiaGate\Saved\GateReport\e093da91568a\index.json` —
198 tests, 196 succeeded, 2 succeeded with warnings, 0 failed, 0 not run, 11.149 s. The two
warning-bearing tests are S-01's `Quality.Secrets.Leak.*`, which each wait 2.05 s on a refused
connect to 127.0.0.1:1 and are 4.2 s of the run's 11.1 s; no S-02 test warns. 18 tests under
`FANTASIA.Security.Bolt.*`, and `BoltPackStreamTests.cpp` is untouched in the working tree, so
the 36 codec tests pass unchanged rather than adjusted.

**The criteria, and the one substitution.** The four refusal paths are asserted at the reader
(`EmptyBuffer`, the four truncations, the five length/count cases, `NegativeThirtyTwoBitCount`,
`NestingPastDepthLimit`) and the cap at the component (`MessageCap.CapIsExact` accumulates
exactly 16 MiB and refuses one byte more, through `FNeo4jTestAccess::FeedTransportBytes`, with
no exported symbol added — DEC-021's placement holds and no CR was owed). Both sides of each
boundary are pinned, so none of it would pass against a reader that refused everything
(`ExactFitDecodes` against `OneByteShortIsRefused`, `NestingAtDepthLimitDecodes` against its
dual). One criterion is met by a stated substitute rather than directly: "`IncomingResponse`
either does not fire or fires with no rows" is unobservable from a module that declares no
`UCLASS`, and `CheckNoResponseAccumulated` asserts the same claim through four pieces of
component state that `HandleServerMessage` would have moved. The substitution is written in the
test file's header, which is the right place for it.

**The mechanism the story left to the diff, verified.** `Require(N)` precedes every read,
`Reserve` and `Append` on all four paths, refuses a negative N, and short-circuits on
`bRefused`, so the refusal logs once; `ReadValue(Depth)` funnels every refusal to a null value
so no partially decoded value is returned; `Depth` is a parameter, so no return path can forget
to undo it. `Stress.Bolt.Cap.RepeatedRefusals` measured six refusal cycles at a 16.0 MiB peak
each, returning to 0.0 MiB above baseline, with no growth across the six: the cap's memory
claim is measured, not asserted.

**The ruling the lead asked for: a narrative that promises more than its criteria deliver.** The
story is accepted against its criteria, which are the testable form of the request, and all of
them are about malformed messages; F-056 and F-060 are driven by well-formed ones, so neither is
a criterion this story fails. Refusing the story would send core-dev work its criteria never
asked for and that P-07 forbade it from taking — a budget on values decoded per message is a
different fix, with its own DEC and its own carve-out ruling, which the security engineer only
made today. But acceptance must not launder the "or stop it" half of the narrative, which is not
delivered: F-060 lets a server end the process. So the verdict stands and three things are owed
beside it, none of them code: the story's narrative clause amended or F-056 and F-060 carried on
a `Known defect:` line; C-neo4j naming F-056, F-057 and F-060 where it states the cap's
guarantee, as it already names F-011, F-012, F-048 and F-055; and F-021 closed with its residue
named in the closing entry rather than silently. A documentation fix is an acceptable answer to
an over-claiming narrative only when it is made everywhere the claim appears, and the threat
model — the one place already corrected — is the document fewest consumers read.

## J-0213 | 2026-11-15 | NOTE | reviewer
- Event: verdict
- Story: US-031
- Verdict: meets request
- Open findings: F-055 (major) and F-058 (minor) compatible; one new major reported to the lead: the unqualified guarantee in Neo4jComponent.h:118-122
- Evidence: gate report e093da91568a/index.json 198 tests 0 failures (read, not re-run); 11 tests FANTASIA.Security.Neo4jTls.*; Neo4jComponent.cpp:140-240, :487-500, :560-621; C-neo4j Version 3 per-transport subsection
- Changes: none

**Verdict: meets request.** All eight criteria met, including the WebSocket criterion, and one
major finding in the diff that is a documentation defect rather than a criterion failure.

**Evidence.** The 19:03:33 gate report read rather than re-run
(`%LOCALAPPDATA%\FANTASIA-Gate\FantasiaGate\Saved\GateReport\e093da91568a\index.json`, 198/0);
11 tests under `FANTASIA.Security.Neo4jTls.*`; and the source read at
`Neo4jComponent.cpp:140-179` (the decision), `:181-240` (the application), `:487-500` (applied
before `BIO_new_ssl_connect`), `:560-610` (both `SSL_get_verify_result` gates) and `:619-621`
(`SendBoltHandshake`, which every refusal returns before).

**The four applied steps, each present and each checked.** `SSL_CTX_set_verify(SSL_VERIFY_PEER)`;
`SSL_CTX_load_verify_locations` with its return tested against 1; the decided host written on
the context's `X509_VERIFY_PARAM` with partial wildcards off; and `SSL_get_verify_result`
inspected on the aborted handshake *and* again after a successful one. Three of the four would
have been F-020 with more code in it. The context assertions are read off a context the test
owns, and `Context.FreshContextVerifiesNothing` establishes that each of them is a witness of
something the component did rather than of OpenSSL's defaults — that dual is what makes the
battery worth trusting. `LOGON` is unreachable by construction, not by a check: every refusal
sets `Failed` and returns before `SendBoltHandshake` and before the receive thread starts.

**T-27's fourth argument: behaviour, not signature.** The first criterion's *Given* enumerates
the situation (an endpoint, a transport, `UseSSL`); its *Then* constrains the member — private,
static, reads only its arguments, returns the four fields as data — and says nothing about
arity. The fifth criterion then requires the verified name to be the routing-supplied backend's,
which three arguments cannot express without overloading the endpoint and losing `+ssc` on the
reconnect. Read together the criteria require the fourth argument; `DecideTransportSecurity` is
right and DEC-024's third rejected alternative is the correct reading of its own story.

**The WebSocket criterion is discharged, by both limbs rather than one.** The decision is taken
from the same function before the branch (`:287`) and the WS branch reads `ActiveSecurity`; and
C-neo4j's `### What an encrypted connection guarantees, by transport` states the difference in
both directions — stricter (`+ssc` unhonourable, which the code also warns about at connect
time) and weaker (the two engine-level inputs, and F-055's routing redirect through
`wss+insecure://`). It covers F-055 in the detail the criterion's purpose clause requires, and
goes further than the criterion asked by naming the transport a caller should choose. The one
criticism is where the statement is not: see the finding below.

**The finding, major, in this diff.** `Neo4jComponent.h:118-122` adds to `UNeo4jComponent`'s
class doc comment — therefore to the class's reflected ToolTip — the unqualified sentence "An
encrypted connection is also an authenticated one: `bolt+s://` and `neo4j+s://` require the
server's certificate to chain to a trust anchor and to carry the host's name, and the connection
is refused — before any credential is sent — when it does not", followed by "`bolt+ssc://` and
`neo4j+ssc://` are the documented way to accept a self-signed certificate". Both sentences are
false on the WebSocket transport, which the same class exposes as a `Transport` property: F-055
and the engine's `bDisableCertValidation` defeat the first, and the component's own warning at
`:336-343` contradicts the second. This is the exact unqualified sentence CR-012 (J-0199) refused
to put in the contract, as "a lie by omission", and the header is the surface more readers meet.
Reported to the lead; the fix is to scope both sentences by transport as the contract does, and
to mirror that in `FANTASIA/Source/FANTASIA/CLAUDE.md`'s new invariant, which the next agent
loads. It should be fixed before F-020 is closed: closing F-020 beside that sentence is how
F-055 gets forgotten.

## J-0214 | 2026-11-15 | NOTE | reviewer
- Event: verdict
- Story: US-034
- Verdict: does not meet request
- Open findings: F-032, F-033, F-049, F-059; one new minor reported to the lead: a test label in Neo4jTransportQualityTests.cpp:113 asserting the opposite of what the tooltip says
- Evidence: OpenAICompatibleComponent.h:84-96 against :128-141; RESTTTSComponent.h:319-326; SecretConfigTests.cpp:149-188; gate report e093da91568a 198 tests 0 failures (read, not re-run)
- Changes: none

**Verdict: does not meet request.** The second, third and fourth criteria are met. The first
is not, and the ruling the lead asked for is that it is two different things, with two
different remedies — one of which is a coder's and is owed now.

**Evidence.** `OpenAICompatibleComponent.h:84-96` (`Config`'s doc comment, which names
`DefaultGame.ini` and the asset trap and says nothing about any preset) against `:128-141`
(`ApplyPreset`'s, which carries the whole sentence); `RESTTTSComponent.h:319-326`;
`SecretConfigTests.cpp:149-188`; the 19:03:33 gate report, 198/0, read rather than re-run.

**Not unsatisfiable: unmet, then wrong.** The first clause — that `Config`'s metadata name
`Apply Provider Preset` as a call that replaces the whole configuration including the key — is
plainly satisfiable, by one sentence on `Config`'s own comment, and it was not done. No test
asserts anything on `Config`'s ToolTip; the coders' test reads the function's, and the tester's
`Quality.Tooltip.ConfigAndPreset` pins `Config`'s silence as current behaviour. I do not accept
"its metadata" as reaching a sibling function's: `FProperty::GetMetaData` and
`UFunction::GetMetaData` are different fields of different objects, which the two tests
demonstrate by having to look in two places, and the story's own narrative settles it — "I want
the editor to tell me what erases that key" is a sentence about where a user is looking when
they decide where the key goes, which is the Details panel on `Config`. So this half is a
defect, not an interpretation, and the remedy is one sentence of core-dev's and one literal in
the test.

The second clause — that the metadata say the key must be set again after the preset — is also
satisfiable, which is why I do not call the criterion unsatisfiable; it is *wrong*. Satisfying
it would print, in the editor, the advice that puts a key into a committed Blueprint graph:
ADR-0001's defect by a second route, and the trap F-032 exists to warn about. T-29's refusal was
correct and what it wrote instead is the honest alternative. The distinction from US-029's case
matters for the remedy: an impossible criterion has to be deleted before the story can ever pass
(CR-009), while a wrong one has to be corrected because meeting it would damage the product, and
the coder who refused it should be recorded as right rather than as non-compliant.

So: the criterion is amended by change request, which is the user's — move the first clause onto
`Config`'s own metadata and replace the set-again clause with the ini route T-29 documented —
and even after that amendment the story is not met until the sentence lands on `Config`. The
amendment is not what closes this; a sentence is.

**One finding of my own, reported to the lead.** `Neo4jTransportQualityTests.cpp:113` asserts
`OnPreset.Contains(TEXT("again"))` under the label "and that the key is set again afterwards".
The tooltip's actual sentence is "Setting the key again after the preset makes it a literal in a
Blueprint graph, which is committed" — the opposite advice. The substring cannot tell one from
the other, and the label makes the gate report state, in green, that the clause T-29 deliberately
refused is satisfied. That is a test laundering an unmet criterion; the literal should be the ini
route the tooltip actually recommends. Minor, M-tests, the tester's file. F-049 and F-059 remain
open and are untouched by this verdict.

## J-0215 | 2026-11-15 | F-061 | reviewer
- Event: opened
- Kind: security
- Severity: major
- Story: US-031
- Module: M-neo4j
- Repro: Neo4jComponent.h:118-122, in the class doc comment and so in its reflected tooltip, states 'An encrypted connection is also an authenticated one' and that '+ssc' is the way to accept a self-signed certificate. Both are false on the WebSocket transport, which the same class exposes as a property: F-055 and the engine-wide bDisableCertValidation defeat the first, and the component's own warning at :336-343 contradicts the second.
- Changes: none

The sentence CR-012 refused to put in C-neo4j as a lie by omission, written instead into the surface more integrators read. core-dev is scoping both sentences by transport, and the same invariant in FANTASIA/Source/FANTASIA/CLAUDE.md with them. Blocks the 5.8 release: a false promise about server authentication is the defect class F-020 was.

## J-0216 | 2026-11-15 | F-062 | reviewer
- Event: opened
- Kind: quality
- Severity: minor
- Story: US-032
- Module: M-neo4j
- Repro: backlog.md:479 (US-032's acceptance criterion) and verification/core-dev.md:81 both state that both of InitiateConnection's call sites pass a bare hostname and the branch is therefore unreachable. F-055 disproved that: ReconnectToBackendAddress passes a server-supplied routing record with its scheme intact. J-0194 corrects it in the journal only.
- Changes: none

US-032 is ready and heads S-03, so an S-03 agent reads a false premise inside the criterion it implements - and the correction's conclusion, that the branch must not be deleted as tidiness because deleting it closes a vulnerability, is exactly what the stale text hides.

## J-0217 | 2026-11-15 | F-063 | reviewer
- Event: opened
- Kind: quality
- Severity: minor
- Story: US-030
- Module: M-neo4j
- Repro: C-neo4j Version 3 names F-011, F-012, F-048 and F-055 but omits F-056, F-057 and F-060 where its Invariants state the cap's guarantee, so the omission reads as a statement that nothing else is open. Its sentence that a large result is many bounded messages and never one big one is the reassurance F-060 falsifies.
- Changes: none

## J-0218 | 2026-11-15 | F-064 | reviewer
- Event: opened
- Kind: quality
- Severity: minor
- Story: US-034
- Module: M-tests
- Repro: Private/Quality/Neo4jTransportQualityTests.cpp:113 asserts the tooltip Contains('again') under the label 'and that the key is set again afterwards', against a sentence that reads 'Setting the key again after the preset makes it a literal in a Blueprint graph, which is committed'. The substring cannot tell the advice from its refusal, so the gate reports in green that the clause T-29 deliberately refused is satisfied.
- Changes: none

## J-0219 | 2026-11-15 | F-065 | reviewer
- Event: opened
- Kind: performance
- Severity: minor
- Story: US-030
- Module: M-neo4j
- Repro: ReadMap calls Require(Count), so a Map32 declaring Count = bytes-remaining passes and Map.Reserve(Count) allocates about 56 bytes per wire byte before the element loop refuses. DEC-020 rejected 2*Count for int32 overflow, correctly; the division form has none and is the valid necessary condition: Count > (Data.Num() - Cursor) / 2.
- Changes: none

Halves the worst-case map reserve for one changed expression. Rides with F-056's budget fix, not before it (P-07).

## J-0220 | 2026-11-15 | F-066 | reviewer
- Event: opened
- Kind: performance
- Severity: minor
- Story: US-031
- Module: M-neo4j
- Repro: BeginPlay to InitiateConnection to TcpConnect runs ApplyTlsVerification (trust-store parse, measured median 6.72 ms, p95 11.29 ms, max 13.70 ms) then the blocking BIO_do_connect and BIO_do_handshake, all on the game thread. ReconnectToBackendAddress is reached from HandleServerMessage, which the recv loop dispatches to the game thread, so a routing reconnect blocks it again mid-session.
- Changes: none

FANTASIA/Source/FANTASIA/CLAUDE.md states that every component doing I/O dispatches off the game thread; C-neo4j says only that queries do. Either the code or the invariant is wrong and nothing records which. Pre-existing for the connect, new and now measured for the 6.72 ms, which an Aura session pays twice. Not US-031's to fix.

## J-0221 | 2026-11-15 | DEC-026 | core-dev
- Module: M-neo4j
- Decision: The Blueprint tooltip and the module state document state the encryption guarantee per transport, mirroring C-neo4j's 'What an encrypted connection guarantees, by transport', instead of making one unqualified claim.
- Reason: UNeo4jComponent exposes Transport as a property, so the class tooltip is read by users of both framings. The unqualified sentence T-27 added was true only on PlainTCP: on WebSocket the engine owns verification, '+ssc' cannot be honoured at all (warned at Neo4jComponent.cpp:333-343), and '+s' is defeated by [LwsWebSocket] bDisableCertValidation, -lws-insecure=true, or a routing record carrying wss+insecure:// (F-055). A wrong promise about server authentication is the defect class F-020 was, and CR-012 had already refused the unqualified form in C-neo4j hours earlier.
- Changes: FANTASIA/Source/FANTASIA/CLAUDE.md

Alternatives rejected: (a) delete the two sentences — leaves the PlainTCP guarantee, which is real and is what a reader needs in order to choose the transport, undocumented in the one place they see it; (b) keep the claim and add a 'see C-neo4j' pointer — a tooltip is read where it stands, so a pointer does not retract a promise; (c) state the qualification only in the contract and leave the header unqualified — the header is the surface more people read, which is exactly how this defect reached users. Also rewrote Source/FANTASIA/CLAUDE.md's invariant to the per-transport form and said why, since that file is what the next agent loads and it stated the unqualified form too. No code, no specifier and no contract text changed; the gate was deliberately not run (build-dev mid-ship-task).

## J-0222 | 2026-11-15 | DEC-027 | build-dev
- Module: M-dist
- Decision: The NVIDIA ACE/Mark/James/Claire fetches become opt-in (--ace/--mark/--james/--claire/--characters/--nvidia) and sit behind a single NVIDIA_AVAILABLE=0 gate in bootstrap.bat; bootstrap.sh gets the same flags, and a GPU_ASSET_AVAILABLE=0 gate for the Linux CUDA whisper stack. Asking for a gated artifact prints why and exits 1 before anything is downloaded. The --no-* forms stay accepted as no-ops.
- Reason: F-014: the opt-out default asked runtime-v3.0 for four nv-*-ue58 assets that do not exist, and curl --fail || exit /b 1 aborted a fresh clone's install. The same was true of bootstrap.sh's fantasia-gpu-cuda13-linux.tar.gz, which no release has ever carried (both confirmed 404 against the live release). The UE 5.8 line ships core-only, so core-only is now the default and the three core assets all resolve 200.
- Changes: none

Alternatives rejected: (1) delete the NVIDIA fetch machinery, which would have to be rewritten when the 5.8 bundles are built - instead one constant per unpublished artifact keeps the wiring live and self-documenting, and flipping it is the whole change needed at publish time. (2) Let the default run attempt the fetch and merely warn on failure: a 404 mid-run leaves a half-installed tree and teaches users to ignore errors. (3) Fail late, after the core artifacts land: the request is explicit, so refusing up front costs the user nothing and leaves no partial state. Side effect fixed in passing: --enable-plugins used to live inside the NVIDIA placement branch, so a core-only run would have silently ignored it; the Plugins/ walk and the enable step now run on every path.

## J-0223 | 2026-11-15 | DEC-028 | build-dev
- Module: M-dist
- Decision: FANTASIAACE is marked not-available-for-5.8 in three places rather than deleted or left silent: its .uplugin declares EngineVersion 5.7.0 (truthful - its prebuilt binaries carry BuildId 47537391), its FriendlyName reads 'FANTASIA ACE (UE 5.7 only - NOT available for UE 5.8)' and its Description opens with the refusal and the reason; README marks the Animation row and the Audio2Face section, and Troubleshooting now answers the 'built with a different engine version' dialog by name. The 5.7 binaries are untouched.
- Reason: F-016: with EngineVersion 5.8.0 the plugin advertised itself as loadable on 5.8 and then failed the module BuildId check (47537391 vs 55116800) with an opaque error. Declaring 5.7.0 makes the engine's own plugin manager state the mismatch up front, and the FriendlyName carries it where a user actually looks - the Plugins browser. The 5.7 line still ships Azure and the 5.6/5.7 branches use these binaries, so deleting them was not an option.
- Changes: none

Alternatives rejected: (1) remove the FANTASIAACE module from the .uplugin, which would make the plugin load and silently do nothing - worse than a refusal. (2) README only: a user who copies both folders never reads it before the editor fails. (3) Delete the 5.7 binaries: forbidden, they are the 5.7 line's shipped build. This marks the defect, it does not fix it - F-016 stays open until the NVIDIA ACE port to 5.8 exists (see F-014's sibling gate in bootstrap). A core-only install is unaffected either way: FANTASIA.uplugin depends only on AudioCapture and never on FANTASIAACE.

## J-0224 | 2026-11-15 | DEC-029 | build-dev
- Module: M-dist
- Decision: The P-12 refresh does not delete the local CUDA whisper build output under FANTASIA/ThirdParty/whisper_cpp/build/. The shared-whisper footgun is verified three ways instead: the two files FANTASIA.Build.cs gates on (build/src/Release/whisper.lib and build/ggml/src/Release/ggml-cuda.lib) are absent, FANTASIA_WHISPER_PREBUILT_CUDA is cleared in the build environment, and both builds log '[FANTASIA-Whisper] Using STAGED SOURCE (CPU only, 24 files)'.
- Reason: Build.cs has carried an env-var gate since the 2026-07 regression, so lib presence alone can no longer flip the shipped DLL onto the shared-whisper path; and the stray files that do exist (whisper.dll, ggml*.dll, ggml{,-base,-cpu}.lib) are not the ones the gate keys on. Deleting them would destroy a long CUDA compile the user may still want, to remove a risk the guard already removes. The build log line is a stronger check than file absence because it reports the branch actually taken.
- Changes: none

Verification of the refresh: clean rebuild of FantasiaGateEditor Win64 Development and DebugGame (plugin Intermediate/Build, harness Intermediate/Build and harness Binaries deleted first, so neither run could report 'Target is up to date'), -MaxParallelActions=6 against this machine's 8 GB pagefile, both exit 0. dumpbin /dependents on all four module DLLs shows libswipl.dll as the only non-system, non-engine import; no whisper.dll, ggml*.dll, cudart/cublas or ACE import, and no delay-load section at all. Both .modules carry BuildId 55116800 (UE 5.8). no-ACE comes from the gate harness .uproject enabling FANTASIA only.

## J-0225 | 2026-11-15 | F-067 | build-dev
- Event: opened
- Kind: quality
- Severity: minor
- Story: US-020
- Module: M-ace
- Repro: NV_ACE_Reference compiles on 5.8 with one line changed, but only because warnings-as-errors is off: the previous porter's '#if (UE_5_5_OR_LATER && !UE_5_7_OR_LATER)' makes IsSupportedTarget() false on 5.7+, which leaves bWarningsAsErrors unset. Ten deprecated call sites each warn that the project will no longer compile in the next release - nine in Source/ACERuntime/Private/SoundWaveConversion.cpp (lines 90, 92, 95, 104, 105, 110, 119, 210, 211; the FSoundWaveProxy to FSoundWaveData migration) and one in Source/OmniverseLiveLink/Private/OmniverseLiveLinkCommands.cpp:30 (FInputGesture to FInputChord).
- Changes: none

This is the 5.9 ACE port, pre-identified, and that is the value of filing it: next time the base will be known rather than discovered.

Two planning consequences. The nine SoundWave sites sit in two functions in one file, so they are a contained single-sitting job rather than a sprawl. And anyone who 'tidies' the !UE_5_7_OR_LATER guard to re-enable warnings-as-errors on 5.8 turns a one-line port into these ten migrations - the guard looks like dead version cruft and is not.

ACE's own code produced no other warnings; the C4458/C4668/C4701/C4005 noise in the build log belongs to the core plugin's svm.cpp and the bundled ggml.

## J-0226 | 2026-11-15 | DEC-030 | build-dev
- Module: M-dist
- Decision: The NVIDIA ACE stack is built and marked for UE 5.8: the three Audio2Face character plugins compile unchanged against 5.8, EngineVersion is 5.8.0 in all four staged NVIDIA .uplugin files, and FANTASIAACE's prebuilt Win64 Development and DebugGame DLLs are rebuilt at BuildId 55116800.
- Reason: UBT ignores a plugin's EngineVersion while the plugin sits under a project's Plugins/, so the 5.7.0 marking was silent there; installing the same plugins into Engine/Plugins/Marketplace/ does enforce it, and a 5.8 deployment done that way would have been refused. FANTASIAACE's 47537391 BuildId against the engine's 55116800 is what stopped the companion plugin loading on 5.8.
- Changes: none

Built from the FANTASIADevelopment host project against UE 5.8 with -MaxParallelActions=6; both configurations report Result: Succeeded. The characters needed no source change: their own IsSupportedTarget guard is #if (UE_5_5_OR_LATER && !UE_5_7_OR_LATER), so on 5.8 it only turns bWarningsAsErrors off, and they emitted no warnings anyway. dumpbin /dependents on UnrealEditor-FANTASIAACE.dll shows UnrealEditor-{ACECore,ACERuntime,Core,CoreUObject,Engine,FANTASIA}.dll and the CRT only. The core FANTASIA binaries the host build touched (the DebugGame pair) were restored byte-for-byte from a pre-build snapshot, so the verified no-ACE/no-CUDA artifacts are unchanged. bootstrap.bat's NVIDIA_AVAILABLE stays 0 and FANTASIAACE.uplugin keeps its 5.7-only marking until the ue58 bundles are published.

## J-0227 | 2026-11-15 | F-016 | lead
- Event: fixed
- Evidence: FANTASIAACE/Binaries/Win64 now carries Development and DebugGame at BuildId 55116800, verified by reading both .modules files directly (was 47537391). Built in the 5.8 host project once NV_ACE_Reference existed for 5.8, which is why it could not be fixed before. dumpbin /dependents shows UnrealEditor-ACECore.dll, UnrealEditor-ACERuntime.dll, UnrealEditor-FANTASIA.dll plus engine and CRT - the two ACE modules and core FANTASIA, as the companion legitimately must, and no libswipl or whisper edge. DEC-030 (J-0226).
- Changes: none

## J-0228 | 2026-11-15 | F-068 | build-dev
- Event: opened
- Kind: quality
- Severity: major
- Story: US-020
- Module: M-dist
- Repro: 1.67 GB of each character plugin's 1.69 GB is byte-identical across Mark, James and Claire, verified by sha256 rather than by size: a2e.trt/{E5E4043F-...}/a2e_network.trt is 1,284,446,484 bytes with sha256 47e9697b... in all three and even under the same GUID, and a2f.trt/{per-char GUID}/data/network.trt is 401,830,164 bytes with sha256 cf1b2947... in all three. Only bs_skin_<Name>.npz, bs_tongue_<Name>.npz and model_data_<Name>.npz differ - about 17.4 MB per character.
- Changes: none

Three bundles as structured for 5.7 means roughly 4.5 GB of release assets carrying about 3.3 GB of pure duplication, and a 5.8 release of about 5.2 GB. Two further pressures: GitHub's per-asset limit is 2 GB and the 1.5 GB character bundles are already near it, and a user who wants one character downloads 1.5 GB where 1.6 GB once plus 20 MB would serve any number of them.

The alternative needs no NVIDIA source change, which is what makes it worth considering: each character module resolves its models from its own plugin directory via IPluginManager::FindPlugin(UE_PLUGIN_NAME)->GetBaseDir(), so one shared asset (the a2e.trt directory plus the single a2f network.trt) plus three roughly 20 MB per-character assets works if bootstrap places the shared payload into each plugin directory. On NTFS hard links make the three placements free; a plain copy costs the same disk as today. Against it: more moving parts in bootstrap, and it diverges from how NVIDIA ships.

Only bites if the full bundles are published; an overlay-only release makes it moot.

## J-0229 | 2026-11-15 | F-069 | build-dev
- Event: opened
- Kind: bug
- Severity: major
- Story: US-020
- Module: M-dist
- Repro: bootstrap.bat:157 sets NV_DIR to %SCRIPT_DIR%NVIDIA-UE57-Bundle on the 5.8 line, while the assets it names are nv-*-ue58*.tar.gz (the header at lines 17-20 also says UE57). The already-present skip tests 'if exist %NV_DIR%\NV_ACE_Reference\NV_ACE_Reference.uplugin', so a machine that bootstrapped the 5.7 line and then the 5.8 line finds leftover 5.7 content in the same staging directory and silently skips the 5.8 download, leaving 5.7 ACE binaries against a 5.8 engine.
- Changes: none

F-013 records the stale directory name as cosmetic. It is not: the name collision makes the idempotence check answer for the wrong engine line, and the failure is silent - the user gets a successful bootstrap and an editor that refuses to load the plugin with the opaque BuildId message. Fix in the same commit that flips NVIDIA_AVAILABLE, since both touch the same block and neither should ship without the other.

## J-0230 | 2026-11-15 | F-068 | lead
- Event: accepted
- Reason: The user chose to publish full bundles for 5.8 as for 5.7, with the duplication in front of them, so the three self-contained character bundles stand.
- Changes: none

Accepted with its cost stated: a roughly 5.2 GB release carrying about 3.3 GB of byte-identical model data, and three assets near GitHub's 2 GB per-asset limit. The shared-asset alternative is recorded in J-0228 and needs no NVIDIA source change, so it remains available if the limit is ever hit or a user complains about downloading 1.5 GB for one character.

What the decision buys: the bundles stay self-contained and match how NVIDIA ships, bootstrap needs no placement logic beyond what it already has, and a 5.8 release does not introduce a new structure at the same time as a new engine line.

## J-0231 | 2026-11-15 | NOTE | ateam-update
- Update: ATeam 0.26.0 -> 0.28.0
- Changes: team.json

Done:
- rendered agent definitions: planner (updated), core-dev (updated), speech-dev (updated), reasoning-dev (updated), build-dev (updated), test-dev (updated), tester (updated), security (updated), reviewer (updated)
- CLAUDE.md protocol block updated

Decisions still pending (/ateam:update):
- no advisor, whom an agent that cannot conclude escalates to (on Fable, run only on the user's approval): add one through /ateam:change (type team)

## J-0232 | 2026-11-15 | NOTE | lead
- Changes: none

Release 5.8, upload in flight: the user approved committing the release-truth edits (bootstrap.bat NVIDIA_AVAILABLE and fetch defaults, FANTASIAACE.uplugin back to 5.8.0, README) once all five nv-*-ue58 bundles have landed on runtime-v3.0 -- their words: 'Commit when landed'. The upload runs at ~1.5 Mbps over Wi-Fi (single sequential gh stream, straight to GitHub, not via the VPN), so ~6 h for the remaining 4.23 GB; only the 966 KB overlay had landed when measured. Verified meanwhile: BuildId 55116800 on all four module manifests, FANTASIATests is UncookedOnly so it is excluded from cooked games, and UnrealEditor-FANTASIA.dll imports only libswipl.dll (no whisper.dll). F-013 stays open by decision: renaming NVIDIA-UE57-Bundle would strand gigabytes outside .gitignore:45 and make C-install.md:23,47 stale, which needs a CR; its upgrade consequence (a 5.7-populated staging dir makes the 5.8 fetch skip silently) is covered by a warning in build-dev's files instead. The promotion to main is NOT a fast-forward: origin/main carries 10 commits Development lacks, including the deprecated OpenAI/Groq/Ollama code 5.8 deleted.

## J-0233 | 2026-11-15 | DEC-031 | build-dev
- Module: M-dist
- Decision: bootstrap.bat's NVIDIA_AVAILABLE gate now distinguishes a named NVIDIA flag (NV_EXPLICIT) from the default: with the flag off, a default run drops the four NVIDIA fetches and installs the core, while a run that named --ace/--mark/--james/--claire/--characters/--nvidia is still refused with an explanation and exit 1.
- Reason: T-33 flips the four FETCH_* defaults to 1 for the 5.8 line. The old gate refused whenever any FETCH_* was 1, so with the new defaults a bare bootstrap.bat on a future engine line whose hand-port is not yet published would refuse everything and exit 1 - bricking the core install. The gate is kept for the next engine bump, so it has to stay correct under the new defaults.
- Changes: none

Alternatives rejected: (a) leave the gate refusing on any requested bundle - makes NVIDIA_AVAILABLE=0 unusable once the defaults are on; (b) drop the gate entirely - loses the up-front refusal that beats a 404 from curl after a gigabyte of core artifacts, and the lead asked for the mechanism to stay; (c) auto-off with no refusal at all - a user who typed --nvidia would get a silent core-only install. Verified by running the parse loop and gate in isolation with NVIDIA_AVAILABLE forced to 0 and to 1 across seven flag combinations.

## J-0234 | 2026-11-15 | F-014 | lead
- Event: fixed
- Evidence: gh release view runtime-v3.0 now lists all four NVIDIA assets in state 'uploaded', at byte sizes identical to the local archives they were packaged from: nv-ace-reference-ue58-full.tar.gz 689029094, nv-audio2face-mark-ue58.tar.gz 1509920991, nv-audio2face-james-ue58.tar.gz 1509911871, nv-audio2face-claire-ue58.tar.gz 1510351662, plus nv-ace-reference-ue58-overlay.tar.gz 966626 and the three unchanged core assets. The bundles are the UE 5.8 hand-port (BuildId 55116800), not the ue57 builds F-014 ruled out. bootstrap.bat's NVIDIA_AVAILABLE and the four FETCH_* defaults are back to 1 in da759764, so the unflagged run installs everything again as ADR-0008 and C-install require.
- Commit: da759764
- Changes: FANTASIA/bootstrap.bat, FANTASIA/bootstrap.sh, FANTASIAACE/FANTASIAACE.uplugin, README.md

## J-0235 | 2026-11-16 | F-017 | security
- Event: opened
- Corrects: J-0130
- Kind: security
- Severity: major
- Module: M-asr
- Story: US-029
- Repro: FANTASIA.WhisperCapture.Resample.ReadsOneFramePastInput; exhaustive sweep of callback sizes 64-8192 shows 3839 of 8129 affected at 44100 Hz, 6237 of 8129 at 88200, and 0 of 8129 at 48000, 96000 and 32000; at 44100 every size up to 1200 frames is clean while 2205 (50 ms) and 4410 (100 ms) over-read on every callback
- Changes: none

Better reachability evidence than J-0130 carried, from an exhaustive sweep that finished after
that entry was written. It does not change the severity; it removes the qualifier "with large
callbacks", which was an artifact of my having spot-checked eleven sizes instead of all of
them.

**Every callback size from 64 to 8192 frames, 500 callbacks each, faithful arithmetic
(repeated `Phase += SrcToDst` in double, `float` rounding at the midpoint below
`NumFrames`):**

| device rate | callback sizes that over-read |
| :--- | :--- |
| 44100 | **3839 of 8129** |
| 88200 | **6237 of 8129** |
| 11025 | 4289 of 8129 |
| 22050 | 1893 of 8129 |
| 48000 | **0** of 8129 |
| 96000 | **0** of 8129 |
| 32000 | **0** of 8129 |

The three zeros confirm the mechanism I claimed generally rather than by sample: 48000, 96000
and 32000 divide 16000 exactly, so every phase value is exactly representable and none can
land below an integer boundary. The 44.1 kHz family is where this bug lives, and on a 44.1 kHz
device roughly **half of all possible callback sizes** are affected. 500 callbacks per size
understates it — sizes whose first hit comes later are counted clean here.

**The affected set has a structure, and it is not "large":**

| frames (44100) | period | result over 5000 callbacks |
| :--- | :--- | :--- |
| 441, 448, 480, 512, 882, 960, 1024, 1200 | 10-27 ms | clean |
| 1440 | 32.7 ms | 1 over-read per 1.6 s |
| 2048 | 46.4 ms | 1 per 21.1 s |
| **2205** | **50 ms** | **every callback** |
| 4096 | 92.9 ms | 1 per 42.2 s |
| **4410** | **100 ms** | **every callback** |

Everything at or below 1200 frames is clean, and the plugin requests 1024
(`WhisperCaptureComponent.cpp:471`), so the shipped configuration is clean **if the backend
honours the request** — which is luck, not design, since the request is advisory and the
callback size is the backend's to choose.

The two catastrophic sizes are the round periods, 50 ms and 100 ms, and the reason is worth
recording because it explains why the affected set otherwise looks arbitrary. `SrcToDst` is
441/160 = 2.75625, not representable in binary. The phase realigns on an exact integer every
160 output samples, so for `NumFrames` = 2205 (k=800) and 4410 (k=1600) the loop's last tap
falls exactly on `NumFrames` in exact arithmetic — and the accumulated error of 800 and 1600
repeated additions puts the computed sum just *below* it, so `Phase < SrcEnd` passes and the
`float` cast then rounds up to `NumFrames`. At 441 (k=160) and 882 (k=320) the accumulated
error happens to land on the other side, so the loop exits correctly and those sizes are
clean. The bug is a sign-of-rounding-error accident, which is why no amount of reading the
code reveals which configurations are affected and why the fix must be structural (index from
the `double`, clamp `Idx0`) rather than a tolerance.

Unchanged: severity major, `Kind=security`, not a blocker, for the reasons in J-0130 — no
untrusted input reaches the index arithmetic, and nothing crosses a trust boundary. All of
this is simulation of the arithmetic; it has still not been observed under a sanitizer on real
hardware, which needs a 44.1 kHz device delivering 1440 frames or more.

## J-0236 | 2026-11-16 | NOTE | security
- Changes: threat-model.md

F-017's reachability in the threat model is replaced with the measured figures from J-0235,
in the M-asr surface section and in Open risks. The severity and the ruling are unchanged;
what changes is that "with large callbacks" was my own spot-check artifact and is now an
exhaustive result.

The two statements that mattered and were imprecise: "reachable continuously on a 44.1 kHz
device with large callbacks" understates it — on a 44.1 kHz device 3839 of 8129 callback
sizes over-read — and "never on the 48 kHz family" is now confirmed for every size from 64 to
8192 at 48000, 96000 and 32000 rather than asserted from the ratio argument alone.

The operationally useful half, recorded because a maintainer will want it before touching the
resampler: every size up to 1200 frames is clean at 44100, the plugin requests 1024, and the
request is advisory — so the shipped configuration is clean by luck, not by design. The two
round periods, 2205 (50 ms) and 4410 (100 ms), over-read on every callback, because the phase
realigns on an exact integer every 160 output samples and the accumulated error of 800 and
1600 repeated additions lands just below the boundary where 160 and 320 additions land just
above. Which configurations are affected is a sign-of-rounding-error accident, so the fix has
to be structural rather than a tolerance.

## J-0237 | 2026-11-16 | F-070 | lead
- Event: opened
- Module: M-tests
- Kind: quality
- Severity: major
- Story: US-034
- Repro: docs/ateam/HISTORY.md cites gate report e093da91568a (19:03:33, 198 tests 0 failures) as evidence in four separate entries, each with the parenthetical 'read, not re-run'. One of them evidences OpenAICompatibleComponent.h:84-96 against :128-141. In the file today :84-96 contains the Apply Provider Preset note beginning at :95, and ApplyPreset is at :151 - line ranges that can only describe the file before that note existed. Gate run 0054e85f132a on the same tree that was pushed as origin/Development: FAILED (5), 1 of 198, FANTASIA.Quality.Tooltip.ConfigAndPreset.
- Changes: none

A gate report is evidence for the tree it ran on and for no other, and this sprint used one report as standing evidence across changes made after it.

The sequence. The gate was genuinely green at 198/0 at 19:03:33. Findings were then written citing that report rather than running the gate, which is cheap and was reasonable for findings that changed nothing. But at least one change was then made in response to a finding - the Apply Provider Preset note added to UOpenAICompatibleComponent::Config, which improved the metadata and discharged F-059's first half - and that change falsified the tester's pin asserting Config names neither the node nor the erasure. Nothing re-ran the gate afterwards. The suite went red at the moment the code got better, and every subsequent statement that the gate was green was a re-read of a report that predated the change.

The consequence was not hypothetical. I pushed that tree to origin/Development, merged main into it so main could fast-forward, and told the user the gate's 198 tests passed. I had not run it. The red surfaced only because I ran the gate on the merge result before recommending the promotion, and the project's Definition of Done requires the gate to pass. A promotion on that evidence would have put a failing suite on a shipped branch.

The report directory e093da91568a no longer exists on disk, so the claim can no longer even be re-checked - which is the second half of the defect: the evidence was not only stale but perishable.

What would prevent it: an entry citing a gate report names the commit or tree the run covered, so a later reader can tell whether it still applies; and a change made in response to a finding re-runs the gate rather than inheriting the previous run's verdict. The cost of the habit here was eight minutes of gate time against shipping a red suite to main.

## J-0238 | 2026-11-16 | F-059 | tester
- Event: fixed
- Evidence: FANTASIA/Source/FANTASIA/Public/OpenAICompatibleComponent.h:95-99 (Config's note names Apply Provider Preset, the erasure of APIKey, and the ini route); gate PASSED 198 tests 0 failures with FANTASIA.Quality.Tooltip.ConfigAndPreset asserting it
- Repro: FANTASIA/Source/FANTASIATests/Private/Quality/Neo4jTransportQualityTests.cpp, test FANTASIA.Quality.Tooltip.ConfigAndPreset
- Changes: none

Ruling, from the metadata as it stands (OpenAICompatibleComponent.h:95-99, Config's @note): F-059's first half, that Config's metadata names neither the node nor the erasure, was true when J-0206 was filed and is fixed now. The note names "Apply Provider Preset", says it replaces this whole configuration, APIKey included, and that a key read from DefaultGame.ini is discarded by that call. The pin at Neo4jTransportQualityTests.cpp asserted the old silence and went stale when the note landed; it is replaced by four positive assertions on Config's tooltip (node name; whole configuration and APIKey; discarded by that call; skip that node, the ini route). The gate ran 198 tests, 0 failures.

Dating, checked rather than taken: J-0206 cites ApplyPreset's doc at :128-141 and J-0214 cites Config's block at :84-96 as silent. Today ApplyPreset's block opens at :134, six lines below the cited :128, and the six lines are exactly the blank line and the five-line note at :94-99. So both entries describe the file without the note, and the note was added afterwards; the header's mtime (2026-11-15 20:39) is after the 19:03 gate report the later entries cite. Git cannot date it: the whole sprint is one commit, 014a33fd.

What does not survive as a defect: the criterion's third clause, that the metadata say the key must be set again after the call. Config's note does not say it, and ApplyPreset's says the opposite: setting the key again makes it a literal in a committed Blueprint graph. That is a decision (T-29, ADR-0001), the product is right, and the criterion's wording is what conflicts with it, as J-0208 and J-0214 both ruled. It is not a finding against the product, so no replacement F is opened; the remedy is a change request to US-034's first criterion (move it onto Config, replace the set-again clause with the ini route), which is the lead's and the user's. Until then the test pins the absence on Config (assertion labelled F-059, pinned: the criterion's clause, not a defect), so that a later edit that adds advice to set the key again fails visibly.

J-0208's first half (a location mismatch, fixable) was correct at the time and is now resolved; its second half stands. J-0214's evidence line against :84-96 is stale on the same account; its conclusion on the criterion's second clause stands. Not touched: F-064, the label at :113 on ApplyPreset's "again" assertion, still open.

## J-0239 | 2026-11-16 | NOTE | lead
- For: REQ-001, US-020, US-021
- Changes: none

UE 5.8 is on main. The user promoted it themselves ('Promote to main'), as main requires.

origin/main moved 897275e0 -> 922f1e1f as a fast-forward; 897275e0 remains an ancestor, so no history was rewritten and the branch's force-push protection was never challenged. origin/main, origin/Development and the local tip are all 922f1e1f.

What main now carries, read back from the remote ref: both .uplugin files at EngineVersion 5.8.0 with FANTASIAACE under its own name again; bootstrap.bat with RELEASE_TAG=runtime-v3.0, NVIDIA_AVAILABLE=1 and the four NVIDIA fetches on by default; and no GroqComponent, OllamaComponent or OpenAIComponent, the 737 lines the 5.8 migration removed, which the merge of main resolved as our deletion.

Evidence at the moment of promotion. Gate report a680c4cc0aa8: 198 tests, 0 failed, 0 not run, every test Success, and no source file newer than that report. dumpbin on the shipped runtime module: libswipl.dll the only non-system import, no whisper.dll. All four .modules at BuildId 55116800. All five nv-*-ue58 assets on runtime-v3.0 served over their public download URLs at HTTP 200 with content-length matching the local archives byte for byte.

What is not evidenced: nobody has run bootstrap.bat from a clean clone and driven a MetaHuman through Audio2Face on 5.8. The ACE 5.8 port building all four plugins from a one-line change is strong evidence that it works; it is not an observation that it does. F-014 and F-016 are closed on the distribution facts, not on an end-to-end run.

Owed still: a change request on US-034's first criterion (its set-the-key-again clause conflicts with ADR-0001, see J-0238), S-02's close, and F-070 on the stale-evidence practice that nearly let a red gate onto this branch.

## J-0240 | 2026-11-16 | F-021 | lead
- Event: fixed
- Story: US-030
- Evidence: Gate a680c4cc0aa8 on 922f1e1f, the commit now on main: FANTASIA.Security.Bolt 18/18, FANTASIA.Stress.Bolt 3/3, and the 36 BoltPackStream codec and round-trip tests, all Success in a run of 198 with 0 failures and 0 not run. FBoltPackStreamReader::Require (BoltPackStream.cpp:192) bounds every read against Data.Num() - Cursor and refuses a negative count; MaxBoltNestingDepth 64 is tested at BoltPackStream.cpp:294; the message cap is enforced on UNeo4jComponent's side because FBoltDechunker carries no FANTASIA_API (F-012), as the sprint plan required rather than exporting a class in passing.
- Residual: The cap bounds bytes on the wire, not what they decode into - F-056 (major, performance) with F-060 (major, security) deciding its severity, and F-057 (minor) on how the cap is charged. F-048 (major, bug) also remains open against US-030. None is a blocker.
- Changes: none

## J-0241 | 2026-11-16 | F-020 | lead
- Event: fixed
- Story: US-031
- Evidence: Gate a680c4cc0aa8 on 922f1e1f, the commit now on main: FANTASIA.Security.Neo4jTls 11/11 and FANTASIA.Stress.Neo4jTls 1/1, all Success in a run of 198 with 0 failures. On the plain TCP transport, which is the default, an encrypted connection is now an authenticated one: SSL_CTX_set_verify(SSL_VERIFY_PEER) is set, SSL_CTX_load_verify_locations' return is tested rather than ignored, and X509_VERIFY_PARAM_set1_host pins the host - with X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS asserted as the witness, because set1_host returns success on an empty string and would silently clear the host list. bolt+s:// and neo4j+s:// require a chain to a trust anchor and the host's name; bolt+ssc:// is the one-character migration for a self-signed certificate, and it appears in the failure's log message and in C-neo4j, as J-0163 required.
- Residual: Scoped, not total, and the header says so per transport (Neo4jComponent.h:118-134). WebSocket only decides: the engine's WebSockets module owns verification there, so the decision is advisory in both directions - that is F-055 (major, security), open, and F-061 (major, security) carries the sentence CR-012 refused to put in C-neo4j. Neither is a blocker. F-020 is closed against the TLS transport this component implements, not against every transport it can select.
- Changes: none

## J-0242 | 2026-11-16 | NOTE | lead
- Story: US-030, US-031, US-034
- For: the S-02 review
- Changes: none

The user accepted all three stories at the S-02 review. Their words: 'Accept all three'.

The review was presented with the evidence standing on 922f1e1f, the commit now on main, from gate report a680c4cc0aa8 (198 tests, 0 failures, 0 not run): US-030 on FANTASIA.Security.Bolt 18/18, FANTASIA.Stress.Bolt 3/3 and the 36 BoltPackStream codec tests; US-031 on FANTASIA.Security.Neo4jTls 11/11 and FANTASIA.Stress.Neo4jTls 1/1; US-034 on FANTASIA.Quality.Tooltip 2/2 and FANTASIA.Quality.Secrets 15/15.

Disclosed before the acceptance, and accepted with them. Against US-030: F-056 (major, performance) that the 16 MiB cap bounds bytes on the wire rather than what they decode into, F-060 (major, security) which decides F-056's severity, F-057 (minor) on how the cap is charged, and F-048 (major, bug). Against US-031: F-055 (major, security) that on the WebSocket transport the component's decision is advisory because the engine's WebSockets module owns verification, and F-061 (major, security). Against US-034: F-049 and F-064, both minor. None is a blocker, so the Definition of Done's bar on open blocker findings is met; F-020 and F-021 closed at J-0241 and J-0240.

Also disclosed: US-034's first criterion cannot be literally met. Its clause asking the metadata to say the key must be set again after Apply Provider Preset conflicts with ADR-0001, the product is right and the wording is wrong, and the remedy is a change request that has not been raised. The story is accepted on the same footing as US-028 was, with the divergence named rather than hidden. The CR remains owed.

Unusually, the code was already on main when the review ran: the user drove the 5.8 release ahead of the sprint's close, so an acceptance here confirms shipped work rather than authorising a ship.

## J-0243 | 2026-11-16 | NOTE | lead
- Finding: F-015
- Module: M-dist
- Changes: none

Second instance of F-015's root cause, found while considering whether a Linux cloud instance could test bootstrap on a clean machine. F-015's repro names only GPU_ASSET at bootstrap.sh:35. The line below it is wrong in the same way and for the same reason:

  DEPS_ASSET="fantasia-build-deps-win64.tar.gz"

That asset does exist on runtime-v3.0, so unlike the GPU fetch it does not 404 - it succeeds, and extracts Win64 build-time static libraries into a Linux tree at ThirdParty/. A silent wrong-platform install is arguably worse than a failed download, because nothing reports it.

So a default bootstrap.sh run on a clean Linux machine today: fetches the whisper model (correct, engine-agnostic), fails the GPU fetch on an asset that has never existed, and installs Win64 libraries. Taken together these say bootstrap.sh is not a working Linux path at all rather than a working path with one bad asset name - which is the useful form of the finding for whoever fixes it, since the remedy is a decision about whether Linux is supported (NG-2 and P-09 say Win64 is), not two string edits.

Severity unchanged at minor on F-015's own reasoning, and this note does not reopen it.

Practical consequence recorded here because it cost us a test: the Claude cloud environments are Linux, so they cannot verify bootstrap on a fresh machine for this project. What needs verifying is bootstrap.bat, rewritten by 112 lines in da759764, together with Win64-only NVIDIA bundles. Its logic has now been executed locally both ways - an unknown argument prints usage and exits 1, and a run with every fetch disabled reaches the completion summary and exits 0 - but the real download, tar extraction and plugin-move path has still never run.

## J-0244 | 2026-11-16 | S-02 | lead
- Event: closed
- Done: US-030, US-031; and US-014 returns to done, which was the goal. US-034 is carried to S-03 as in-progress
- Review: The user accepted all three stories (J-0242, their words: Accept all three), with the major residuals named before the acceptance. US-030 and US-031 went to done; US-014 followed once F-020 and F-021 closed. US-034 did not: the Definition of Done needs a reviewer verdict of meets request, the verdict standing against it is J-0214 does not meet request, and the user's acceptance satisfies a different clause and cannot substitute. The planner refused the status change on that ground and was right. No story was sent back and no request was rejected this sprint, so there is nothing for the user to confirm or overturn
- Retrospective: What slowed it: T-31 and T-27 were dispatched in parallel on non-overlapping files, but the test module links the runtime module, so one gate ran against the other's half-written code (rule at J-0188: two code tasks in one compile unit go in sequence); and two cross-ownership edits traceable to my briefs, which is F-052. Findings by kind: 11 quality, 5 bug, 3 security, 3 performance - the security ones (F-055, F-060, F-061) say the threat model's surface did not cover the WebSocket transport, and the performance ones (F-056, F-065, F-066) say NFR-001 to NFR-003 still have no measures, unchanged since S-01. Ownership and comms: .ateam/ownership.jsonl does not exist, so it logged nothing, and two real crossings happened anyway - the empty log is not evidence for switching enforcement on, which is F-052's point. Checker request statistics: 2 accepted, 1 rejected, 1 needs-info, so the brief and the principles are not too thin. Quality tools the gate reports missing: gates.quality is empty for both cpp and csharp, so the clean-code promise still has no mechanical check, third sprint owed. Scope discipline was tested and failed exactly as S-01 predicted: five findings fixed that blocked none of the sprint's stories and were never planned (F-014, F-016, F-050, F-051, F-059). Owed as change requests, none raised yet: US-034's first criterion, US-014's first criterion, and F-070's preventive measure, which is a process change and so belongs in a principle or a verification profile rather than in prose. The code cleanup passes were deliberately skipped: the code was already on main and gate green, and a tidiness pass over shipped code was not worth the risk at that hour
- Cost: $34.55 at the 2026-11-13 list prices (lead 64%, planner 19%, security 11%); cold-cache rewrites $2.33; flags: 0 - against a $160 budget, but the figure is incomplete: ateam_cost.py read the sprint window as 2026-11-16 only and so missed most of the nine tasks, which ran on 2026-11-15
- Commit: 922f1e1f
- Changes: backlog.md, sprint.md

Verification. The gate went from 150 tests to 198, all passing, 0 failures and 0 not run, in report a680c4cc0aa8 against commit 922f1e1f - the commit now on main. The 48 added tests are concentrated where the sprint worked: FANTASIA.Security.Bolt 18 and FANTASIA.Security.Neo4jTls 11, both the security engineer's; FANTASIA.Stress.Bolt 3 and FANTASIA.Stress.Neo4jTls 1, the tester's sprint-level pass; FANTASIA.Quality.Bolt 5, FANTASIA.Quality.Neo4j 5 and FANTASIA.Quality.Tooltip 2.

Findings: 23 opened, 9 major and 13 minor, no blocker; 3 fixed and 1 accepted within the sprint, 19 still open. The two blockers the sprint existed to close are closed with evidence, F-021 at J-0240 and F-020 at J-0241, and F-020's closure is explicitly scoped to the TLS transport this component implements rather than to every transport it can select - the WebSocket gap is F-055 and is open.

Stress measures against NFRs: the stress pass exists as four tests but no NFR gained a measure, so NFR-001 to NFR-003 are in the same state S-01 left them. That is the second sprint running and it is now the oldest item on the board.

The sprint was interrupted by the UE 5.8 release, which the user drove to completion ahead of this close, so the review confirmed shipped work rather than authorising a ship. One defect reached main as a result: F-069, major, where I told build-dev not to fix the staging-directory collision while citing F-013's minor rating, when F-069 was the finding that governed and said in terms that the fix must ship in the same commit as the NVIDIA_AVAILABLE flip. Its fix is in hand and did not need the change request I had feared.

## J-0245 | 2026-11-16 | NOTE | lead
- For: S-03 planning
- Story: US-034
- Changes: none

T-30 was planned and never dispatched, and nobody noticed until the memory tidy-up.

S-02's task table (J-0165) assigned T-30 to speech-dev: the M-tts half of US-034, F-033's tooltip. T-30 appears exactly once in the journal, in that plan, with no dispatch, no DEC and no report. speech-dev concluded during its tidy-up that it did no task in S-02 and was right; my brief to it had implied otherwise, and the sprint ran nine tasks where the plan named ten.

The work landed anyway. RESTTTSComponent.h:319-320 carries the section-rule sentence F-033 asked for, and FANTASIA.Secrets.Tooltip.Headers and FANTASIA.Quality.Secrets.Section.DerivedClass pass in the green gate. So the outcome was reached without the task that was supposed to reach it, which is why nothing failed loudly: the gate cannot tell who wrote a passing test's subject.

F-032 and F-033 both stay open, and this is deliberate rather than an oversight. I came close to closing them on the strength of the tooltips and re-read them first. Neither finding's defect is the missing tooltip:

- F-032's defect is that ApplyPreset assigns Config wholesale and erases a key read from DefaultGame.ini. Preserving the key across that call would change a documented behaviour of C-llm, which P-19 excludes from the 5.8 line. The finding's own words are that the planner may want it for 5.9 and that the tooltip could say so now. The tooltip says so; the behaviour stands.
- F-033's defect is that LoadConfig reads the ini section of the instance's own class, so a Headers line under the base class reaches only the base class. That is engine behaviour, not a plugin bug, and the finding proposes a tooltip sentence as what 5.8 admits. The sentence is there; the behaviour stands.

So both are open with their admitted mitigation landed, which is a different state from either fixed or accepted, and the journal has no event for it. Worth settling at S-03 planning: either accept them with a Reason naming P-19 and the mitigation, or carry them to 5.9 with F-032's preservation change. Leaving them open with no disposition is what lets them be rediscovered every sprint.

Two process points for the same planning. A task in the plan needs something that fails when it is not dispatched; nine of ten ran and the sprint closed green. And US-034 was accepted while the two findings it was created to address are still open - the acceptance was of its criteria, which is correct, but the story's purpose and its criteria had drifted apart without anyone saying so.

## J-0246 | 2026-11-16 | DEC-032 | build-dev
- Module: M-dist
- Decision: bootstrap.bat identifies a staged NVIDIA plugin by a release stamp written inside it - FANTASIA-bootstrap-release.txt, holding the RELEASE_TAG that populated it - and not by the staging directory name. A fetch is skipped only when the plugin is present AND stamped with this release; an absent or mismatched stamp means the content came from another engine line, so that one directory is deleted inside the staging directory and re-fetched. NV_DIR stays NVIDIA-UE57-Bundle.
- Reason: F-069: the skip predicate asked whether a plugin was extracted here, when the question is whether a plugin for THIS engine line was. The release tag is the right identity - no hardcoded engine BuildId to drift at the next bump, and no rename, so C-install.md:23,47 and architecture.md:296 keep describing a directory that still has that name. Invariant: bootstrap deletes nothing outside the staging directory it creates and git ignores; a plugin already moved into the project Plugins folder belongs to the user and is only mentioned in the notice.
- Changes: none

Alternatives rejected: renaming NV_DIR per engine line (a C-install + architecture change, and it leaves the old directory on disk forever); pinning the engine BuildId, or an asset-name substring, in the predicate (drifts at every engine bump, and says nothing about what was actually extracted); clearing a plugin already moved into the project's Plugins\ folder (bootstrap must never delete the user's project content, so that case stays with the warning notice, which now also says the staging directory is checked automatically).

Shape notes. --force is untouched, so it still overlays a same-release tree rather than clearing it; only a tree from another release is cleared. The stamp is written last, after the extraction succeeded, so an interrupted run leaves no stamp and the next run replaces what it staged. When a stale tree cannot be removed (a file held open), the run stops with the path to delete by hand instead of extracting onto a half-deleted tree.

Two batch traps are documented in the file beside the code they bite: a .bat invoked without `call` runs in place of its caller and never returns (which is why the test harness needs a real curl.exe shim), and `echo %RELEASE_TAG%>file` reads the tag's trailing digit as a stream number and writes a truncated tag, so the redirection comes first.

## J-0247 | 2026-11-16 | F-069 | lead
- Event: fixed
- Story: US-020
- Evidence: bootstrap.bat's skip predicate now asks whether a plugin for THIS release is staged, not merely whether one is staged. Each extraction writes FANTASIA-bootstrap-release.txt holding RELEASE_TAG inside the staged plugin, per plugin rather than per directory so an interrupted run leaves no stamp; a fetch is skipped only when the plugin is present and stamped with this release, and a tree from another release is cleared inside the staging directory and re-fetched. NV_DIR keeps its name, so .gitignore:45 is untouched and C-install.md:23,47 and architecture.md:296 keep describing a directory that still has that name - no rename and no change request. DEC-032 at J-0246. Verified by me independently of build-dev's 41-check harness, in a scratch copy of the script (cmp byte-identical) placed where no Plugins ancestor could trigger the move step: with a matching runtime-v3.0 stamp the run skips, downloads nothing and exits 0 with the fixture intact; with the stamp removed, which is the 5.7-populated case F-069 describes, it does not skip, prints that this release's build replaces it, attempts the download, and the 5.7 sentinel file is gone - the stale tree was cleared rather than kept. The failed download exits 1 leaving no stamp, so a retry re-attempts instead of trusting a partial. setlocal EnableDelayedExpansion is at line 2, so the !NV_STAMP! comparison is live and not inert text. CRLF preserved: 549 pairs, 0 lone LF, 0 lone CR.
- Residual: Two cases the stamp cannot see, both pre-existing and neither this finding's. A successful install empties and removes the staging directory, so the next run re-fetches 4.9 GB and then declines to overwrite the copies already in the user's Plugins folder, stranding the fresh ones in staging; fixing that means reading a stamp inside the installed plugin, which needs a decision about touching the user's Plugins folder at all. And the same old predicate is still live on the 5.7 branch, so a clone that bootstraps 5.8 and then checks out 5.7 keeps the 5.8 bundles - Alice is on that line.
- Changes: none
