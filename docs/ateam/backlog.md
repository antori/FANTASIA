# Product backlog
- As of: J-0159
- Approved: yes

US-001 to US-019 were recovered at adoption from the FANTASIATests automation suite
(the most reliable statement of current behaviour), the README feature inventory and the
component headers; a story numbered above those comes from a request and names it on its
`Source:` line. A story marked `done` describes what the system does today, bugs
included; `Confidence: medium` means no test pins it yet. A `Tests:` path marked
`[does not exist yet]` is where that story's test belongs — a characterisation test where
the story is `done`, the acceptance test where the work has not been done — and nothing
verifies the story today. Nine such files are still named below, down from twelve: S-01
produced `run-tests.ps1`, `SecretConfigTests.cpp` and `WhisperCaptureComponentTests.cpp`,
the last of which also discharges US-002's path.

A finding's fix arrives as its own story, never by reopening the `done` story the defect
lives in: a story marked `done` says what the system does today, and reopening it would
make that false while the fix is pending. So F-008's fix was US-028 and not a second pass
over US-005 and US-008, which describe behaviour the fix does not change; the finding
keeps naming those two as where the defect sits. The same holds for work no story covers
at all, such as the project's missing mechanical gate, which is US-027.

**A `done` story that acquires an open `blocker` finding leaves `done`, and the fix still
arrives as its own story.** The two rules are separable and both hold. The Definition of
Done requires no open blocker finding, so a story carrying one does not meet it whatever
its description says, and the checker is right to refuse the overload. The story's
description of present behaviour stands and is kept accurate by its `Known defect:` lines;
what changes is only the claim that the project is finished with it. US-014 is the live
case: it is `ready` with `Blocked by: F-020, F-021`, its behaviour is described as it is,
and US-030 carries the fixes. It returns to `done` when both findings close.

**P-19 admits a security fix on the shipped line whatever its surface** (CR-004): a
finding logged `Kind=security` by the security engineer at `blocker` or `major` is
admissible even when its fix touches a contract, with the contract edit limited to the
minimum that describes the new behaviour. Three findings are admitted on the 5.8 line and
no others — F-011, F-020 and F-021, all in M-neo4j, carried by US-030, US-031 and US-032. F-012 is minor
and quality in the same module and stays deferred: the module is not the criterion.
Everything else P-19 refuses, it still refuses, and the admitted item is the fix rather
than the refactor convenient to carry with it (P-07).

F-007's other half stays out for that reason and not for want of will. The Bolt message
layer cannot be linked from the test module at all (F-012), and exporting it widens
C-neo4j's surface; US-029 characterises the microphone capture path instead, which needs
nothing exported. What is exported, the PackStream codec, has 36 tests, and US-030 adds
the bounds-check tests F-021 needs.

A story may carry a `Target line:` field naming the engine line it is for — the line whose
branch the work lands on. A story without one targets the shipped line, which is the
engine version the brief names (5.8 today). Work for a line that has not shipped yet is
logged here with that line named, so that it has somewhere to live before the line exists
instead of waiting unlogged. P-19 decides what the shipped line admits and sends
everything larger to the next line, so a story's target line and its size are the same
question asked twice — except for an admitted security fix, whose size does not decide it.

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
- Tests: FANTASIA/Source/FANTASIATests/Private/WhisperCaptureComponentTests.cpp — discharged by US-029
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
- Status: ready
- Estimate: L
- Acceptance: Given a Bolt endpoint, When the transport is Plain TCP (the default) or WebSocket, with or without TLS, Then handshake, authentication and query exchange succeed against both Aura and a local install; and Given a result set, Then each row exposes nodes, relationships and simple values as distinct cell types; and Given any PackStream value, Then encoding and decoding round-trips it exactly.
- Blocked by: F-020, F-021 — two open blocker findings against this story's subject, which is why the status is `ready` rather than `done`. The story's acceptance criteria describe behaviour the system has today, but the Definition of Done requires no open blocker finding, so this story does not meet it. It returns to `done` when both close; the fixes arrive as US-030 and US-031, not here.
- Known defect: F-020 — the TLS path sets no verify mode, loads no trust store and checks no hostname, so a `bolt+s://` or `neo4j+s://` connection is encrypted but unauthenticated and an on-path attacker reads the LOGON credentials and every row. F-021 — the PackStream reader takes string, byte, list and map lengths off the wire without bounding them against the buffer, so a hostile or faulty server drives an out-of-range read. F-011 — `BeginPlay` overwrites `UseSSL` from the endpoint scheme, so `UseSSL = true` against a `bolt://` endpoint connects in plaintext silently. All three are admitted to the 5.8 line by P-19's security carve-out (CR-004) and carried by US-030 (F-021), US-031 (F-020) and US-032 (F-011), in that order.
- Known defect: F-012 — nothing in `BoltProtocol.h` carries `FANTASIA_API`, so handshake, HELLO, LOGON, RUN, PULL and the dechunker cannot be linked from the test module. Minor and quality, so the carve-out does not admit it; it waits for 5.9.
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

