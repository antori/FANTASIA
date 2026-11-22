# Third-party licenses

FANTASIA bundles or fetches third-party software. Each component is
listed below with its license and upstream source. Where a `LICENSE`
or `COPYING` file ships with the component in its source tree, that
file is authoritative; this document is a summary index.

Nothing here grants any rights beyond what the upstream licenses do.
If you redistribute FANTASIA (or your project containing it), you must
also redistribute the corresponding license text from each component.

## Tracked in this repository

### whisper.cpp + ggml
- **Source:** `FANTASIA/ThirdParty/whisper_cpp/`
- **Upstream:** https://github.com/ggerganov/whisper.cpp
- **License:** MIT — Copyright (c) 2023-2024 The ggml authors
- **License text:** `FANTASIA/ThirdParty/whisper_cpp/LICENSE`

### aGrUM
- **Source:** `FANTASIA/ThirdParty/aGrUM/`
- **Upstream:** https://agrum.gitlab.io/
- **License:** LGPL v3
- **Notes:** FANTASIA links the prebuilt static library
  `aGrUM/Libraries/aGrUM.x64.lib` against the FANTASIA module. LGPL
  permits static linking provided users can relink against a different
  aGrUM build; we ship the headers so this is satisfiable.

### gRPC + Protocol Buffers
- **Source:** `FANTASIA/ThirdParty/grpc/`
- **Upstream:** https://github.com/grpc/grpc , https://github.com/protocolbuffers/protobuf
- **License:** Apache License 2.0 (gRPC), BSD 3-Clause (protobuf)
- **License text:** included in the gRPC source distribution

### OpenSSL 1.1.1t
- **Source:** `FANTASIA/ThirdParty/OpenSSL/1.1.1t/`
- **Upstream:** https://www.openssl.org/source/old/1.1.1/
- **License:** Dual OpenSSL + SSLeay license (the legacy 1.1.x licensing
  scheme; later 3.x versions move to Apache 2.0).

### nlohmann/json
- **Source:** `FANTASIA/ThirdParty/nlohmann/`
- **Upstream:** https://github.com/nlohmann/json
- **License:** MIT — Copyright (c) 2013-2024 Niels Lohmann

### kdepp
- **Source:** `FANTASIA/ThirdParty/kdepp/`
- **Upstream:** https://github.com/duncanmcn/kdepp
- **License:** MIT — by Duncan McNae

### SWI-Prolog
- **Source:** `FANTASIA/ThirdParty/SWIProlog/`
- **Upstream:** https://www.swi-prolog.org/
- **License:** SWI-Prolog license, equivalent to LGPL v2 with a static-
  linking exception. Prebuilt `libswipl.dll` and the runtime DLLs in
  `Binaries/Win64/` (archive4pl, bdb4pl, crypto4pl, etc.) are shipped
  under this license.

## Fetched at install time via `bootstrap.{bat,sh}` (runtime-v3.0 Release)

Everything in this section is downloaded from a GitHub Release of this
repository and is **not** tracked in git. The release is the one named by
`RELEASE_TAG` in `bootstrap.bat` / `bootstrap.sh`; on this engine line that
is `runtime-v3.0`. Earlier engine lines (UE 5.6 / 5.7) have their own
release tag and their own NVIDIA bundles, so read the tag out of the
bootstrap script of the branch you cloned rather than assuming this one.

A default run with no flags installs **all seven** assets below; the
`--no-*` flags opt out of individual components. Because the default
install brings every one of them down, all the licenses below apply to
an ordinary installation — none of this section describes an opt-in
extra.

### Whisper model — `whisper-large-v3-turbo-q5_0.bin`
Installed as `FANTASIA/Resources/ggml-large-v3-turbo-q5_0.bin`.
- **Origin:** The model is a ggml-format quantization of OpenAI's
  Whisper large-v3-turbo. The original weights and inference code were
  released by OpenAI in September 2022.
- **License:** MIT — Copyright (c) 2022 OpenAI
- **Upstream:** https://github.com/openai/whisper ;
  ggml conversions are typically hosted under
  https://huggingface.co/ggerganov/whisper.cpp

### Prebuilt GPU stack — `fantasia-gpu-cuda13-win64.tar.gz`
Bundles:

- `fantasia_whisper_cuda.dll`, `ggml.dll`, `ggml-base.dll`,
  `ggml-cpu.dll`, `ggml-cuda.dll` — built from `whisper.cpp` source
  (MIT, see above).
- `cudart64_13.dll`, `cublas64_13.dll`, `cublasLt64_13.dll` — NVIDIA
  CUDA 13 redistributable runtime libraries.

For the NVIDIA components:

- **License:** NVIDIA CUDA Toolkit EULA, "Attachment A — Distributable
  Code". The three filenames above are explicitly listed as permitted
  for redistribution alongside end-user applications.
- **Upstream:** https://docs.nvidia.com/cuda/eula/ (full EULA text)
- **Attribution:** NVIDIA CUDA, Copyright © 2007-2025 NVIDIA Corporation.
  Used under the terms of the CUDA Toolkit EULA.

