#!/usr/bin/env bash
# Default-level tests: measure each effect's gain at its default parameters (see tests/host/README.md).
# Rule: between 0 dB and +6 dB at default settings, never attenuating; distortion and compression aim for +3 dB.
set -uo pipefail
cd "$(dirname "$0")/../.."          # Software/GuitarPedal
T=tests/host
B=${BUILD_DIR:-/tmp/gp_host_tests}
mkdir -p "$B"
sh Effect-Modules/Nam/update_models.sh --no-calibrate   # the NAM models are one file each in Effect-Modules/Nam/models/
COMMON="-std=c++20 -O1 -I$T/stub -I$T/levels -I. -IEffect-Modules -IUtil -Idependencies/DaisySP/Source -Idependencies/DaisySP/Source/Utility \
 -Idependencies/DaisySP/DaisySP-LGPL/Source -include daisysp-lgpl.h -Idependencies/q/q/q_lib/include -Idependencies/q/infra/include -Idependencies/gcem/include"
LGPL=dependencies/DaisySP/DaisySP-LGPL/Source
BASEMOD="Effect-Modules/base_effect_module.cpp Util/audio_utilities.cpp Util/heap_guard.cpp"
FAILS=0
run() { # run NAME SOURCES...   (extra compiler flags in XFLAGS)
    local name=$1; shift
    echo; echo "=== $name"
    if g++ $COMMON ${XFLAGS:-} "$T/levels/test_level_$name.cpp" "$@" -o "$B/level_$name" 2>&1 | grep -E "error|undefined" | head -5; [ -x "$B/level_$name" ]; then
        "$B/level_$name" || FAILS=$((FAILS + 1))
    else FAILS=$((FAILS + 1)); fi
}
run distortion Effect-Modules/distortion_module.cpp $BASEMOD $LGPL/Filters/tone.cpp
run distortion_family Effect-Modules/preamp_module.cpp Effect-Modules/screamer_module.cpp Effect-Modules/muff_fuzz_module.cpp Effect-Modules/wavefolder_module.cpp Effect-Modules/exciter_module.cpp Effect-Modules/bass_dist_module.cpp Effect-Modules/octave_fuzz_module.cpp Effect-Modules/tape_sat_module.cpp $BASEMOD
run compressor Effect-Modules/compressor_module.cpp $BASEMOD $LGPL/Dynamics/compressor.cpp
run overdrive Effect-Modules/overdrive_module.cpp $BASEMOD dependencies/DaisySP/Source/Effects/overdrive.cpp
XFLAGS="-Idependencies/CloudSeed" run jupiter Effect-Modules/jupiter_module.cpp Effect-Modules/cloudseed_pool.cpp $BASEMOD dependencies/CloudSeed/AudioLib/Biquad2.cpp dependencies/CloudSeed/AudioLib/ShaRandom.cpp dependencies/CloudSeed/AudioLib/ValueTables.cpp dependencies/CloudSeed/FastSin.cpp dependencies/CloudSeed/Utils/Sha256.cpp
XFLAGS="-Idependencies/CloudSeed" run neptune Effect-Modules/neptune_module.cpp Effect-Modules/cloudseed_pool.cpp $BASEMOD $LGPL/Filters/tone.cpp dependencies/CloudSeed/AudioLib/Biquad2.cpp dependencies/CloudSeed/AudioLib/ShaRandom.cpp dependencies/CloudSeed/AudioLib/ValueTables.cpp dependencies/CloudSeed/FastSin.cpp dependencies/CloudSeed/Utils/Sha256.cpp
run uranus Effect-Modules/uranus_module.cpp Util/granularplayermod.cpp Util/operator.cpp $BASEMOD dependencies/DaisySP/Source/Synthesis/oscillator.cpp dependencies/DaisySP/Source/Control/adsr.cpp dependencies/DaisySP/Source/Control/phasor.cpp
run balnea Effect-Modules/balnea_module.cpp $BASEMOD dependencies/DaisySP/Source/Synthesis/oscillator.cpp dependencies/DaisySP/Source/Control/adsr.cpp dependencies/DaisySP/Source/Filters/svf.cpp
run duck Effect-Modules/duck_compressor_module.cpp Effect-Modules/SimpleCompressor/SimpleComp.cpp Effect-Modules/SimpleCompressor/SimpleEnvelope.cpp $BASEMOD
run pluto Effect-Modules/pluto_module.cpp $BASEMOD $LGPL/Effects/reverbsc.cpp $LGPL/Filters/tone.cpp $LGPL/Filters/atone.cpp $LGPL/Dynamics/balance.cpp
run cassini Effect-Modules/cassini_module.cpp $BASEMOD $LGPL/Effects/reverbsc.cpp dependencies/DaisySP/Source/Control/adsr.cpp dependencies/DaisySP/Source/Filters/svf.cpp
XFLAGS="-I$T/stub_cmsis" run mars Effect-Modules/mars_module.cpp Effect-Modules/ImpulseResponse/ImpulseResponse.cpp $BASEMOD $LGPL/Filters/tone.cpp $LGPL/Filters/atone.cpp $LGPL/Dynamics/balance.cpp
run mercury Effect-Modules/mercury_module.cpp $BASEMOD
XFLAGS="-I$T/stub_cmsis" run saturn Effect-Modules/saturn_module.cpp $BASEMOD dependencies/DaisySP/Source/Synthesis/oscillator.cpp
XFLAGS="-I$T/stub_cmsis" run venus Effect-Modules/venus_module.cpp $BASEMOD dependencies/DaisySP/Source/Effects/sampleratereducer.cpp dependencies/DaisySP/Source/Synthesis/oscillator.cpp $LGPL/Filters/tone.cpp
run earth Effect-Modules/earth_module.cpp Effect-Modules/Dattorro/Dattorro.cpp $BASEMOD dependencies/DaisySP/Source/Effects/overdrive.cpp
XFLAGS="-I$T/stub_cmsis" run nam Effect-Modules/nam_a2_module.cpp Effect-Modules/ImpulseResponse/ImpulseResponse.cpp Effect-Modules/ImpulseResponse/ir_data.cpp $BASEMOD
echo
if [ "$FAILS" -eq 0 ]; then echo "ALL DEFAULT-LEVEL TESTS IN WINDOW"; else echo "DEFAULT-LEVEL TESTS OUT OF WINDOW: $FAILS"; fi
exit $((FAILS > 0))
