# Verification profile: build-dev
- Coder: build-dev
- Modules: M-dist
- Language: csharp
- Reviewer: reviewer
- Tester: tester
- As of: J-0059
## This sprint (S-01)

One task: **T-02, the gate** (US-027, `FANTASIA/run-tests.ps1`, new file). It is the
sprint's highest-value deliverable because P-04 has no mechanism at all today —
`gates.task_completed` is `null` — and everything else this sprint is verified by hand
until it exists.

What the reviewer examines hardest, in order:

1. **The script cannot exit zero without having compiled and run tests.** Three ways to
   verify nothing (the compile failed, the editor wrote no report, the report names zero
   tests) and one way to look wrong (a deliberately failing test) are each demonstrated
   with their exit code, not argued. A gate that passes when the editor failed to boot is
   the single failure mode that makes a gate worse than none, because it is believed.
2. **The exit code comes from the exported report, not from the editor's exit status.**
   `UnrealEditor-Cmd.exe` exits zero in cases that are not successes.
3. **Nothing machine-specific is hard-coded** beyond the one documented default engine
   path, and the maintainer's host project is neither read nor written. ADR-0016 says why:
   that project associates with an absent UE 5.6, enables `LowEntryExtStdLib` which has no
   5.8 build, and carries its own C++ modules — three obstacles that belong to a project
   outside this repository and cannot be fixed from here, which is why the gate generates
   its own harness instead.
4. **The measurement is a measurement.** A cold and a warm number, taken from runs, with
   the proposed `gates.task_completed` command following from the warm one by the rule in
   US-027's acceptance. "Fast enough" is not a judgement this task is allowed to make
   without the number.
5. **P-07.** The script is the story. Not a bootstrap change, not a `.Build.cs` change, not
   a fix for F-013, F-014 or F-015 while passing through.

Note for the reviewer: a gate run recompiles the tracked DLLs under
`FANTASIA/Binaries/Win64/`, so a dirty working tree after this task is expected and P-12's
refresh-commit discipline is unchanged. The script says so before it exits.

## Reviewer checklist

