# Threat model
- As of: J-0211
- Owner: security
- Scope: design version 3; all twelve modules (M-core, M-asr, M-tts, M-llm, M-prolog, M-probabilistic, M-ml, M-neo4j, M-ace, M-demo, M-tests, M-dist); no services — FANTASIA is a library loaded into someone else's process

## How to read this

FANTASIA is not a service. It is two Unreal plugins that load into a host project's editor
or game process and run with that process's full privileges: the filesystem, the network,
the user's microphone, and the ability to start other programs through the embedded Prolog
engine. There is no server to defend and no caller to authenticate. The attack surface is
the set of places where the plugin takes in bytes it did not write — a network response, a
file, a configuration value, a string from a model — and the set of places where a string it
is handed becomes something other than data.

Two consequences shape everything below.

**The host project is the privileged caller.** Every Blueprint-facing function in the
plugin is reachable by anything in the same process, and the plugin makes no access
decisions: there is no authentication, no authorisation and no privilege separation
anywhere in it, by design. "Deny by default" has no referent here. What replaces it is the
duty to say, in the contracts, which calls are dangerous with untrusted input — and that
duty is where the project is weakest (F-024).

**There is almost no mechanical verification of any of this.** The project has no CI (an
accepted risk in architecture.md). A gate exists since US-027 — `FANTASIA/run-tests.ps1`,
168 tests, 0 failures — and a maintainer runs it by hand, so a security test placed in the
suite is checked by something. Eighteen of the 168 are security tests, all in
`FANTASIA/Source/FANTASIATests/Private/Security/BoltPackStreamBoundsTests.cpp` and all
against F-021's threat; every other "no test" below is a case owed. Where a row below says a threat is
mitigated, the mitigation is in the code and I have read it; where it says a test exists, I
have read the test.

## Assets

- **The host project's API keys** — an ElevenLabs `xi-api-key`, an OpenAI-compatible
  `APIKey`. They buy model and synthesis time, so they are money. They belong to the
  integrator, reach the plugin as `Config` UPROPERTYs from the host project's
  `DefaultGame.ini` (ADR-0001), and must stay out of source, out of committed assets and
  out of logs (P-15). The plugin declares ten properties that hold or carry a secret; seven
  read from configuration as ADR-0001 says, and the three on `AFANTASIATestActor` are stored
  in the owning asset's defaults instead (F-029).
- **The Neo4j credentials** — `user` and `password` on `UNeo4jComponent`, and through them
  the whole graph database. Sent on every connection in the Bolt LOGON message.
- **The end user's voice and the transcript of it** — captured continuously by
  `UWhisperCaptureComponent` when an application starts capture. Personal data. It is
  recognised on the machine and M-asr sends it nowhere; where it goes afterwards is the
  host project's choice.
- **The conversation** — prompts, the user's turns, and the model's replies. Leaves the
  machine to whichever endpoint the integrator configures.
- **The knowledge base** — the Prolog clause database, and the Bayesian networks and
  influence diagrams built or loaded at runtime. Shared process-wide for Prolog (F-028).
- **The user's machine and the loaded process** — the plugin can read and write arbitrary
  paths, load a DLL, and execute arbitrary goals in an embedded Prolog engine. This is the
  asset an attacker actually wants.
- **The shipped artifacts** — the committed prebuilt DLLs and the ~5 GB of release assets
  bootstrap fetches. Their integrity is what stands between one compromised download and
  every user of the plugin.
- **The user's project tree** — bootstrap writes into it, including into directories the
  editor later loads binaries from.

## Actors

- **The integrator** (the game or application developer): trusted. Writes the Blueprints,
  sets the configuration, decides what text reaches which component. Everything the plugin
  does, it does on this actor's behalf and with no check.
- **The end user / player**: untrusted as a source of data. Supplies microphone audio,
  typed text, and sometimes files. Not expected to be hostile in a single-player game; is
  the hostile party in any application that accepts input from someone who is not the
  machine's owner.
- **The language model or agent graph**: its output is untrusted input to everything it
  drives. A conversational agent's whole point is that attacker-influenceable text flows
  through it, so model output is treated here as if an attacker wrote it.
- **Authors of anything the model reads** (documents, retrieved graph rows, other players'
  messages): untrusted, and the source of prompt injection. The plugin has no retrieval of
  its own; this actor enters through the integrator's pipeline and through Neo4j rows.
- **Remote services** — ElevenLabs, any OpenAI-compatible endpoint, a LangGraph server:
  trusted to be *reachable*, not trusted for the content or the shape of what they return.
  Their responses are parsed by first-party code (F-023, F-027).
- **The Neo4j server**: the same, and authenticated on the plain TCP transport — a `+s`
  scheme requires a certificate that chains to the engine's trust store and carries the
  host's name, so the server is the one the endpoint names, not whoever answers for it. On
  the WebSocket transport the plugin's own decision does not govern the connection (F-055),
  and under `+ssc` the server is unauthenticated by request. Its bytes reach a parser
  that bounds every read, count and nesting depth against the buffer and refuses what does
  not fit, so what it can still do with them is stop the connection decoding rather than
  read the process's memory (F-048).
- **The release host and the network to it** (GitHub): trusted only to the extent that TLS
  to `github.com` is trusted. The archives themselves are unauthenticated (F-026).
- **The maintainer**: trusted, and the only publisher of binaries. A single maintainer with
  no CI is the project's stated operating condition, and it means the distribution boundary
  rests entirely on one machine.
- **Other code in the same process** — other plugins, other Blueprints, the editor itself:
  the same trust domain. The plugin neither defends against it nor isolates its own
  instances from each other (F-028).

## Trust boundaries

- **TB-1 player's microphone and audio files -> M-asr.** Contract: C-asr. Audio is
  recognised locally and no network call is made, ever — the one boundary in the project
  where data arrives and nothing leaves. Crossing is mediated by `FWhisperResult`. Mitigated
  in part: the live-capture path takes no attacker-chosen lengths; the file path trusts every
  header field it reads (F-022).
- **TB-2 untrusted text (a model's reply, a user's utterance, a document) -> the plugin's
  string sinks.** No contract mediates this, because the plugin does not create the flow:
  the integrator does. The sinks are `SWIPLqueryString`, `SWIPLfindAll`,
  `SWIPLconsultString` and `openPrologFile` (Prolog code and arbitrary processes), the
  Cypher string in `submitQuery`, a TTS `BodyTemplate`, a LangGraph `threadID` spliced into
  a URL path, and every caller-supplied filesystem path. Unmitigated: nothing validates,
  escapes or confines at any of these, and only C-rest-tts's `{text}` substitution escapes
  its input at all. The contracts do not warn (F-024).
- **TB-3 the plugin -> remote HTTP services.** Contracts: C-rest-tts,
  C-elevenlabs-protocol, C-llm. The API key crosses outbound in a header; the conversation
  and the text to synthesize cross outbound in a body; the reply crosses inbound. Partly
  mitigated: requests are built with the JSON serialiser, not string concatenation, and
  Unreal's HTTP module verifies TLS (the plugin sets no `bVerifyPeer` override anywhere).
  Unmitigated: no URL in the plugin is required to be `https`, so a configured or
  mistyped `http://` endpoint sends the key in the clear, and three of the five LLM presets
  default to `http://localhost`.
- **TB-4 the Neo4j server <-> M-neo4j.** Contract: C-neo4j. Credentials cross outbound;
  PackStream bytes cross inbound. Mitigated outbound on the plain TCP transport: a `+s`
  scheme verifies the chain against the engine's bundle and the host's name against the
  certificate, and refuses the connection before a credential is written. Unmitigated
  outbound: on the WebSocket transport the connection is governed by the URL string rather
  than by that decision (F-055), and TLS is chosen by the endpoint scheme, silently
  overriding the property (F-011).
  Mitigated inbound: every length, count and nesting depth in a PackStream value is bounded
  against the buffer before use and a message is capped at 16 MiB, so the worst a hostile
  server reaches is a connection that decodes nothing (F-048).
- **TB-5 the host project's configuration -> the components.** Contract: ADR-0001 and the
  `Config` specifier on the owning property **and** `Config = Game` on the class, which
  together are the whole mechanism: `LoadConfig` walks a class's own `Config` properties and
  descends into nothing else. Mitigated for the seven secret-bearing properties on the
  component classes, all of which carry both. Not mitigated for `AFANTASIATestActor`'s three
  credential fields, which carry neither — the actor is not a `Config = Game` class, so
  marking its properties would not be enough on its own (F-029). For those three the secret
  lands in a committed asset instead, which means the boundary runs the wrong way: the value
  enters from the user's repository rather than from their configuration. F-008 remains open
  over the two component properties it named, because a specifier does not remove a key an
  asset already holds: the asset must be cleared and re-saved, which is the user's action,
  not the plugin's.
