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
sh Effect-Modules/Nam/update_models.sh --no-calibrate   # the NAM models are one file each in Effect-Modules/Nam/models/

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

step "Reverb Delay: block path vs per-sample path (bit-exact, every delay mode, modulation target and routing)"
if build "$B/t_reverb_delay" $T/test_reverb_delay.cpp Effect-Modules/reverb_delay_module.cpp $LGPL/Effects/reverbsc.cpp $LGPL/Filters/tone.cpp \
    $DSP/Synthesis/oscillator.cpp $BASEMOD; then
    RDFAIL=0
    for cfg in 0 1 2 3 4 5 6 7 8 9 10 11 12; do # the module is a process-wide static: one process per run
        "$B/t_reverb_delay" sample $cfg 48 "$B/rd_sample_$cfg.bin" > /dev/null || RDFAIL=1
        "$B/t_reverb_delay" mono $cfg 48 "$B/rd_mono_$cfg.bin" > /dev/null
        "$B/t_reverb_delay" mono $cfg 37 "$B/rd_mono37_$cfg.bin" > /dev/null
        "$B/t_reverb_delay" samplest $cfg 48 "$B/rd_samplest_$cfg.bin" > /dev/null
        "$B/t_reverb_delay" stereo $cfg 48 "$B/rd_stereo_$cfg.bin" > /dev/null
        for pair in "sample mono" "sample mono37" "samplest stereo"; do
            set -- $pair
            if cmp -s "$B/rd_$1_$cfg.bin" "$B/rd_$2_$cfg.bin"; then :; else echo "Reverb Delay config $cfg: $2 differs from $1"; RDFAIL=1; fi
        done
    done
    if [ "$RDFAIL" -eq 0 ]; then echo "Reverb Delay: block path equals the per-sample path in all 13 configurations   OK"; else FAILS=$((FAILS + 1)); fi
else FAILS=$((FAILS + 1)); fi

step "Reverb Delay: the tempo LED blinks with the delay time (read twice per block, as the firmware does)"
if build "$B/t_reverb_delay_led" $T/test_reverb_delay_led.cpp Effect-Modules/reverb_delay_module.cpp $LGPL/Effects/reverbsc.cpp $LGPL/Filters/tone.cpp \
    $DSP/Synthesis/oscillator.cpp $BASEMOD; then
    for args in "default block" "default sample" "100 block" "1000 block" "1500 sample"; do # one process per run
        check "$B/t_reverb_delay_led" $args
    done
else FAILS=$((FAILS + 1)); fi

step "Delay and Reverb Delay in one firmware: no clashing names, no shared state"
if build "$B/t_delay_coexist" $T/test_delay_coexist.cpp Effect-Modules/delay_module.cpp Effect-Modules/reverb_delay_module.cpp $LGPL/Effects/reverbsc.cpp \
    $LGPL/Filters/tone.cpp $DSP/Synthesis/oscillator.cpp Util/tape_modulator.cpp $BASEMOD; then
    "$B/t_delay_coexist" both "$B/co_both.bin" && "$B/t_delay_coexist" delay "$B/co_delay.bin" > /dev/null && "$B/t_delay_coexist" reverbdelay "$B/co_rd.bin" > /dev/null
    if python3 -I - "$B" <<'PY'
import sys
b = sys.argv[1]
rd = lambda n: open(f"{b}/{n}.bin", "rb").read()
both, d, r = rd("co_both"), rd("co_delay"), rd("co_rd")
h = len(both) // 2  # first half: Delay, second half: Reverb Delay
ok = both[:h] == d[:h] and both[h:] == r[h:] and any(both[:h]) and any(both[h:])
print("Delay and Reverb Delay together equal each alone (bit-exact):", "OK" if ok else "MISMATCH")
sys.exit(0 if ok else 1)
PY
    then :; else FAILS=$((FAILS + 1)); fi
else FAILS=$((FAILS + 1)); fi

