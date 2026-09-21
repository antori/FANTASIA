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
| **NVIDIA ACE** *(optional companion plugin)* | Audio-driven facial lip-sync and emotion. Ships as the separate **FANTASIAACE** companion plugin, which adds `UACERESTTTSComponent` and requires NVIDIA's [NV_ACE_Reference](https://developer.nvidia.com/ace). Core FANTASIA produces the audio; FANTASIAACE animates a face from it. See [Installation](#installation). |

## Quick Start

The fastest way to see FANTASIA running is the demo project:

> **[FANTASIA-LangGraphDemo](https://codeberg.org/antori/FANTASIA-LangGraphDemo)** — a ready-to-run UE project wiring LangGraph + Whisper + ElevenLabs into a working Conversational AI pipeline. Clone, open, hit play.

If you want to start from scratch, install the plugin into your own project (next section) and consult the [Wiki](https://github.com/antori82/FANTASIA/wiki) for component-by-component setup guides.

## Installation

FANTASIA is distributed prebuilt — for most users, no compilation is required.

1. If your UE project does not already have one, create a `Plugins` folder at the project root.
2. Clone or download this repository and put the `FANTASIA` and `FANTASIAACE` folders into that `Plugins` folder.
3. From `Plugins/FANTASIA/FANTASIA/`, run `bootstrap.bat` (Windows) or `bootstrap.sh` (Linux/macOS). **By default it installs everything** needed for a typical interactive MetaHuman, fetched from this repo's GitHub Releases (hosted there instead of LFS to keep clones lean). On Windows the default pulls:
   - **Whisper ASR model** (~574 MB).
   - **Prebuilt GPU whisper stack** (~540 MB) — flip `UWhisperSubsystem.Backend = GPU`, no rebuild.
   - **Win64 build-time static libs** (~330 MB: aGrUM, gRPC, OpenSSL) for C++ recompiles.
   - **NVIDIA ACE A2F runtime** (~656 MB) + **"Mark"** (~1.5 GB), **"James"** (~1.5 GB) and **"Claire"** (~1.4 GB, Audio2Face-3D diffusion model) sample A2F characters.

   Because it runs from inside your `Plugins/` folder, the NVIDIA ACE/Mark/James/Claire plugins are **placed automatically** next to FANTASIA — no manual move. Add `--enable-plugins` to also switch the whole stack on in your project's `.uproject` (a `.uproject.bak` is written first). The script is idempotent (re-running is safe) and prints a final summary.
4. Launch Unreal Engine. The plugins appear in the **Plugins** list (already enabled if you used `--enable-plugins`; otherwise enable them in the Plugins panel).

> If you run the bootstrap from a **standalone clone** (not yet inside a project's `Plugins/`), it can't find a `Plugins/` folder to install into, so the NVIDIA plugins stay staged in `FANTASIA/NVIDIA-UE57-Bundle/` — move those folders into your `Plugins/` yourself.

**Opt-out flags** (default installs everything; use these to skip parts):

- `--no-gpu` — skip the CUDA GPU whisper stack.
- `--no-deps` — skip the build-time static libs (Blueprint-only users who never recompile).
- `--no-ace` — skip the NVIDIA ACE A2F runtime.
- `--no-mark` / `--no-james` / `--no-claire` — skip one sample character; `--no-characters` skips all three.
- `--no-nvidia` — skip ACE + Mark + James + Claire entirely (no MetaHuman lip-sync).
- `--minimal` — Whisper model only (no GPU, deps, or NVIDIA components).
- `--enable-plugins` — also enable the FANTASIA stack in your project's `.uproject` (writes a `.uproject.bak` first).
- `--force` — re-download even if files are already present.

### NVIDIA Audio2Face lip-sync (bundled)

Audio2Face lives in the companion plugin `FANTASIAACE`, which drives a MetaHuman's face via NVIDIA's `NV_ACE_Reference`. The bootstrap fetches a complete, UE 5.7-recompiled NVIDIA stack — `NV_ACE_Reference` (A2F runtime + models) plus the ready-to-use `Mark`, `James` and `Claire` sample characters — and, because you run it from inside your `Plugins/` folder, places them next to FANTASIA automatically (a standalone clone instead leaves them staged in `FANTASIA/NVIDIA-UE57-Bundle/` for you to move). Enable FANTASIA + FANTASIAACE + NV_ACE_Reference (+ a character) — or pass `--enable-plugins` to do that for you — and use `UACERESTTTSComponent` / `UACEElevenLabsTTSComponent` to animate the face from synthesized speech.

- **You don't need lip-sync:** run `bootstrap.bat --no-nvidia` and copy only `FANTASIA`. The core plugin works out of the box on any system.
- **DLSS / Streamline:** not bundled — NVIDIA's RTX SDK license forbids standalone redistribution, and they aren't required for lip-sync. Install them from the Epic Marketplace if you want the rendering-performance boost.
- **MetaHumans:** the `Mark`/`James`/`Claire` samples are included; for your own character add a MetaHuman via MetaHuman Creator / Fab / Quixel Bridge.

The recompiled NVIDIA plugins target UE 5.7 (the official Marketplace build is 5.6 only). They're redistributed in good faith for this non-commercial academic project, with all NVIDIA license texts retained — see `NVIDIA-UE57-Bundle/NV_ACE_Reference/README-FANTASIA-UE5.7-bundle.txt`.

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

Special thanks to the students who contributed to FANTASIA:

- [Antimo De Biase](https://www.linkedin.com/in/antimo-de-biase-3660b6202/) — Neo4j component prototype development
- Fabrizio Musella — aGrUM integration prototype development
- [Roberto Basile Giannini](https://www.linkedin.com/in/rbgdotbrush/) — General improvements and aGrUM updates (Influence Diagrams integration)
- [Marco Galantino](https://www.linkedin.com/in/marco-galantino-232a07300) — TTS general interface
- [Yegor Napadystyy](https://www.linkedin.com/in/yegor-napadystyy-b5482b2bb/) — Audio2Face prototype connector
- [Lorenzo Bracale](https://www.linkedin.com/in/lorenzo-bracale-6001b1331/) — Prolog component prototype development
- [Raffaele Di Maso](https://www.linkedin.com/in/raffaele-di-maso-a976b6377/) — FANTASIA Interaction Model development
