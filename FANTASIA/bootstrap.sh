#!/usr/bin/env bash
set -euo pipefail

# ─────────────────────────────────────────────────────────────────────────────
#  FANTASIA artifact fetcher (Linux/macOS)
#
#  Downloads the heavy artifacts that don't live in git LFS anymore:
#    - Whisper ASR model            -> Resources/ggml-large-v3-turbo-q5_0.bin
#    - Prebuilt GPU whisper stack   -> Binaries/Linux/ (libfantasia_whisper_cuda.so + deps)
#    - Win64 build-time static libs -> ThirdParty/{aGrUM,grpc,OpenSSL}/...
#       (Win64 only at the moment; Linux build deps build from source via UE)
#
#  Default with no args: install everything published for this platform and
#  engine line. Use --no-* flags to opt out.
#
#  NOTE: the NVIDIA ACE / Mark / James / Claire bundles (interactive-MetaHuman
#  lip-sync) are Win64-only (TensorRT/cuBLAS + Windows A2F runtime), and on
#  the UE 5.8 line they are not published at all -- see bootstrap.bat. They
#  are opt-in here too, and asking for one explains that instead of failing
#  on a 404.
#
#  Usage:
#    bootstrap.sh                  install everything available (default)
#    bootstrap.sh --no-gpu         skip the CUDA GPU whisper stack
#    bootstrap.sh --no-deps        skip build-time libs (Blueprint-only users)
#    bootstrap.sh --minimal        Whisper model only
#    bootstrap.sh --force          re-download even if present
#    ( --ace / --mark / --james / --claire / --characters / --nvidia exist for
#      parity with bootstrap.bat and are refused with an explanation here;
#      the --no-* forms are accepted, and are no-ops, for old command lines. )
# ─────────────────────────────────────────────────────────────────────────────

RELEASE_TAG="runtime-v3.0"
RELEASE_BASE="https://github.com/antori82/FANTASIA/releases/download/${RELEASE_TAG}"
MODEL_ASSET="whisper-large-v3-turbo-q5_0.bin"
GPU_ASSET="fantasia-gpu-cuda13-linux.tar.gz"
DEPS_ASSET="fantasia-build-deps-win64.tar.gz"

# Is the prebuilt Linux CUDA whisper stack published in ${RELEASE_TAG}? No
# release has carried one yet -- the GPU stack is built for Win64 only, and on
# Linux whisper runs on the CPU backend (or you build the CUDA stack yourself
# with build_whisper_cuda.sh). Flip this to 1 in the same commit that publishes
# ${GPU_ASSET}; the fetch below is already wired.
GPU_ASSET_AVAILABLE=0

FETCH_MODEL=1
FETCH_GPU=1
FETCH_DEPS=1
WANT_NVIDIA=0
FORCE=0
while [ $# -gt 0 ]; do
    case "$1" in
        --no-model)     FETCH_MODEL=0 ;;
        --no-gpu)       FETCH_GPU=0 ;;
        --no-deps)      FETCH_DEPS=0 ;;
        --minimal)      FETCH_GPU=0; FETCH_DEPS=0 ;;
        --ace|--mark|--james|--claire|--characters|--nvidia)
            WANT_NVIDIA=1 ;;
        --no-ace|--no-mark|--no-james|--no-claire|--no-characters|--no-nvidia)
            : ;;  # already the default: the NVIDIA bundles are opt-in
        --force)        FORCE=1 ;;
        *) echo "Unknown argument: $1"; echo "Usage: bootstrap.sh [--no-model|--no-gpu|--no-deps|--minimal] [--force]"; exit 1 ;;
    esac
    shift
done

# Refuse an NVIDIA request up front, before downloading anything: it was asked
# for explicitly, so a clear refusal now beats a 404 a gigabyte later.
if [ "${WANT_NVIDIA}" = "1" ]; then
    cat <<'EOF'

 The NVIDIA ACE / Mark / James / Claire bundles cannot be installed here.

 Why, twice over:
   - They are Win64-only: the Audio2Face runtime is a Windows build on
     TensorRT/cuBLAS, so there is no Linux or macOS version to fetch.
   - On the UE 5.8 line they are not published for any platform. NVIDIA
     ships its ACE plugins for UE 5.6 only, so every later engine version
     needs a hand-port, and the UE 5.8 port does not exist yet.

 What you can do:
   - Run bootstrap.sh with no NVIDIA flag for a complete core install. Core
     FANTASIA needs none of these: ASR, LLM, TTS, Neo4j, Prolog and the
     Bayesian stack all work without them. Only Audio2Face lip-sync (the
     FANTASIAACE companion plugin) does, and FANTASIAACE is not available
     for UE 5.8 either.
   - For lip-sync today, use the UE 5.7 line on Windows: the `5.7` branch of
     this repository, whose bootstrap.bat fetches the bundles from
     runtime-v2.0.
EOF
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RES_DIR="${SCRIPT_DIR}/Resources"
BIN_DIR="${SCRIPT_DIR}/Binaries/Linux"
TP_DIR="${SCRIPT_DIR}/ThirdParty"

mkdir -p "${RES_DIR}" "${BIN_DIR}" "${TP_DIR}"

command -v curl >/dev/null 2>&1 || { echo "ERROR: curl not found on PATH."; exit 1; }
command -v tar  >/dev/null 2>&1 || { echo "ERROR: tar not found on PATH.";  exit 1; }