step "Harmonic Tremolo: the tempo LED blinks once per LFO cycle, at the tapped tempo, whatever the depth"
if build "$B/t_ht_led" $T/test_harmonic_tremolo_led.cpp Effect-Modules/harmonic_tremolo_module.cpp $DSP/Synthesis/oscillator.cpp $BASEMOD; then
    for args in "60 0.5" "120 0.5" "120 0.0" "120 1.0" "180 0.3" "90 0.5 stereo"; do check "$B/t_ht_led" $args; done
else FAILS=$((FAILS + 1)); fi

step "Distortion and Overdrive: block paths vs per-sample paths (bit-exact; every distortion type, oversampling on and off)"
if build "$B/t_dist_block" $T/test_distortion_block.cpp Effect-Modules/distortion_module.cpp Effect-Modules/overdrive_module.cpp $LGPL/Filters/tone.cpp \
    $DSP/Effects/overdrive.cpp $BASEMOD; then
    DBFAIL=0
    for type in 0 1 2 3 4 5; do for os in 0 1; do # Distortion keeps its filters in file-level variables: one process per run
        "$B/t_dist_block" sample $type $os 48 "$B/db_sample_${type}_$os.bin" > /dev/null || DBFAIL=1
        "$B/t_dist_block" mono $type $os 48 "$B/db_mono_${type}_$os.bin" > /dev/null
        "$B/t_dist_block" stereo $type $os 37 "$B/db_stereo_${type}_$os.bin" > /dev/null
        for m in mono stereo; do
            cmp -s "$B/db_sample_${type}_$os.bin" "$B/db_${m}_${type}_$os.bin" || { echo "Distortion type $type oversampling $os: $m block path differs from the per-sample path"; DBFAIL=1; }
        done
    done; done
    [ "$DBFAIL" -eq 0 ] && echo "Distortion: block paths (mono 48, stereo 37) equal the per-sample path, 6 types, oversampling on and off   OK" || FAILS=$((FAILS + 1))
    check "$B/t_dist_block" overdrive
else FAILS=$((FAILS + 1)); fi

