# Product backlog
- As of: J-0054
- Approved: yes

US-001 to US-019 were recovered at adoption from the FANTASIATests automation suite
(the most reliable statement of current behaviour), the README feature inventory and the
component headers; a story numbered above those comes from a request and names it on its
`Source:` line. A story marked `done` describes what the system does today, bugs
included; `Confidence: medium` means no test pins it yet and the first sprint owes it
one. A `Tests:` path marked `[does not exist yet]` is where that story's test belongs — a
characterisation test where the story is `done`, the acceptance test where the work has
not been done — and nothing verifies the story today; ten of the test files named below do
not exist.

A story may carry a `Target line:` field naming the engine line it is for — the line whose
branch the work lands on. A story without one targets the shipped line, which is the
engine version the brief names (5.8 today). Work for a line that has not shipped yet is
logged here with that line named, so that it has somewhere to live before the line exists
instead of waiting unlogged. P-19 decides what the shipped line admits and sends
everything larger to the next line, so a story's target line and its size are the same
question asked twice.

One consequence of P-19 is open, and belongs in front of whoever is choosing what to work
on: **F-011 is excluded from the 5.8 line.** Any form of its fix — removing `UseSSL`,
refusing to connect on a mismatch, or at minimum warning — changes C-neo4j, and the bar
admits nothing that touches a contract. So for the life of the 5.8 line, a user who sets
`UseSSL = true` against a `bolt://` endpoint connects in plaintext and is told nothing,
and their Neo4j password crosses the network in the clear. The finding stays open and the
fix waits for the 5.9 line. F-008, the other open major, is admitted: two missing `Config`
specifiers, no contract touched.

### US-001: Transcribe recorded speech locally
- Story: As a researcher, I want speech recognised on the machine without a cloud service, so that recordings with human participants never leave the lab.
- Priority: Must
- Status: done
- Estimate: L
- Acceptance: Given a Whisper model file in Resources and an audio buffer at 16 kHz mono, When Transcribe is called through the subsystem, Then a result carrying full text and per-segment text with start and end times is returned, and no network call is made.
- Tests: FANTASIA/Source/FANTASIATests/Private/WhisperBlueprintLibraryTests.cpp
- Source: FANTASIA/Source/FANTASIA/Public/WhisperSubsystem.h; README.md
- Depends on: none
- Provenance: recovered
- Confidence: medium

### US-002: Listen through the microphone in real time
- Story: As a researcher, I want the agent to hear a speaker continuously, with silence detected for it, so that a conversation needs no push-to-talk.
- Priority: Must
- Status: done
- Estimate: L
- Acceptance: Given a capture component with AudioDeviceIndex -1, When the component starts, Then the OS default input device is opened, speech is segmented by voice activity, and each utterance is transcribed and broadcast as it completes.
- Tests: FANTASIA/Source/FANTASIATests/Private/WhisperCaptureComponentTests.cpp [does not exist yet]
- Source: FANTASIA/Source/FANTASIA/Public/WhisperCaptureComponent.h
- Depends on: US-001
- Provenance: recovered
- Confidence: medium

### US-003: Choose CPU or GPU inference without rebuilding
- Story: As a Blueprint-only user, I want to switch Whisper to the GPU by setting a property, so that I get GPU speed without a C++ toolchain.
- Priority: Should
- Status: done
- Estimate: M
- Acceptance: Given the CUDA whisper stack staged in Binaries/Win64, When Backend is set to GPU before LoadModel, Then every whisper call is routed through the loaded CUDA library; and Given the CUDA library is absent, Then a warning is logged and inference falls back to CPU. Whether the CUDA runtime itself resolves is not checked, so a present-but-unusable stack is not caught.
- Tests: FANTASIA/Source/FANTASIATests/Private/WhisperBackendTests.cpp [does not exist yet]
- Source: FANTASIA/Source/FANTASIA/Private/WhisperBackend.h
- Depends on: US-001
- Provenance: recovered
- Confidence: medium

