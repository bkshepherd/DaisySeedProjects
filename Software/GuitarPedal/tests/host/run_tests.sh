#!/usr/bin/env bash
# Host-side (PC) equivalence tests for the block-processing path of the effect modules.
# See tests/host/README.md. Run from anywhere: tests/host/run_tests.sh
#
# BASE_REF   git ref holding the original per-sample implementation to compare against (default: main)
# BUILD_DIR  where binaries and dumps go (default: /tmp/gp_host_tests)
set -uo pipefail
cd "$(dirname "$0")/../.."          # Software/GuitarPedal
T=tests/host
BASE=${BASE_REF:-main}
B=${BUILD_DIR:-/tmp/gp_host_tests}
PREFIX=$(git rev-parse --show-prefix)
mkdir -p "$B"

CXX=${CXX:-g++}
COMMON="-std=c++20 -O1 -I$T/stub -I. -IEffect-Modules -IUtil -Idependencies/DaisySP/Source -Idependencies/DaisySP/Source/Utility \
 -Idependencies/DaisySP/DaisySP-LGPL/Source -include daisysp-lgpl.h -Idependencies/q/q/q_lib/include -Idependencies/q/infra/include \
 -Idependencies/gcem/include"
LGPL=dependencies/DaisySP/DaisySP-LGPL/Source
DSP=dependencies/DaisySP/Source
BASEMOD="Effect-Modules/base_effect_module.cpp Util/audio_utilities.cpp Util/heap_guard.cpp"
REVSC=$LGPL/Effects/reverbsc.cpp

FAILS=0
step() { echo; echo "=== $*"; }
check() { if "$@"; then :; else FAILS=$((FAILS + 1)); fi; }
build() { # build OUT SOURCES... (extra flags in $XFLAGS)
    local out=$1; shift
    $CXX $COMMON ${XFLAGS:-} "$@" -o "$out" 2>&1 | grep -E "error|undefined" | head -5
    [ -x "$out" ]
}
base_file() { git show "$BASE:${PREFIX}$1"; } # file as it was in BASE_REF

step "Reverb + Dattorro: block path vs per-sample path"
if XFLAGS= build "$B/t_reverb_dattorro" $T/test_reverb_dattorro.cpp Effect-Modules/reverb_module.cpp Effect-Modules/dattorro_reverb_module.cpp \
    Effect-Modules/Dattorro/Dattorro.cpp $BASEMOD $REVSC; then check "$B/t_reverb_dattorro"; else FAILS=$((FAILS + 1)); fi

step "IR: block path vs per-sample path (CMSIS-DSP replaced by a stand-in FIR, see stub_cmsis/arm_math.h)"
if XFLAGS="-I$T/stub_cmsis" build "$B/t_ir" $T/test_ir.cpp Effect-Modules/ir_module.cpp Effect-Modules/ImpulseResponse/ImpulseResponse.cpp \
    Effect-Modules/ImpulseResponse/ir_data.cpp $BASEMOD; then check "$B/t_ir"; else FAILS=$((FAILS + 1)); fi

step "NAM: direct block path vs per-sample path delayed by one block"
if XFLAGS="-I$T/stub_cmsis" build "$B/t_nam" $T/test_nam.cpp Effect-Modules/nam_a2_module.cpp Effect-Modules/ImpulseResponse/ImpulseResponse.cpp \
    Effect-Modules/ImpulseResponse/ir_data.cpp $BASEMOD; then
    for eq in 0 1; do for ir in 0 1; do for md in sample mono stereo; do # the model is a process-wide static: one process per run
        "$B/t_nam" $md $eq $ir "$B/nam_${md}_${eq}_${ir}.bin" > /dev/null
    done; done; done
    check python3 -I $T/compare_nam.py "$B"
else FAILS=$((FAILS + 1)); fi

