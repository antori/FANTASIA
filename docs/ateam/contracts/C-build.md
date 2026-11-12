# C-build: Compilation, linking and runtime staging
- As of: J-0027
- Version: 1
- Provider: M-dist
- Consumers: M-asr, M-probabilistic, M-demo, M-tests
- Tests: FANTASIA/Source/FANTASIATests/Private/BootstrapContractTests.cpp
- Provenance: recovered

## Specification

Files: `FANTASIA.Build.cs`, `FANTASIATests.Build.cs`, `FANTASIAACE.Build.cs`, and the
two `.uplugin` descriptors. This contract exists because the coupling it describes is
invisible to the include graph: source code cannot see it, but source code depends on
it.

Preprocessor definitions the source may read:

- `WITH_AGRUM_BINDING` — 1 on Win64, 0 elsewhere. Read by M-tests and M-demo.
- `FANTASIA_WITH_CUDA` — 1 when CUDA was compiled into the module, 0 otherwise. Read by
  M-asr (`WhisperSubsystem.cpp`).
- `WITH_GRPC_BINDING` — defined to 1 unconditionally. Read by nobody (F-002).
- Third-party quieting and compatibility defines, read by no first-party source:
  `GOOGLE_PROTOBUF_INTERNAL_DONATE_STEAL_INLINE=0`, `GPR_FORBID_UNREACHABLE_CODE=0`,
  `_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS`, `_GNU_SOURCE`, `_UCRT_LEGACY_INFINITY`.

Engine modules available to first-party code:

- Public, from `FANTASIA`: `Core`, `Projects`, `AudioCapture`, `AudioCaptureCore`,
  `AudioMixer`, `AudioMixerCore`, `SignalProcessing`, plus `OpenSSL` and `zlib` added
  when the gRPC block loads.
- Private, from `FANTASIA`: `CoreUObject`, `Engine`, `Slate`, `SlateCore`, `Voice`,
  `Json`, `JsonUtilities`, `HTTP`, `WebSockets`.
- `FANTASIATests` additionally depends on `FANTASIA` itself and adds the `kdepp`
  include path.
- `FANTASIAACE` depends on `FANTASIA`, `ACERuntime` and `ACECore` publicly, and
  `Slate`, `SlateCore`, `Projects` and `Json` privately.

Third-party link surface: aGrUM as `agrumID.lib`, `agrumBN.lib`, `agrumBASE.lib` in
that order; gRPC with protobuf and abseil; OpenSSL 1.1.1t; whisper and ggml static
libraries, with the CUDA variants under the opt-in; SWI-Prolog as `libswipl.lib` and
`libswipl.dll.a`.

Runtime staging: `libswipl.dll` and the SWI-Prolog pack DLLs are registered as runtime
dependencies next to the module binary and must be copied beside the host project's
binaries, or the plugin fails to initialise at editor startup. Staging skips files
already deployed rather than forcing a copy.

Language settings in all three modules: C++20, exceptions enabled, RTTI off, no unity
build, explicit or shared PCHs. `CStandard = C11` is set by the `FANTASIA` module only;
`FANTASIATests` and `FANTASIAACE` leave it at the default.

Export macros: UBT generates `FANTASIA_API` and `FANTASIAACE_API` from the module
descriptors, so they belong to this contract and not to the code that uses them. They
are applied unevenly across the public headers, and a type without one cannot be used
from another module (F-012).

## Errors

- A missing third-party library directory makes `Directory.GetFiles` throw during
  project generation, so the failure is a build-configuration error, not a runtime one.
  This is what a user sees after `bootstrap --no-deps` followed by a C++ rebuild.
- `WITH_AGRUM_BINDING=0` is produced on non-Win64 but does not actually disable the
  binding, because M-probabilistic ignores it (F-005). Such a build fails to compile.
- An engine whose `CompatibleChangelist` differs from the committed `.modules` BuildId
  makes the editor refuse the prebuilt plugin, reporting that it was built with a
  different engine version.

## Invariants

- aGrUM static libraries are listed ID, then BN, then BASE. MSVC's single-pass linker
  requires each library to precede its dependency, and BASE is the leaf (ADR-0009).
- CUDA is never linked implicitly. Linking a prebuilt CUDA whisper requires an explicit
  opt-in; the default is the statically linked CPU backend (ADR-0011).
- `UnrealEditor-FANTASIA.dll` must import no `whisper.dll`. A stray `whisper.lib` under
  `ThirdParty/whisper_cpp/build/` flips the build to shared whisper, which breaks every
  clone. The dumpbin check under P-12 is what catches it; only `libswipl.dll` and the
  SWI-Prolog packs belong among the imports.
- A new third-party library, engine module or define is a change to this contract and
  needs an ADR (P-06).
- The `.uplugin` `EngineVersion` and the committed `.modules` BuildId describe the same
  engine. They move together or users are locked out (NFR-005).
- gRPC, protobuf and abseil are linked and include-pathed although no first-party source
  uses them. Removing them is a change request, not a cleanup, because it changes this
  contract and the release asset that carries the libraries (F-002).
- No test covers any of this at adoption. The `Tests:` path above is where a
  characterisation test — defines present, imports clean, BuildId matching — belongs.
