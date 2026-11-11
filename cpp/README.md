# OpusBWE.AXERA C++ runtime

AX650 C++ runtime for the Opus OSCE-BWE bandwidth extension (16 kHz → 48 kHz).
The neural feature net (conv1 / conv2 / GRU / transposed conv) runs on the NPU
through `axmodel`; the adaptive filtering / shaping / upsampling uses the
upstream Opus C DSP (BSD-3-Clause), unchanged apart from one hook.

## Layout

```
src-equivalent: bwe_ax650.c   harness (wav 16k in -> wav 48k out)
                ax_bwe.c/.h   AX_ENGINE wrapper for the feature net (1 frame / call)
patches/osce_ax_bwe.patch     hook that replaces bbwe_feature_net() with the NPU call
build_ax650.sh                cross-compile script
```

## Build

```bash
export TOOLCHAIN_ROOT=/path/to/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu
export BSP_MSP_DIR=/path/to/ax650n_bsp_sdk/msp/out
export OPUS_SRC=/path/to/opus            # pinned commit 0145a6248c2171bb6c4e03fb54fe016b5123e37e
bash cpp/build_ax650.sh
```

## Run on board

```bash
export LD_LIBRARY_PATH=/soc/lib:$LD_LIBRARY_PATH
./bin/bwe_ax650 models/bwe_feature_net.axmodel input_16k.wav output_48k.wav
```

## Notes

- The C implementation delays the output by `OSCE_BWE_OUTPUT_DELAY` (21 samples);
  the harness removes this delay for file-based use.
- `bbwe_feature_net()` is replaced only when built with `-DUSE_AX_BWE`; without the
  patch the upstream CPU implementation is used.
