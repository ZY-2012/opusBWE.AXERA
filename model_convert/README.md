# 模型转换（OSCE-BWE feature net → AX650 AXMODEL）

```bash
# 1) 导出 ONNX（流式：1 帧/次，conv 历史 + GRU 状态作为输入输出）
python model_convert/export_feature_net.py --frames 1 --out export/bwe_feature_net_t1.onnx

# 2) 真实特征标定（必须用真实 16 kHz 语音算出的 114 维特征）
python model_convert/make_calib.py samples/listen/16k/*.wav

# 3) Pulsar2 8.0 编译
pulsar2 build --config model_convert/pulsar2_config.json
```

- 输入：`features (1,1,114)`、`conv1_state (1,114,2)`、`conv2_state (1,128,2)`、`gru_state (1,1,128)`
- 输出：`cond (1,2,128)` + 三个更新后的状态
- 量化：U16 激活 / S8 权重 / U16 输出，MinMax

> 标定必须用真实特征（范围 −8…+1）。用随机噪声（±0.5）标定时输入被裁，
> 板端 cond cosine 只有 0.80；真实特征标定为 0.9998。
