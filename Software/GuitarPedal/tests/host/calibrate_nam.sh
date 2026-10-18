#!/usr/bin/env bash
# Builds and runs tests/host/levels/calibrate_nam.cpp: prints "<index> <name> <gain in dB>" for every NAM model in Effect-Modules/Nam/models/
# (what the calibration in update_models.sh uses). Run from anywhere; BUILD_DIR says where the binary goes (default /tmp/gp_host_tests).
set -uo pipefail
cd "$(dirname "$0")/../.."          # Software/GuitarPedal
T=tests/host
B=${BUILD_DIR:-/tmp/gp_host_tests}
mkdir -p "$B"
COMMON="-std=c++20 -O1 -I$T/stub -I$T/levels -I. -IEffect-Modules -IUtil -Idependencies/DaisySP/Source -Idependencies/DaisySP/Source/Utility \
 -Idependencies/DaisySP/DaisySP-LGPL/Source -include daisysp-lgpl.h -Idependencies/q/q/q_lib/include -Idependencies/q/infra/include -Idependencies/gcem/include -I$T/stub_cmsis"
rm -f "$B/calibrate_nam"
g++ $COMMON $T/levels/calibrate_nam.cpp Effect-Modules/nam_a2_module.cpp Effect-Modules/ImpulseResponse/ImpulseResponse.cpp Effect-Modules/ImpulseResponse/ir_data.cpp \
    Effect-Modules/base_effect_module.cpp Util/audio_utilities.cpp Util/heap_guard.cpp -o "$B/calibrate_nam" 2>&1 | grep -E "error|undefined" | head -5
[ -x "$B/calibrate_nam" ] || { echo "build failed" >&2; exit 1; }
"$B/calibrate_nam"