- **TB-6 GitHub release assets -> the user's project tree.** Contract: C-install.
  Unmitigated: no checksum, no signature, and one destination is the directory M-asr loads
  a DLL from (F-026).
- **TB-7 the maintainer's machine -> every user, through the committed prebuilt binaries.**
  Contract: C-build, and P-12's rebuild-and-dumpbin discipline. Partly mitigated: the
  dumpbin import check (P-12) is a real integrity check, and it is run by hand. The
  binaries are the product for users who never compile (NFR-005), so this boundary carries
  the most trust of any in the project and has the least machinery behind it.
- **TB-8 the process boundary — and the absence of one inside it.** The plugin has the
  host process's privileges and no sandbox. The one place the design implies isolation
  between instances is `USWIPrologComponent`, and there is none (F-028).

## Surface by module

### M-core
- Entry points: none at runtime. `FFANTASIAModule::StartupModule` logs two URLs; the rest
  of the module is reflected data types.
- Becomes code or commands: none.
- Deserialises: nothing itself; its `USTRUCT`s are deserialised by the engine as part of
  asset and config loading.
- Secrets: none declared here, but it is the header the secret-bearing components include,
  and a struct it defined would inherit the `Config`-on-the-owning-property trap F-008
  describes.
- Decides access: nothing.
- Threats: *tampering* with a user's compiled Blueprints through an ABI change — mitigated
  by P-03 and the reflected-type invariants in C-fantasia-types, pinned for defaults by
  `FANTASIATypesTests.cpp`; *information disclosure* — none, the module holds no state
  beyond the module object. `paramObject.h` and the two gRPC guard headers are reachable
  from nothing (F-006), so they are surface for no benefit, like F-002's link surface.

### M-asr
- Entry points: `UWhisperSubsystem::LoadModel`, `LoadModelBySize`, `LoadModelByName`,
  `TranscribeAudioFile`, `TranscribeAudioData`, `LoadWavFile`;
  `UWhisperCaptureComponent::StartCapture` and the capture configuration;
  `UWhisperBlueprintLibrary`'s pure helpers.
- Becomes code or commands: one dynamic library load —
  `FPlatformProcess::GetDllHandle` on `fantasia_whisper_cuda.dll`, with the path built from
  `IPluginManager`'s base directory and a fixed name (`WhisperBackend.cpp`), so there is no
  search-order hijack. The DLL itself arrives unauthenticated (F-026).
- Deserialises: WAV files (`LoadWavFile`), and the GGML model file, which is parsed by
  vendored whisper.cpp/ggml C code. Microphone frames from the engine's audio capture.
- Secrets: none. This module is the project's answer to not sending audio to a cloud
  (G-4), and it makes no network call — verified: no `Http`, `FSocket` or `WebSocket`
  reference exists in any M-asr file.
- Decides access: nothing. It does read the user's microphone, which is a privilege the
  host application is responsible for asking about.
- Threats:
  - *Denial of service and information disclosure through a crafted WAV file*:
    **unmitigated** — the declared data-chunk size is used as a length with no clamp (heap
    over-read up to 2 GB, read into the transcript), zero channels and zero sample rate are
    used as divisors, and a negative chunk size makes the chunk walk loop forever (F-022).
    No test.
  - *Tampering through the model file*: **unmitigated** — a 574 MB binary parsed by vendored
    C, fetched with no integrity check (F-026). No test.
  - *Path traversal*: `LoadModelByName` is Blueprint-callable and joins its argument onto
    three search directories with no containment check, so a name with `../` escapes them.
    `LoadModel` routes a separator-free string to it and otherwise treats the string as an
    absolute path, deliberately. Unmitigated and, with a trusted caller, low.
  - *Spoofing a GPU backend*: C-asr documents that only the library's presence is checked,
    so a library that loads but whose CUDA runtime does not resolve is not detected. A
    correctness wart with no attacker in it; recorded because the contract admits it.
  - *Over-read in the capture resampler*: F-017 (**major**, open; raised from minor in
    J-0130), pinned by `FANTASIA.WhisperCapture.Resample.ReadsOneFramePastInput`.
    `ResampleAndMixToMono` indexes with `FloorToInt(static_cast<float>(Phase))` while
    bounding the loop on the `double`, so one frame past the input is read whenever the
    carried phase lands within half a float ulp below `NumFrames`. No untrusted data reaches
    it — the sample values never enter the index arithmetic, and the two operands are the
    device's sample rate and the backend's callback size — so it is not a blocker. It is
    major because the garbage value propagates: into `SetRecentPeak`'s unclamped
    float-to-int conversion, which is the VAD's only input, and into the high-pass filter's
    persistent state (F-041). Reachable continuously on a 44.1 kHz device with large
    callbacks, never on the 48 kHz family, where the ratio is an exact integer.
  - *Captured audio validated too late*: F-041 (minor, open) — the NaN, Inf, denormal and
    range check lives in `TranscribeBuffer`, downstream of both the peak publication and the
    filter, so one non-finite sample latches the IIR state for the session and ASR silently
    returns nothing. No test.
    The capture path is otherwise the project's most thoroughly characterised real-time code
    since US-029; the rest of `WhisperCaptureComponent.cpp` remains F-007's gap.
  - *Widened surface for the sake of a test*: none. The characterisation seam is a single
    `friend struct FWhisperCaptureTestAccess` inside the private section, which changes no
    export, no reflection and no Blueprint-facing signature, so it reaches the DSP steps
    without making them reachable from anywhere else.

### M-tts
- Entry points: `URESTTTSComponent::TTSSynthesize`, `EndTurn`, `TTSGetSound`,
  `TTSGetRawSound`; the `URL`, `Headers` and `BodyTemplate` configuration;
  `UElevenLabsTTSComponent`'s ten properties; the five virtuals a subclass overrides;
  `FElevenLabsProtocol`'s static functions.
- Becomes code or commands: a **URL** and an **HTTP request body**, both from
  configuration. `BodyTemplate.Replace("{text}", EscapedText)` is a template substitution,
  with a hand-written escape covering `\`, `"`, `\n`, `\r` and `\t` only — the other C0
  control characters (U+0000–U+001F) are passed through raw, which RFC 8259 forbids inside a
  JSON string, so synthesis text containing one produces an invalid body. In GET mode the
  text is `UrlEncode`d, which is correct. The escape assumes `{text}` sits inside a JSON
  string literal in the template; a template that puts it anywhere else is unprotected, and
  nothing checks. `FElevenLabsProtocol::BuildRequest` interpolates `VoiceID` into the URL
  path with no encoding, so a `VoiceID` containing `/`, `?` or `#` rewrites the request.
- Deserialises: the HTTP response body — a WAV container, a raw PCM16 stream, base64 audio
  chunks inside server-sent events, and the ElevenLabs timing JSON.
- Secrets: the `xi-api-key`, which travels in `Headers` or in `UElevenLabsTTSComponent::Key`.
  `Key`, `URL`, `BodyTemplate` and `Headers` all carry `Config` on a `Config = Game` class,
  so the map an API key goes into is read from the host project's `DefaultGame.ini`. F-008
  stays open over it: a project whose asset already holds a key keeps it there until the
  asset is cleared and re-saved, and the doc comment is the only place that says so. No key
  is logged — the module's only two `UE_LOG` calls report a failed
  request, and neither names a header or a key. The second of them logs the whole response
  body at Error level (`RESTTTSComponent.cpp:348`), which is a log-hygiene wart of the same
  kind as F-027's.