If you redistribute the GPU stack outside of FANTASIA's own
distribution, link the NVIDIA EULA above prominently in your own
licensing documentation.

### Win64 build-time libraries — `fantasia-build-deps-win64.tar.gz`
Extracted into `FANTASIA/ThirdParty/`. It carries the prebuilt Win64
static libraries for the components already listed under "Tracked in
this repository" — aGrUM, OpenSSL, SWI-Prolog, gRPC and Protocol
Buffers — whose licenses and upstreams are given in that section. The
headers and license files of those components are tracked in the
repository; only the compiled libraries are fetched, under the same
licenses.

### NVIDIA ACE bundle — `nv-ace-reference-ue58-full.tar.gz`
A complete `NV_ACE_Reference` plugin (NVIDIA version
`2.5.0-20250614-2282`) for Unreal Engine 5.8: NVIDIA's MIT-licensed
Unreal modules recompiled unmodified against UE 5.8, together with the
unmodified NVIDIA components the plugin needs. Staged into
`NVIDIA-UE57-Bundle/` (the directory name is historical and is used on
every engine line) and then moved next to FANTASIA in the project's
`Plugins/` folder.

- **Upstream:** https://docs.nvidia.com/ace/ace-unreal-plugin/latest/ ;
  NVIDIA's own plugin is published on the Epic Games / Fab Marketplace
  for UE 5.6. The UE 5.8 recompilation is this project's; provenance is
  documented in `NV_ACE_Reference/README-FANTASIA-UE5.8-bundle.txt`
  inside the bundle.

**Unreal modules — MIT.** Thirteen modules, shipped as source under
`Source/` and as `UnrealEditor-<module>.dll` (Development and DebugGame)
under `Binaries/Win64/`:
`A2FCommon`, `A2FLocal`, `A2FRemote`, `ACECore`, `ACEEditor`,
`ACEGraphNode`, `ACERuntime`, `AIMWrapper`, `AnimStream`, `GPTLocal`,
`LegacyA2FRemote`, `OmniverseAudioMixer`, `OmniverseLiveLink`.
The two ARKit pose-mapping assets under `Content/`, and `Config/` and
`Resources/`, are part of the same MIT-licensed plugin.

- **License:** MIT — SPDX-FileCopyrightText: Copyright (c) 2022-2025
  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
- **License text:** `NV_ACE_Reference/LICENSE (Plugin).txt`

**ACE client library — NVIDIA proprietary.** `Binaries/Win64/nvacl.dll`,
with its import library and headers under `Source/ThirdParty/Nvacl/`.
Redistributed unmodified; your use is governed by NVIDIA's own terms.
The open-source crates it is built from are credited individually in
`Source/ThirdParty/Nvacl/AttributionNotices/` (about one hundred Rust
crates, predominantly MIT and Apache-2.0, including `tokio`, `tonic`,
`hyper`, `rustls`, `ring`, `serde` and `prost`); that directory is the
authoritative license text for them.

**NVIGI inference runtime — NVIDIA proprietary, plus bundled
open-source libraries.** `ThirdParty/Nvigi/Binaries/Win64/`:

- NVIDIA proprietary: `nvaim.core.framework.dll`, `audio2x.dll`,
  `nvaim.plugin.a2e.trt.cuda.dll`, `nvaim.plugin.a2f.trt.cuda.dll`,
  `nvaim.plugin.a2x.cloud.grpc.dll`, `nvaim.plugin.a2x.pipeline.dll`,
  `nvaim.plugin.animgraph.cloud.grpc.dll`,
  `nvaim.plugin.gpt.ggml.cuda.dll`, `nvaim.plugin.hwi.cuda.dll`.
