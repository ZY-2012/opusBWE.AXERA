#!/usr/bin/env bash
# Cross-compile OpusBWE.AXERA for AX650 (aarch64).
#
#   TOOLCHAIN_ROOT=/path/to/gcc-arm-9.2-... \
#   BSP_MSP_DIR=/path/to/ax650n_bsp_sdk/msp/out \
#   OPUS_SRC=/path/to/opus \
#   bash cpp/build_ax650.sh
#
# The script builds libopus (OSCE/BWE enabled) with the small NPU patch applied
# to dnn/osce.c, then links bin/bwe_ax650.
set -euo pipefail
CPP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$CPP_DIR/.." && pwd)"
TOOLCHAIN_ROOT="${TOOLCHAIN_ROOT:-$ROOT/cpp/toolchains/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu}"
BSP_MSP_DIR="${BSP_MSP_DIR:-$ROOT/cpp/toolchains/ax650n_bsp_sdk/msp/out}"
OPUS_SRC="${OPUS_SRC:-$ROOT/origin/opus}"
BUILD="$ROOT/cpp/build"

# auto-fetch toolchain / BSP / Opus source / weights when they are missing
if [[ ! -x "$TOOLCHAIN_ROOT/bin/aarch64-none-linux-gnu-gcc" || \
      ! -f "$BSP_MSP_DIR/include/ax_engine_api.h" || \
      ! -f "$OPUS_SRC/dnn/osce.c" || \
      ! -f "$OPUS_SRC/dnn/bbwenet_data.c" ]]; then
  echo "== toolchain / BSP / Opus source missing, running cpp/download_toolchains.sh"
  bash "$CPP_DIR/download_toolchains.sh"
fi

if [[ ! -x "$TOOLCHAIN_ROOT/bin/aarch64-none-linux-gnu-gcc" ]]; then
  echo "ERROR: toolchain not found: $TOOLCHAIN_ROOT" >&2
  exit 2
fi
if [[ ! -f "$BSP_MSP_DIR/include/ax_engine_api.h" ]]; then
  echo "ERROR: AX650 BSP not found: $BSP_MSP_DIR" >&2
  exit 2
fi
if [[ ! -f "$OPUS_SRC/dnn/osce.c" ]]; then
  echo "ERROR: Opus source not found: $OPUS_SRC" >&2
  exit 2
fi

# apply the NPU hook (idempotent)
if ! grep -q "USE_AX_BWE" "$OPUS_SRC/dnn/osce.c"; then
  patch -d "$OPUS_SRC" -p1 < "$CPP_DIR/patches/osce_ax_bwe.patch"
fi

cat > "$BUILD-toolchain.cmake" <<CMAKE
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER $TOOLCHAIN_ROOT/bin/aarch64-none-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER $TOOLCHAIN_ROOT/bin/aarch64-none-linux-gnu-g++)
CMAKE

rm -rf "$BUILD"
cmake -S "$OPUS_SRC" -B "$BUILD" \
  -DCMAKE_TOOLCHAIN_FILE="$BUILD-toolchain.cmake" \
  -DOPUS_OSCE=ON -DBUILD_SHARED_LIBS=OFF -DOPUS_BUILD_PROGRAMS=OFF -DOPUS_BUILD_TESTING=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="-DUSE_AX_BWE -I$CPP_DIR"
cmake --build "$BUILD" --target opus -j"$(nproc)"

mkdir -p "$ROOT/bin"
CC="$TOOLCHAIN_ROOT/bin/aarch64-none-linux-gnu-gcc"
$CC -O3 -mcpu=cortex-a55 -DUSE_AX_BWE -DENABLE_OSCE -DENABLE_OSCE_BWE \
  -I "$OPUS_SRC/include" -I "$OPUS_SRC/dnn" -I "$OPUS_SRC/silk" -I "$OPUS_SRC/celt" \
  -I "$OPUS_SRC/src" -I "$BUILD" -I "$CPP_DIR" -I "$BSP_MSP_DIR/include" \
  "$CPP_DIR/bwe_ax650.c" "$CPP_DIR/ax_bwe.c" "$BUILD/libopus.a" \
  -L "$BSP_MSP_DIR/lib" -lax_engine -lax_sys -lax_interpreter -lm -lpthread \
  -o "$ROOT/bin/bwe_ax650"
"$TOOLCHAIN_ROOT/bin/aarch64-none-linux-gnu-strip" --strip-unneeded "$ROOT/bin/bwe_ax650"
echo "Built: $ROOT/bin/bwe_ax650"