### US-027: Decide that a task is done with one command
- Story: As the maintainer, I want one command that compiles the plugin and runs its automation suite against the installed engine and then says pass or fail, so that "done" is a mechanical fact instead of a claim.
- Priority: Must
- Status: done
- Estimate: M
- Acceptance: Given no engine can be resolved — `FANTASIA_UE_ROOT` unset and the documented default path absent, or an engine whose `Build.version` major and minor differ from the version both `.uplugin` descriptors name — When the script runs, Then it exits non-zero naming the variable it read, the path it tried and the version it found, and compiles nothing; and Given a resolved engine, When the script runs with no arguments, Then it prepares a harness project outside this repository's working tree that reaches the plugin as an ordinary project plugin, by a directory junction at `<harness>/Plugins/FANTASIA` naming this repository's `FANTASIA` directory alone, discards UnrealBuildTool's cached `Makefile.bin` for the harness target, compiles the `FANTASIA` and `FANTASIATests` modules for that project's editor target, boots `UnrealEditor-Cmd.exe` on it unattended with no RHI, splash or sound, runs `Automation RunTests FANTASIA`, and exits zero only when the exported report names at least 109 tests and zero failures; and Given the compile fails, or the editor writes no report, or the report names zero tests, Then the script exits non-zero and says which of the three happened, so a run that verified nothing never exits zero; and Given one test is made to fail deliberately, When the script runs, Then it exits non-zero and that test's name appears in its output; and Given a completed editor run, When the log's `InternalLoadLibrary` line for `FANTASIATests` names any directory other than the one this run compiled into, Then the script exits non-zero, because a module loaded from elsewhere is a suite run against code this run did not build; and Given a compile that succeeded, When the emitted `.modules` BuildId differs from the engine's `CompatibleChangelist`, Then the script exits non-zero (C-build's last invariant, and the error users actually hit); and Given any completed run, Then the script prints its elapsed wall-clock seconds, and a cold run and a warm run with no source change are both measured and recorded in the journal; and Given those two numbers, Then the command proposed to the lead for `gates.task_completed` is the whole script when the warm number is at most 240 seconds, or the script restricted to a test prefix when it is more, with the full-suite form proposed as a quality gate instead — so the sprint ends with a gate either way; and Given a run recompiled the tracked binaries under `FANTASIA/Binaries/Win64/`, Then the script says so before it exits, because P-12 governs those files.
- Tests: FANTASIA/run-tests.ps1
- Open: the gate is its own executable criterion. The refusal; the pass; the three ways of verifying nothing; the deliberate failure; and the wrong-module-loaded guard are each exercised once and recorded in the journal entries that report the measurement (DEC-007 to DEC-010, J-0086 to J-0089).
- Source: brief.md §Open questions (the project's one mechanical gate is missing); P-04; ADR-0016
- Depends on: none
- Provenance: stated
- Confidence: medium
- Open: FANTASIAACE is outside this gate's reach. Its 5.8 binaries do not exist (F-016), its NVIDIA dependency is not built for 5.8 (F-014), and it has no test module at all, so the junction names the `FANTASIA` directory alone and the gate says so rather than appearing to cover it. It comes into reach with US-020 and US-021.
- Open: the mechanism clause of the second acceptance criterion was rewritten after the story was accepted. It named `AdditionalPluginDirectories`, which measurement showed makes the gate report PASSED on a deliberately failing test; the junction replaces it, and ADR-0017 records why (CR-005). The story's intent is unchanged, and the user accepted the story with the edited criterion as its present text (J-0147).
- Open: nothing. Every criterion is met, the gate is green at 150 tests and 0 failures, and the user accepted the story at review (J-0147), which was the one Definition of Done clause the story could not satisfy by itself.
- Open: a small follow-up, not a defect in what landed — a 24-way compile can exhaust the machine's commit limit and fail with MSVC C3859 and C1076, which the gate reports as an honest compile failure but does not name as an environment cause (J-0094).

### US-028: Keep the API key out of the project's assets
- Story: As a researcher who commits their Unreal project to a repository, I want every key and header I configure to be read from the project's DefaultGame.ini, so that using a FANTASIA component cannot put my API key into a committed asset.
- Priority: Must
- Status: done
- Target line: 5.8
- Estimate: S
- Acceptance: Given the plugin's classes as compiled, When each secret-bearing property is read through reflection, Then all six carry the `Config` flag — `UOpenAICompatibleComponent::Config` and `URESTTTSComponent::Headers`, which do not today, beside `UElevenLabsTTSComponent::Key`, `UNeo4jComponent::password`, `URESTTTSComponent::URL` and `URESTTTSComponent::BodyTemplate`, which do — and the test fails if any one of them ever loses it again; and Given a configuration entry for either fixed class holding a key, When an instance reloads its configuration, Then the configured value is present on the instance, which is what proves a `Config` specifier on a struct-typed property actually serialises (the reason the specifiers on the struct's inner fields never fired); and Given either fixed property's declaration, Then its doc comment — which is the tooltip the user reads in the editor — states that the value is read from the project's `DefaultGame.ini` and that a project which already typed a key into an asset must re-save that asset for the key to leave it, so the fix is not reported as complete on the strength of the specifier alone; and Given the fix, Then both properties keep their name, type, category and `EditAnywhere, BlueprintReadWrite` specifiers, so no compiled Blueprint asset in a user's project is invalidated and the change stays inside what P-19 admits on the 5.8 line.
- Tests: FANTASIA/Source/FANTASIATests/Private/SecretConfigTests.cpp
- Source: F-008; CR-001 (the user admitted this fix to the 5.8 line explicitly)
- Depends on: none
- Provenance: stated
- Confidence: high
- Open: F-031 is the residual this story does not close, and it is the honest form of the story's own sentence. `Config` adds a load-time source; it does not make a property transient, and both fixed properties stay `EditAnywhere, BlueprintReadWrite`, so a key typed into the Details panel is still serialised into the Blueprint or level asset (pinned by `FANTASIA.Quality.Secrets.Asset.TypedKeyIsSerialized`). What holds is the ini route: with the key in `DefaultGame.ini` the instance inherits it from the CDO and nothing is serialised. Closing the residual means removing `EditAnywhere` or making the value transient, which changes the Blueprint surface (P-03, P-19), so only the user can accept it.
- Open: F-029 was the same defect a third time, in `AFANTASIATestActor`, and was fixed in this sprint's tail (J-0093); its own residual is F-031's. core-dev's view, which the lead shares, is that the real fix there is to drop the actor's three secret properties and exercise the components' configured values instead. That is a Blueprint-surface change and needs a change request.
- Open: F-032 and F-033 are minor quality findings against this story's work and are not closed by it.
- Open: both acceptance criteria are met — nine secret-bearing properties carry `Config`, the ini round-trip is pinned, and both tooltips state the residual rather than over-claiming — and the user accepted the story at review with F-031 named as its residual (J-0147).
- Open: F-011 is the other half of this story's subject and is **not** in it. P-19's security carve-out (CR-004) now admits it to the 5.8 line, and it is carried by US-032 rather than here.