- Decides access: nothing.
- Threats:
  - *Credential disclosure into a committed asset*: mitigated for new projects by the
    `Config` specifier, and **not** mitigated for a project that already stored a key in an
    asset — that key stays until the asset is re-saved, which no code can do (F-008, open).
    The flag is pinned for six secret-bearing properties by `FANTASIA.Secrets.ConfigFlag`
    (`SecretConfigTests.cpp`, DEC-002), which reads `CPF_Config` off the reflected property
    and is the project's one existing test of a security property. It does not cover the
    four secret-bearing properties outside those two components.
  - *Credential disclosure on the wire*: **unmitigated** — nothing requires `URL` to be
    `https`, and `Headers` is sent to whatever `URL` says.
  - *Denial of service through a crafted synthesis response*: **unmitigated** — two
    division-by-zero crashes and an infinite loop in the WAV header parse, on the thread
    that handles the response (F-023). No test.
  - *Resource exhaustion*: the per-call `Buffer` map and `BuffersByCaller` are keyed by
    caller-supplied id and `Buffer` is never cleared by anyone, M-ace included, so raw PCM
    accumulates for the component's lifetime (roughly 32 KB per second of speech). Bounded
    by how much an application synthesizes, not by an attacker, so it is a growth defect
    rather than an attack — but there is no cap and no eviction. No finding; it belongs to
    the tester's stress plan.
  - *Tampering with playback order*: C-rest-tts records submission order and does not
    enforce it, because the base component has no consumer (F-009). Not a security property
    on its own; it means the ordering guarantee an integrator reads about lives in M-ace.

### M-llm
- Entry points: `UOpenAICompatibleComponent::GetChatCompletion`, `ApplyPreset`, the five
  preset factories and the `Config` struct; `ULangGraphComponent::createLangGraphThread`,
  `deleteLangGraphThread`, `langGraphQuery`, and its `endpoint` and `port`.
- Becomes code or commands: a **URL** and a **prompt**. The request body is built with
  `FJsonObject` and `FJsonSerializer`, so conversation text cannot break out of the JSON —
  the right structure, and worth stating because it is the one injection sink in the project
  that is correctly handled. The URL is not: `LangGraphComponent.cpp` concatenates
  `endpoint + ":" + port + "/threads/" + threadID + "/runs/..."` with no encoding, so a
  `threadID` containing `../`, `?` or `#` rewrites the request path against the agent
  server. `threadID` comes from the server's own thread-create response or from the
  application, which may have taken it from a model.
- Deserialises: the chat-completion response, and the SSE stream. The stream is scanned with
  a regex (`data: (\{.*\})`) and each match is then parsed as JSON.
- Secrets: `FOpenAICompatibleConfig::APIKey`, sent as a bearer token. It reaches the
  component because the **owning** property `UOpenAICompatibleComponent::Config` carries
  `Config`; the inner fields' own `Config` specifiers do nothing, because `LoadConfig` never
  descends into a struct property, and that asymmetry is what F-008 was. F-008 stays open
  over the key an existing asset already holds. `ULangGraphComponent` holds no credential at
  all: there is nowhere to put one, so
  an authenticated LangGraph deployment is unreachable from the plugin. The key is never
  logged; the whole response body is logged at Error level on a non-2xx status, which is a
  log-hygiene wart (noted in F-027).
- Decides access: nothing.
- Threats:
  - *Prompt injection*: **unmitigated, and structurally so.** `FChatTurn` carries a role and
    content, the component sends them in order, and nothing marks any turn as untrusted or
    quotes document text as data. Role separation is the only control, and it is the
    integrator's to use. This is the boundary the project cannot close on its own: a plugin
    that delivers a model's reply to an application cannot decide what the application does
    with it. What it can do — and does not — is say so in C-llm.
  - *Model output driving a privileged sink*: **unmitigated.** The reply is broadcast as a
    string and the plugin offers, in the same process, a Prolog engine that executes strings
    (F-024) and a Cypher call that takes a query string. Nothing in this repository connects
    them; the plugin ships both halves and warns about neither.
  - *Denial of service through a malformed response*: **unmitigated** —
    `Choices[0]->AsObject()->GetObjectField(...)` dereferences a null shared pointer when the
    element is not an object (F-027). No test.
  - *Credential disclosure*: `ApplyPreset` overwrites `Config` wholesale including `APIKey`
    (documented in C-llm), and a preset may replace an `https` URL with the
    `http://localhost` of a local preset. Both are documented behaviours, neither is a
    defect, and both are ways a key ends up somewhere its owner did not intend.
  - *Spend*: an unbounded number of `GetChatCompletion` calls costs money. No rate limit,
    no call budget, no concurrency cap; `TimeoutSeconds` (default 60, clamped 1–300) is the
    only bound of any kind. Only the integrator can set a budget, and nothing tells them to.

### M-prolog
- Entry points: `SWIPLsubmitQuery`, `SWIPLqueryString`, `SWIPLfindAll`, `nextSolution`,
  `SWIPLassert`, `SWIPLretract`, `openPrologFile`, `SWIPLconsultString`, and
  `USWIPrologHelpers`' term constructors.
- Becomes code or commands: **everything here does.** This is the sharpest surface in the
  project. `SWIPLqueryString` and `SWIPLfindAll` parse the caller's string with `PlCompound`
  and run it through `PlQuery`; `SWIPLconsultString` asserts the caller's string as a clause;
  `openPrologFile` consults a caller-supplied path, which executes that file's directives. No
  `sandbox:safe_goal`, no predicate allow-list, no path confinement — so `shell/1`,
  `process_create/3`, `open/3` and `delete_file/1` are all reachable from a string
  (F-024). `startProlog` also calls `_putenv` to set `SWI_HOME_DIR` process-wide from the
  project directory, and sets the engine's working directory to `<ProjectDir>/Resources/`,
  which is a de facto root for relative consults and is not enforced for absolute ones.
- Deserialises: Prolog source — from a string, or from a file.
- Secrets: none.
- Decides access: nothing. The term-object path (`SWIPLsubmitQuery` with
  `USWIPrologTerm`s) is the structured alternative to a string and is the one an integrator
  should use for anything built from input: atoms are quoted and the tree is translated
  element by element (`SWIPrologThread::translateTerm`). It is not a sandbox — a compound
  named `shell` with one argument is still `shell/1` — but it cannot be made to inject a new
  goal, because the shape is fixed by the caller's object graph rather than by text.
- Threats:
  - *Remote code execution through an injected goal*: **unmitigated** (F-024, major). The
    mitigation that exists is the term-object API and the fact that nothing in this
    repository feeds untrusted text to the string API; the mitigation that is missing is a
    word of warning in C-prolog and a safe-goal check. No test.
  - *Path traversal and code execution through `openPrologFile`*: **unmitigated** (F-024).
  - *Cross-instance disclosure and tampering*: **unmitigated** (F-028, minor) — one engine,
    one knowledge base and one solution broadcast for every component in the process, and
    `Shutdown()` is never called, so clauses outlive the session.
  - *Elevation of privilege across threads*: mitigated — all `UObject` translation happens
    on the game thread and only plain data is queued to the worker
    (`SWIPrologThread.cpp`), so no `UObject` crosses threads and results are marshalled back
    with `AsyncTask`. This is a real and carefully built mitigation.
  - *Denial of service*: a goal that does not terminate occupies the worker thread forever;
    there is no timeout and no way to cancel. `ThreadHandle` is dereferenced with no null
    check in all eight entry points, so a call before `BeginPlay` crashes. The behaviour of
    the `PL_halt(1)` on a failed `PL_initialise` — whether it terminates the editor or
    returns — **I could not determine** without running it against a broken deployment, and
    it matters because a missing `libswipl.dll` is the project's most common installation
    failure.

### M-probabilistic
- Entry points: `UBayesianNetwork` and `UInfluenceDiag`'s build, inference and query calls;
  `setBN(Filename)`, `writeBIF(file)`, `writeBIFXML(file)`.
- Becomes code or commands: no interpreter, but three filesystem paths taken from the
  caller with no confinement and no extension check — one read, two write. The writers are
  an arbitrary-file-write primitive for any caller that does not control its path.
- Deserialises: BIF files, through `gum::BIFReader`.
- Secrets: none.
- Decides access: nothing.
- Threats:
  - *Denial of service through a malformed model file*: **unmitigated, and the contract says
    otherwise** — `setBN` is the only aGrUM call in the module with no `try`/`catch`, so a
    syntax error throws out of a Blueprint-callable function and terminates the process;
    `proceed()`'s error count is also dropped (F-025). C-probabilistic's invariant that
    aGrUM exceptions are caught at the boundary is true of the Blueprint-value paths and
    false of the one file path.
  - *Tampering through an arbitrary write*: **unmitigated** — `writeBIF` and `writeBIFXML`
    (F-025).
  - *Resource exhaustion*: exact inference on a large network is unbounded in time and
    memory by construction. It is dispatched off the game thread, which is the right
    structure, and there is no cap. Not an attack path unless the model comes from
    elsewhere — which `setBN` is exactly the way to arrange.
  - The vendored aGrUM 2.3.2 library is the parser behind all of this. `WITH_AGRUM_BINDING`
    cannot disable it (F-005), so there is no build in which this surface is absent.

