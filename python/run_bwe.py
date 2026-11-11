#!/usr/bin/env python3
"""Opus OSCE-BWE reference: 16 kHz wav -> 48 kHz wav (PyTorch, extracted from xiph/opus)."""
import argparse
import os
import sys

import numpy as np
import torch
from scipy.io import wavfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from models import model_dict  # noqa: E402
from utils.bwe_features import load_inference_data  # noqa: E402

CKPT = os.path.join(HERE, "..", "model", "bbwenet_v2.pth")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input", help="16 kHz wav")
    ap.add_argument("output", help="48 kHz wav")
    ap.add_argument("--ckpt", default=CKPT)
    args = ap.parse_args()

    ckpt = torch.load(args.ckpt, map_location="cpu", weights_only=False)
    setup = ckpt["setup"]
    model = model_dict[setup["model"]["name"]](*setup["model"]["args"], **setup["model"]["kwargs"])
    model.load_state_dict(ckpt["state_dict"])
    model.eval()

    signal, features = load_inference_data(args.input, **setup["data"])
    print("signal", tuple(signal.shape), "features", tuple(features.shape))
    with torch.no_grad():
        out = model(signal.view(1, 1, -1), features.unsqueeze(0)).squeeze().numpy()
    wavfile.write(args.output, 48000, (2 ** 15 * out).astype(np.int16))
    print("wrote", args.output, f"{len(out)/48000:.2f}s")


if __name__ == "__main__":
    main()