### US-029: Pin the microphone capture path
- Story: As the maintainer, I want the live capture path's present behaviour pinned by tests, so that a change which degrades the real-time audio front end fails a test instead of being noticed in a session months later.
- Priority: Should
- Status: done
- Target line: 5.8
- Estimate: M
- Acceptance: Given a `UWhisperCaptureComponent` constructed with no audio device and no model, When its configuration is read, Then every `Config` property's default is asserted as the header declares it, C-asr's two stated invariants among them: `AudioDeviceIndex` is -1, and `bEnableHighPassFilter` and `bEnableNoiseGate` are both true; and Given no capture is running, When `SetMuted` and `ClearBuffer` are driven in sequence, Then `IsMuted()` reports what was set and `GetBufferDuration()` is zero; and Given a capture session is running — `IsCapturing()` true, with audio already buffered — When `StartCapture` is called a second time, Then it returns false while `IsCapturing()` stays true, it warns exactly once that capture is already running, and it leaves the running session undisturbed, the buffered audio unchanged in duration, because the guard returns before the session counter is incremented and before the per-session state is reset; this is the only one of `StartCapture`'s three false paths a test can drive without hardware, and the two that need it — a device that will not open and a stream that will not start — are US-033's and not this story's (F-018); and Given synthetic interleaved audio at a rate other than 16 kHz, When the resample-and-mix step runs, Then the output is mono and its frame count is the input count scaled by 16000 over the source rate to within one frame, and audio already 16 kHz mono passes through sample for sample; and Given a buffer of known samples, When the RMS, high-pass and noise-gate steps run, Then the RMS equals the arithmetic root mean square to within 1e-4, a buffer below the gate threshold is attenuated while one above it is not, and the high-pass coefficients for a given cutoff are pinned at the values the implementation produces today; and Given a value that looks wrong, Then it is pinned as it is and reported as a finding, because these are characterisation tests and changing behaviour here is not in this story; and Given the tests reach private members, Then the only edit to `WhisperCaptureComponent.h` is one `friend` declaration naming the test harness type, so no exported surface, no contract and no Blueprint-facing signature changes (C-asr unchanged, P-19 satisfied); and Given the whole file runs, Then it opens no audio device, loads no model, makes no network call and finishes in under a second.
- Tests: FANTASIA/Source/FANTASIATests/Private/WhisperCaptureComponentTests.cpp
- Open: this is the file US-002 has owed since adoption; it now holds 25 tests and discharges that path.
- Source: F-007 (the larger of the two worst gaps); US-002, whose `Tests:` path this discharges
- Depends on: none
- Provenance: stated
- Confidence: medium
- Open: the acceptance criterion about `StartCapture` states the already-capturing guard, which is the only one of `StartCapture`'s three false paths (`WhisperCaptureComponent.cpp:531`, `:566`, `:581`) a test can drive without hardware; the two device-failure paths are US-033's. Both halves of the criterion say the guard, not only its *When*, and CR-009 carries the reasoning: the guard's entry condition is `bIsCapturing == true`, so "returns false with `IsCapturing()` still false" states the opposite of what the one reachable path leaves behind, while `OpenCaptureDevice` treats any `AudioDeviceIndex` outside the enumerated range — negative or too large — as the system default and opens it, so no value of the property reaches the path the criterion named (F-018, DEC-005). The phrase "with an unopenable device" is deleted rather than reinterpreted, so no later reader takes that path for covered.
- Open: the criterion names only what `FANTASIA.WhisperCapture.Controls.StartCaptureWhileCapturing` asserts today — the return value, `IsCapturing()`, one warning containing "already capturing", and the buffer still 0.5 s — so the story adds no test. The guard also leaves `CaptureSessionId` and the resampler phase untouched, since it returns before `++CaptureSessionId` and before the reset at `:538-559`; nothing asserts that, because `FWhisperCaptureTestAccess` exposes the phase but not the session counter, and the test appends through the resampler's fast path, where the phase is zero on both sides. It is an invariant a refactor could plausibly break, so it is recorded here rather than promised in a criterion no test keeps (DEC-016).
- Open: the story's other criteria stand as written, among them the one limiting the edit to `WhisperCaptureComponent.h` to a single `friend` declaration, which is why the seam cannot live in this story.
- Open: the work landed and no criterion is outstanding — 25 tests across defaults, controls, resampling, RMS, high-pass and noise gate, green in the gate. The characterisation found three defects and pinned them as they are, which is what it was for: F-017 (`ResampleAndMixToMono` reads one frame past its input when the carried phase lands within one float ulp below the frame count; one wrong sample per callback at 4410-frame callbacks, and undefined behaviour), F-019 (the header calls the high-pass Direct Form II Transposed and the code implements Direct Form I; `ResampleAndMixToMono` is named and documented as mixing to mono but selects the loudest channel per tap; `ComputeRMS` accumulates in float and drifts about 1.3e-4 over 30 s, above the 1e-4 this story allows for short buffers). All three are M-asr's to fix, and each fix updates the test that pins the current value.
- Open: the other half of F-007, the Bolt message layer, is not here and cannot be. Nothing in `BoltProtocol.h` carries `FANTASIA_API`, so handshake, HELLO, LOGON, RUN, PULL and the dechunker cannot be linked from the test module (F-012); exporting them widens C-neo4j's surface, which P-19 still keeps off the 5.8 line because F-012 is minor and quality rather than security. What is exported — the PackStream codec — has 36 tests, and US-030 adds the bounds-check tests F-021 needs.