### M-ml
- Entry points: `UKernelDensityEstimator::setData`, `evaluate`, `getPdf`;
  `ULibSVM::train`, `predict`, `load(path)`; `Linspace`.
- Becomes code or commands: none. One filesystem path, `ULibSVM::load`, uncontained.
- Deserialises: a libsvm model file, through vendored `svm_load_model` — a C text parser in
  3312 lines of unmapped third-party code (`svm.cpp`). This is the module's whole untrusted
  surface and it is handed straight to the library.
- Secrets: none.
- Decides access: nothing.
- Threats:
  - *Memory corruption through a crafted model file*: **unmitigated**, delegated entirely
    to vendored libsvm, whose loader is `fscanf`-based. No finding opened: I have not read
    the vendored parser closely enough to name a defect in it, and saying "libsvm might have
    a bug" is not a finding. The honest statement is that an untrusted `.model` file reaches
    an unaudited C parser with no validation in front of it.
  - *Denial of service*: `train()` reads `trainingSet[0]` before checking the array is
    non-empty, so an empty training set is an out-of-bounds access — caught by `TArray`'s
    range check in development builds, not in Shipping. `load` on a malformed file leaves
    the estimator without a model and the next `predict` returns empty, which is documented
    and pinned (`PredictNoModel`).
  - *Integrity of results*: a ragged training set produces a wrong model rather than an
    error, because libsvm does not check — documented in C-math. Not security, listed
    because a silently wrong classifier is the kind of thing that gets trusted.
  - `Linspace` returns `std::vector<float>` across a prebuilt module boundary (F-010): an
    ABI hazard, not an attack.

### M-neo4j
- Entry points: `UNeo4jComponent::submitQuery` and its `endpoint`, `port`, `user`,
  `password`, `Transport` and `UseSSL` configuration; the exported PackStream codec; the
  (unexported, F-012) Bolt message layer. `UseSSL` is the one entry point whose value is
  mostly not its own: `BeginPlay` forces it from the endpoint scheme for nine of the ten
  schemes the component meets (`Neo4jComponent.cpp:251-256`), so it is honoured only for
  `neo4j://` and for an endpoint with no scheme at all (F-011). A second entry point is not
  the component's at all: the routing table, through which the server names the host the
  client reconnects to (`Neo4jComponent.cpp:1351-1368`).
- Becomes code or commands: **a Cypher query**. Parameters are passed properly — the
  `TMap<FString, FString>` is converted to a Bolt parameter map and sent in RUN beside the
  query, never spliced into it — so the safe structure exists and works. But the map is
  string-to-string, so any caller who needs a number, a list, a label, a relationship type
  or a property name has no parameter to put it in and must concatenate it into the query
  string. The API therefore pushes callers toward string-built Cypher for exactly the values
  that cannot be parameterised in Cypher anyway, and nothing in C-neo4j says which values
  may be interpolated or how.
- Deserialises: **everything the server sends** — the 4-byte handshake reply (length-guarded),
  Bolt chunks, and PackStream values of every type, recursively. Three bounds hold over that
  surface: `FBoltPackStreamReader::Require(N)` gates every read, `Reserve` and `Append` and
  refuses a negative `N` as well as an over-long one; `ReadValue` carries its nesting as a
  parameter and refuses past 64 containers; and `UNeo4jComponent::OnDataReceived` refuses a
  message past 16 MiB before the bytes reach the dechunker. A refusal yields a null value,
  exhausts the reader and logs once. All three bound the **bytes on the wire**, and nothing
  bounds what they decode into or how long it is kept: one wire byte becomes about 208 bytes
  of heap (F-056) and the decoded records of one query are retained with no limit until the
  server ends it (F-060).
- Secrets: `password` and `user`, both `Config` UPROPERTYs, neither logged — verified across
  every `UE_LOG` in `Neo4jComponent.cpp`. They are sent in HELLO or LOGON on every
  connection.
- Decides access: nothing locally; the server authorises, and the component sends the
  credentials it was given.
- Threats:
  - *Spoofing the server, and disclosure of the credentials and the whole session*:
    **mitigated on the plain TCP transport** — a `+s` scheme decides to verify, and the
    decision is applied in four steps each of which is present and asserted: the engine's
    certificate bundle is loaded and the load's return tested, `SSL_VERIFY_PEER` is set,
    the host's name is written on the context's X509 verification parameters, and
    `SSL_get_verify_result` is inspected after a successful handshake rather than a
    completed handshake being read as a verified one. All four are applied to the `SSL_CTX`
    before the BIO exists, because an `SSL` inherits them at creation. Any failure returns
    from `TcpConnect` before a socket exists, so `HELLO` and the `LOGON` that carries `user`
    and `password` are unreachable by construction, and the refusal names the failed check
    and the `+ssc` migration (F-020, closed by US-031; DEC-023, DEC-024, DEC-025).
    Evidence: `FANTASIA/Source/FANTASIATests/Private/Security/Neo4jTlsVerificationTests.cpp`,
    11 tests. The decision is asserted for all ten schemes on both framings with both values
    of `UseSSL`, the application is read off an `SSL_CTX` the test owns and the component
    configured, and the hostname — which cannot be read back as a string on OpenSSL 1.1.1t,
    where `X509_VERIFY_PARAM_get0_host` does not exist — is read by exercising it: the
    parameters the component wrote accept a self-signed certificate carrying the decided name
    and reject one carrying another with `X509_V_ERR_HOSTNAME_MISMATCH`. Three duals keep the
    battery honest, because every assertion above would also pass against a context that
    verified everything: an unconfigured context verifies nothing and accepts any name,
    `+ssc` leaves the context stating exactly that, and an endpoint that parses to no host is
    refused while the same endpoint under `+ssc` is accepted.
    **Not mitigated on the WebSocket transport**, where the decision is advisory (F-055).
  - *Bypassing the verification decision on the WebSocket transport*: **unmitigated** — the
    WebSocket URL, not the decision, is what governs that connection, and two inputs write
    that URL outside the decision: the engine-wide `[LwsWebSocket] bDisableCertValidation`
    key and the `-lws-insecure=` switch, which the plugin cannot observe, and a routing
    address that contains a scheme, which the server supplies (F-055, major). No test: the
    WebSocket path has no seam and the engine's verification is not reachable from this
    suite.
  - *Disclosure through plaintext*: **unmitigated** — `BeginPlay` overwrites a user-set
    `UseSSL` from the endpoint scheme and says nothing (F-011, major). P-19's security
    carve-out admits the fix to the 5.8 line (CR-004), and CR-010 settles its shape:
    `UseSSL` goes, and the scheme alone decides encryption.
  - *Memory corruption from a server message*: **mitigated** — the reader
    refuses rather than clamps on every length, count and nesting depth the buffer cannot
    satisfy, and the component caps one message at 16 MiB (F-021, closed by US-030; DEC-020,
    DEC-021). Evidence:
    `FANTASIA/Source/FANTASIATests/Private/Security/BoltPackStreamBoundsTests.cpp`, 18 tests
    over the adversarial encodings — a null base pointer, each multi-byte reader truncated
    mid-value, a declared length or count beyond the buffer for string, bytes, list, map and
    structure, a negative 32-bit length and count, nesting past the limit, and an
    unterminated chunk stream — with the two duals that keep them honest, that nesting
    within the limit and a value ending on the buffer's last byte still decode.
  - *Memory exhaustion from a server message*: **partly mitigated, and the residue is
    measured.** The cap bounds the bytes on the wire and nothing bounds what they become.
    `sizeof(FBoltValue)` is 176 bytes and every alternative of the variant is resident at
    once, so `MakeShared` spends a 192-byte block plus a 16-byte shared pointer on each
    scalar: a well-formed `List32` of one-byte integers at exactly the cap — 16777216 wire
    bytes, within every bound the reader and the component enforce — decodes into 3141 MiB
    resident in 1066 ms, 196 bytes of heap per wire byte, and `OnDataReceived` runs on the
    game thread, so one message freezes a real-time application for about a second (F-056,
    major, `Kind=security`; measured by
    `FANTASIA/Source/FANTASIATests/Private/Stress/BoltStressTests.cpp`, test
    `FANTASIA.Stress.Bolt.Reader.DecodeAmplification` with `-StressFullAmplification`,
    which is the only run that is evidence because allocation cost is superlinear). That
    spike is survivable; what makes it fatal is that nothing releases it. Each RECORD's
    decoded fields are retained in `CurrentRecords` (`Neo4jComponent.cpp:1038`), emptied
    only when a query starts (`:1123`) or when the terminating SUCCESS arrives (`:1429`),
    and the client asks for every row (`PULL n=-1`, `BoltProtocol.cpp:297-301`), so a server
    that sends RECORDs and never the SUCCESS is retained against without limit: about
    300 MiB of upload holds about 60 GiB (F-060, major, `Kind=security`, no test yet).
    F-060 is the half of F-021's item 4 that US-030 did not close — the dechunker's
    unbounded accumulation was capped and the component's was not — and the two findings
    compose: a budget per message does not touch a flood of small messages, which still
    retains about 36 times what it costs to send. Both fixes are a private budget and both
    are admissible on the 5.8 line; the narrower `FBoltValue` that would remove the factor
    itself is exported and named in C-neo4j, so it is a contract version and not a security
    fix (J-0209, J-0210).
  - *Loss of the connection after a refused message*: **unmitigated** — the cap refusal
    resets the dechunker mid-stream, so the framing cannot be resynchronised and the
    component reports itself connected while decoding nothing, with no error on its
    delegates (F-048, major, `Kind=bug`: the ruling and its conditions are in Open risks).
  - *Cypher injection*: partly mitigated by the parameter map, undermined by its
    string-only type and unaddressed by the contract. No finding opened: fixing it means
    changing the parameter type, which is a C-neo4j version and a Blueprint-ABI change, so it
    is a change request rather than a defect. It is in Open risks below.
  - *Redirection through the routing table*: with `neo4j://` the server tells the client
    which host to connect to and the client obeys. That is the protocol, and it is safe only
    if the server is authenticated, which on the plain TCP transport it now is: the scheme's
    promise comes from the configured endpoint and the name verified comes from the host
    being connected to, carried in one field so the two cannot drift apart, so a redirect to
    a backend is a redirect to a backend that must prove its own name. On that transport the
    address is stripped of any scheme before it is used, so a routing record of
    `wss+insecure://evil` resolves nowhere. The WebSocket transport builds its URL from the
    raw address instead and passes such a record through verbatim (F-055).
  - *Information disclosure through diagnostics*: `response` holds the last raw response as
    a Blueprint-readable property, and node and relationship properties are flattened into
    string maps. Graph data therefore reaches Blueprints wholesale; whether that matters is
    the integrator's data question, not the plugin's.

