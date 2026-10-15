"""Compare the NAM dumps: the direct block path must equal the per-sample path delayed by exactly one 48-sample block."""
import array, sys
D = sys.argv[1]
def load(n):
    a = array.array('f'); a.frombytes(open(f"{D}/{n}.bin", "rb").read()); return a
fails = 0
for eq in (0, 1):
    for ir in (0, 1):
        s, m, st = [load(f"nam_{x}_{eq}_{ir}") for x in ("sample", "mono", "stereo")]
        n = len(s) // 2
        sl, ml, mr, stl = s[:n], m[:n], m[n:], st[:n]
        d = max(abs(sl[i + 48] - ml[i]) for i in range(n - 48))
        dlr = max(abs(a - b) for a, b in zip(ml, mr)); dst = max(abs(a - b) for a, b in zip(ml, stl))
        pk = max(abs(x) for x in ml)
        ok = d == 0 and dlr == 0 and dst == 0 and pk > 0.001
        fails += not ok
        print(f"NAM eq={eq} irCab={ir}: direct vs per-sample delayed 48: maxDiff={d:g}; R==L {dlr:g}; stereo==mono {dst:g}; peak={pk:.4f} {'OK' if ok else 'MISMATCH'}")
sys.exit(1 if fails else 0)