### US-030: Refuse a Bolt message that does not fit its own buffer
- Story: As a researcher whose agent reads from a Neo4j server, I want a malformed or hostile Bolt message to be rejected rather than decoded, so that a server on the other end of the socket cannot read my process's memory or stop it.
- Priority: Must
- Status: ready
- Target line: 5.8
- Estimate: M
- Acceptance: Given a `FBoltPackStreamReader` over a buffer, When `ReadValue` would read a byte at or past `Data.Num()`, Then it stops and yields a null value and the reader reports itself exhausted, rather than indexing past the end — asserted for `ReadByte` on an empty buffer, where `Data.GetData()` is null, and for each of the 16-, 32- and 64-bit readers truncated mid-value; and Given a string, bytes, list or map marker whose length or count read from the message exceeds the bytes remaining, When the value is read, Then it is rejected and no allocation of that size is attempted — in particular `{0xD2, 0x7F, 0xFF, 0xFF, 0xFF}` yields a null value and reserves nothing, and a negative 32-bit count reaches neither `Reserve` nor `Append`; and Given a message of N repeated `0x91` bytes, which is N nested single-element lists, When it is read, Then `ReadValue` stops at a stated depth limit and yields a null value instead of recursing once per byte; and Given a server that sends chunks and never sends the zero-length terminator, When `FBoltDechunker::ProcessBuffer` accumulates them, Then accumulation stops at a stated maximum message size and the message is discarded with a logged reason; and Given each of the four refusals, Then the reason is logged once and the component does not crash, and `IncomingResponse` either does not fire or fires with no rows, which is what C-neo4j already says of a FAILURE; and Given the 36 existing codec tests, When the bounds checks are in place, Then all 36 still pass unchanged, because every one of them feeds the reader bytes the writer produced and none of them is near a limit.
- Tests: FANTASIA/Source/FANTASIATests/Private/Security/BoltPackStreamBoundsTests.cpp
- Source: F-021 (blocker, security); admitted to the 5.8 line by P-19's security carve-out (CR-004)
- Depends on: none
- Provenance: stated
- Confidence: high
- Open: this is first of the three admitted Bolt fixes, on the security engineer's recommended order, because it is the only one whose evidence needs no new infrastructure: the exported half of the codec is already linkable from the test module and already has 36 tests. It is also the one fix with no user-visible behaviour change on a well-behaved server, so it can land without a decision about what users are allowed to configure.
- Open: C-neo4j's error clause for `ReadValue` currently describes the unbounded reader as it is (Version 2). This story rewrites that clause, which is the minimum edit CR-004 permits and no more; the four refusals above are the new behaviour and nothing else in the contract moves.
- Open: the depth limit and the maximum message size are numbers this story must choose and state in the contract rather than leave to the implementation. Bolt's own chunk size is 16 KB and messages are assembled across chunks, so the message cap is the one that needs justifying against real result sets — a large `PULL` response is a legitimately big message.

