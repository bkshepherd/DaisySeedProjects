# The dispersion of the spring reverb (Effect-Modules/spring_reverb_module.cpp: kDispersionCoefs and kDispersionCounts are the result of this fit, run as
# `python3 tools/fit_spring_dispersion.py`).
# Fits the number of stretched allpass sections A(z^K) = (a + z^-K) / (1 + a z^-K), a > 0, so that the group delay follows that of a low-pass ladder (a helical spring below its
# transition frequency fC): extra delay(f) = T0 * (1 / sqrt(1 - (f/fC)^2) - 1), growing from nothing at the lows to a maximum at fC (K = fs / (2 fC): the first peak of the stretched allpass).
import math, sys
FS, K, FC = 48000.0, 6, 4000.0
T0 = float(sys.argv[1]) if len(sys.argv) > 1 else 0.0155   # s; extra delay at 0.9 fC is 1.29 * T0
A = [0.3, 0.45, 0.6, 0.7, 0.8, 0.88]
FREQ = [100.0 * (3900.0 / 100.0) ** (i / 39.0) for i in range(40)]
def tau(a, f):
    w = 2 * math.pi * f / FS
    return K * (1 - a * a) / (1 + a * a + 2 * a * math.cos(K * w)) / FS
def target(f):
    x = min(f / FC, 0.985)
    return T0 * (1 / math.sqrt(1 - x * x) - 1)
# the allpass chain has a minimum delay at DC (K * (1 - a) / (1 + a) per section): fit the extra delay above that, with a weight that favours the part that matters
base = lambda n: sum(n[j] * tau(A[j], 0.0) for j in range(len(A)))
tg = [target(f) for f in FREQ]
W = [1.0 / max(t, 0.002) for t in tg]
basis = [[tau(a, f) for f in FREQ] for a in A]
n = [0.0] * len(A)
def err(n):
    b0 = base(n)
    return sum(((sum(n[j] * basis[j][i] for j in range(len(A))) - b0 - tg[i]) * W[i]) ** 2 for i in range(len(FREQ)))
step = 4.0
for it in range(6000):
    improved = False
    for j in range(len(A)):
        for d in (step, -step):
            m = list(n); m[j] = max(0.0, m[j] + d)
            if err(m) < err(n) - 1e-12: n = m; improved = True
    if not improved:
        step *= 0.5
        if step < 0.01: break
r = [int(round(x)) for x in n]
print("real counts", [round(x, 2) for x in n], "total", round(sum(n), 1), "| rounded", r, "total", sum(r), "err", round(err([float(x) for x in r]), 3))
print("%8s %12s %12s" % ("f Hz", "target ms", "chain ms (above DC)"))
b0 = sum(r[j] * tau(A[j], 0.0) for j in range(len(A)))
for f in (100, 300, 600, 1000, 1500, 2000, 2500, 3000, 3300, 3600, 3800, 4000, 4300, 5000):
    print("%8d %12.1f %12.1f" % (f, 1000 * target(f), 1000 * (sum(r[j] * tau(A[j], f) for j in range(len(A))) - b0)))
print("delay at DC of the chain: %.1f ms" % (1000 * b0))