### US-004: Export a transcript as subtitles
- Story: As a researcher, I want a transcript as SRT or VTT, so that I can caption a recorded session without another tool.
- Priority: Could
- Status: done
- Estimate: S
- Acceptance: Given a transcription result with timed segments, When FormatAsSRT or FormatAsVTT is called, Then well-formed subtitle text is returned with timestamps that carry hours correctly past one hour.
- Tests: FANTASIA/Source/FANTASIATests/Private/WhisperBlueprintLibraryTests.cpp
- Source: FANTASIA/Source/FANTASIA/Public/WhisperBlueprintLibrary.h
- Depends on: US-001
- Provenance: recovered
- Confidence: high

### US-005: Speak streamed audio in order, as it arrives
- Story: As a researcher, I want synthesized speech to start playing before the whole utterance has been generated, and to play in the order requested, so that the agent answers without a pause.
- Priority: Must
- Status: done
- Estimate: L
- Acceptance: Given several synthesis calls issued back to back, When audio chunks arrive interleaved, Then each call's audio is buffered separately and enqueued in request order, and audio not already at 16 kHz is resampled to it; and Given a consumer is attached by a subclass, Then buffers are drained in that order at a paced rate and PlaybackComplete fires per id; and Given the HTTP call fails, Then the error is logged and nothing enters the audio pipeline. The core component supplies the buffering and the ordering, not the consumer (F-009).
- Tests: FANTASIA/Source/FANTASIATests/Private/Pcm16StreamDecoderTests.cpp
- Source: FANTASIA/Source/FANTASIA/Public/RESTTTSComponent.h
- Depends on: none
- Provenance: recovered
- Confidence: medium

### US-006: Synthesize speech with ElevenLabs
- Story: As a researcher, I want the agent's voice produced by ElevenLabs over its streaming API, so that it sounds like a person without local synthesis.
- Priority: Must
- Status: done
- Estimate: M
- Acceptance: Given a voice id, model id and API key from configuration, When speech is requested, Then a streaming request is built against the ElevenLabs endpoint and each returned chunk is decoded from base64 into PCM16 and handed to playback, including when a server-sent line arrives split across reads.
- Tests: FANTASIA/Source/FANTASIATests/Private/ElevenLabsProtocolTests.cpp
- Source: FANTASIA/Source/FANTASIA/Public/ElevenLabsProtocol.h
- Depends on: US-005
- Provenance: recovered
- Confidence: high

### US-007: Control pronunciation, speed and language of synthesis
- Story: As a linguist, I want to fix how particular words are pronounced and set speaking rate and language, so that the agent reads domain terms correctly.
- Priority: Should
- Status: done
- Estimate: S
- Acceptance: Given a pronunciation map, a speed and a language code, When a request is built, Then every whole word matching a key case-insensitively is replaced with its value, keeping the original leading capitalisation, while a key inside a longer word is left alone; and the speed appears in the voice settings and the language code in the request body; and Given empty text or several mapped keys, Then substitution is still correct.
- Tests: FANTASIA/Source/FANTASIATests/Private/ElevenLabsProtocolTests.cpp
- Source: FANTASIA/Source/FANTASIA/Public/ElevenLabsProtocol.h
- Depends on: US-006
- Provenance: recovered
- Confidence: high

### US-008: Talk to any OpenAI-compatible model
- Story: As a researcher, I want one component to reach OpenAI, Groq, Ollama, LM Studio or vLLM, so that switching provider is a configuration change and not a code change.
- Priority: Must
- Status: done
- Estimate: M
- Acceptance: Given a provider preset, When the component is configured from it, Then endpoint, model and authentication header match that provider; and Given a chat history and a streaming request, Then partial deltas are broadcast as they arrive and a final assembled message once the stream ends.
- Tests: FANTASIA/Source/FANTASIATests/Private/OpenAICompatibleConfigTests.cpp
- Source: FANTASIA/Source/FANTASIA/Public/OpenAICompatibleComponent.h
- Depends on: none
- Provenance: recovered
- Confidence: medium