step "SciFi: original ($BASE) vs new"
mkdir -p "$B/sf_old"; base_file Effect-Modules/scifi_module.cpp > "$B/sf_old/scifi_module.cpp"; base_file Effect-Modules/scifi_module.h > "$B/sf_old/scifi_module.h"
SFSRC="Effect-Modules/base_effect_module.cpp $REVSC $DSP/Effects/overdrive.cpp"
if XFLAGS="-I$B/sf_old" build "$B/sf_old_bin" $T/test_scifi.cpp "$B/sf_old/scifi_module.cpp" $SFSRC && build "$B/sf_new_bin" $T/test_scifi.cpp Effect-Modules/scifi_module.cpp $SFSRC; then
    "$B/sf_old_bin" sample "$B/sf_old.bin"; "$B/sf_new_bin" sample "$B/sf_ps.bin"; "$B/sf_new_bin" block "$B/sf_blk.bin"
    check python3 -I $T/compare_exact.py SciFi "$B" sf_old.bin sf_ps.bin sf_blk.bin
else FAILS=$((FAILS + 1)); fi

step "Tape Delay: original ($BASE) vs new"
mkdir -p "$B/td_old"; base_file Effect-Modules/tape_delay_module.cpp > "$B/td_old/tape_delay_module.cpp"; base_file Effect-Modules/tape_delay_module.h > "$B/td_old/tape_delay_module.h"
# The original computes the wet/dry balance from an uninitialised smoothed mix at startup and only on a Mix change (a known bug, fixed in the new code and
# covered by test_tape_delay_defaults.cpp). Mirror the same fix into the reference so this step still checks that the refactor itself changed nothing.
python3 -I - "$B/td_old/tape_delay_module.cpp" <<'PYFIX'
import sys
p = sys.argv[1]; s = open(p).read()
for old, new in (("    UpdateMix();\n    ResetInternalState();\n}", "    ResetInternalState();\n    UpdateMix();\n}"),
                 ("    fonepole(m_smoothedRepeats, GetParameterAsFloat(REPEATS), 0.02f);\n", "    fonepole(m_smoothedRepeats, GetParameterAsFloat(REPEATS), 0.02f);\n    UpdateMix();\n")):
    assert old in s, "reference patch no longer applies: " + old[:40]
    s = s.replace(old, new, 1)
open(p, "w").write(s)
PYFIX
TDSRC="Util/tape_modulator.cpp $BASEMOD $REVSC $LGPL/Filters/tone.cpp $DSP/Filters/svf.cpp $DSP/Synthesis/oscillator.cpp"
if XFLAGS="-I$B/td_old" build "$B/td_old_bin" $T/test_tape_delay.cpp "$B/td_old/tape_delay_module.cpp" $TDSRC && build "$B/td_new_bin" $T/test_tape_delay.cpp Effect-Modules/tape_delay_module.cpp $TDSRC; then
    for inp in mono stereo; do
        "$B/td_old_bin" sample "$B/td_old_$inp.bin" $inp; "$B/td_new_bin" sample "$B/td_ps_$inp.bin" $inp; "$B/td_new_bin" block "$B/td_blk_$inp.bin" $inp
        check python3 -I $T/compare_exact.py "Tape Delay ($inp in)" "$B" td_old_$inp.bin td_ps_$inp.bin td_blk_$inp.bin
    done
else FAILS=$((FAILS + 1)); fi

step "Tape Delay: defaults regression (startup state, Mix tracking)"
if XFLAGS= build "$B/t_td_defaults" $T/test_tape_delay_defaults.cpp Effect-Modules/tape_delay_module.cpp $TDSRC; then check "$B/t_td_defaults"; else FAILS=$((FAILS + 1)); fi

step "Venus: sliced STFT vs original Fourier, block vs per-sample"
if XFLAGS="-I$T/stub_cmsis" build "$B/t_venus" $T/test_venus.cpp Effect-Modules/venus_module.cpp $BASEMOD $DSP/Effects/sampleratereducer.cpp $DSP/Synthesis/oscillator.cpp $LGPL/Filters/tone.cpp; then check "$B/t_venus"; else FAILS=$((FAILS + 1)); fi

