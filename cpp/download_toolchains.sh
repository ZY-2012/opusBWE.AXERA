#!/usr/bin/env bash
# Download everything needed to build OpusBWE.AXERA for AX650:
#   1) Arm GNU 9.2 aarch64 cross compiler
#   2) AX650 BSP SDK (ax_engine / ax_sys headers + libs)
#   3) Opus source at the pinned commit (OSCE/BWE enabled)
#   4) Opus OSCE-BWE model weights (opus_data -> dnn/bbwenet_data.c)
#
# Everything lands in cpp/toolchains/ and origin/ (both git-ignored).
set -euo pipefail

CPP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$CPP_DIR/.." && pwd)"
TOOLCHAINS_DIR="${CPP_DIR}/toolchains"
OPUS_DIR="${ROOT}/origin/opus"

GCC_URL="https://developer.arm.com/-/media/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz"
GCC_DIR="gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu"
BSP_URL="https://github.com/AXERA-TECH/ax650n_bsp_sdk.git"
OPUS_URL="https://github.com/xiph/opus.git"
OPUS_COMMIT="0145a6248c2171bb6c4e03fb54fe016b5123e37e"
OPUS_DATA_URL="https://media.xiph.org/opus/models/opus_data-a5177ec6fb7d15058e99e57029746100121f68e4890b1467d4094aa336b6013e.tar.gz"
OPUS_DATA_SHA="a5177ec6fb7d15058e99e57029746100121f68e4890b1467d4094aa336b6013e"

has() { command -v "$1" >/dev/null 2>&1; }

fetch() {  # fetch <url> <out>
  if has wget; then wget -c -q --show-progress -O "$2" "$1"
  else curl -L --retry 3 -C - -o "$2" "$1"; fi
}

mkdir -p "$TOOLCHAINS_DIR"

# ---------------------------------------------------------------- toolchain
if [[ -x "${TOOLCHAINS_DIR}/${GCC_DIR}/bin/aarch64-none-linux-gnu-g++" ]]; then
  echo "Found ${TOOLCHAINS_DIR}/${GCC_DIR}"
else
  echo "Downloading Arm GNU 9.2 aarch64 toolchain"
  ( cd "$TOOLCHAINS_DIR" && fetch "$GCC_URL" gcc.tar.xz && tar -xf gcc.tar.xz && rm -f gcc.tar.xz )
  echo "Done: ${TOOLCHAINS_DIR}/${GCC_DIR}"
fi

# ---------------------------------------------------------------------- BSP
if [[ -f "${TOOLCHAINS_DIR}/ax650n_bsp_sdk/msp/out/include/ax_engine_api.h" ]]; then
  echo "Found ${TOOLCHAINS_DIR}/ax650n_bsp_sdk"
else
  echo "Cloning AX650 BSP SDK (~600 MB)"
  if has git; then
    git clone --depth=1 "$BSP_URL" "${TOOLCHAINS_DIR}/ax650n_bsp_sdk"
  else
    ( cd "$TOOLCHAINS_DIR" && fetch "${BSP_URL%.git}/archive/refs/heads/main.tar.gz" bsp.tar.gz \
        && tar -xzf bsp.tar.gz && mv ax650n_bsp_sdk-main ax650n_bsp_sdk && rm -f bsp.tar.gz )
  fi
  echo "Done: ${TOOLCHAINS_DIR}/ax650n_bsp_sdk"
fi

# --------------------------------------------------------------------- Opus
if [[ -f "${OPUS_DIR}/dnn/osce.c" ]]; then
  echo "Found ${OPUS_DIR}"
else
  echo "Cloning Opus source"
  mkdir -p "$(dirname "$OPUS_DIR")"
  git clone "$OPUS_URL" "$OPUS_DIR"
  git -C "$OPUS_DIR" checkout "$OPUS_COMMIT"
fi

# ---------------------------------------------------------------- weights
if [[ -f "${OPUS_DIR}/dnn/bbwenet_data.c" ]]; then
  echo "Found Opus OSCE-BWE weights (dnn/bbwenet_data.c)"
else
  echo "Downloading Opus OSCE-BWE model weights (~130 MB)"
  fetch "$OPUS_DATA_URL" "${OPUS_DIR}/opus_data.tar.gz"
  echo "${OPUS_DATA_SHA}  ${OPUS_DIR}/opus_data.tar.gz" | sha256sum -c -
  tar -xzf "${OPUS_DIR}/opus_data.tar.gz" -C "$OPUS_DIR"
  rm -f "${OPUS_DIR}/opus_data.tar.gz"
  echo "Done: ${OPUS_DIR}/dnn/bbwenet_data.c"
fi

echo
echo "Toolchains ready. Build with:"
echo "  bash cpp/build_ax650.sh"