### M-ace
- Entry points: `UACERESTTTSComponent` and `UACEElevenLabsTTSComponent` — everything in
  C-rest-tts plus `A2Fpointer`, `A2FParameters`, `A2FProvider`, `EmotionParameters` and the
  ElevenLabs settings duplicated from M-tts (F-003).
- Becomes code or commands: no new sink. `A2FProvider` is an `FName` matched against
  providers installed in the engine, so an unknown name produces no animation rather than
  loading anything.
- Deserialises: nothing of its own; it drains the PCM buffers M-tts filled, so M-tts's
  response parsing (F-023) is upstream of it.
- Secrets: `UACEElevenLabsTTSComponent::Key` **does** carry `Config`, and is one of the four
  secret-bearing properties that F-008's six-row table does not name (the other three are
  F-029's). Both ACE components inherit `Headers` from `URESTTTSComponent`, so F-008's
  second half applies here too and is fixed by the same specifier — which also means a fix
  verified only against the base class is verified for this module as well.
- Decides access: nothing.
- Threats:
  - *Resource leakage*: an un-closed turn leaks the Audio2Face session and bleeds one
    utterance into the next — mitigated only by the integrator remembering `bEndTurn`, which
    C-ace-tts makes mandatory and no code enforces. It is a leak with a documented cause, not
    an attack.
  - *Tampering through divergent duplication*: F-003 means ten properties are kept in step
    by hand and have already diverged once. A security-relevant property (the key) is among
    them.
  - *Supply chain*: this module only loads when `NV_ACE_Reference` is enabled, and that
    plugin plus the character bundles are fetched by bootstrap with no integrity check
    (F-026). The companion plugin is also shipping 5.7 binaries under a 5.8 descriptor
    (F-016), which is an availability defect, not a security one.
  - This module has **no tests and no test module** (C-ace-tts admits it), so nothing here
    is pinned.

### M-demo
- Entry points: `AFANTASIATestActor`'s Blueprint-callable exercise functions.
- Becomes code or commands: it calls `SWIPLqueryString`, `SWIPLconsultString` and
  `submitQuery` — with string literals only, verified line by line
  (`FANTASIATestActor.cpp`), and the comment above the Prolog calls shows the author knew
  the string is executed directly. No untrusted data reaches a sink here.
- Deserialises: nothing.
- Secrets: **three of them, and this is the module's real surface.** `LLMAPIKey`,
  `Neo4jPassword` and `ElevenLabsKey` are `EditAnywhere, BlueprintReadWrite` properties on
  the actor with **no `Config` specifier**, copied into the live components when an exercise
  runs. The actor cannot be used against a real endpoint without typing live credentials
  into its details panel, and the values are then serialised into the level or actor asset
  (F-029). The Neo4j path at least checks: it skips the exercise and logs "no password"
  rather than connecting anonymously.
- Decides access: nothing.
- Threats:
  - *Credential disclosure into a committed asset*: **unmitigated** (F-029, major). The same
    failure as F-008, in the module its table does not reach, and here the designed use is
    what triggers it.
  - *Surface for no benefit*: the actor ships inside the runtime module (F-004), so every
    user's build contains a spawnable actor that constructs all eight components and three
    credential fields. It adds no sink the components do not already expose to any
    Blueprint, so the capability is unchanged; what it adds is a place for a secret to be
    typed.

### M-tests
- Entry points: none. Nothing may import from it.
- Becomes code or commands: nothing.
- Deserialises: test fixtures it writes itself.
- Secrets: **this is the module's whole security surface.** `SecretConfigTests.cpp` is a
  test *about* credentials: it reads the `CPF_Config` flag off the reflected property rather
  than inferring it from a value, writes its fixture values into the in-memory `GGameIni`
  through a scoped RAII entry that restores or removes the key in its destructor, never
  flushes to disk, and operates on transient objects (DEC-002). No real key appears in it
  and the user's `DefaultGame.ini` is never touched. That is the discipline every later test
  must follow.