### US-009: Drive a remote LangGraph agent
- Story: As a researcher, I want the conversation handled by a LangGraph workflow I host, so that the agent's logic lives in Python while the body lives in Unreal.
- Priority: Must
- Status: done
- Estimate: M
- Acceptance: Given a LangGraph endpoint and thread, When a user turn is sent, Then stream chunks are parsed in both supported shapes, Anthropic content-block arrays included, and broadcast once the turn is complete rather than per fragment.
- Tests: FANTASIA/Source/FANTASIATests/Private/LangGraphComponentTests.cpp [does not exist yet]
- Source: FANTASIA/Source/FANTASIA/Public/LangGraphComponent.h
- Depends on: none
- Provenance: recovered
- Confidence: medium

### US-010: Reason in Prolog from Blueprints
- Story: As a researcher, I want to assert and retract facts and run queries against an embedded Prolog engine, so that an agent's knowledge is declarative and inspectable.
- Priority: Must
- Status: done
- Estimate: L
- Acceptance: Given the embedded SWI-Prolog engine initialised on its own thread, When a query is submitted from the game thread, Then the engine runs it off the game thread and the solution bindings are delivered back on the game thread; and Given the Prolog runtime DLLs are missing, Then initialisation fails with a logged diagnostic rather than a crash.
- Tests: FANTASIA/Source/FANTASIATests/Private/SWIPrologComponentTests.cpp [does not exist yet]
- Source: FANTASIA/Source/FANTASIA/Public/SWIPrologComponent.h, SWIPrologThread.h
- Depends on: US-011
- Provenance: recovered
- Confidence: medium

### US-011: Build Prolog terms from Blueprints
- Story: As a Blueprint-only user, I want to construct atoms, variables, numbers, compounds and lists as objects, so that I can build a query without writing Prolog syntax in a string.
- Priority: Must
- Status: done
- Estimate: M
- Acceptance: Given the term helper library, When an atom, variable, integer, float, compound, list or nested compound is built and rendered, Then the resulting term text is the Prolog form of that structure.
- Tests: FANTASIA/Source/FANTASIATests/Private/SWIPrologHelpersTests.cpp
- Source: FANTASIA/Source/FANTASIA/Public/SWIPrologHelpers.h
- Depends on: none
- Provenance: recovered
- Confidence: high

### US-012: Build and query a Bayesian network
- Story: As a researcher, I want to define discrete variables, arcs and conditional probability tables and run exact inference, so that an agent can act under uncertainty.
- Priority: Must
- Status: done
- Estimate: L
- Acceptance: Given variables added with their domains and arcs between them, When inference runs with Lazy Propagation, Shafer-Shenoy or Variable Elimination, Then posteriors are returned for the queried variables; and Given a duplicate variable name, a duplicate arc or an erased node, Then the operation is rejected or the structure updated without corrupting the network; and Given an entropy or id-from-name query, Then the value matches the network.
- Tests: FANTASIA/Source/FANTASIATests/Private/BayesianNetworkTests.cpp
- Source: FANTASIA/Source/FANTASIA/Public/BayesianNetwork.h
- Depends on: none
- Provenance: recovered
- Confidence: high

### US-013: Build and solve an influence diagram
- Story: As a researcher, I want chance, decision and utility nodes with arcs and to solve for the best decision, so that the agent can choose and not only believe.
- Priority: Must
- Status: done
- Estimate: L
- Acceptance: Given chance, decision and utility nodes with arcs, When Shafer-Shenoy LIMID inference runs, Then optimal decisions and expected utilities are returned; and Given a duplicate node or an erased node, Then the diagram stays consistent.
- Tests: FANTASIA/Source/FANTASIATests/Private/InfluenceDiagTests.cpp
- Source: FANTASIA/Source/FANTASIA/Public/InfluenceDiag.h
- Depends on: US-016
- Provenance: recovered
- Confidence: high

