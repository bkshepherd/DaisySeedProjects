#!/usr/bin/env bash
# Default-level tests: measure each effect's gain at its default parameters (see tests/host/README.md).
# Rule: between 0 dB and +6 dB at default settings, never attenuating; distortion and compression aim for +3 dB.
set -uo pipefail
cd "$(dirname "$0")/../.."          # Software/GuitarPedal
T=tests/host
B=${BUILD_DIR:-/tmp/gp_host_tests}
mkdir -p "$B"
COMMON="-std=c++20 -O1 -I$T/stub -I$T/levels -I. -IEffect-Modules -IUtil -Idependencies/DaisySP/Source -Idependencies/DaisySP/Source/Utility \
 -Idependencies/DaisySP/DaisySP-LGPL/Source -include daisysp-lgpl.h -Idependencies/q/q/q_lib/include -Idependencies/q/infra/include -Idependencies/gcem/include"
LGPL=dependencies/DaisySP/DaisySP-LGPL/Source
BASEMOD="Effect-Modules/base_effect_module.cpp Util/audio_utilities.cpp"
FAILS=0
run() { # run NAME SOURCES...   (extra compiler flags in XFLAGS)
    local name=$1; shift
    echo; echo "=== $name"
    if g++ $COMMON ${XFLAGS:-} "$T/levels/test_level_$name.cpp" "$@" -o "$B/level_$name" 2>&1 | grep -E "error|undefined" | head -5; [ -x "$B/level_$name" ]; then
        "$B/level_$name" || FAILS=$((FAILS + 1))
    else FAILS=$((FAILS + 1)); fi
}
run distortion Effect-Modules/distortion_module.cpp $BASEMOD $LGPL/Filters/tone.cpp
run compressor Effect-Modules/compressor_module.cpp $BASEMOD $LGPL/Dynamics/compressor.cpp
run overdrive Effect-Modules/overdrive_module.cpp $BASEMOD dependencies/DaisySP/Source/Effects/overdrive.cpp
run venus Effect-Modules/venus_module.cpp $BASEMOD dependencies/DaisySP/Source/Effects/sampleratereducer.cpp dependencies/DaisySP/Source/Synthesis/oscillator.cpp $LGPL/Filters/tone.cpp
XFLAGS="-I$T/stub_cmsis" run nam Effect-Modules/nam_a2_module.cpp Effect-Modules/ImpulseResponse/ImpulseResponse.cpp Effect-Modules/ImpulseResponse/ir_data.cpp $BASEMOD
echo
if [ "$FAILS" -eq 0 ]; then echo "ALL DEFAULT-LEVEL TESTS IN WINDOW"; else echo "DEFAULT-LEVEL TESTS OUT OF WINDOW: $FAILS"; fi
exit $((FAILS > 0))
