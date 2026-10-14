#!/usr/bin/env bash
# Prints how much heap (memory from `new`) each converted effect allocates in its constructor and Init(), measured on the host. See PERFORMANCE_AND_MEMORY.md.
set -uo pipefail
cd "$(dirname "$0")/../.."          # Software/GuitarPedal
T=tests/host
B=${BUILD_DIR:-/tmp/gp_host_tests}
mkdir -p "$B"
DSP=dependencies/DaisySP/Source; LGPL=dependencies/DaisySP/DaisySP-LGPL/Source; CS=dependencies/CloudSeed
g++ -std=c++20 -O1 -w -I$T/stub -I$T/stub_cmsis -I. -IEffect-Modules -IUtil -I$DSP -I$DSP/Utility -I$LGPL -include daisysp-lgpl.h -Idependencies/gcem/include -I$CS \
  $T/heap_report.cpp Effect-Modules/venus_module.cpp Effect-Modules/jupiter_module.cpp Effect-Modules/saturn_module.cpp Effect-Modules/neptune_module.cpp Effect-Modules/uranus_module.cpp \
  Effect-Modules/cloudseed_pool.cpp Effect-Modules/base_effect_module.cpp Util/audio_utilities.cpp Util/granularplayermod.cpp Util/operator.cpp \
  $DSP/Effects/sampleratereducer.cpp $DSP/Synthesis/oscillator.cpp $DSP/Control/adsr.cpp $DSP/Control/phasor.cpp $LGPL/Filters/tone.cpp \
  $CS/AudioLib/Biquad2.cpp $CS/AudioLib/ShaRandom.cpp $CS/AudioLib/ValueTables.cpp $CS/FastSin.cpp $CS/Utils/Sha256.cpp -o "$B/heap_report" 2>&1 | grep -E "error|undefined" | head -5
"$B/heap_report"