- Decides access: nothing.
- Threats: *credential disclosure through a fixture or a log* — mitigated by the pattern
  above, and by the rule that a test needing a live endpoint or a database skips cleanly
  rather than authenticating. Nothing enforces either mechanically, and with no CI there is
  no build log for a leaked key to land in — which removes the usual consequence, not the
  rule. `Private/Security/` is mine and holds F-021's and F-020's batteries and the
  `FNeo4jTestAccess` seam header they share; every other "no test" above is work owed.
  The seam header is a shared file by necessity: it is the one definition of the struct
  `UNeo4jComponent` befriends, so two translation units cannot define it differently, and
  M-neo4j's coder extends its forwarders when the component's private shape changes. Every
  member is a reader or a single call, and the one that can open a socket says so.
  F-020's battery adds a second kind of surface to this module: it links OpenSSL
  (`FANTASIATests.Build.cs`, the engine module and never the plugin's own path, F-044) and
  builds a 2048-bit key and two self-signed certificates in memory, which never touch disk
  and are freed with the test. A test that generated a key pair into a file, or trusted one
  on the machine's store, would be the first thing in this suite to leave state behind.

### M-dist
- Entry points: `bootstrap.bat` / `bootstrap.sh` and their flags; `setup_whisper.*`;
  `build_whisper_cuda.*`; `enable-plugins.ps1`; the `.Build.cs` files; the committed
  `Binaries/` trees.
- Becomes code or commands: the whole module is commands. `curl` invocations with URLs
  built from `RELEASE_BASE`, `tar` extractions into the plugin tree and the user's project,
  a `powershell -ExecutionPolicy Bypass` call on `enable-plugins.ps1`, and a `powershell`
  one-liner that walks parent directories looking for `Plugins`. Paths interpolated into
  these come from `%~dp0` and the user's flags, quoted throughout, with the `SCRIPT_DIR`
  and bundled-`tar` traps C-install records already fixed.
- Deserialises: four to seven downloaded archives, totalling about 5 GB.
- Secrets: none in the scripts, and none anywhere in the repository — a pattern scan over
  all 6639 tracked files for `sk-`, `gsk_`, `ghp_`, `AKIA`, `AIza` and an `xi-api-key`
  assignment returns nothing, and the repository contains no `.uasset` and no committed
  `DefaultGame.ini` (the only tracked `.ini` is `FilterPlugin.ini`, which is empty of
  entries). F-008's leak lands in the *host project's* asset, in the user's repository, not
  in ours.
- Decides access: it decides what gets linked and what gets staged, which is why it decides
  how much attack surface exists. F-002's dead gRPC, protobuf and abseil link surface is
  surface for no benefit.
- Threats:
  - *Tampering with the installed stack / code execution on the user's machine*:
    **unmitigated** — no checksum or signature on any download, extraction into the plugin's
    own tracked directory and into the directory M-asr loads a DLL from, `curl -L` without
    `--proto '=https'`, and URLs built from `antori82` while the repository is `antori`, so
    every download rests on a GitHub rename redirect for a username nobody holds (F-026,
    major). The only control is TLS to `github.com`.
  - *Availability*: the 5.8 line's default install cannot complete, because four named
    NVIDIA assets do not exist on `runtime-v3.0` (F-014, major); `bootstrap.sh` names a
    Linux GPU asset that never existed (F-015).
  - *Tampering with the shipped binaries* (TB-7): partly mitigated by P-12's
    rebuild-and-dumpbin discipline, which is a real check with a real artifact (the import
    list), run by hand on one machine. The committed DLL is the product for Blueprint-only
    users, so this is the highest-trust path in the project.
  - *Integrity of the engine match*: the `.uplugin` `EngineVersion` and the committed
    `.modules` BuildId must agree or the editor refuses the plugin. An availability control,
    and the one place a mismatch is caught mechanically.
  - **Nothing in this module has a single test.**

## Dependencies

No language here has a dependency manifest, so there is nothing for an audit tool to read:
C++ dependencies are vendored source and prebuilt libraries, and the C# is UnrealBuildTool
script with no NuGet packages. "Run the language's audit" has no mechanism on this project,
and inventing one is a dependency decision of its own (P-06). What replaces it is this
table. Every version in it is read from the file named beside it — a header macro, a
`CMakeLists.txt` or a build script — and not from any document, because the documents are
what the audit checks. Where the version is all the audit has, the row says so rather than
implying a CVE feed was consulted.

| Dependency | Version, and the file it was read from | Where | Security relevance |
| :--- | :--- | :--- | :--- |
| OpenSSL | 1.1.1t, 7 Feb 2023 — `ThirdParty/OpenSSL/1.1.1t/include/Win64/VS2015/openssl/opensslv.h:43` (`OPENSSL_VERSION_NUMBER 0x1010114f`), and `OPENSSL_VERSION_TEXT` is the same string in UE 5.8's own copy. **Both copies are linked and they are the same bytes.** `FANTASIA.Build.cs:48` declares the engine's `OpenSSL` module, which adds `libssl.lib` and `libcrypto.lib` itself (`OpenSSL.Build.cs:51-52`); `FANTASIA.Build.cs:54` *also* globs every `.lib` out of the plugin's own tree, reached through a variable named `EngineThirdPartyPath` that is assigned `ModuleDirectory/../../ThirdParty/` (`:44`). sha256 of the plugin's `libssl.lib` and `libcrypto.lib` equals the engine's (F-044). | TLS for M-neo4j | The version is the engine's choice, not the project's: `OpenSSL.Build.cs:12` hardcodes `1.1.1t` and UE 5.8 ships no other version, so no change available here moves it. The 1.1.1 branch is past upstream end of life (2023-09-11) and 1.1.1u, v and w followed what is linked — an externally owned risk, ruled below rather than tracked as a finding. What *is* the project's own is the duplicate link and the 23 MB of redundant unverified download (F-044, F-026). |
| SWI-Prolog | 9.2.4 — `ThirdParty/SWIProlog/headers/SWI-Prolog.h:63` (`PLVERSION 90204`) | M-prolog, as `libswipl.dll` + packs | Executes code by design (F-024). The DLLs are committed (64 tracked files under `PrologDlls/`) and staged next to the host binaries. |
| aGrUM | 2.3.2 — `ThirdParty/aGrUM/Includes/agrum/config.h:40` (`GUM_VERSION`) | M-probabilistic, three per-module static libs (`FANTASIA.Build.cs:25-27`) | Parses BIF files (F-025). ADR-0015 moves it to 3.x under US-022. The headers are tracked; the 249 MB of libraries are fetched build output that `.gitignore` does not cover (F-042). |
| whisper.cpp + ggml | whisper.cpp 1.8.4 — `ThirdParty/whisper_cpp/CMakeLists.txt:3`; ggml 0.9.8 — `ThirdParty/whisper_cpp/ggml/CMakeLists.txt:5-7` | M-asr, compiled from the vendored tree through the wrappers in `Private/whisper_staged/`, plus a fetched CUDA DLL | Parses the model file; the CUDA DLL is loaded at runtime from an unauthenticated download (F-026). The shipped build cannot report which version it contains: `FANTASIA.Build.cs` defines `WHISPER_VERSION="0.0.0"`, `GGML_COMMIT="unknown"` on the staged path, so `whisper_version()` in a user's binary tells nobody whether their copy predates a fix. |
| libsvm | 3.35 — `Source/FANTASIA/Public/svm.h:6` (`LIBSVM_VERSION 335`) | M-ml | Parses model files with `fscanf`. Current upstream; the surface is the parser, not the version. |
| kdepp | header-only; no version marker in `kde.h` or `kdemv.h`, so the version is unknowable from the tree | M-ml | No parsing, no I/O. |
| nlohmann/json | 3.7.3 — `ThirdParty/nlohmann/json.hpp:33-35` (Nov 2019, roughly twenty releases behind) | — | Referenced only by an include path (`FANTASIA.Build.cs:33`); no first-party file includes it and no aGrUM header does either. Dead weight, so the age costs nothing today — but it is an invitation to use a six-year-old parser on untrusted input. |
| gRPC + protobuf + abseil | gRPC 1.51.1 — `ThirdParty/grpc/include/grpcpp/version_info.h:25`; protobuf 3.21.6 — `.../protobuf/src/google/protobuf/stubs/common.h:85` (`3021006`); abseil LTS 20220623.0 — `.../abseil-cpp/absl/base/config.h:114-115` | — | A late-2022 stack, linked and include-pathed, used by nothing (F-002). Advisories against gRPC and protobuf of that vintage are open by version; how much of the code survives `/OPT:REF` into the shipped DLL is unknown, so the exposure is unmeasured rather than proven. The resolution is removal, not an upgrade. |
| NVIDIA ACE / NVIGI runtime, A2F models | fetched bundles, redistributed unmodified; no version marker inspected here | M-ace, optional | Proprietary binaries loaded into the process, fetched with no integrity check (F-026). |
| Unreal Engine HTTP, WebSockets, Json, AudioCapture | 5.8 | M-tts, M-llm, M-neo4j, M-asr | The HTTP module's TLS verification is the mitigation that makes TB-3 safe; the plugin overrides none of it. |

**Provenance.** Headers and source are tracked in git; every prebuilt binary library is
fetched by `bootstrap.{bat,sh}` from a GitHub release and verified in no way (F-026). The
release tag is `runtime-v3.0` (`bootstrap.bat:46`, `bootstrap.sh:30`); `THIRD_PARTY_LICENSES.md:59`
and the `.gitignore` comment block still say `runtime-v2.0`, and
`THIRD_PARTY_LICENSES.md:25` tells a redistributor that the plugin links `aGrUM.x64.lib`, a
file that is not in the tree and not in any build file. A reader of the licence index cannot
tell what the build fetches or links.

**What the sprint-level audit could and could not do.** It read versions and provenance, and
compared them against what is published about those versions. It did not query a CVE feed per
dependency, and there is no mechanism in the project that would: no manifest, no lockfile, no
scanner that understands a vendored tree. So the standing risk is unchanged and now stated
precisely — upgrades are done by hand, wholesale, with no automated notification when an
upstream advisory lands, and three of the ten rows above are pinned to 2022-era or
end-of-life code. That is the dependency risk under the vendoring, accepted in
architecture.md.

## Open risks

Blockers first; a blocker means a story that depends on it cannot be accepted while it is
open.

- **F-020 (blocker, open; US-031's fix in the tree)** — the TLS path that authenticated no
  server now loads the engine's certificate bundle, sets `SSL_VERIFY_PEER`, writes the host
  on the context's verification parameters and inspects the verify result after the
  handshake, and refuses before a credential is written. The finding's reproduction — the
  grep that returned nothing — returns five call sites, and the behaviour is asserted in
  `FANTASIA/Source/FANTASIATests/Private/Security/Neo4jTlsVerificationTests.cpp`, which
  passes. The finding closes when US-031 is accepted. What the fix leaves behind is F-055,
  the WebSocket transport, which the fix deliberately does not reach.
- **F-055 (major, open)** — on the WebSocket transport the component's security decision is
  advisory: the URL string governs the connection, and two inputs write it outside the
  decision. One is the engine's, `[LwsWebSocket] bDisableCertValidation` in `GEngineIni` or
  `-lws-insecure=true` on the command line, which forces the certificate callback to accept
  anything for every WebSocket in the process
  (`LwsWebSocketsManager.cpp:93,97`, `LwsWebSocket.cpp:554,741`); a key an integrator sets
  for an unrelated development endpoint therefore removes authentication from the database
  connection, with nothing in the plugin able to see it. The other is the server's: the
  WebSocket branch builds its URL from the raw address rather than from the decided host, and
  maps eight schemes but not `wss+insecure://`, which the engine's client accepts and which
  means allow-self-signed, so a routing record containing that scheme is passed through
  verbatim and the reconnect carries `LOGON` over an unverified connection the server chose.
  The planner's note that this branch is dead holds only for a well-behaved server.
- **F-021 (blocker, open; US-030's fix in the tree)** — the reader that bounds-checked
  nothing on bytes off a socket now refuses every length, count and nesting depth the buffer
  cannot satisfy, and the component caps one message at 16 MiB. The reproduction the finding
  names is an assertion in
  `FANTASIA/Source/FANTASIATests/Private/Security/BoltPackStreamBoundsTests.cpp` and passes
  there; the finding closes when US-030 is accepted. What the fix leaves behind is F-048,
  F-056 and F-060.
- **F-056 (major, open, `Kind=security`)** — the cap bounds wire bytes and not decoded size,
  so a well-formed message at the cap costs 3141 MiB and 1066 ms of game thread, about 196
  bytes of heap per wire byte. **Ruled a security finding**, reversing the tester's
  `Kind=performance` at its own request. It is denial of service across TB-4 driven wholly
  by bytes from the untrusted side, and it is the surviving half of F-021's threat, so
  without the label the claim that exhaustion from a server message is mitigated has nothing
  holding it honest. My carve-out test — would I be glad to see the fix the label licenses —
  passes here where it failed for the EOL OpenSSL (J-0137) and for F-048 (J-0173): a running
  budget on values decoded per message inside `FBoltPackStreamReader`, refusing exactly as
  `Require` and `MaxBoltNestingDepth` do, private, no exported symbol, no Blueprint-facing
  signature, the same shape as F-021's own admitted fix, and unable to cause F-048 because
  `PopMessage` has already resolved the message boundary before the reader runs. Major and
  not blocker: no boundary crossed, no secret moved, nothing becomes code, the spike is
  bounded and transient, and US-030 strictly reduced it. US-030 is acceptable with it open,
  on the condition that its own narrative, C-neo4j and this document stop claiming a server
  cannot stop the process; the last of the three is done (J-0209).
- **F-060 (major, open, `Kind=security`)** — decoded records are retained with no bound
  until the server ends the query, so F-056's bounded spike becomes unbounded retention and
  process death from a few hundred MiB of upload. This is the half of F-021's item 4 that
  US-030 did not close: the dechunker's unbounded accumulation was capped, the component's
  was not. Admissible fix, and the reason the label is justified: a private budget on what
  one query retains, failing the query through the FAILURE path that already exists in the
  same function on the same thread (`Neo4jComponent.cpp:1021-1029`, `:1046-1057`) rather
  than through a teardown. Major for the same reasons as F-056, and it does not bar US-030,
  whose criteria name the dechunker. **It owes a test**, which is cheap: feed N framed
  RECORD messages through the seam with no terminating SUCCESS and read `CurrentRecords`
  back (J-0210).
- **F-048 (major, open, `Kind=bug`)** — a server that forces one cap refusal leaves the Bolt
  stream unresynchronisable, so the component reports itself connected and decodes nothing.
  Measured, not inferred: `Quality.Neo4j.Cap.RefusalLeavesStreamUnrecovered` forces one
  refusal with a 20 MiB message and then delivers 0 of 3 following replies against 3 of 3 on
  the control.
  **Ruled a bug and not a security finding, on three reasons and not four.** A fourth
  reason once given — that a plan expected the connection to keep working — is withdrawn:
  no such plan exists, the sentence belongs to the planner's profile for core-dev
  (`docs/ateam/verification/core-dev.md:88`), it predates the finding and 'survives' in it
  can only mean 'not torn down' (J-0207). The ruling stands on what remains, below.
  It passes the STRIDE test on denial of
  service — the trigger is attacker-controlled across TB-4, one 16 MiB message suffices, and
  the effect outlasts the attacker's presence — but `Kind=security` is not a neutral record
  here: under CR-004 it licenses the fix onto the shipped 5.8 line whatever surface it
  touches, and the fix available for an unresynchronisable stream is a connection teardown
  on a framing violation. That changes what C-neo4j promises about a connection, fires on a
  merely faulty server as readily as on a hostile one, and lands in the disconnect path
  beside a live receive thread. The risk of taking it unbarred exceeds the risk it answers,
  so it goes through the ordinary route. Three conditions flip the ruling: a cap refusal
  reachable by a message a legitimate server sends, rather than one past 16 MiB; a second
  `Dechunker->Reset()` reachable mid-stream from any other path (today there are two reset
  sites, `Neo4jComponent.cpp:141` per connection and `:579` on the refusal, and only the
  second is reachable mid-stream); or any way to reach the desync that also reaches memory,
  a secret or a tool call.
  F-057 bears on the first condition and does not meet it. The cap is charged to the whole
  transport read, so a message under the cap is refused when a read straddles the end of one
  message and the start of the next, which does make the refusal reachable without a message
  past 16 MiB. But the band is a single Bolt message within one transport delivery of the
  cap — 16 KiB of the 16 MiB range — and it takes a result that already sits at the edge of
  what the cap permits, so the desync is not reachable in ordinary operation and the balance
  that decided the ruling does not move. What would move it is the cap being lowered, or a
  result shape that routinely approaches it; both would put F-057's band in everyday use.
- **F-057 and F-058 (minor, open, `Kind=bug`) — ruled not security**, which is recorded
  because the question was asked rather than assumed. F-057's error is strict and never
  permissive: charging the cap to the whole read refuses up to one transport delivery early
  and can never let more than the cap through, so nothing is gained by provoking it that a
  compliant message does not already get, and its only security-adjacent consequence is the
  widening of F-048's trigger assessed above. One correction to its wording: C-neo4j is
  incomplete here rather than wrong — it states the slack on the permissive side only, and
  the strict side needs the same sentence. F-058 mangles a bracketed IPv6 literal, and the
  single thing that would make a hostname parse bug a security defect is the connect target
  and the verified name diverging. They cannot: `ExtractHostname`'s one result is passed to
  `InitiateConnection` as the host to reach and from there into `DecideTransportSecurity` as
  the name to verify (`Neo4jComponent.cpp:266`, `:285`), and keeping them in one value is
  the stated mitigation for the routing redirect. `[` resolves to nothing and no certificate
  carries it, a userinfo prefix left in the host behaves the same way, and an endpoint that
  parses to no host is already refused with a dual test on it. The risk to watch is not this
  defect but any future change that hands the socket a differently parsed host from the one
  verified.
- **F-008 (major, open; US-028's fix in the tree)** — the two component properties now read
  from `DefaultGame.ini`, and the residue is what no specifier can reach: a key already
  stored in a user's asset stays there until that asset is cleared and re-saved. The
  property doc comments are the only thing that tells anyone, and nothing verifies that a
  given project has done it.
- **F-029 (major, open)** — the same leak on `AFANTASIATestActor`'s three credential fields,
  which cannot be used without typing a live key into an asset, in an actor that ships in
  the runtime module. The actor is not a `Config = Game` class, so it is not one specifier
  away from safe. Outside US-028's scope.
- **F-011 (major, open; admitted to 5.8 by CR-004, shape chosen in CR-010)** — `UseSSL` is
  silently overwritten from the endpoint scheme, so a user who asks for encryption against
  `bolt://` gets plaintext. US-032 removes the property, leaving the scheme as the only
  thing that decides encryption.
- **F-022 (major, open)** — a crafted WAV file read by M-asr over-reads the heap by up to
  2 GB into the transcript, and crashes or hangs on three other fields.
- **F-023 (major, open)** — a crafted synthesis response crashes or hangs M-tts in the HTTP
  completion path.
- **F-024 (major, open)** — M-prolog's string entry points execute arbitrary goals and
  consult arbitrary files, with no sandbox and no warning in C-prolog, in a plugin whose
  purpose is to put a language model in the same process.
- **F-025 (major, open)** — M-probabilistic's BIF reader is the one aGrUM call with no
  exception handler, contradicting C-probabilistic, and its two writers take an unconfined
  path.
- **F-026 (major, open)** — bootstrap verifies nothing about ~5 GB of downloads, extracts
  into the plugin's own tree and into the directory a DLL is loaded from, and builds its
  URLs from a stale owner name.
- **F-044 (minor, open)** — the module links OpenSSL twice: the engine's `OpenSSL` module
  dependency and, on top of it, a byte-identical vendored copy globbed in from the plugin's
  own tree inside `LoadGrpc`. Harmless while the bytes match; a link-order accident in the
  code that negotiates TLS the moment a bootstrap release or an engine patch makes them
  diverge.
- **F-042 (major, open)** — `.gitignore:35` still names aGrUM 1.x's `agrum.x64.lib`, so the
  249 MB of 2.x libraries in the tree are not ignored, are untracked, and are one
  `git add -A` away from being pushed as LFS objects into a repository users clone. P-17
  states that these artifacts "are gitignored"; for this one directory that is false, and
  the libraries themselves arrive unverified (F-026).
- **F-017 (major, open)** — M-asr's capture resampler reads one frame past its input
  whenever the carried phase lands within half a float ulp below the frame count:
  continuously on a 44.1 kHz device with large callbacks, never on the 48 kHz family. No
  untrusted input reaches it, so it is not a blocker; it is major because the out-of-bounds
  value propagates into the VAD's only input and into persistent filter state, where it can
  silently stop ASR for the rest of a session (F-041), and because it makes the capture path
  uninspectable under ASan or Page Heap.
- **The linked OpenSSL is end-of-life, and no action available here changes that. Ruled: not
  a finding.** `bolt+s://` runs on OpenSSL 1.1.1t, a branch upstream stopped supporting on
  2023-09-11, with 1.1.1u, v and w released after it. Three things decide the ruling. The
  version is Epic's: `OpenSSL.Build.cs:12` hardcodes it, UE 5.8 ships no alternative, and the
  plugin's vendored libraries are the engine's own bytes (F-044), so there is no bump for
  build-dev to make — only a migration that would put a second OpenSSL in a process whose
  HTTP module links the engine's, which is a way to make this worse rather than better. The
  only fix a finding would license is that migration, which is worse than the risk it
  answers, and under CR-004 a `Kind=security` finding licenses it onto the shipped line
  whatever surface it touches — so the finding would authorise the one action nobody should
  take. And the advisories after 1.1.1t are denial of service and certificate-policy
  correctness, weighed against a path that now verifies the chain, tests the trust store's
  load and checks the host's name: the engine's HTTP module and WebSocket client have run on
  these same bytes throughout. A finding naming a condition nobody here can close would stay open for
  the life of the line and teach the team that an open finding is weather. **It becomes
  actionable, as a finding and an ADR under P-06, when either Epic ships a newer OpenSSL in
  an engine the project targets, or the project decides to carry its own TLS for Bolt** — and
  the second is a design decision for the user, not a defect for a coder.
- **F-041 (minor, open)** — M-asr validates captured audio in `TranscribeBuffer`, downstream
  of both the peak publication and the filter, so one non-finite sample latches the IIR
  state for the session, and `SetRecentPeak`'s unclamped float-to-int conversion is a second
  undefined behaviour on the VAD's input.
- **F-027 (minor, open)** — a malformed LLM response crashes the process through an
  unchecked `AsObject()`.
- **F-028 (minor, open)** — one Prolog engine, one knowledge base and one solution broadcast
  for every component in the process, with no shutdown.
- **Prompt injection, beyond anything the plugin can fix.** Untrusted text reaches a model
  and the model's reply reaches the application, which may hand it to a Prolog engine, a
  Cypher query or a filesystem path. The plugin cannot close this; it can refuse to be the
  easy half of it, by documenting the sinks (F-024) and by giving Cypher a parameter type
  that does not force concatenation (below). Accepted as a risk of the product's shape, not
  as an excuse for the two items that are fixable.
- **Cypher parameters are string-only.** `submitQuery` takes `TMap<FString, FString>`, so a
  caller with a number, a list, a label or a property name has no parameter to use. No
  finding: the fix changes the signature, which is a C-neo4j version and a Blueprint-ABI
  change, so it belongs in a change request on the 5.9 line.
- **No scheme restriction on any configured URL.** `URESTTTSComponent::URL`,
  `FOpenAICompatibleConfig::URL` and `ULangGraphComponent::endpoint` accept `http://`, and
  the API key goes to whatever they name. Three LLM presets default to `http://localhost`,
  which is the intended local case. No finding: a hard `https`-only rule would break the
  local case, and the right answer is a documented warning plus a log line when a credential
  is about to cross an unencrypted connection.
- **No bound on model and synthesis spend.** No rate limit, no call budget, no concurrency
  cap anywhere in M-llm or M-tts; a runaway Blueprint loop is a bill. Only the integrator
  can decide the budget; nothing tells them they own it.
- **Unbounded buffer growth in M-tts.** `Buffer` is never cleared by any code path, so
  synthesized PCM accumulates for a component's lifetime. A growth defect rather than an
  attack; it belongs in the tester's stress plan.
- **Vendored libsvm's model loader is unaudited.** An untrusted `.model` file reaches an
  `fscanf`-based C parser with no validation in front of it. No finding, because I have not
  found a specific defect in the vendored code and the vendored tree is unmapped.
- **F-002 (major, open)** — a linked and include-pathed gRPC, protobuf and abseil stack that
  nothing uses: attack surface and 330 MB for no benefit.
- **Two threats of the list have tests**, both in M-neo4j and both offline.
  `BoltPackStreamBoundsTests.cpp` holds 18 against F-021 and `Neo4jTlsVerificationTests.cpp`
  11 against F-020, under
  `FANTASIA/Source/FANTASIATests/Private/Security/`. They were the cheapest security
  evidence available, because the exported PackStream codec and the static decision-plus-
  application seam both run with no device, no network and no database. Every other "no test"
  in the surface sections above is a case owed, and the order they are owed in is the order
  of this list. F-022's and F-023's are next: both are pure functions, and neither needs a
  seam to reach. A third threat of the list has a test that is not mine:
  `Stress/BoltStressTests.cpp` measures F-056, and the measurement is the finding, so
  nothing is owed under `Private/Security/` for it — the battery that would belong to me is
  the one that pins the budget once there is a budget to pin. F-060 is the case to write
  next after F-022 and F-023, because its growth is assertable through the existing seam
  with no device, no network and no database.
- **No credential is in the repository, and the sprint did not add one.** The working tree
  was scanned against `HEAD` rather than by commit, because S-01's whole product is
  uncommitted: the text diff of all 24 modified tracked files, the six untracked source and
  script files, and the printable strings of both rebuilt DLLs, against sixteen credential
  patterns. The only match is the literal `APIKey="injected"` in `UnrealEditor-FANTASIATests.dll`,
  which is the ini-syntax fixture from `Quality/SecretConfigRobustnessTests.cpp:217`. The
  secret tests are disciplined about this by construction: one distinctive fake key
  (`SecretTestSupport.h`), GConfig entries written in memory and restored or removed in a
  destructor, and no `GConfig->Flush` anywhere, so nothing reaches a host project's ini. This
  says nothing about a *user's* assets, which is F-031 and F-029.
- **No CI, and a gate that a person has to remember to run.** `FANTASIA/run-tests.ps1`
  exists since US-027 and the Definition of Done names it, but nothing runs it on a schedule
  or on a push, and the suite still has to contain the tests. A threat model whose
  mitigations are only read is a document, not a control.
