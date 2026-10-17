#!/usr/bin/env bash
# Compares the NAM effect with the reference implementation (NeuralAmpModelerCore, https://github.com/sdatkinson/NeuralAmpModelerCore).
# For every model in Effect-Modules/Nam/models/ it builds a reference .nam (the A2 Lite network of a2_lite_template.json with that model's weights),
# renders a guitar-like signal through the reference, through nam_a2_daisy::A2Player and through NamA2Module, and compares. See tests/host/README.md.
#
# NAM_CORE   a recent NeuralAmpModelerCore checkout, one that reads file version 0.7.0 and the A2 networks (default: $HOME/dev/NeuralAmpModelerCore;
#            the copy inside the nam-pedal project is too old)
# TOL_DB     the least signal/error ratio in dB that passes (default 60)
# BUILD_DIR  where binaries and dumps go (default: /tmp/gp_nam_reference)
set -uo pipefail
cd "$(dirname "$0")/../../.."          # Software/GuitarPedal
T=tests/host
H=$T/nam_reference
NAM_CORE=${NAM_CORE:-$HOME/dev/NeuralAmpModelerCore}
B=${BUILD_DIR:-/tmp/gp_nam_reference}
CXX=${CXX:-g++}
mkdir -p "$B"
sh Effect-Modules/Nam/update_models.sh
[ -d "$NAM_CORE/NAM" ] || { echo "NeuralAmpModelerCore not found in $NAM_CORE (set NAM_CORE)"; exit 2; }

echo "=== build"
$CXX -std=c++20 -O2 -I"$NAM_CORE" -I"$NAM_CORE/Dependencies/eigen" -I"$NAM_CORE/Dependencies/nlohmann" $H/render_reference.cpp $(find "$NAM_CORE/NAM" -name "*.cpp") -o "$B/render_reference" 2>&1 | grep -E "error" | head -5
COMMON="-std=c++20 -O1 -I$T/stub -I. -IEffect-Modules -IUtil -Idependencies/DaisySP/Source -Idependencies/DaisySP/Source/Utility \
 -Idependencies/DaisySP/DaisySP-LGPL/Source -include daisysp-lgpl.h -Idependencies/q/q/q_lib/include -Idependencies/q/infra/include \
 -Idependencies/gcem/include -I$T/stub_cmsis"
$CXX $COMMON -DPLAYER $H/render_ours.cpp -o "$B/render_player" 2>&1 | grep -E "error|undefined" | head -5
$CXX $COMMON $H/render_ours.cpp Effect-Modules/nam_a2_module.cpp Effect-Modules/ImpulseResponse/ImpulseResponse.cpp Effect-Modules/ImpulseResponse/ir_data.cpp \
    Effect-Modules/base_effect_module.cpp Util/audio_utilities.cpp Util/heap_guard.cpp -o "$B/render_module" 2>&1 | grep -E "error|undefined" | head -5
[ -x "$B/render_reference" ] && [ -x "$B/render_player" ] && [ -x "$B/render_module" ] || { echo "build failed"; exit 1; }

echo "=== input and reference models"
python3 - "$B/in.f32" <<'PY'
import array, math, random, sys
random.seed(7)
sr, n = 48000, 48000 * 4
x = array.array("f", [0.0]) * n
# plucked notes (E2 A2 D3 G3 B3 E4 and a chord) with harmonics and a decay, quiet to loud, then picking noise and a loud burst
notes = [(82.4, 0.0, 0.05), (110.0, 0.4, 0.1), (146.8, 0.8, 0.2), (196.0, 1.2, 0.3), (246.9, 1.6, 0.5), (329.6, 2.0, 0.7)]
for f, t0, a in notes:
    for k in range(int(t0 * sr), min(n, int((t0 + 1.2) * sr))):
        t = (k / sr) - t0
        x[k] += a * math.exp(-3 * t) * sum(math.sin(2 * math.pi * f * h * t) / h for h in (1, 2, 3, 4, 5))
for k in range(int(2.8 * sr), n):
    x[k] += 0.15 * (random.random() - 0.5) * (1.0 if k < 3.2 * sr else 0.2)
for k in range(int(3.3 * sr), int(3.4 * sr)):
    x[k] += 0.9 * math.sin(2 * math.pi * 220 * k / sr)
m = max(abs(v) for v in x)
x = array.array("f", [v * 0.9 / m for v in x])
open(sys.argv[1], "wb").write(x.tobytes())
PY
python3 $H/make_reference_models.py "$B" || exit 1

echo "=== render"
i=0
for name in $(cut -d" " -f1 "$B/order.txt"); do
    "$B/render_reference" "$B/$name.nam" "$B/in.f32" "$B/ref_$name.f32" || exit 1
    "$B/render_player" $i "$B/in.f32" "$B/player_$i.f32" > /dev/null
    "$B/render_module" $i "$B/in.f32" "$B/module_$i.f32" > /dev/null
    i=$((i + 1))
done

echo "=== compare"
python3 -I $H/compare.py "$B" "${TOL_DB:-60}"
