# OpusBWE.AXERA

**AX650 上的 16 kHz → 48 kHz 音频超分 / 带宽扩展（Opus OSCE-BWE）。轻量、真流式：axmodel 288 KB + C++ 可执行 2.3 MB，逐 20 ms 帧因果推理，C++ 全链路 RTF 0.099（约 10× 实时）。**

![RTF](https://img.shields.io/badge/RTF-0.099%20%2810x%20realtime%29-brightgreen) ![Size](https://img.shields.io/badge/axmodel-288%20KB-green) ![Lightweight](https://img.shields.io/badge/lightweight-0.37M%20params-blue) ![Streaming](https://img.shields.io/badge/streaming-causal%20%7C%2020%20ms-blue) ![License](https://img.shields.io/badge/license-Apache--2.0-blue) ![HuggingFace](https://img.shields.io/badge/%F0%9F%A4%97-OpusBWE.AXERA-yellow)

- HuggingFace: https://huggingface.co/HY-2012/opusBWE.AXERA
- GitHub: https://github.com/ZY-2012/opusBWE.AXERA
- 上游: https://github.com/xiph/opus

> **AX650 板端实测说明**：AudioSR、LavaSR、FlowHigh、FastWave、OpusBWE 均已在 AX650N 板端跑通。
>
> **对比入口**
> GitHub: [LavaSR](https://github.com/ZY-2012/LavaSR.AXERA) · [AudioSR](https://github.com/ZY-2012/AudioSR.AXERA) · [FlowHigh](https://github.com/ZY-2012/FlowHigh.AXERA) · [FastWave](https://github.com/ZY-2012/FastWave.AXERA)
> HuggingFace: [LavaSR](https://huggingface.co/HY-2012/LavaSR.AXERA) · [AudioSR](https://huggingface.co/HY-2012/AudioSR.AXERA) · [FlowHigh](https://huggingface.co/HY-2012/FlowHigh.AXERA) · [FastWave](https://huggingface.co/HY-2012/FastWave.AXERA)

## 目录

- 快速开始
- 结果
- 推理速度
- 模型转换
- 数据来源
- License

## 快速开始（AX650 板端）

从 HuggingFace 下载 `models/`、`bin/`、`python/`、`samples/`，在包根目录运行：

```bash
# C++（默认）
bash run_ax650.sh samples/listen/16k/speech_2_input_16k.wav output.wav

# Python 封装
bash run_ax650.sh samples/listen/16k/speech_2_input_16k.wav output.wav --python
```

输入必须是 **16 kHz** wav，输出固定 48 kHz。

## 结果

| 版本 | 输入（16 kHz） | OpusBWE.AXERA 输出（48 kHz） |
|---|---|---|
| **speech_2** | [▶](https://cdn.jsdelivr.net/gh/ZY-2012/opusBWE.AXERA@main/samples/listen/16k/speech_2_input_16k.wav)<br><img src="samples/listen/spectrograms/speech_2_input_spec.png" width="360"> | [▶](https://cdn.jsdelivr.net/gh/ZY-2012/opusBWE.AXERA@main/samples/listen/48k/speech_2_output_48k.wav)<br><img src="samples/listen/spectrograms/speech_2_output_spec.png" width="360"> |
| **speech_3** | [▶](https://cdn.jsdelivr.net/gh/ZY-2012/opusBWE.AXERA@main/samples/listen/16k/speech_3_input_16k.wav)<br><img src="samples/listen/spectrograms/speech_3_input_spec.png" width="360"> | [▶](https://cdn.jsdelivr.net/gh/ZY-2012/opusBWE.AXERA@main/samples/listen/48k/speech_3_output_48k.wav)<br><img src="samples/listen/spectrograms/speech_3_output_spec.png" width="360"> |

### 16k → 48k 频谱对比（上：输入，下：输出）

<img src="samples/listen/spectrograms/speech_2.png" width="720">

## 推理速度：RTF

AX650（BSP V3.10.2），20 ms 一帧（320 → 960 点）：

| 链路 | RTF |
|---|---:|
| C++（NPU + Opus DSP） | **0.099** |
| Python 封装（同上） | ≈ 0.10 |

## 模型转换

```bash
python model_convert/export_feature_net.py --frames 1 --out export/bwe_feature_net_t1.onnx
python model_convert/make_calib.py samples/listen/16k/*.wav
pulsar2 build --config model_convert/pulsar2_config.json
```

## 数据来源

- 上游模型/代码：xiph/opus（BSD-3-Clause），OSCE-BWE 权重 `bbwenet_v2.pth`
- 16 kHz 示例：公开人声样例经 48k → 16k 降采样

## License

Apache-2.0（本仓库代码）。上游 Opus 及 OSCE-BWE 模型为 BSD-3-Clause。
