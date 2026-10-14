#!/usr/bin/env bash
# Startup-state audit: does every effect set up the state derived from its parameter defaults when only Init() has run, as on the pedal?
# ParameterChanged() only fires when a value changes, and first-boot storage equals the metadata defaults, so an effect that derives state from a
# parameter only inside ParameterChanged() never applies that parameter's default at startup (the Compressor and Tape Delay had this).
#
# For each effect module the audit runs a test signal through three instances (one process each) and compares the outputs:
#   startup  - Init() only
#   touched  - Init(), then every parameter moved away from its default and straight back (which fires ParameterChanged for each)
#   poison   - like startup, but the object's memory is pre-filled with 0xFF to expose reads of uninitialised members
# A second startup run checks that the effect is deterministic (rand() is made constant by the harness). See tests/host/README.md.
#
# usage: tests/host/run_defaults_audit.sh [effect_file_stem ...]      audit all effects (or just the named ones, e.g. tape_delay_module)
#        tests/host/run_defaults_audit.sh --bisect <stem>             touch one parameter at a time to find which defaults are not applied
# BUILD_DIR (default /tmp/gp_host_tests) holds binaries and dumps. Exit status is non-zero on any unexpected finding.
set -uo pipefail
cd "$(dirname "$0")/../.."          # Software/GuitarPedal
T=tests/host
B=${BUILD_DIR:-/tmp/gp_host_tests}/audit
mkdir -p "$B/obj"
CXX=${CXX:-g++}
FLAGS="-std=c++20 -O1 -w -I$T/stub -I$T/stub_cmsis -I. -IEffect-Modules -IUtil -Idependencies/DaisySP/Source -Idependencies/DaisySP/Source/Utility \
 -Idependencies/DaisySP/DaisySP-LGPL/Source -Idependencies/CloudSeed -Idependencies/q/q/q_lib/include -Idependencies/q/infra/include \
 -Idependencies/gcem/include -include daisysp-lgpl.h"

# Findings that are known and understood. An unexpected finding fails the audit; a known one that disappears is reported so it can be removed here.
#   cloudseed_module:STARTUP!=TOUCHED  Init() forces the reverb line count to 3 (a comment explains why); selecting any preset afterwards restores the
#                                      controller's own line count, so startup differs from a preset selected from the menu.
#   modal_keys_module:UNINIT-READ      the DaisySP ModalVoice member has uninitialised state until the first note (the keyboard effects are disabled
#                                      in the default build).
KNOWN="cloudseed_module:STARTUP!=TOUCHED modal_keys_module:UNINIT-READ"

# Effect classes: stem -> class name, found from the headers
python3 -I - > "$B/effects.txt" <<'PYEOF'
import glob, os, re
for h in sorted(glob.glob('Effect-Modules/*_module.h')):
    for c in re.findall(r'class\s+(\w+)\s*:\s*public\s+BaseEffectModule', open(h).read()):
        print(os.path.basename(h)[:-2], c)
PYEOF

