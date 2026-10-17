#!/usr/bin/env python3
"""compare.py <dir> <tolerance-dB>: for every model and path, the error of ours against the reference (signal-to-error ratio and maximum difference)."""
import array, math, sys
D = sys.argv[1]
tol = float(sys.argv[2])
order = [l.split() for l in open(f"{D}/order.txt")]
def load(p):
    a = array.array("f"); a.frombytes(open(p, "rb").read()); return a
fails = 0
for i, (name, gain) in enumerate(order):
    gain = float(gain)
    ref = load(f"{D}/ref_{name}.f32")
    for path in ("player", "module"):
        ours = load(f"{D}/{path}_{i}.f32")
        if path == "module":
            ours = array.array("f", [x / gain for x in ours])  # undo the model's outputGain (Level and Gain are at unity)
        n = min(len(ref), len(ours))
        skip = 4800  # the first 0.1 s: the models are pre-warmed differently (see README)
        num = sum(ref[k] ** 2 for k in range(skip, n)); err = sum((ref[k] - ours[k]) ** 2 for k in range(skip, n))
        mx = max(abs(ref[k] - ours[k]) for k in range(skip, n)); pk = max(abs(x) for x in ref)
        snr = 10 * math.log10(num / err) if err > 0 else 999.0
        ok = snr >= tol and pk > 0.01
        fails += not ok
        print(f"{name:20s} {path:6s}: reference peak {pk:.3f}, max diff {mx:.2e}, signal/error {snr:6.1f} dB {'OK' if ok else 'MISMATCH'}")
sys.exit(1 if fails else 0)
