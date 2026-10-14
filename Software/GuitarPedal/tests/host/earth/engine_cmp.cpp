// Runs a Dattorro engine with Earth's settings and writes its impulse response (L, R). Built twice by tests/host/compare_earth_engine.sh: against Earth's own
// engine (FunBox project) and against this repo's.
// args: out.bin timeScale modSpeedScale modDepth [engineSampleRate]
#include "Dattorro/Dattorro.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
int main(int argc, char **argv) {
    const float ts = atof(argv[2]), msScale = atof(argv[3]), modDepth = atof(argv[4]);
    const float sr = (argc > 5) ? atof(argv[5]) : 48000.0f;   // sample rate the engine is told
    static Dattorro reverb(sr, 16, 4.0);                // as Earth: samplerate, max_lfo_depth, max_timescale
    reverb.setTimeScale(ts);
    reverb.setPreDelay(0.0);
    reverb.setInputFilterLowCutoffPitch(0.0);
    reverb.setInputFilterHighCutoffPitch(10.0 - 7.0 * 0.0);
    reverb.enableInputDiffusion(true);
    reverb.setDecay(0.877465);
    reverb.setTankDiffusion(0.7);
    reverb.setTankFilterLowCutFrequency(0.0);
    reverb.setTankFilterHighCutFrequency(10.0);
    reverb.setTankModSpeed((0.3 + 0.3 * 15) * msScale);
    reverb.setTankModDepth(modDepth * 8);
    reverb.setTankModShape(0.5);
    reverb.clear();
    const size_t total = 48000 * 4;
    std::vector<float> l(total), r(total);
    for (size_t i = 0; i < total; i++) {
        const float x = (i == 0) ? 1.0f : 0.0f;
        reverb.process(x, x);
        l[i] = reverb.getLeftOutput(); r[i] = reverb.getRightOutput();
    }
    FILE *f = fopen(argv[1], "wb"); fwrite(l.data(), 4, total, f); fwrite(r.data(), 4, total, f); fclose(f);
}