# Shared objects (all DaisySP, Util, helper modules), built once and rebuilt when any of their sources or the host stubs are newer
SHARED_DIRS="Util Effect-Modules/Dattorro Effect-Modules/Chopper Effect-Modules/ImpulseResponse dependencies/CloudSeed $T/stub $T/stub_cmsis"
if [ ! -f "$B/libshared.a" ] || [ -n "$(find $SHARED_DIRS Effect-Modules/base_effect_module.cpp Effect-Modules/base_effect_module.h -type f -newer "$B/libshared.a" -print -quit)" ]; then
    echo "building shared objects..."
    rm -f "$B/libshared.a" "$B"/obj/*.o
    { find dependencies/DaisySP/Source dependencies/DaisySP/DaisySP-LGPL/Source -name "*.cpp"; ls Util/*.cpp
      echo Effect-Modules/base_effect_module.cpp; echo Effect-Modules/Chopper/chopper.cpp; echo Effect-Modules/Dattorro/Dattorro.cpp
      echo Effect-Modules/ImpulseResponse/ImpulseResponse.cpp; echo Effect-Modules/ImpulseResponse/ir_data.cpp
      ls dependencies/CloudSeed/AudioLib/*.cpp dependencies/CloudSeed/FastSin.cpp dependencies/CloudSeed/Utils/Sha256.cpp; } |
    xargs -P "$(nproc)" -I{} sh -c "$CXX $FLAGS -c {} -o $B/obj/\$(echo {} | tr / _).o 2>/dev/null || echo 'could not compile {}'"
    rm -f "$B/libshared.a"; ar rcs "$B/libshared.a" "$B"/obj/*.o
fi

build() { # build STEM -> $B/bin_STEM
    local stem=$1 cls
    cls=$(grep "^$stem " "$B/effects.txt" | cut -d' ' -f2)
    rm -f "$B/bin_$stem"
    $CXX $FLAGS -DEFFECT_HDR="\"$stem.h\"" -DEFFECT_CLASS="$cls" $T/audit/audit_effect.cpp "Effect-Modules/$stem.cpp" "$B/libshared.a" -o "$B/bin_$stem" 2> "$B/err_$stem.txt"
    [ -x "$B/bin_$stem" ]
}

if [ "${1:-}" = "--bisect" ]; then
    stem=$2
    build "$stem" || { echo "build failed (see $B/err_$stem.txt)"; exit 1; }
    ( cd "$B"; ./bin_$stem list x > list_$stem.txt; ./bin_$stem startup b_${stem}_s.bin > /dev/null
      while read -r id name; do
          ./bin_$stem touch:$id b_${stem}_$id.bin > /dev/null 2>&1
          python3 -I - "$stem" "$id" "$name" <<'PYEOF'
import array, math, sys
stem, id, name = sys.argv[1], sys.argv[2], " ".join(sys.argv[3:])
def load(n):
    a = array.array('f'); a.frombytes(open(f"b_{stem}_{n}.bin", "rb").read()); return a
s, t = load("s"), load(id)
peak = max((abs(x) for x in s if not math.isnan(x)), default=0.0) or 1.0
d = max((abs(x - y) for x, y in zip(s, t)), default=0.0) / peak
print(f"  param {id:>2} {name:16s} " + (f"changes the output by {d:.2e} of peak (its default is not applied at startup)" if d > 1e-5 else "ok"))
PYEOF
      done < list_$stem.txt )
    exit 0
fi

if [ $# -gt 0 ]; then LIST="$*"; else LIST=$(cut -d' ' -f1 "$B/effects.txt"); fi
FAILS=0; CLEAN=0; SEEN=""
for stem in $LIST; do
    if ! build "$stem"; then echo "$stem: BUILD FAILED (see $B/err_$stem.txt)"; FAILS=$((FAILS + 1)); continue; fi
    ( cd "$B" && timeout 120 ./bin_$stem startup out_${stem}_s1.bin && timeout 120 ./bin_$stem startup out_${stem}_s2.bin &&
      timeout 120 ./bin_$stem touched out_${stem}_t.bin && timeout 120 ./bin_$stem poison out_${stem}_p.bin ) > /dev/null 2>&1 || { echo "$stem: RUN FAILED"; FAILS=$((FAILS + 1)); continue; }
    line=$(python3 -I - "$stem" "$B" <<'PYEOF'
import array, math, sys
stem, A = sys.argv[1], sys.argv[2]
def load(n):
    a = array.array('f'); a.frombytes(open(f"{A}/out_{stem}_{n}.bin", "rb").read()); return a
s1, s2, t, p = load("s1"), load("s2"), load("t"), load("p")
def diff(a, b):
    if any(math.isnan(x) for x in b) != any(math.isnan(x) for x in a): return float('inf')
    return max((abs(x - y) for x, y in zip(a, b) if not (math.isnan(x) and math.isnan(y))), default=0.0)
peak = max((abs(x) for x in s1 if not math.isnan(x)), default=0.0)
rel = lambda d: d / peak if peak > 0 else (0.0 if d == 0 else float('inf'))
flags = []
if any(math.isnan(x) for x in s1): flags.append("NaN-output")
det = diff(s1, s2)
if det != 0: flags.append("NONDETERMINISTIC")
else:
    if rel(diff(s1, t)) > 1e-5: flags.append("STARTUP!=TOUCHED")
    if rel(diff(s1, p)) > 1e-5: flags.append("UNINIT-READ")
print(f"{stem:26s} peak {peak:6.3f}  startup-vs-touched {rel(diff(s1, t)):9.2e}  startup-vs-poison {rel(diff(s1, p)):9.2e}  " + (" ".join(flags) if flags else "ok"), "|", " ".join(f"{stem}:{f}" for f in flags))
PYEOF
)
    echo "${line%% | *}"
    for f in ${line##*| }; do
        SEEN="$SEEN $f"
        case " $KNOWN " in *" $f "*) echo "    (known, see the KNOWN list in this script)";; *) echo "    UNEXPECTED: $f"; FAILS=$((FAILS + 1));; esac
    done
done
for k in $KNOWN; do
    stem=${k%%:*}
    case " $LIST " in *" $stem "*) case " $SEEN " in *" $k "*) ;; *) echo "note: known finding $k no longer occurs; remove it from KNOWN";; esac;; esac
done
echo
if [ "$FAILS" -eq 0 ]; then echo "DEFAULTS AUDIT: no unexpected findings"; else echo "DEFAULTS AUDIT: $FAILS unexpected finding(s)"; fi
exit $((FAILS > 0))