step "Venus FFT: the real CMSIS-DSP real FFT (firmware table configuration) vs ShyFFT"
CMS=dependencies/libDaisy/Drivers/CMSIS-DSP
CMSDEFS="-DARM_DSP_CONFIG_TABLES -DARM_FFT_ALLOW_TABLES -DARM_TABLE_TWIDDLECOEF_F32_2048 -DARM_TABLE_BITREVIDX_FLT_2048 -DARM_TABLE_TWIDDLECOEF_RFFT_F32_4096 -DARM_TABLE_TWIDDLECOEF_F32_512 -DARM_TABLE_BITREVIDX_FLT_512 -DARM_TABLE_TWIDDLECOEF_RFFT_F32_1024 -D__GNUC_PYTHON__"
CMSINC="-I$CMS/Include -I$CMS/PrivateInclude"
mkdir -p "$B/cmsis"
CMSOBJS=""
CMSOK=1
for f in TransformFunctions/arm_rfft_fast_f32 TransformFunctions/arm_rfft_fast_init_f32 TransformFunctions/arm_cfft_f32 TransformFunctions/arm_cfft_init_f32 \
         TransformFunctions/arm_cfft_radix8_f32 TransformFunctions/arm_bitreversal2 CommonTables/arm_common_tables CommonTables/arm_const_structs; do
    o="$B/cmsis/$(basename $f).o"
    gcc -O1 -w $CMSDEFS $CMSINC -c "$CMS/Source/$f.c" -o "$o" || CMSOK=0
    CMSOBJS="$CMSOBJS $o"
done
if [ "$CMSOK" = 1 ] && $CXX -std=c++20 -O1 -w $CMSDEFS $CMSINC -I$T/stub -IEffect-Modules -IUtil $T/test_cmsis_fft.cpp $CMSOBJS -o "$B/t_cmsis_fft" 2>&1 | grep -E "error|undefined" | head -5; [ -x "$B/t_cmsis_fft" ]; then check "$B/t_cmsis_fft"; else FAILS=$((FAILS + 1)); fi

step "Saturn: block vs per-sample, 256-sample blocks, signal path, controls"
if XFLAGS="-I$T/stub_cmsis" build "$B/t_saturn" $T/test_saturn.cpp Effect-Modules/saturn_module.cpp $BASEMOD $DSP/Synthesis/oscillator.cpp; then check "$B/t_saturn"; else FAILS=$((FAILS + 1)); fi

step "Jupiter: block vs per-sample (engine block size 1), controls, Stereo In"
JPSRC="dependencies/CloudSeed/AudioLib/Biquad2.cpp dependencies/CloudSeed/AudioLib/ShaRandom.cpp dependencies/CloudSeed/AudioLib/ValueTables.cpp \
 dependencies/CloudSeed/FastSin.cpp dependencies/CloudSeed/Utils/Sha256.cpp"
rm -rf "$B/jp_n1"; mkdir -p "$B/jp_n1/Effect-Modules" "$B/jp_n1/dependencies"
cp Effect-Modules/jupiter_module.cpp Effect-Modules/jupiter_module.h Effect-Modules/cloudseed_pool.cpp "$B/jp_n1/Effect-Modules/"
cp -r dependencies/CloudSeed "$B/jp_n1/dependencies/CloudSeed"; rm -rf "$B/jp_n1/dependencies/CloudSeed/build"
sed -i 's/static const int bufferSize = [0-9]*;/static const int bufferSize = 1;/' "$B/jp_n1/dependencies/CloudSeed/ReverbController.h"
if XFLAGS="-I$B/jp_n1/Effect-Modules -I$B/jp_n1/dependencies/CloudSeed" build "$B/t_jupiter" $T/test_jupiter.cpp "$B/jp_n1/Effect-Modules/jupiter_module.cpp" "$B/jp_n1/Effect-Modules/cloudseed_pool.cpp" \
   $(for f in $JPSRC; do echo "$B/jp_n1/$f" | sed "s|$B/jp_n1/dependencies|$B/jp_n1/dependencies|"; done) $BASEMOD; then check "$B/t_jupiter"; else FAILS=$((FAILS + 1)); fi

step "Neptune: block vs per-sample (engine block size 1), delay modes, reverb modes, controls"
rm -rf "$B/np_n1"; mkdir -p "$B/np_n1/Effect-Modules" "$B/np_n1/dependencies"
cp Effect-Modules/neptune_module.cpp Effect-Modules/neptune_module.h Effect-Modules/cloudseed_pool.cpp "$B/np_n1/Effect-Modules/"
cp -r Effect-Modules/Neptune "$B/np_n1/Effect-Modules/Neptune"
cp -r dependencies/CloudSeed "$B/np_n1/dependencies/CloudSeed"; rm -rf "$B/np_n1/dependencies/CloudSeed/build"
sed -i 's/static const int bufferSize = [0-9]*;/static const int bufferSize = 1;/' "$B/np_n1/dependencies/CloudSeed/ReverbController.h"
if XFLAGS="-I$B/np_n1/Effect-Modules -I$B/np_n1/dependencies/CloudSeed" build "$B/t_neptune" $T/test_neptune.cpp "$B/np_n1/Effect-Modules/neptune_module.cpp" "$B/np_n1/Effect-Modules/cloudseed_pool.cpp" \
   $(for f in $JPSRC; do echo "$B/np_n1/$f"; done) $BASEMOD $LGPL/Filters/tone.cpp; then check "$B/t_neptune"; else FAILS=$((FAILS + 1)); fi

