# Model weights

The float checkpoint used for the reference model is the official Opus data pack:

```
https://media.xiph.org/opus/models/opus_data-a5177ec6fb7d15058e99e57029746100121f68e4890b1467d4094aa336b6013e.tar.gz
sha256 a5177ec6fb7d15058e99e57029746100121f68e4890b1467d4094aa336b6013e
```

Extract it next to this folder (`dnn/models/bbwenet_v2.pth` inside the tarball) and
copy `bbwenet_v2.pth` here. The checkpoint is only needed to re-export the ONNX
model / rebuild the calibration set; the board package ships the quantized
`axmodel` directly.

License: BSD-3-Clause (xiph/opus).
