#!/usr/bin/env bash
# Compares the Dattorro engine of the FunBox Earth pedal with this repository's (fixed) engine, in impulse responses, with Earth's settings.
# Needs a clone of the FunBox project: FUNBOX_DIR (default ../../../FunBox relative to Software/GuitarPedal). Skips with a message if it is not there.
#
# What it found (see Effect-Modules/Earth/README.md): at the same engine sample rate (32 kHz) the two engines agree (same echo pattern), but at 48 kHz Earth's
# engine behaves like a reverb with shorter delays, because it never wires up its maximum sample rate (the repo's engine fixed that). The Earth module scales Size
# by 32000 / sample rate to keep Earth's room sizes. Earth's own engine also overruns its fixed 13-slot SDRAM table of delay buffers (this script patches a scratch
# copy to 64 slots so that it can run on a PC).
set -uo pipefail
cd "$(dirname "$0")/../.."          # Software/GuitarPedal
FUNBOX=${FUNBOX_DIR:-../../../FunBox}
EARTH=$FUNBOX/software/Earth
B=${BUILD_DIR:-/tmp/gp_host_tests}/earth
if [ ! -d "$EARTH/Dattorro" ]; then echo "FunBox clone not found at $FUNBOX (set FUNBOX_DIR); skipping."; exit 0; fi
mkdir -p "$B"; rm -rf "$B/engine"; cp -r "$EARTH/Dattorro" "$B/engine_src"; mkdir -p "$B/engine"; mv "$B/engine_src" "$B/engine/Dattorro"
sed -i 's/sdramData\[13\]\[37000\]/sdramData[64][37000]/' "$B/engine/Dattorro/dsp/delays/InterpDelay.hpp" "$B/engine/Dattorro/dsp/delays/InterpDelay.cpp"
FL="-std=c++20 -O2 -w -Itests/host/stub"
g++ $FL -I"$B/engine" tests/host/earth/engine_cmp.cpp "$B/engine/Dattorro/Dattorro.cpp" "$B/engine/Dattorro/dsp/filters/OnePoleFilters.cpp" "$B/engine/Dattorro/dsp/delays/InterpDelay.cpp" -o "$B/cmp_earth" || exit 1
g++ $FL -IEffect-Modules tests/host/earth/engine_cmp.cpp Effect-Modules/Dattorro/Dattorro.cpp -o "$B/cmp_repo" || exit 1
( cd "$B"
  ./cmp_earth earth32.bin 2.0 1.0 0.0 32000; ./cmp_repo repo32.bin 2.0 1.0 0.0 32000
  ./cmp_earth earth48.bin 2.0 1.0 0.0 48000; ./cmp_repo repo48.bin 2.0 1.0 0.0 48000; ./cmp_repo repo48_comp.bin 1.3333333 1.0 0.0 48000
  python3 -I - <<'PYEOF'
import array, math
def load(n):
    a = array.array('f'); a.frombytes(open(n, 'rb').read()); h = len(a) // 2; return a[:h]
def peaks(a, lo, hi, k=6, pool=48):
    # the earliest of the strongest `pool` peaks: a feel for where the early echoes sit
    idx = sorted(range(lo, hi), key=lambda i: -abs(a[i]))[:pool]; out = []
    for i in sorted(idx):
        if not out or i - out[-1] > 40: out.append(i)
    return out[:k]
e32, r32, e48, r48, rc = (load(f) for f in ("earth32.bin", "repo32.bin", "earth48.bin", "repo48.bin", "repo48_comp.bin"))
print("early echoes (samples), engine told 32 kHz:  Earth", peaks(e32, 0, 3000), " repo", peaks(r32, 0, 3000))
print("early echoes (samples), engine told 48 kHz:  Earth", peaks(e48, 0, 3000), " repo", peaks(r48, 0, 3000))
print("tank echoes from 2000 samples on:  Earth @48k", peaks(e48, 2000, 15000, 4), "\n                                   repo  @48k", peaks(r48, 2000, 15000, 4),
      "\n                                   repo  @48k with Size x 2/3", peaks(rc, 2000, 15000, 4))
PYEOF
)