### US-014: Query a Neo4j graph with Cypher
- Story: As a researcher, I want Cypher queries run against Neo4j and results returned as typed rows, so that an agent's knowledge can live in a graph database.
- Priority: Must
- Status: done
- Estimate: L
- Acceptance: Given a Bolt endpoint, When the transport is Plain TCP (the default) or WebSocket, with or without TLS, Then handshake, authentication and query exchange succeed against both Aura and a local install; and Given a result set, Then each row exposes nodes, relationships and simple values as distinct cell types; and Given any PackStream value, Then encoding and decoding round-trips it exactly.
- Known defect: F-011 — `BeginPlay` overwrites `UseSSL` from the endpoint scheme, so `UseSSL = true` against a `bolt://` endpoint connects in plaintext silently. The fix changes C-neo4j, so P-19 excludes it from the 5.8 line; it waits for 5.9.
- Tests: FANTASIA/Source/FANTASIATests/Private/BoltPackStreamTests.cpp
- Source: FANTASIA/Source/FANTASIA/Public/Neo4jComponent.h, BoltProtocol.h, BoltPackStream.h
- Depends on: none
- Provenance: recovered
- Confidence: medium

### US-015: Train and apply a support vector machine
- Story: As a researcher, I want to train an SVM on labelled samples and predict from Blueprints, so that a classifier can be built inside the project.
- Priority: Could
- Status: done
- Estimate: M
- Acceptance: Given labelled training samples and SVM parameters, When training runs and prediction follows, Then predicted labels are returned; and Given no model or no samples, Then prediction fails safely with a logged reason instead of crashing.
- Tests: FANTASIA/Source/FANTASIATests/Private/LibSVMTests.cpp
- Source: FANTASIA/Source/FANTASIA/Public/LibSVM.h
- Depends on: none
- Provenance: recovered
- Confidence: high

### US-016: Estimate a probability density without a parametric model
- Story: As a researcher, I want a kernel density estimate and evenly spaced sample points, so that I can turn observations into a distribution the reasoning components can use.
- Priority: Could
- Status: done
- Estimate: S
- Acceptance: Given observations, When the estimator is built and evaluated at a point or asked for its pdf over a range, Then the values match the kernel estimate; and Given a count and bounds, including a negative range, Then Linspace returns that many evenly spaced points inclusive of both ends.
- Tests: FANTASIA/Source/FANTASIATests/Private/MathUtilitiesTests.cpp
- Source: FANTASIA/Source/FANTASIA/Public/MathUtilities.h
- Depends on: none
- Provenance: recovered
- Confidence: high

### US-017: Animate a MetaHuman face from synthesized speech
- Story: As a researcher, I want the agent's face driven by the speech it is producing, so that the character looks like it is talking.
- Priority: Must
- Status: ready
- Estimate: L
- Acceptance: Given FANTASIAACE and NV_ACE_Reference enabled and an A2F provider configured, When speech is synthesized through an ACE TTS component, Then audio samples are streamed to Audio2Face and the resulting animation drives the face; and Given the turn ends with EndTurn set, Then the A2F session is closed so no audio bleeds into the next utterance; and Given FANTASIAACE is absent, Then core FANTASIA still synthesizes and plays audio.
- Tests: FANTASIAACE/Source/FANTASIAACETests/Private/ACERESTTTSComponentTests.cpp [does not exist yet]
- Source: FANTASIAACE/Source/FANTASIAACE/Public/ACERESTTTSComponent.h, ACEElevenLabsTTSComponent.h
- Depends on: US-005, US-006
- Provenance: recovered
- Confidence: medium

### US-018: Install the whole stack with one command
- Story: As a new user, I want one script to fetch everything a typical interactive MetaHuman needs and put it where it belongs, so that I can start from a clone and a single run.
- Priority: Must
- Status: ready
- Estimate: L
- Acceptance: Given a clone placed inside a project's Plugins folder, When bootstrap runs with no arguments, Then the Whisper model, GPU stack, build-time libs and the NVIDIA ACE bundle plus sample characters are fetched from the release assets and the NVIDIA plugins are placed next to FANTASIA; and Given it is re-run, Then nothing is refetched and a summary is printed; and Given an opt-out flag, Then only that part is skipped.
- Tests: FANTASIA/Source/FANTASIATests/Private/BootstrapContractTests.cpp [does not exist yet]
- Source: FANTASIA/bootstrap.bat, FANTASIA/bootstrap.sh
- Depends on: US-020
- Provenance: recovered
- Confidence: medium

