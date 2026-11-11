#!/usr/bin/env python3
"""Export the Opus OSCE-BWE feature net (conv1/conv2/GRU/tconv) to ONNX — streaming version.

The NPU part of the bandwidth extension model turns the 114-dim per-frame features
into the 128-dim conditioning used by the host-side adaptive filters. The conv
layers keep a 2-frame history (streaming), the GRU keeps its hidden state, so the
graph can be called frame-chunk by frame-chunk without boundary artefacts.

inputs : features (1,T,114), conv1_state (1,114,2), conv2_state (1,128,2), gru_state (1,1,128)
outputs: cond (1,2T,128), conv1_state, conv2_state, gru_state
"""
import argparse
import os
import sys

import numpy as np
import torch

HERE = os.path.dirname(os.path.abspath(__file__))
PYDIR = os.path.join(HERE, "..", "python")
sys.path.insert(0, PYDIR)

from models import model_dict  # noqa: E402

CKPT = os.path.join(HERE, "..", "model", "bbwenet_v2.pth")


class FeatureNetStream(torch.nn.Module):
    def __init__(self, feature_net):
        super().__init__()
        self.ft = feature_net

    def forward(self, features, conv1_state, conv2_state, gru_state):
        x = features.permute(0, 2, 1)                       # (1, 114, T)
        xin1 = torch.cat([conv1_state, x], dim=-1)          # (1, 114, T+2)
        c1 = torch.tanh(self.ft.conv1(xin1))                # (1, 128, T)
        state1 = xin1[:, :, -2:]
        xin2 = torch.cat([conv2_state, c1], dim=-1)         # (1, 128, T+2)
        c2 = torch.tanh(self.ft.conv2(xin2))                # (1, 128, T)
        state2 = xin2[:, :, -2:]
        c3 = torch.tanh(self.ft.tconv(c2))                  # (1, 128, 2T)
        c3 = c3.permute(0, 2, 1)                            # (1, 2T, 128)
        y, state3 = self.ft.gru(c3, gru_state)
        return y, state1, state2, state3


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ckpt", default=CKPT)
    ap.add_argument("--out", default=os.path.join(HERE, "..", "export", "bwe_feature_net.onnx"))
    ap.add_argument("--frames", type=int, default=90)
    args = ap.parse_args()

    ck = torch.load(args.ckpt, map_location="cpu", weights_only=False)
    model = model_dict["bbwenet"](**ck["setup"]["model"]["kwargs"])
    model.load_state_dict(ck["state_dict"])
    model.eval()

    net = FeatureNetStream(model.feature_net).eval()
    T = args.frames
    feats = torch.randn(1, T, 114) * 0.5
    s1 = torch.zeros(1, 114, 2)
    s2 = torch.zeros(1, 128, 2)
    s3 = torch.zeros(1, 1, 128)
    with torch.no_grad():
        y = net(feats, s1, s2, s3)
    print("torch out", [tuple(t.shape) for t in y])

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    torch.onnx.export(net, (feats, s1, s2, s3), args.out, opset_version=13,
                      input_names=["features", "conv1_state", "conv2_state", "gru_state"],
                      output_names=["cond", "conv1_state_out", "conv2_state_out", "gru_state_out"],
                      dynamo=False)
    print("onnx ->", args.out, f"{os.path.getsize(args.out)/1e6:.2f} MB")

    try:
        import onnxruntime as ort
        sess = ort.InferenceSession(args.out, providers=["CPUExecutionProvider"])
        outs = sess.run(None, {"features": feats.numpy(), "conv1_state": s1.numpy(),
                               "conv2_state": s2.numpy(), "gru_state": s3.numpy()})
        ref = [t.detach().numpy() for t in y]
        for name, o, r in zip(["cond", "s1", "s2", "s3"], outs, ref):
            cos = float(np.dot(o.ravel(), r.ravel()) /
                        (np.linalg.norm(o) * np.linalg.norm(r) + 1e-12))
            print(f"  {name}: cosine {cos:.7f} max err {np.abs(o-r).max():.2e}")
    except ImportError:
        pass


if __name__ == "__main__":
    main()