### US-031: Authenticate the Neo4j server, not just encrypt the connection
- Story: As a researcher connecting to Neo4j over TLS, I want the server's certificate verified against a trust store and its hostname checked, so that `bolt+s://` means what Neo4j's documentation says it means and an attacker who can answer for the host cannot read my password.
- Priority: Must
- Status: ready
- Target line: 5.8
- Estimate: S
- Acceptance: Given a `bolt+s://` or `neo4j+s://` endpoint, When the TLS connection is built, Then the context loads a trust store, verification is set to peer verification, the endpoint's hostname is set for certificate matching, and the handshake's verification result is inspected — so a certificate that does not chain to the trust store, or whose names do not match the endpoint, fails; and Given such a failure, Then the component does not connect, does not send `LOGON`, and logs which check failed, because the first thing on the session is the user and password in cleartext inside the tunnel; and Given a `bolt+ssc://` or `neo4j+ssc://` endpoint, Then a self-signed certificate is accepted, which is what the `+ssc` schemes mean and is the documented way to opt out; and Given the four TLS schemes, Then which of them verifies is stated in C-neo4j beside the existing invariant that the scheme decides TLS, because today all four behave as the weakest; and Given the WebSocket transport with TLS, Then it verifies on the same rule as the plain-TCP path or the difference is stated, so that the two transports do not make different promises; and Given a test, When the TLS branch is built for a `+s` endpoint, Then verification being enabled is asserted rather than inferred from the absence of a failure.
- Tests: FANTASIA/Source/FANTASIATests/Private/Security/Neo4jTlsVerificationTests.cpp
- Source: F-020 (blocker, security); admitted to the 5.8 line by P-19's security carve-out (CR-004)
- Depends on: US-030
- Provenance: stated
- Confidence: medium
- Open: **this story breaks a connection that works today**, and that is the point of it: a user pointed at a server with a self-signed certificate on a `bolt+s://` URL connects now and will not after. The migration is one character — `bolt+ssc://` — and the story must say so in the log message and in the contract, or the first report will be "the update broke my database". The user should confirm that trade at review; it is the reason this is a separate story from US-030.
- Open: which trust store is a decision this story takes and records. The engine ships a certificate bundle and OpenSSL has `SSL_CTX_set_default_verify_paths`, which on Windows does not read the system store; the two give different answers on a machine with a corporate root, so the choice needs stating rather than defaulting.
- Open: F-020 and F-011 overlap in one place. This story decides what a verified connection means; US-032 decides when a connection is encrypted at all. Neither subsumes the other and both touch `Neo4jComponent.cpp`, which is why they are sequenced rather than parallel.