### US-019: Use the plugin without compiling anything
- Story: As a Blueprint-only user, I want the plugin to work from the clone as it is, so that I never need Visual Studio or a C++ toolchain.
- Priority: Must
- Status: ready
- Estimate: M
- Acceptance: Given the committed Win64 binaries and the engine version named in FANTASIA.uplugin, When the plugin is enabled in a Blueprint-only project, Then the editor loads it without reporting a different engine version, every component appears in the Blueprint palette, and no module requires a rebuild.
- Tests: FANTASIA/Source/FANTASIATests/Private/BootstrapContractTests.cpp [does not exist yet]
- Source: FANTASIA/Binaries/Win64/
- Depends on: US-018
- Provenance: recovered
- Confidence: medium

### US-020: Re-port the NVIDIA ACE plugins to Unreal 5.8 and publish the bundles
- Story: As a new user on the 5.8 line, I want the default install to bring the Audio2Face half with it as it did on 5.7, so that one command still gets me a lip-syncing MetaHuman instead of a failed download.
- Priority: Must
- Status: ready
- Target line: 5.8
- Estimate: L
- Acceptance: Given NVIDIA's ACE source and Unreal 5.8, When NV_ACE_Reference and the Mark, James and Claire character plugins are rebuilt against it, Then each one's `.modules` BuildId equals the running engine's effective compatible changelist and the editor loads them without reporting a different engine version; and Given those builds packaged under the four asset names bootstrap.bat already requests for the 5.8 line — `nv-ace-reference-ue58-full.tar.gz` and `nv-audio2face-mark/james/claire-ue58.tar.gz` — When the release bootstrap points at is listed, Then every one of those names is present on it and a default run with no flags completes with no failed download; and Given the bundles are republished, Then THIRD_PARTY_LICENSES.md carries the upstream licence text of every module and model they ship.
- Tests: FANTASIA/Source/FANTASIATests/Private/BootstrapContractTests.cpp [does not exist yet]
- Source: REQ-001
- Depends on: none
- Provenance: stated
- Confidence: high

### US-021: Refresh the FANTASIAACE prebuilt binaries for Unreal 5.8
- Story: As a Blueprint-only user on 5.8, I want FANTASIAACE to load from the clone the way the core plugin does, so that lip-sync is not the one component I am told was built with a different engine version.
- Priority: Must
- Status: ready
- Target line: 5.8
- Estimate: M
- Acceptance: Given NV_ACE_Reference built for 5.8, When the FANTASIAACE Win64 Development and DebugGame binaries are rebuilt and committed as their own refresh commit, Then every committed FANTASIAACE `.modules` BuildId equals the core plugin's and the editor loads FANTASIAACE without reporting a different engine version; and Given dumpbin is run over the rebuilt DLLs, Then they import no library the plugin does not itself ship or stage.
- Tests: FANTASIA/Source/FANTASIATests/Private/BootstrapContractTests.cpp [does not exist yet]
- Source: REQ-001
- Depends on: US-020
- Provenance: stated
- Confidence: high

### US-022: Migrate the probabilistic stack to aGrUM 3.x
- Story: As a researcher relying on FANTASIA's reasoning components, I want them built on the aGrUM line upstream actually maintains, so that the fixes and the model families arriving there reach me instead of stopping at a version nobody maintains.
- Priority: Must
- Status: ready
- Target line: 5.9
- Estimate: L
- Acceptance: Given a candidate aGrUM 3.x release, When its modules are built on Win64 as static libraries `/MD` without OpenMP, Then the version adopted is the one that built, and its module target names and link order are recorded from that build and not from the changelog; and Given the ported module and tests, When the FANTASIATests suite runs, Then it passes in full, and each of the four behaviour changes that move a pinned result carries a recorded verdict that the new behaviour is the one this plugin wants, with the test rewritten to pin that behaviour deliberately — the LIMID optimal policy now keeping its decision parents, `MEU()`'s variance now including covariances between utilities, `evidenceProbability()` accounting for connected components that carry no target, and the aggregators now working on values rather than parent indices; and Given a build with `WITH_AGRUM_BINDING` defined to 0, When the module is compiled, Then either the binding is genuinely absent or the guard is gone, so no configuration is left in which the define claims to disable a binding it cannot (F-005); and Given the 3.x libraries are shipped or fetched, Then THIRD_PARTY_LICENSES.md carries their upstream licence text (P-16); and Given the port is green, Then C-probabilistic and C-build are rewritten to describe 3.x — the confirmed library targets and link order among them — with both Versions bumped, and the Win64 prebuilt binaries are rebuilt and dumpbin-verified before the change counts as delivered (P-12).
- Tests: FANTASIA/Source/FANTASIATests/Private/BayesianNetworkTests.cpp; FANTASIA/Source/FANTASIATests/Private/InfluenceDiagTests.cpp
- Source: CR-003 (ADR-0015)
- Depends on: none
- Provenance: stated
- Confidence: medium
- Open: the target 3.x version is unsettled upstream and is fixed by the first build, so the estimate is a guess until then. C-probabilistic is also the plugin's public Blueprint surface, so bumping its Version invalidates compiled Blueprint assets in user projects.

