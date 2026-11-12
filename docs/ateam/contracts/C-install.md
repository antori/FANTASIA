# C-install: Getting from a clone to a running plugin
- As of: J-0027
- Version: 1
- Provider: M-dist
- Consumers: none
- Tests: FANTASIA/Source/FANTASIATests/Private/BootstrapContractTests.cpp
- Provenance: recovered

## Specification

Files: `bootstrap.bat`, `bootstrap.sh`, `build_whisper_cuda.bat`,
`build_whisper_cuda.sh`, `setup_whisper.bat`, `setup_whisper.ps1`, `setup_whisper.sh`,
`enable-plugins.ps1`, the committed `Binaries/` trees, and the README sections that
describe them. This is the contract with the user, and the one the project's third goal
rests on.

`bootstrap.bat` / `bootstrap.sh`, run from `Plugins/FANTASIA/FANTASIA/`, installs
everything by default (ADR-0008), fetching from one named GitHub release (ADR-0007).
**Which release is the most install-breaking fact in the project and belongs here**: the
5.8 line fetches from `runtime-v3.0`, set as `RELEASE_TAG` at the top of each script.
The NVIDIA assets on that tag are `nv-ace-reference-ue58-full.tar.gz` and
`nv-audio2face-{mark,james,claire}-ue58.tar.gz`, staged into a directory still named
`NVIDIA-UE57-Bundle/` (F-013). The default run pulls:

- the Whisper ASR model, about 574 MB
- the prebuilt GPU whisper stack, about 540 MB, enabled later by setting
  `UWhisperSubsystem.Backend = GPU`
- the Win64 build-time static libraries — aGrUM, gRPC, OpenSSL — about 330 MB, needed
  only for C++ recompiles
- the NVIDIA ACE Audio2Face runtime, about 656 MB, and the `Mark`, `James` and `Claire`
  sample characters, about 1.5 GB each for the first two and 1.4 GB for Claire

Opt-out flags: `--no-model`, `--no-gpu`, `--no-deps`, `--no-ace`, `--no-mark`,
`--no-james`, `--no-claire`, `--no-characters`, `--no-nvidia`, `--minimal`. Plus `--enable-plugins`,
which switches the stack on in the host `.uproject` after writing a `.uproject.bak`,
and `--force`, which refetches regardless of what is present.

`build_whisper_cuda.{bat,sh}` builds the CUDA whisper stack locally. `--shared` (the
default path users want) stages `fantasia_whisper_cuda.dll` with its `ggml*.dll`,
`cudart64_*.dll` and `cublas64_*.dll` into `Binaries/Win64/` and needs no plugin
rebuild; `--static` compiles CUDA into the module and does.

## Errors

- Run from a standalone clone rather than from inside a project's `Plugins/` folder,
  bootstrap cannot find a `Plugins/` folder to install into and leaves the NVIDIA
  plugins staged in `FANTASIA/NVIDIA-UE57-Bundle/` for the user to move. It says so.
- A failed download leaves that component absent and reports it in the final summary;
  other components still install.
- With the GPU stack absent or a CUDA runtime that will not load, setting `Backend = GPU`
  logs a warning and falls back to CPU (C-asr).
- With the SWI-Prolog runtime DLLs not beside the host project's binaries, the plugin
  fails to initialise at editor startup. This is the project's most common installation
  failure and the README's one troubleshooting entry.
- An engine version mismatch against the committed binaries makes the editor refuse the
  plugin (C-build).

## Invariants

- Re-running bootstrap is safe: present files are not refetched, and the run ends with a
  summary of what was done. Idempotence is the property users rely on, so a change that
  breaks it breaks this contract.
- The default run needs no flags and no decisions: the turnkey path is the unflagged one
  (ADR-0008).
- No compilation is required on any default path. `--no-deps` is the flag for users who
  will never recompile, and it must stay safe for them (NFR-005).
- Nothing in the fetched set is tracked in git, and bootstrap is the only place that
  knows where the assets live (P-17).
- `--enable-plugins` writes `.uproject.bak` before touching the `.uproject`, always.
- In `bootstrap.bat`, `SCRIPT_DIR` is captured **before** the argument-parsing loop,
  because Windows `shift` shifts `%0` and a later `%~dp0` would resolve to the working
  directory, landing every artifact one directory too high. `tar` is pinned to the
  bundled bsdtar so drive-letter extract paths work. `set` assignments carry no trailing
  space. `bootstrap.sh` needs none of the first, because bash's `shift` leaves `$0`
  alone.
- Every fetched component appears in THIRD_PARTY_LICENSES.md with its upstream licence
  (P-16). DLSS and Streamline are never fetched or bundled: their licence forbids
  standalone redistribution, and lip-sync does not need them.
- No test covers any of this at adoption.
