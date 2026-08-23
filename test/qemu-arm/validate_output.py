#!/usr/bin/env python3
"""
Sanity-validate a QEMU unit-test output WAV.

Checks (grounded in drumlogue hardware constraints):
  - all samples finite (NaN/Inf would indicate broken DSP state)
  - peak within absurdity bound (>32 fails — units clamp well below this)
  - RMS above silence floor (catches all-zero regression)
  - denormal sample fraction reported (denormals penalize the Cortex-A7)
  - DC offset reported

Usage:
  validate_output.py <output.wav> [--sample-rate 48000]

Skips gracefully (exit 0) when numpy is unavailable.
Prints a machine-readable line: WAV_METRICS frames=.. peak=.. rms=..
denormal_pct=.. dc=..
Exit codes: 0 = pass/skip, 1 = violation.
"""

import sys

try:
    import struct
    import numpy as np
except ImportError:
    print("WAV_VALIDATE SKIP: numpy not available")
    sys.exit(0)


def read_wav(path):
    """Minimal RIFF/WAVE reader: PCM16, PCM32 and IEEE-float32."""
    with open(path, "rb") as f:
        riff = f.read(12)
        if riff[:4] != b"RIFF" or riff[8:12] != b"WAVE":
            raise ValueError("not a RIFF/WAVE file")
        fmt = None
        raw = None
        while True:
            hdr = f.read(8)
            if len(hdr) < 8:
                raise ValueError("truncated WAV (no data chunk)")
            cid, size = struct.unpack("<4sI", hdr)
            chunk = f.read(size)
            if size % 2:
                f.read(1)  # pad byte
            if cid == b"fmt ":
                fmt = struct.unpack("<HHIIHH", chunk[:16])
            elif cid == b"data":
                raw = chunk
                break
        if fmt is None or raw is None:
            raise ValueError("missing fmt/data chunk")
        audio_fmt, n_ch, sr, _, _, bits = fmt
        if audio_fmt == 1 and bits == 16:
            data = np.frombuffer(raw, dtype=np.int16).astype(np.float32)
            data /= 32768.0
        elif audio_fmt == 1 and bits == 32:
            data = np.frombuffer(raw, dtype=np.int32).astype(np.float32)
            data /= 2147483648.0
        elif audio_fmt == 3 and bits == 32:
            data = np.frombuffer(raw, dtype=np.float32)
        else:
            raise ValueError(f"unsupported format tag={audio_fmt} bits={bits}")
        return data, n_ch, sr


def main():
    if len(sys.argv) < 2:
        print("usage: validate_output.py <output.wav> [--sample-rate N]")
        sys.exit(2)

    path = sys.argv[1]
    try:
        data, n_ch, sr = read_wav(path)
    except Exception as e:
        print(f"WAV_VALIDATE FAIL: cannot read {path}: {e}")
        sys.exit(1)

    frames = data.size // n_ch
    data = data.reshape(frames, n_ch)
    mono = data.mean(axis=1)

    nan_inf = int(np.sum(~np.isfinite(mono)))
    peak = float(np.max(np.abs(data))) if frames else 0.0
    rms = float(np.sqrt(np.mean(np.square(mono)))) if frames else 0.0
    nonzero = np.abs(mono) > 0.0
    denormal_pct = float(100.0 * np.mean((np.abs(mono) < 1e-20) & nonzero)) if frames else 0.0
    dc = float(np.mean(mono)) if frames else 0.0

    print(f"WAV_METRICS frames={frames} ch={n_ch} sr={sr} peak={peak:.4f} rms={rms:.6f} "
          f"denormal_pct={denormal_pct:.2f} dc={dc:+.5f}")

    failures = []
    if nan_inf:
        failures.append(f"{nan_inf} non-finite samples")
    if peak > 32.0:
        failures.append(f"absurd peak {peak:.2f}")
    if rms < 1e-6:
        failures.append("output is silent")

    if failures:
        print("WAV_VALIDATE FAIL: " + "; ".join(failures))
        sys.exit(1)
    print("WAV_VALIDATE PASS")


if __name__ == "__main__":
    main()