### US-023: Reason about dialogue state over time
- Story: As a researcher building a conversational agent, I want to reason over the last k turns as one dynamic Bayesian network, so that the agent can hold a belief about what is going on now without my hand-rolling the time dimension.
- Priority: Must
- Status: draft
- Target line: 5.9
- Estimate: L
- Acceptance: Given a k-order dynamic network defined from Blueprints — variables, the memory order k, intra-slice and inter-slice arcs, and their CPTs — When evidence for a sequence of slices is supplied and inference runs, Then filtered and smoothed posteriors are returned per slice and per variable, and the result is deterministic for the same model and evidence; and Given logged observations and a request to learn the memory order, When learning runs, Then the estimated order is reported, the work happens off the game thread and completion is signalled on it; and Given the component in use, Then it decides nothing about turn-taking or about what the agent says — the application reads the posterior and chooses (NG-1, P-08); and Given a project with this component and no other configured, Then it works alone (NFR-004).
- Tests: FANTASIA/Source/FANTASIATests/Private/DynamicBayesianNetworkTests.cpp [does not exist yet]
- Source: CR-003 (ADR-0015)
- Depends on: US-022
- Provenance: stated
- Confidence: medium
- Open: draft until the component surface is designed — whether it extends C-probabilistic or arrives as its own contract is decided with the migration, and that choice decides whether user Blueprint assets are invalidated a second time.

### US-024: Ask what would have happened instead
- Story: As a researcher, I want to ask what would have followed had the agent acted differently, so that an interaction can be analysed causally and not only by correlation.
- Priority: Could
- Status: draft
- Target line: 5.9
- Estimate: L
- Acceptance: Given a causal model over a network and an intervention, When an impact query runs, Then the identified estimand and the resulting distribution are returned, or the query is refused as unidentifiable with a logged reason rather than answered wrongly; and Given a counterfactual query over an observed case, Then the counterfactual distribution is returned under the same identification discipline; and Given the capability lands, Then it arrives as a component the application drives and not as a policy about what the agent says (NG-1, P-08).
- Tests: FANTASIA/Source/FANTASIATests/Private/CausalModelTests.cpp [does not exist yet]
- Source: CR-003 — a deferred family: wanted, and arriving as its own request
- Depends on: US-022
- Provenance: stated
- Confidence: low
- Open: no module satisfies this yet, deliberately. M-probabilistic's responsibility is discrete probabilistic models and influence diagrams and does not stretch to a causal model; the placement is part of this family's own request.

### US-025: Decide under probabilities that are intervals
- Story: As a researcher whose conditional tables are informed guesses, I want to reason with probability intervals rather than single numbers, so that the agent's uncertainty about its own model is visible instead of invented.
- Priority: Could
- Status: draft
- Target line: 5.9
- Estimate: L
- Acceptance: Given a credal network defined from Blueprints with interval-valued conditional tables, When inference runs, Then lower and upper posterior bounds are returned per variable and the bounds contain the point posterior the equivalent precise network would give; and Given an inconsistent set of intervals, Then the model is rejected with a logged reason rather than silently normalised; and Given the capability lands, Then it arrives as a component the application drives (NG-1, P-08).
- Tests: FANTASIA/Source/FANTASIATests/Private/CredalNetworkTests.cpp [does not exist yet]
- Source: CR-003 — a deferred family: wanted, and arriving as its own request
- Depends on: US-022
- Provenance: stated
- Confidence: low
- Open: no module satisfies this yet, deliberately; the placement is part of this family's own request.