step "Crusher, Chopper, Modulated Tremolo, Harmonic Tremolo: block paths vs per-sample paths (bit-exact)"
if build "$B/t_block_conv" $T/test_block_conversions.cpp Effect-Modules/crusher_module.cpp Effect-Modules/chopper_module.cpp Effect-Modules/Chopper/*.cpp \
    Effect-Modules/modulated_tremolo_module.cpp Effect-Modules/harmonic_tremolo_module.cpp Util/frequency_detector_q.cpp $DSP/Synthesis/oscillator.cpp \
    $DSP/Effects/tremolo.cpp $DSP/Control/adsr.cpp $BASEMOD; then
    check "$B/t_block_conv"
    BCFAIL=0
    for cfg in 0 1 2 3 4 5; do # the Crusher's pitch detector is shared by all instances: one process per run
        for mode in sample samplemono mono stereo; do "$B/t_block_conv" tracking $mode $cfg "$B/bc_${mode}_$cfg.bin"; done
        cmp -s "$B/bc_samplemono_$cfg.bin" "$B/bc_mono_$cfg.bin" || { echo "Crusher with pitch tracking, config $cfg: the mono block path differs"; BCFAIL=1; }
        cmp -s "$B/bc_sample_$cfg.bin" "$B/bc_stereo_$cfg.bin" || { echo "Crusher with pitch tracking, config $cfg: the stereo block path differs"; BCFAIL=1; }
    done
    [ "$BCFAIL" -eq 0 ] && echo "  Crusher with pitch tracking on: block paths equal the per-sample paths, 6 configurations   OK" || FAILS=$((FAILS + 1))
else FAILS=$((FAILS + 1)); fi

step "Distortion family (Preamp, Screamer, Muff Fuzz, Wavefolder, Exciter, Bass Dist, Octa Fuzz, Tape Sat, Chainsaw): block path, bounds, behaviour"
FAMSRC="Effect-Modules/preamp_module.cpp Effect-Modules/screamer_module.cpp Effect-Modules/muff_fuzz_module.cpp Effect-Modules/wavefolder_module.cpp \
 Effect-Modules/exciter_module.cpp Effect-Modules/bass_dist_module.cpp Effect-Modules/octave_fuzz_module.cpp Effect-Modules/tape_sat_module.cpp Effect-Modules/chainsaw_module.cpp Effect-Modules/booster_module.cpp"
if build "$B/t_dist_family" $T/test_distortion_family.cpp $FAMSRC $BASEMOD; then check "$B/t_dist_family"; else FAILS=$((FAILS + 1)); fi

step "Spring Reverb: the dispersive chain, the echoes of a burst (spacing, sweep, Drip), the decay time, Dwell, Tone, Mix, block path, bounds"
if build "$B/t_spring_reverb" $T/test_spring_reverb.cpp Effect-Modules/spring_reverb_module.cpp $BASEMOD; then check "$B/t_spring_reverb"; else FAILS=$((FAILS + 1)); fi

step "Booster (clean, treble, mid, warm): the frequency response of each type, Gain, Tone, Level, saturation"
if build "$B/t_booster" $T/test_booster.cpp Effect-Modules/booster_module.cpp $BASEMOD; then check "$B/t_booster"; else FAILS=$((FAILS + 1)); fi

step "Dist Bench (four distortion stages in series): each type as on its own, the generic controls, block path, bounds, type change"
if build "$B/t_dist_bench" $T/test_dist_bench.cpp Effect-Modules/dist_bench_module.cpp $FAMSRC Effect-Modules/overdrive_module.cpp $DSP/Effects/overdrive.cpp \
    Effect-Modules/distortion_module.cpp $LGPL/Filters/tone.cpp $BASEMOD; then
    check "$B/t_dist_bench"
else FAILS=$((FAILS + 1)); fi

step "Tap tempo series (Util/tap_tempo.h): three taps, the tempo from the last two"
if build "$B/t_tap_tempo" $T/test_tap_tempo.cpp; then check "$B/t_tap_tempo"; else FAILS=$((FAILS + 1)); fi

step "Delay: switching it on starts from silence (on its own and in a chain), Trails keep the repeats, playing during the clearing is kept"
if build "$B/t_delay_clear" $T/test_delay_clear.cpp Effect-Modules/delay_module.cpp Effect-Modules/effect_chain.cpp $DSP/Dynamics/crossfade.cpp \
    $LGPL/Filters/tone.cpp $DSP/Synthesis/oscillator.cpp Util/tape_modulator.cpp $BASEMOD; then check "$B/t_delay_clear"; else FAILS=$((FAILS + 1)); fi

step "Chain Switches (Util/chain_switches.h): which slots the footswitches get, the Switches parameter, the slot interface of EffectChain"
if build "$B/t_chain_switches" $T/test_chain_switches.cpp Effect-Modules/effect_chain.cpp Effect-Modules/screamer_module.cpp Effect-Modules/wavefolder_module.cpp \
    Effect-Modules/tape_sat_module.cpp $DSP/Dynamics/crossfade.cpp $BASEMOD; then check "$B/t_chain_switches"; else FAILS=$((FAILS + 1)); fi

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

step "CloudSeed: the wet-only LED pulses on a sine curve, once per half second"
if XFLAGS="-Idependencies/CloudSeed" build "$B/t_cs_led" $T/test_cloudseed_led.cpp Effect-Modules/cloudseed_module.cpp Effect-Modules/cloudseed_pool.cpp $CSSRC Util/audio_utilities.cpp Util/heap_guard.cpp; then
    for mode in block sample; do check "$B/t_cs_led" $mode; done
else FAILS=$((FAILS + 1)); fi

echo
if [ "$FAILS" -eq 0 ]; then echo "ALL HOST TESTS PASSED"; else echo "HOST TESTS FAILED: $FAILS step(s)"; fi
exit $((FAILS > 0))
