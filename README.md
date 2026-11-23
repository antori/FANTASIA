<!-- PROJECT LOGO -->
<br />
<p align="center">
  <a href="https://github.com/antori82/FANTASIA">
    <img src="images/banner_readme_1920x500.png" alt="FANTASIA">
  </a>

  <h3 align="center">FANTASIA</h3>
  <p align="center"><i>Framework for Advanced Natural Tools and Applications with Social Interactive Agents</i></p>

  <p align="center">
    <a href="https://github.com/antori82/FANTASIA/wiki"><strong>Documentation Wiki »</strong></a>
    ·
    <a href="https://codeberg.org/antori/FANTASIA-LangGraphDemo">LangGraph Demo Project</a>
    ·
    <a href="https://www.youtube.com/channel/UChH-PYQw-_IZJM7nYJywevg">YouTube</a>
  </p>
</p>

<!-- TABLE OF CONTENTS -->
<details open="open">
  <summary><h2 style="display: inline-block">Table of Contents</h2></summary>
  <ol>
    <li><a href="#about-the-project">About the Project</a></li>
    <li><a href="#whats-in-the-box">What's in the Box</a></li>
    <li><a href="#quick-start">Quick Start</a></li>
    <li><a href="#installation">Installation</a></li>
    <li><a href="#troubleshooting">Troubleshooting</a></li>
    <li><a href="#citing-fantasia">Citing FANTASIA</a></li>
    <li><a href="#projects-using-fantasia">Projects using FANTASIA</a></li>
    <li><a href="#contact">Contact</a></li>
    <li><a href="#acknowledgements">Acknowledgements</a></li>
  </ol>
</details>

<!-- ABOUT THE PROJECT -->
## About the Project

<img src="images/Interaction.png" alt="FANTASIA Interaction" height="300">
<br/><br/>

FANTASIA is a collection of tools for Unreal Engine that we have developed over the years to work with Embodied Conversational Agents at the University of Padua and at the University of Naples Federico II in Italy. Because we have been working with different characters for different tasks, we built these tools so that the functionalities of one agent could be transferred to another with minimal effort. Given the growing interest in Virtual Humans on UE, we decided to take all the dirty code we had, re-engineer it, and ship it to the community. We hope it will be useful to colleagues in academia, and we hope it will help anyone produce amazing things.

The plugin exposes everything through Blueprint-friendly Actor Components and UObjects, so you can wire up a full conversational pipeline — speech in, reasoning, speech out — without writing C++.

## What's in the Box

FANTASIA bundles a set of independent components. Use only what you need.

### Speech

