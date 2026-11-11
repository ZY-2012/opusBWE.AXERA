# OpusBWE.AXERA C++ runtime

AX650 C++ runtime for the Opus OSCE-BWE bandwidth extension (16 kHz → 48 kHz).
The neural feature net (conv1 / conv2 / GRU / transposed conv) runs on the NPU
through the axmodel; the adaptive filtering / shaping / upsampling uses the
upstream Opus C DSP (BSD-3-Clause), unchanged apart from one hook.

## Build

```bash
bash cpp/download_toolchains.sh     # Arm GNU 9.2 + AX650 BSP + Opus source + BWE weights (~1.3 GB, once)
bash cpp/build_ax650.sh             # -> bin/bwe_ax650
```

`download_toolchains.sh` fetches:

| 依赖 | 来源 | 大小 |
|---|---|---|
| Arm GNU 9.2 aarch64 交叉编译器 | developer.arm.com | ~500 MB |
| AX650 BSP SDK（ax_engine/ax_sys） | github.com/AXERA-TECH/ax650n_bsp_sdk | ~620 MB |
| Opus 源码（pin `0145a624`） | github.com/xiph/opus | ~30 MB |
| OSCE-BWE 权重（`dnn/bbwenet_data.c`） | media.xiph.org `opus_data` | ~130 MB |

`build_ax650.sh` calls `download_toolchains.sh` automatically if anything is missing.
Already have the dependencies? Override the paths:

```bash
TOOLCHAIN_ROOT=/path/to/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu \
BSP_MSP_DIR=/path/to/ax650n_bsp_sdk/msp/out \
OPUS_SRC=/path/to/opus \
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