### US-032: Stop silently downgrading a connection the user asked to encrypt
- Story: As a researcher who set `UseSSL`, I want the plugin to honour that or refuse, so that I am never told the connection is encrypted while my Neo4j password crosses the network in the clear.
- Priority: Must
- Status: draft
- Target line: 5.8
- Estimate: S
- Acceptance: Given an endpoint whose scheme implies no TLS and `UseSSL` set true, When the component starts, Then the configured intent is not silently discarded — the shape of "not silently" is the decision this story needs and the three candidates are below; and Given whichever shape is chosen, Then C-neo4j's configuration clause stops describing `UseSSL` as a property `BeginPlay` overwrites from the scheme, and states the new rule instead; and Given the fix, Then an endpoint whose scheme does imply TLS behaves as it does today, so no working configuration changes meaning.
- Tests: FANTASIA/Source/FANTASIATests/Private/Security/Neo4jTransportSelectionTests.cpp
- Source: F-011 (major, security); admitted to the 5.8 line by P-19's security carve-out (CR-004)
- Depends on: US-031
- Provenance: stated
- Confidence: low
- Open: `draft` and not `ready`, because the fix has three shapes and the choice is the user's, not the planner's. (A) **Remove `UseSSL`** and let the scheme decide alone, which is what every official Neo4j driver does and what C-neo4j's invariant already says; it is the cleanest rule and it deletes a `Config` UPROPERTY, which is a Blueprint-surface change (P-03) invalidating any user asset that set it. (B) **Refuse to connect** when `UseSSL` is true and the scheme says plaintext, naming both in the error; no surface change, and it turns a silent downgrade into a loud failure for anyone currently relying on the override. (C) **Warn and continue**, which costs nothing and leaves the password in the clear for a user who does not read the log — the smallest fix and the one that least resembles a fix.
- Open: the planner recommends (A) with (B) as the fallback if the Blueprint-surface cost is judged too high. The carve-out admits the surface change, so (A) is now available where it was not before CR-004; whether it is *wanted* is a separate question, because deleting a property is permanent for the line's users in a way that refusing a bad combination is not.
- Open: this story is last of the three on the security engineer's recommended order, and it is the one whose cost is already written into the project's own history: P-19's rationale cited F-011 for a sprint as the price the bar deliberately paid. CR-004 stopped paying it; this story is what collecting looks like.