- NVIDIA TensorRT runtime: `nvinfer_10.dll`, `nvinfer_plugin_10.dll` —
  redistributable runtime libraries under the NVIDIA TensorRT license
  (https://docs.nvidia.com/deeplearning/tensorrt/sla/).
- NVIDIA CUDA redistributables: `cublas64_12.dll`, `cublasLt64_12.dll` —
  CUDA Toolkit EULA, "Attachment A — Distributable Code"
  (https://docs.nvidia.com/cuda/eula/).
- Open source: `abseil_dll.dll` (Apache-2.0), `libprotobuf.dll`
  (BSD-3-Clause), `cares.dll` (MIT), `re2.dll` (BSD-3-Clause),
  `libcrypto-3-x64.dll` and `libssl-3-x64.dll` (OpenSSL 3, Apache-2.0),
  `zlib1.dll` and `zlibwapi.dll` (zlib license).
- **License text:** `ThirdParty/Nvigi/3rd-party-licenses.md` inside the
  bundle carries the full, verbatim license text for every open-source
  component NVIGI is built from — around fifty entries, among them
  abseil-cpp, gRPC, protobuf, BoringSSL, c-ares, re2, zlib, libcurl,
  mbedtls, ONNX and ONNX Runtime, OpenFST, OpenTelemetry, llama.cpp,
  whisper.cpp, GGML, Pybind11, GoogleTest and xxhash, together with the
  model-weight entries (`whisper-small`, `phi-2-GGUF`, `BGE Weights`)
  those components carry. That file is authoritative for all of them.

**Model packages — NVIDIA Models Community License.**
`ThirdParty/Nvigi/Models/` ships the cloud-plugin and GPT model
manifests (`nvaim.plugin.a2x.cloud`, `nvaim.plugin.animgraph.cloud`,
`nvaim.plugin.gpt.cloud`, `nvaim.plugin.gpt.ggml`): `model.json` /
`nvaim.model.config.json` descriptors, plus the per-package `License.txt`
and `Notice.txt` where NVIDIA ships them. The headers for the runtime are
under `ThirdParty/Nvigi/Public/`.

- **License:** NVIDIA Models Community License (2025.04.15), shipped in
  the bundle as `NVIDIA Models Community License (2025.04.15) FINAL.pdf`.
  Note in particular that it does **not** permit offering these models as
  a text-to-speech or animation service.

**Not in this bundle:** the local GPT (Minitron) weights under
`ThirdParty/Nvigi/Models/nvaim.plugin.gpt.ggml`, about 2.4 GB, are
deliberately omitted — FANTASIA talks to language models through its own
components and uses ACE only for Audio2Face and Audio2Emotion. Only the
manifests are shipped. `*.pdb` symbols and `Intermediate/` are omitted
too.

### NVIDIA Audio2Face characters — `nv-audio2face-{mark,james,claire}-ue58.tar.gz`
Three character plugins, `NvAudio2FaceMark`, `NvAudio2FaceJames` and
`NvAudio2FaceClaire` (NVIDIA version `2.4.0-20250610-2270`), recompiled
for UE 5.8 and staged and installed exactly as the ACE bundle is. Each
one carries the local network weights that `NV_ACE_Reference` does not:
without at least one of them, Audio2Face has no model to run.

**Unreal module — MIT.** One module per plugin — `A2FLocalMark`,
`A2FLocalJames`, `A2FLocalClaire` — shipped as source under `Source/`
and as `UnrealEditor-A2FLocal<Name>.dll` (Development and DebugGame).

- **License:** MIT, the same text NVIDIA ships with the ACE plugin.
- **License text:** `LICENSE (Plugin).txt` in each plugin.

**Audio2Face network — NVIDIA Models Community License.**
`ThirdParty/Nvigi/Models/nvaim.plugin.a2f.trt/{…}/`, model version `v3.0`,
"Audio2Face <Name> (diffusion) (60 fps)": the TensorRT network
`data/network.trt` with `network_info.json` and `trt_info.json`, the
blend-shape and model data `data/model_data_<Name>.npz`,
`data/bs_skin_<Name>.npz`, `data/bs_tongue_<Name>.npz`, and the
configuration files `data/a2f_config.json`,
`data/model_config_<Name>.json`, `data/bs_skin_config_<Name>.json`,
`data/bs_tongue_config_<Name>.json`, with `model.json` and `VERSION.md`.

- **License:** NVIDIA Models Community License (2025.04.15), shipped in
  each plugin as `NVIDIA Models Community License (2025.04.15) FINAL.pdf`.

**Audio2Emotion network — NVIDIA proprietary.**
`ThirdParty/Nvigi/Models/nvaim.plugin.a2e.trt/{…}/`, version
`2.1.0-2025-03-17`: `a2e_network.trt` with `a2e_ms_config.json`,
`network_info.json`, `trt_info.json`, `model.json`, `device.id` and
`VERSION.md`. The same package is present in all three character
bundles.

- **License:** the package ships its own notice at
  `…/PACKAGE-LICENSES/audio2emotion-nets-LICENSE.txt` — Copyright (c)
  2022 NVIDIA CORPORATION, all intellectual property rights retained by
  NVIDIA. That file is authoritative for these weights; refer to NVIDIA
  for redistribution terms.

**Prefer to take NVIDIA's licenses at the source?** Install NVIDIA's
official ACE plugin from the Epic Games / Fab Marketplace and use the
much smaller `nv-ace-reference-ue58-overlay.tar.gz`, which contains only
the recompiled MIT module DLLs, the `.modules` file, the `.uplugin` and
the MIT license, and is extracted over your own NVIDIA installation.

## Required separately, not bundled

### DLSS and Streamline
**Not bundled on any engine line, and not fetched by any bootstrap
flag.** NVIDIA's RTX SDK license forbids standalone redistribution, and
neither is needed for Audio2Face lip-sync. Install them from the Epic
Games / Fab Marketplace if you want the rendering-performance boost.

### MetaHuman content
Licensed by Epic Games and obtained from Epic directly (MetaHuman
Creator / Fab / Quixel Bridge). FANTASIA redistributes no MetaHuman
assets.

### NVIDIA local GPT (Minitron) weights
Omitted from the ACE bundle as described above. If you enable NVIDIA's
`GPTLocal` module and it reports a missing model, fetch that model
directory from NVIDIA's official plugin release.
