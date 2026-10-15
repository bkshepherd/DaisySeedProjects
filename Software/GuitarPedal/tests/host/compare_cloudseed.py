"""CloudSeed: (1) the new module code built with block size 1 must be bit-identical to the original;
(2) the shipped block size changes the feedback timing slightly, so only check it is finite and close in level."""
import array, math, sys
D = sys.argv[1]
def load(n):
    a = array.array('f'); a.frombytes(open(f"{D}/{n}", "rb").read()); return a
old, n1, blk = load("cs_old.bin"), load("cs_n1.bin"), load("cs_new.bin")
N = 48 * 1000
d = max(abs(a - b) for a, b in zip(old, n1))
ok1 = d == 0
print(f"CloudSeed: new module code at block size 1 vs original: maxDiff={d:g} {'OK (bit-exact)' if ok1 else 'MISMATCH'}")
names = ["FChorus", "DullEchos", "Hyperplane", "MedSpace", "Hallway", "RubiKa", "SmallRoom", "90s"]
ok2 = True
for stereo in (0, 1):
    for p in range(8):
        base = ((stereo * 8 + p) * 2) * N
        a, b = old[base:base + N], blk[base:base + N]
        ra = math.sqrt(sum(x * x for x in a) / N); rb = math.sqrt(sum(x * x for x in b) / N)
        ta = math.sqrt(sum(x * x for x in a[3 * N // 4:]) / (N // 4)); tb = math.sqrt(sum(x * x for x in b[3 * N // 4:]) / (N // 4))
        fin = all(math.isfinite(x) for x in b)
        good = fin and abs(rb - ra) < 0.1 * ra and abs(tb - ta) < 0.15 * ta
        ok2 &= good
        print(f"  {'stereo' if stereo else 'mono  '} {names[p]:10s} rms {ra:.4f}->{rb:.4f}  tail rms {ta:.5f}->{tb:.5f} {'OK' if good else 'OUT OF TOLERANCE'}")
sys.exit(0 if ok1 and ok2 else 1)