### US-033: Pin what the capture path does when the microphone will not open
- Story: As a researcher whose microphone is absent or held by another application, I want the plugin's refusal to start capture pinned by tests, so that a change which turns that refusal into a crash, a leaked device or a component that believes it is capturing fails a test instead of reaching a session.
- Priority: Should
- Status: ready
- Target line: 5.8
- Estimate: S
- Acceptance: Given the component reaches the capture device through a private indirection that `FWhisperCaptureTestAccess` can replace, When a test supplies an opener that reports failure, Then `StartCapture` returns false, `IsCapturing()` is false, the component's tick is not enabled, its buffer is empty and no audio device is opened — which pins the `!OpenCaptureDevice()` path at `WhisperCaptureComponent.cpp:566`; and Given an opener that reports success without touching hardware, When a test supplies a stream starter that reports failure, Then `StartCapture` returns false, `IsCapturing()` is false, the tick is disabled, the error is logged exactly once, the component holds no capture object, and `CloseCaptureDevice` has run — `DeviceSampleRate` and `DeviceNumChannels` are both zero and the resampler phase is zero — which pins `:581`'s cleanup rather than leaving it read from the code; and Given a value either test finds surprising — the buffered audio a refused start discards, the session counter it advances before it can fail — Then it is pinned as it is and reported as a finding, because changing capture behaviour is not in this story; and Given the seam, Then nothing it adds carries `FANTASIA_API`, no `UPROPERTY`, `UFUNCTION`, delegate or enum declaration changes, and the hooks are private and settable only through the `friend struct FWhisperCaptureTestAccess` the header already declares, so C-asr's interface is unchanged, no compiled Blueprint asset is invalidated and P-19 admits the change on the 5.8 line; and Given a capture session that runs, When audio callbacks arrive, Then no indirection this story adds is consulted per callback or per sample — each is consulted once per `StartCapture` — so NFR-001's path is unchanged, which the diff decides: nothing inside the capture callback or `ResampleAndMixToMono` is edited; and Given the whole test file runs, Then the 25 tests US-029 landed pass unchanged, the file still opens no audio device, loads no model and makes no network call, and it finishes in under a second.
- Tests: FANTASIA/Source/FANTASIATests/Private/WhisperCaptureComponentTests.cpp
- Source: F-018, which closes when this lands; CR-009; US-029, whose device-failure criterion this carries
- Depends on: none
- Provenance: stated
- Confidence: high
- Open: not in S-01, and the approval that created this story schedules no work. Two tasks when it is scheduled, the seam first: `speech-dev` for the indirection in M-asr, `test-dev` for the two cases in the file US-029 landed.
- Open: two hooks are the minimum the criteria need, and the shape beyond that is M-asr's to choose. The two failures sit on different objects: the device-open failure is a call to the component's own `OpenCaptureDevice`, while the stream-start failure is a call on `AudioCapture`, a `TUniquePtr<Audio::FAudioCapture>` whose type is concrete and non-virtual, so faking the object is a far larger change than indirecting the one call. `CloseCaptureDevice` already tolerates a null capture object, so a test can exercise `:581` without one.
- Open: the accessors the tests need — `DeviceSampleRate`, `DeviceNumChannels`, the session counter — are forwarding-only additions to `FWhisperCaptureTestAccess`, where the suite already reaches this component's private state. The session counter also makes US-029's guard invariant assertable, which is a free addition and not a criterion here.
- Open: C-asr gains the two tests as evidence for its `StartCapture` error clause when this lands, and that clause's distinction stands: the seam is reachable only from the test module, so no value of `AudioDeviceIndex` provokes the device-open failure on a shipped build. F-039 is the neighbouring correction to the same invariant's citation and is independent of this story.

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
