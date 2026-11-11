#!/usr/bin/env python3
"""Build the calibration set for the BWE feature net from real 16 kHz speech.

IMPORTANT: the features have a wide range (-8 .. +1). Calibrating with random
noise (+-0.5) clips the input and drops the board cosine to 0.80.
"""
import os
import sys
import tarfile

import numpy as np
import torch

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "python"))
from models import model_dict  # noqa: E402
from utils.bwe_features import load_inference_data  # noqa: E402

CKPT = os.path.join(HERE, "..", "model", "bbwenet_v2.pth")
OUT = os.path.join(HERE, "calib_real")
FRAMES = 90
N = 12


def main(wavs):
    ck = torch.load(CKPT, map_location="cpu", weights_only=False)
    os.makedirs(f"{OUT}/features", exist_ok=True)
    os.makedirs(f"{OUT}/gru_state", exist_ok=True)
    idx = 0
    for w in wavs:
        try:
            _, feat = load_inference_data(w, **ck["setup"]["data"])
        except Exception as e:  # noqa: BLE001
            print("skip", w, e)
            continue
        for s in range(0, len(feat) - FRAMES, FRAMES):
            np.save(f"{OUT}/features/{idx:04d}.npy",
                    feat[s:s + FRAMES].unsqueeze(0).numpy().astype(np.float32))
            np.save(f"{OUT}/gru_state/{idx:04d}.npy", np.zeros((1, 1, 128), dtype=np.float32))
            idx += 1
            if idx >= N:
                break
        if idx >= N:
            break
    for n in ("features", "gru_state"):
        with tarfile.open(f"{OUT}/{n}.tar.gz", "w:gz") as tf:
            for f in sorted(os.listdir(f"{OUT}/{n}")):
                tf.add(f"{OUT}/{n}/{f}", arcname=f)
    print("calibration samples:", idx)


if __name__ == "__main__":
    main(sys.argv[1:])