- Contracts provided: **C-build** — the defines the source may read (`WITH_AGRUM_BINDING`, `FANTASIA_WITH_CUDA`, and `WITH_GRPC_BINDING`, which nothing reads), the engine modules available to first-party code, the third-party link surface, and the runtime staging of `libswipl.dll` and the SWI-Prolog packs, without which the plugin fails to initialise at editor startup. A missing third-party directory throws during project generation, so the failure is a build-configuration error, not a runtime one — which is what a user sees after `bootstrap --no-deps` followed by a rebuild. **C-install** — re-running bootstrap must be safe and must refetch nothing; the default run must need no flags and no decisions; a failed download leaves that component absent, reports it in the summary, and does not stop the others.
- Contracts consumed: none. M-dist is the only module with no code dependency on another — and the only one every other module depends on at build time rather than through an include, which is why `C-build` exists at all.
- Invariants: aGrUM links ID, then BN, then BASE (ADR-0009; ADR-0015 amends it and the names are confirmed by *building*, never asserted). CUDA is never linked implicitly — a prebuilt CUDA whisper needs an explicit opt-in (ADR-0011). **`UnrealEditor-FANTASIA.dll` must import no `whisper.dll`**: a stray `whisper.lib` under `ThirdParty/whisper_cpp/build/` flips the build to shared whisper and breaks every clone, and only `libswipl.dll` and the SWI-Prolog packs belong among the imports. The `.uplugin` `EngineVersion` and the committed `.modules` BuildId describe the same engine and move together. `--enable-plugins` writes `.uproject.bak` before touching anything. In `bootstrap.bat`, `SCRIPT_DIR` is captured **before** the argument loop, because Windows `shift` shifts `%0`; `tar` is pinned to the bundled bsdtar; `set` assignments carry no trailing space. Nothing fetched is tracked in git, and bootstrap is the only place that knows where the assets live.
- Language checks: see `references/languages/csharp.md`; here especially — this is UnrealBuildTool C#, not .NET application code: no `Directory.GetFiles` on a path that may not exist (that is the `--no-deps` failure), platform guards around every Win64-only block, and no new `PublicDefinitions` entry without a reader.
- Principles at risk: **P-12** — this owner *is* P-12: whenever runtime source lands, the prebuilt binaries are rebuilt, dumpbin-verified and committed as their own commit. F-016 is the open proof that the rule needs a mechanism, since `94e906a8` refreshed one of the two binary sets. **P-16** — every bundled or fetched component carries its licence text; aGrUM 3.x's is owed. **P-17** — heavy artifacts stay in release assets. **P-06** — the whole third-party link surface is this module's. **P-19** — the bar's "touches no dependency" clause makes most changes here non-minor by construction.
- NFRs touched: **NFR-005** (installability: zero compilation steps, BuildId matching the engine).
- Known weak spots: **F-014** (major) bootstrap asks `runtime-v3.0` for four `nv-*-ue58` assets that were never published, so the default 5.8 install fails for anyone not passing `--no-nvidia`. **F-016** (major) FANTASIAACE ships 5.7 binaries under a 5.8 `.uplugin`. **F-002** (major) gRPC, protobuf and abseil are linked, include-pathed and defined, and used by nothing — roughly 330 MB of fetched libraries for a dead dependency left by the Azure removal. **F-013** (minor) the 5.8 line stages `ue58` assets into a directory named `NVIDIA-UE57-Bundle/`. **F-015** (minor) `bootstrap.sh` names a Linux GPU asset that has never existed. `FANTASIA.Build.cs` is the most-churned first-party file in the project at 29 commits. **Nothing here has a single test.**
- Evidence to produce: for any binary refresh, the dumpbin import list showing no `whisper.dll` and the `.modules` BuildId matching the engine's effective compatible changelist; for any bootstrap change, an idempotence run (run twice, second run fetches nothing) and a run of each opt-out flag it touches; for any link-list change, the build log, not the changelog. For T-02: five runs with their exit codes (refused engine, clean pass, each of the three verify-nothing cases, deliberate failure), the cold and warm wall-clock numbers, and the command proposed for `gates.task_completed` with the rule that produced it.

## Tester plan

- Unit: there is no C# test harness and adding one is a dependency decision (P-06), so the testable surface is behavioural rather than unit. The highest-value check, and the one the planner named as the best candidate for its own request: **bootstrap verifies that every asset it names exists on the release before it starts downloading**. That single check would have caught F-013, F-014 and F-015, and the same discipline would have caught the half-finished binaries refresh behind F-016.
- Security surface: `docs/ateam/threat-model.md` § M-dist — the security engineer writes it in T-01, this sprint. For T-02 specifically: the script takes paths from the environment and generates source files that a compiler then runs over, so every path is validated and quoted before use, nothing is written inside the repository's tracked tree except the binaries a compile produces, and the generated project carries no key, no endpoint and no credential. In outline for the module: bootstrap downloads roughly 5 GB of archives over HTTPS and extracts them into the user's project, so the asset URLs, their integrity and the extraction paths are the surface; note that `RELEASE_BASE` is built from the `antori82` owner name while the repository is now `antori`, so the download URL depends on a GitHub rename redirect for a username that could in principle be reclaimed. No checksum is verified today.
- Stress (S-01): the gate's own runtime is this sprint's one measurement, and it is the input to every later decision about what a task gate may do. Record cold and warm separately; a warm run is what a coder pays per task. Beyond that: not applicable in the usual sense. The equivalent is a cold-machine install: a clean clone, no cached assets, bootstrap with no flags, through to the editor loading both plugins — which is also the only end-to-end check that US-018 and US-019 are true, and both are currently `ready` rather than `done` precisely because nobody can say they are.
- Tools: `dumpbin` for the import check (the one mechanical verification this module has), `gh release view` to compare the named assets against the published ones, and the editor for a load check.
- Paths: no test path exists for this module, and that is the gap. A `BootstrapContractTests.cpp` under `FANTASIA/Source/FANTASIATests/Private/` is named by US-018 and US-019 and does not exist yet; writing it is the tester's, and it would have to shell out, which is itself worth a decision.
