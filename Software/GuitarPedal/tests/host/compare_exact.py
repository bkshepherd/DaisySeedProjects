"""Compare dumps against the original (BASE_REF) implementation; they must be bit-identical.
usage: compare_exact.py NAME DIR OLD_FILE NEW_PS_FILE NEW_BLOCK_FILE [SEGMENTS]"""
import array, math, sys
name, D, fo, fp, fb = sys.argv[1:6]
def load(n):
    a = array.array('f'); a.frombytes(open(f"{D}/{n}", "rb").read()); return a
old, ps, blk = load(fo), load(fp), load(fb)
d1 = max(abs(a - b) for a, b in zip(old, ps)); d2 = max(abs(a - b) for a, b in zip(old, blk))
pk = max(abs(x) for x in old); fin = all(math.isfinite(x) for x in blk)
ok = d1 == 0 and d2 == 0 and pk > 0.01 and fin
print(f"{name}: original vs new per-sample maxDiff={d1:g}; original vs new block maxDiff={d2:g}; peak={pk:.3f}; finite={fin} {'OK (bit-exact)' if ok else 'MISMATCH'}")
sys.exit(0 if ok else 1)