# ── Fetch whisper model ──────────────────────────────────────────────────
if [ "${FETCH_MODEL}" = "1" ]; then
    MODEL_PATH="${RES_DIR}/ggml-large-v3-turbo-q5_0.bin"
    SKIP_MODEL=0
    if [ "${FORCE}" = "0" ] && [ -f "${MODEL_PATH}" ]; then
        # Size sanity check: the q5_0 model is ~574 MB.
        MODEL_SIZE=$(stat -c%s "${MODEL_PATH}" 2>/dev/null || stat -f%z "${MODEL_PATH}")
        if [ "${MODEL_SIZE}" -gt 104857600 ]; then
            echo "Whisper model already present at ${MODEL_PATH} (skipping)."
            SKIP_MODEL=1
        else
            echo "Existing model at ${MODEL_PATH} looks incomplete (${MODEL_SIZE} bytes); re-downloading."
        fi
    fi
    if [ "${SKIP_MODEL}" = "0" ]; then
        echo
        echo "Downloading whisper model ${MODEL_ASSET} from ${RELEASE_TAG} ..."
        echo "  (this is ~574 MB)"
        if ! curl -L --fail --progress-bar -o "${MODEL_PATH}" "${RELEASE_BASE}/${MODEL_ASSET}"; then
            echo "ERROR: failed to download ${MODEL_ASSET}."
            rm -f "${MODEL_PATH}"
            exit 1
        fi
        echo "Model downloaded to ${MODEL_PATH}"
    fi
fi

# ── Fetch GPU stack ──────────────────────────────────────────────────────
if [ "${FETCH_GPU}" = "1" ] && [ "${GPU_ASSET_AVAILABLE}" = "0" ]; then
    echo
    echo "NOTE: no prebuilt Linux CUDA whisper stack is published in ${RELEASE_TAG}, so it"
    echo "      is being skipped. Whisper runs on the CPU backend; to get the GPU backend,"
    echo "      build it yourself with ./build_whisper_cuda.sh. Nothing else is affected."
    FETCH_GPU=0
fi

if [ "${FETCH_GPU}" = "1" ]; then
    GPU_MARKER="${BIN_DIR}/libfantasia_whisper_cuda.so"
    SKIP_GPU=0
    if [ "${FORCE}" = "0" ] && [ -f "${GPU_MARKER}" ]; then
        echo "GPU stack already present at ${BIN_DIR} (skipping)."
        SKIP_GPU=1
    fi
    if [ "${SKIP_GPU}" = "0" ]; then
        GPU_TGZ="$(mktemp -t fantasia-gpu.XXXXXX.tar.gz)"
        echo
        echo "Downloading GPU whisper stack ${GPU_ASSET} from ${RELEASE_TAG} ..."
        echo "  (this is ~540 MB)"
        if ! curl -L --fail --progress-bar -o "${GPU_TGZ}" "${RELEASE_BASE}/${GPU_ASSET}"; then
            echo "ERROR: failed to download ${GPU_ASSET}."
            rm -f "${GPU_TGZ}"
            exit 1
        fi
        echo "Extracting into ${BIN_DIR} ..."
        tar -xf "${GPU_TGZ}" -C "${BIN_DIR}"
        rm -f "${GPU_TGZ}"
        echo "GPU stack staged in ${BIN_DIR}"
    fi
fi

# ── Fetch build-time static libs ─────────────────────────────────────────
if [ "${FETCH_DEPS}" = "1" ]; then
    DEPS_MARKER="${TP_DIR}/aGrUM/Libraries/agrumBASE.lib"
    SKIP_DEPS=0
    if [ "${FORCE}" = "0" ] && [ -f "${DEPS_MARKER}" ]; then
        echo "Build-time static libs already present under ${TP_DIR} (skipping)."
        SKIP_DEPS=1
    fi
    if [ "${SKIP_DEPS}" = "0" ]; then
        DEPS_TGZ="$(mktemp -t fantasia-deps.XXXXXX.tar.gz)"
        echo
        echo "Downloading Win64 build deps ${DEPS_ASSET} from ${RELEASE_TAG} ..."
        echo "  (~60 MB compressed, expands to ~330 MB)"
        if ! curl -L --fail --progress-bar -o "${DEPS_TGZ}" "${RELEASE_BASE}/${DEPS_ASSET}"; then
            echo "ERROR: failed to download ${DEPS_ASSET}."
            rm -f "${DEPS_TGZ}"
            exit 1
        fi
        echo "Extracting into ${SCRIPT_DIR} ..."
        tar -xf "${DEPS_TGZ}" -C "${SCRIPT_DIR}"
        rm -f "${DEPS_TGZ}"
        echo "Build deps staged under ${TP_DIR}"
    fi
fi

echo
echo "─────────────────────────────────────────────────"
echo " Bootstrap complete."
echo "─────────────────────────────────────────────────"
echo " CPU users: nothing else needed; the plugin is ready."
echo " GPU users: set UWhisperSubsystem.Backend = GPU on the"
echo " subsystem and reopen the editor. The runtime dispatcher"
echo " will dlopen libfantasia_whisper_cuda.so automatically,"
echo " once you have built it (see build_whisper_cuda.sh)."