| Component | Purpose |
| --- | --- |
| **Whisper ASR** | Local, on-device speech-to-text via [whisper.cpp](https://github.com/ggerganov/whisper.cpp). Optional CUDA GPU acceleration. Real-time microphone capture with VAD and streaming modes. |
| **ElevenLabs TTS** | Streaming text-to-speech via REST. |
| **Generic REST TTS** | Base class for plugging in any HTTP TTS service. |

### Language Models

| Component | Purpose |
| --- | --- |
| **OpenAI-Compatible LLM** | One component for OpenAI, Groq, Ollama, LM Studio, vLLM, and any other OpenAI-compatible API. Streaming and non-streaming, with Blueprint presets. |
| **LangGraph** | Drive a remote LangGraph workflow as a conversational agent. |

### Reasoning

| Component | Purpose |
| --- | --- |
| **SWI-Prolog** | Embedded Prolog engine. Assert/retract facts, run queries, build terms from Blueprints. |
| **Bayesian Networks** | Probabilistic inference using [aGrUM](https://agrum.gitlab.io/) (Lazy Propagation, Shafer-Shenoy, Variable Elimination). |
| **Influence Diagrams** | Decision-making under uncertainty using [aGrUM](https://agrum.gitlab.io/) (Shafer-Shenoy LIMID inference). |

### Data

| Component | Purpose |
| --- | --- |
| **Neo4j** | Cypher queries against a Neo4j graph database over the native Bolt protocol. Plain TCP framing (default, the same transport native Neo4j drivers use) or WebSocket, with optional TLS — works with Aura and local installs. |

### Math & ML

| Component | Purpose |
| --- | --- |
| **SVM** | Train and predict with Support Vector Machines via [libsvm](https://www.csie.ntu.edu.tw/~cjlin/libsvm/). |
| **Kernel Density Estimator** | Non-parametric PDF estimation. |

### Animation

| Integration | Purpose |
| --- | --- |
| **NVIDIA ACE** *(optional companion plugin)* | Audio-driven facial lip-sync and emotion. Ships as the separate **FANTASIAACE** companion plugin, which adds `UACERESTTTSComponent` and requires NVIDIA's [NV_ACE_Reference](https://developer.nvidia.com/ace). Both are available on this line, and `bootstrap.bat` installs the NVIDIA side by default — see [NVIDIA Audio2Face lip-sync](#nvidia-audio2face-lip-sync). Core FANTASIA does not depend on it; core produces the audio, ACE animates a face from it. |

## Quick Start

The fastest way to see FANTASIA running is the demo project:

> **[FANTASIA-LangGraphDemo](https://codeberg.org/antori/FANTASIA-LangGraphDemo)** — a ready-to-run UE project wiring LangGraph + Whisper + ElevenLabs into a working Conversational AI pipeline. Clone, open, hit play.

If you want to start from scratch, install the plugin into your own project (next section) and consult the [Wiki](https://github.com/antori82/FANTASIA/wiki) for component-by-component setup guides.

## Installation

FANTASIA is distributed prebuilt — for most users, no compilation is required.

> **This is the UE 5.8 release.** The `FANTASIAACE` companion plugin and the NVIDIA Audio2Face bundles are available on this line, and `bootstrap.bat` installs the NVIDIA side by default — see [NVIDIA Audio2Face lip-sync](#nvidia-audio2face-lip-sync).

1. If your UE project does not already have one, create a `Plugins` folder at the project root.
2. Get this repository and put the `FANTASIA` folder into that `Plugins` folder. There are two routes, and they are not interchangeable:
   - **Download the ZIP** (*Code → Download ZIP*) — the simplest route, and the one with no prerequisites. GitHub resolves the repository's Git LFS objects server-side, so the archive already contains the real prebuilt binaries.
   - **`git clone`** — needs **[Git LFS](https://git-lfs.com)** installed, with `git lfs install` run once on the machine. Git for Windows ships it and enables it during setup; elsewhere, and in CI images and containers, it is usually absent — `git lfs version` tells you. The prebuilt DLLs are tracked as LFS objects, and without the filter the clone still *succeeds*: every `.dll` arrives as a few lines of text under its real name, and the editor then reports a missing or corrupt module rather than anything about LFS. If you already cloned without it, install Git LFS and run `git lfs pull` in the clone.
3. From `Plugins/FANTASIA/FANTASIA/`, run `bootstrap.bat` (Windows) or `bootstrap.sh` (Linux/macOS). **By default it installs everything published for your platform**, fetched from this repo's GitHub Releases (these multi-gigabyte artifacts are release assets rather than tracked files — not in LFS either — which is what keeps a clone small). The default pulls:
   - **Whisper ASR model** (~574 MB).
   - **Prebuilt GPU whisper stack** (~540 MB, Windows) — flip `UWhisperSubsystem.Backend = GPU`, no rebuild.
   - **Win64 build-time static libs** (~330 MB: aGrUM, gRPC, OpenSSL) for C++ recompiles.
   - **NVIDIA Audio2Face bundles** (~4.9 GB, Windows only): `NV_ACE_Reference` plus the ready-to-use `Mark`, `James` and `Claire` sample characters, moved next to FANTASIA in your `Plugins` folder automatically. `--no-nvidia` skips them.

   The first three are everything the *core* plugin needs: ASR, LLM, TTS, Neo4j, Prolog and the Bayesian stack all work without the NVIDIA bundles. Add `--enable-plugins` to also switch the FANTASIA stack on in your project's `.uproject` (a `.uproject.bak` is written first). The script is idempotent (re-running is safe) and prints a final summary.
4. Launch Unreal Engine. The plugin appears in the **Plugins** list (already enabled if you used `--enable-plugins`; otherwise enable it in the Plugins panel).

**Flags:**

- `--no-gpu` — skip the CUDA GPU whisper stack.
- `--no-deps` — skip the build-time static libs (Blueprint-only users who never recompile).
- `--minimal` — Whisper model only (no GPU stack, no build deps, no NVIDIA bundles).
- `--enable-plugins` — also enable the FANTASIA stack in your project's `.uproject` (writes a `.uproject.bak` first).
- `--force` — re-download even if files are already present.
- `--no-ace` / `--no-mark` / `--no-james` / `--no-claire` / `--no-characters` / `--no-nvidia` — skip some or all of the NVIDIA Audio2Face bundles. The positive forms (`--ace`, `--characters`, `--nvidia`, …) are still accepted, and are no-ops now that the bundles are installed by default, so command lines written for earlier releases keep working.

### NVIDIA Audio2Face lip-sync

Audio2Face lives in the companion plugin `FANTASIAACE`, which drives a MetaHuman's face via NVIDIA's `NV_ACE_Reference`. Both are available on the UE 5.8 line:

- NVIDIA publishes its ACE plugins for **UE 5.6 only**, so every later engine version needs a hand-port. The UE 5.8 port is done, and the bundles in this line's release are built from it.
- `bootstrap.bat` installs them by default: `NV_ACE_Reference` plus the ready-to-use `Mark`, `James` and `Claire` sample characters, placed next to FANTASIA in your `Plugins` folder automatically. `FANTASIAACE` itself ships prebuilt for UE 5.8 in this repository. Enable `FANTASIA`, `FANTASIAACE`, `NV_ACE_Reference` and the character plugins in the editor's **Plugins** panel, or run `bootstrap.bat --enable-plugins`.
- **Upgrading a clone that already has them?** The bootstrap leaves NVIDIA plugins that are already installed exactly as they are. If this clone fetched them for an earlier engine version, delete them first — both from the `NVIDIA-UE57-Bundle/` staging folder next to `bootstrap.bat` (the name is historical; it is used on every engine line) and from your project's `Plugins` folder — or you keep the older build on UE 5.8.
- **Core FANTASIA does not depend on it.** Core produces the audio and `FANTASIAACE` only animates a face from it, so `--no-nvidia` costs you nothing else: ASR, LLM, TTS, Neo4j, Prolog and the Bayesian stack are unaffected.

Those NVIDIA plugins are redistributed in good faith for this non-commercial academic project, with all NVIDIA license texts retained; see `NV_ACE_Reference/README-FANTASIA-UE5.8-bundle.txt` for provenance. **DLSS / Streamline** are not bundled on any line — NVIDIA's RTX SDK license forbids standalone redistribution, and they aren't required for lip-sync; install them from the Epic Marketplace if you want the rendering-performance boost.

### Optional: Whisper on GPU (CUDA)

The shipped FANTASIA DLL uses CPU inference for Whisper ASR. GPU acceleration is opt-in via runtime dispatch: no plugin rebuild required.

If you ran `bootstrap.bat` / `bootstrap.sh` during installation, the GPU stack (`fantasia_whisper_cuda.dll` + `ggml*.dll` + CUDA runtime DLLs) is already staged in `FANTASIA/Binaries/Win64/`. To enable it, set `UWhisperSubsystem.Backend = GPU` (Blueprint or `DefaultGame.ini`) before calling `LoadModel`. The plugin's runtime dispatcher will `LoadLibrary` the CUDA DLL and route every `whisper_*` call through it; if the DLL or the CUDA runtime fails to load, FANTASIA logs a warning and falls back to CPU automatically.

If you skipped the GPU stack (`--no-gpu`) or your GPU is not sm_89 (RTX 40-series) and you want kernels tuned for your hardware, you can build the GPU stack yourself:

1. Install the NVIDIA CUDA Toolkit (12.x or 13.x).
2. Run `FANTASIA/build_whisper_cuda.bat --shared` (Windows) or `FANTASIA/build_whisper_cuda.sh --shared` (Linux). The script builds whisper.cpp as a shared library with CUDA enabled and stages `fantasia_whisper_cuda.dll` (plus its `ggml*.dll` and `cudart64_*.dll` / `cublas64_*.dll` dependencies) into `FANTASIA/Binaries/Win64/` (or `Binaries/Linux/`).

Passing `--static` instead compiles CUDA directly into `UnrealEditor-FANTASIA.dll`; that path requires a full plugin rebuild and is rarely needed (the shared-library default avoids it).

> **See the [Wiki](https://github.com/antori82/FANTASIA/wiki) for the current installation procedure.**

## Third-party licenses

FANTASIA bundles or fetches code from several third-party projects (whisper.cpp, ggml, aGrUM, gRPC, SWI-Prolog, OpenSSL, the NVIDIA CUDA runtime, OpenAI's Whisper model weights, and others). See [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) for the per-component license summary. If you redistribute FANTASIA, you must also carry the corresponding upstream license text from each component.

## Troubleshooting

If the editor says a plugin was **"built with a different engine version"**, you are running an engine build whose compatible changelist differs from the one the shipped DLLs were built against. Rebuild the plugin from source, or use the release matching your engine version.

If the editor reports a FANTASIA module as **missing or corrupt**, and you installed by `git clone`, check Git LFS first: run `git lfs version`, then `git lfs pull` in the clone. A clone made without the LFS filter leaves each prebuilt DLL as a few lines of pointer text under its real name, so the files look present and are not loadable.

If the editor reports that your project cannot be launched because of FANTASIA, verify that the SWI-Prolog runtime DLLs shipped alongside `UnrealEditor-FANTASIA.dll` in `FANTASIA/Binaries/Win64/` have been copied next to your project's binaries. The Prolog component loads its dependencies at editor startup; if they are missing, the plugin fails to initialize.

For component-specific problems (API keys, microphone permissions, model file locations, etc.), see the [Wiki](https://github.com/antori82/FANTASIA/wiki).

## Citing FANTASIA

If you use FANTASIA for your research, please cite the following papers:

> Origlia, A., Cutugno, F., Rodà, A., Cosi, P., & Zmarich, C. (2019). FANTASIA: a framework for advanced natural tools and applications in social, interactive approaches. *Multimedia Tools and Applications*, 78(10), 13613–13648.

> Origlia, A., Di Bratto, M., Di Maro, M., & Mennella, S. (2022). Developing Embodied Conversational Agents in the Unreal Engine: The FANTASIA Plugin. *Proceedings of the 30th ACM International Conference on Multimedia*, 6950–6951.

## Projects using FANTASIA

- **[SPECIAL](https://www.specialprojectunina.com/)** — Supporting Patients with Embodied Conversational Interfaces and Argumentative Language (funded by the University of Naples Federico II): a Virtual Human to increase health literacy and combat stereotypes about epilepsy.
- **[VISIT3D](https://www.logogramma.com/artificial-intelligence/progetti-di-ricerca-e-sviluppo-sperimentale-co-finanziati/visit3d/)** — Logogramma S.r.l. in collaboration with the URBAN/ECO Research Center (University of Naples Federico II): a Virtual Reality guide for reflection on the visual arts.

If you build something with FANTASIA we'd love to hear about it — get in touch!

<!-- CONTACT -->
## Contact

**Antonio Origlia** — antonio.origlia [at] unior.it

- Project: [github.com/antori82/FANTASIA](https://github.com/antori82/FANTASIA)
- Wiki: [github.com/antori82/FANTASIA/wiki](https://github.com/antori82/FANTASIA/wiki)
- YouTube: [Project channel](https://www.youtube.com/channel/UChH-PYQw-_IZJM7nYJywevg)

<!-- ACKNOWLEDGEMENTS -->
## Acknowledgements

FANTASIA would not be the same without the precious contribution from the Linguistics field provided by:

- [Dr. Maria Di Maro](https://www.linkedin.com/in/maria-di-maro-b43261120/) — (PhD) Common ground management and clarification request generation
- [Dr. Martina Di Bratto](https://www.linkedin.com/in/martina-di-bratto-95034322b/) — (PhD) Argumentative dialogue management
- [Dr. Elena Sofia Safina](https://www.linkedin.com/in/elena-sofia-safina) - (PhD) Stigma manifestations in language

Special thanks to the students who contributed to FANTASIA:

- [Antimo De Biase](https://www.linkedin.com/in/antimo-de-biase-3660b6202/) — Neo4j component prototype development
- Fabrizio Musella — aGrUM integration prototype development
- [Roberto Basile Giannini](https://www.linkedin.com/in/rbgdotbrush/) — General improvements and aGrUM updates (Influence Diagrams integration)
- [Marco Galantino](https://www.linkedin.com/in/marco-galantino-232a07300) — TTS general interface
- [Yegor Napadystyy](https://www.linkedin.com/in/yegor-napadystyy-b5482b2bb/) — Audio2Face prototype connector
- [Lorenzo Bracale](https://www.linkedin.com/in/lorenzo-bracale-6001b1331/) — Prolog component prototype development
- [Raffaele Di Maso](https://www.linkedin.com/in/raffaele-di-maso-a976b6377/) — FANTASIA Interaction Model development