step "Test Tone: waveforms, level, frequency, smooth changes, input ignored"
if XFLAGS= build "$B/t_test_tone" $T/test_test_tone.cpp Effect-Modules/test_tone_module.cpp $BASEMOD; then check "$B/t_test_tone"; else FAILS=$((FAILS + 1)); fi

step "Uranus: block vs per-sample, controls, MIDI notes and CCs (granular and FM synth)"
if XFLAGS= build "$B/t_uranus" $T/test_uranus.cpp Effect-Modules/uranus_module.cpp Util/granularplayermod.cpp Util/operator.cpp $BASEMOD $DSP/Synthesis/oscillator.cpp $DSP/Control/adsr.cpp $DSP/Control/phasor.cpp; then check "$B/t_uranus"; else FAILS=$((FAILS + 1)); fi

step "Balnea: block vs per-sample, reverse line vs the original, delay time, tap tempo, MIDI"
if XFLAGS="-I$T" build "$B/t_balnea" $T/test_balnea.cpp Effect-Modules/balnea_module.cpp $BASEMOD $DSP/Synthesis/oscillator.cpp $DSP/Control/adsr.cpp $DSP/Filters/svf.cpp; then check "$B/t_balnea"; else FAILS=$((FAILS + 1)); fi

step "Fit text: the biggest font that is wide enough, a cut with ~ when even the smallest is too narrow (UI/fit_text.h)"
if XFLAGS="-IUI" build "$B/t_fit_text" $T/test_fit_text.cpp; then check "$B/t_fit_text"; else FAILS=$((FAILS + 1)); fi

step "Heap guard: the message reaches the handler (the _sbrk limit itself only exists on the Daisy)"
if XFLAGS= build "$B/t_heap_guard" $T/test_heap_guard.cpp Util/heap_guard.cpp; then check "$B/t_heap_guard"; else FAILS=$((FAILS + 1)); fi

step "Duck Compressor: side chain, ratio/threshold/attack/release, clean audio, never boosts, block vs per-sample"
if XFLAGS= build "$B/t_duck" $T/test_duck.cpp Effect-Modules/duck_compressor_module.cpp Effect-Modules/SimpleCompressor/SimpleComp.cpp Effect-Modules/SimpleCompressor/SimpleEnvelope.cpp $BASEMOD; then check "$B/t_duck"; else FAILS=$((FAILS + 1)); fi

step "Pluto: block vs per-sample, loopers (speed, direction, modes, pause, clear), effects, LED"
if XFLAGS="-I$T" build "$B/t_pluto" $T/test_pluto.cpp Effect-Modules/pluto_module.cpp $BASEMOD $REVSC $LGPL/Filters/tone.cpp $LGPL/Filters/atone.cpp $LGPL/Dynamics/balance.cpp; then check "$B/t_pluto"; else FAILS=$((FAILS + 1)); fi

step "Cassini: block vs per-sample, sampler (record, pitch, trim, banks, modes), looper, filter, LED"
if XFLAGS="-I$T" build "$B/t_cassini" $T/test_cassini.cpp Effect-Modules/cassini_module.cpp $BASEMOD $REVSC $DSP/Control/adsr.cpp $DSP/Filters/svf.cpp; then check "$B/t_cassini"; else FAILS=$((FAILS + 1)); fi

step "Mars: network vs the original (RTNeural reference data), block vs per-sample, controls, delay and its second tap"
if XFLAGS="-I$T -I$T/stub_cmsis" build "$B/t_mars" $T/test_mars.cpp Effect-Modules/mars_module.cpp Effect-Modules/ImpulseResponse/ImpulseResponse.cpp $BASEMOD $LGPL/Filters/tone.cpp $LGPL/Filters/atone.cpp $LGPL/Dynamics/balance.cpp; then check "$B/t_mars"; else FAILS=$((FAILS + 1)); fi