### US-026: Learn the model's shape from logged interactions
- Story: As a researcher with interaction logs, I want the structure of a network estimated from the data rather than authored by hand, so that a study can discover the dependencies it did not already assume.
- Priority: Could
- Status: draft
- Target line: 5.9
- Estimate: L
- Acceptance: Given a labelled sample of observations, When structure learning runs, Then a network — or a partial ancestral graph where the algorithm returns one — is produced and is reproducible from the same sample and parameters; and Given the learned structure, Then it can be loaded into the existing network component and queried; and Given the learning runs, Then it happens off the game thread with completion signalled on it; and Given the capability lands, Then it ships no model weights and hosts nothing: it estimates a graphical model from the project's own data, which is the shape of the SVM training US-015 already ships and not the training, hosting or serving NG-4 denies (the reading is settled, not re-argued).
- Tests: FANTASIA/Source/FANTASIATests/Private/StructureLearningTests.cpp [does not exist yet]
- Source: CR-003 — a deferred family: wanted, and arriving as its own request
- Depends on: US-022
- Provenance: stated
- Confidence: low
- Open: no module satisfies this yet, deliberately. M-probabilistic's responsibility does not stretch to a learner; the placement is part of this family's own request.

### NFR-001: Latency — speech heard to transcript available
- Priority: Must
- Status: draft
- Scenario: Source: a speaker talking to the agent / Stimulus: an utterance ends and voice activity detection closes the segment / Environment: Win64, the engine running at interactive frame rate, Whisper on the configured backend / Artifact: M-asr / Response: the transcript is broadcast and the game thread is never blocked / Measure: unknown — no budget is stated anywhere; to be measured on the reference machine and then fixed
- Provenance: inferred
- Confidence: low

### NFR-002: Latency — text submitted to first audible sample
- Priority: Must
- Status: draft
- Scenario: Source: the application / Stimulus: a synthesis request is issued / Environment: Win64, streaming TTS endpoint reachable / Artifact: M-tts for decode and buffering, M-ace for the paced consumer / Response: playback begins before the full utterance has been synthesized, and ordering across concurrent requests is preserved / Measure: unknown — to be measured and fixed
- Provenance: inferred
- Confidence: low

### NFR-003: Synchronisation — audio to facial animation
- Priority: Should
- Status: draft
- Scenario: Source: the TTS stream / Stimulus: audio samples are handed to Audio2Face / Environment: FANTASIAACE with NV_ACE_Reference and an A2F provider / Artifact: M-ace / Response: blendshapes track the audio closely enough that the face reads as speaking, and the session closes at end of turn / Measure: unknown — to be measured and fixed
- Provenance: inferred
- Confidence: low

### NFR-004: Modifiability — components stay independent
- Priority: Must
- Status: draft
- Scenario: Source: a user building an agent / Stimulus: one component is enabled and used / Environment: a project with FANTASIA enabled / Artifact: every component module / Response: the component works without any other component being configured, and no component's types change when another's do / Measure: a Blueprint using exactly one component compiles and runs; no component module depends on another component module in the dependency view, only on M-core and M-ml
- Provenance: stated
- Confidence: high

### NFR-005: Installability — no compilation required
- Priority: Must
- Status: draft
- Scenario: Source: a new user / Stimulus: clone, bootstrap, enable / Environment: Win64, the engine version named in FANTASIA.uplugin, no C++ toolchain / Artifact: M-dist and the committed binaries / Response: the plugin loads and all components are usable / Measure: zero compilation steps, and the prebuilt .modules BuildId equals the engine's CompatibleChangelist
- Provenance: stated
- Confidence: high

## Dropped

None. No story has ever been dropped. Capabilities absent from this plugin — AWS Polly,
Azure ASR, TTS and NLU, provider-specific OpenAI, Groq and Ollama components, and
in-core Audio2Face — have no story to retire; ADR-0004, ADR-0005 and ADR-0012 say why
they are absent.