step "Mercury: network vs the original (RTNeural-NAM reference data), prewarm, block vs per-sample, EQ, model changes"
if XFLAGS="-I$T" build "$B/t_mercury" $T/test_mercury.cpp Effect-Modules/mercury_module.cpp $BASEMOD; then check "$B/t_mercury"; else FAILS=$((FAILS + 1)); fi

step "Earth: reverb arena, octave, freeze, overdrive swell, block vs per-sample"
if XFLAGS= build "$B/t_earth" $T/test_earth.cpp Effect-Modules/earth_module.cpp Effect-Modules/Dattorro/Dattorro.cpp $BASEMOD $DSP/Effects/overdrive.cpp; then check "$B/t_earth"; else FAILS=$((FAILS + 1)); fi

step "CloudSeed: original ($BASE) vs new (bit-exact at block size 1; shipped block size checked for level/finite)"
CSSRC="dependencies/CloudSeed/AudioLib/Biquad2.cpp dependencies/CloudSeed/AudioLib/ShaRandom.cpp dependencies/CloudSeed/AudioLib/ValueTables.cpp \
 dependencies/CloudSeed/FastSin.cpp dependencies/CloudSeed/Utils/Sha256.cpp Effect-Modules/base_effect_module.cpp"
rm -rf "$B/cs_old" "$B/cs_n1"; mkdir -p "$B/cs_old/Effect-Modules" "$B/cs_old/dependencies" "$B/cs_n1/Effect-Modules" "$B/cs_n1/dependencies"
base_file Effect-Modules/cloudseed_module.cpp > "$B/cs_old/Effect-Modules/cloudseed_module.cpp"
base_file Effect-Modules/cloudseed_module.h > "$B/cs_old/Effect-Modules/cloudseed_module.h"
cp Effect-Modules/linear_change.h "$B/cs_old/Effect-Modules/"; cp -r dependencies/CloudSeed "$B/cs_old/dependencies/CloudSeed"; rm -rf "$B/cs_old/dependencies/CloudSeed/build"
base_file dependencies/CloudSeed/ReverbController.h > "$B/cs_old/dependencies/CloudSeed/ReverbController.h"
cp Effect-Modules/cloudseed_module.cpp Effect-Modules/cloudseed_module.h Effect-Modules/linear_change.h "$B/cs_n1/Effect-Modules/"
cp -r dependencies/CloudSeed "$B/cs_n1/dependencies/CloudSeed"; rm -rf "$B/cs_n1/dependencies/CloudSeed/build"
sed -i 's/static const int bufferSize = [0-9]*;/static const int bufferSize = 1;/' "$B/cs_n1/dependencies/CloudSeed/ReverbController.h"
if XFLAGS="-I$B/cs_old/Effect-Modules -I$B/cs_old/dependencies/CloudSeed" build "$B/cs_old_bin" $T/test_cloudseed.cpp "$B/cs_old/Effect-Modules/cloudseed_module.cpp" $CSSRC \
   && XFLAGS="-I$B/cs_n1/Effect-Modules -I$B/cs_n1/dependencies/CloudSeed" build "$B/cs_n1_bin" $T/test_cloudseed.cpp "$B/cs_n1/Effect-Modules/cloudseed_module.cpp" Effect-Modules/cloudseed_pool.cpp $CSSRC \
   && XFLAGS="-Idependencies/CloudSeed" build "$B/cs_new_bin" $T/test_cloudseed.cpp Effect-Modules/cloudseed_module.cpp Effect-Modules/cloudseed_pool.cpp $CSSRC; then
    "$B/cs_old_bin" sample "$B/cs_old.bin"; "$B/cs_n1_bin" block "$B/cs_n1.bin"; "$B/cs_new_bin" block "$B/cs_new.bin"
    check python3 -I $T/compare_cloudseed.py "$B"
else FAILS=$((FAILS + 1)); fi

echo
if [ "$FAILS" -eq 0 ]; then echo "ALL HOST TESTS PASSED"; else echo "HOST TESTS FAILED: $FAILS step(s)"; fi
exit $((FAILS > 0))
