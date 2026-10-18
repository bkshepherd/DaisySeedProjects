// The tempo LED of Harmonic Tremolo blinks once per LFO cycle, and the LFO runs at the tapped tempo (SetTempo(bpm): one cycle per beat),
// whatever the depth. The pedal reads the LED twice per 48-sample block.
// test_harmonic_tremolo_led <bpm> <depth 0..1> [stereo]
#include "harmonic_tremolo_module.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace bkshepherd;
int main(int argc, char **argv) {
    const int bpm = atoi(argv[1]);
    const float depth = (float)atof(argv[2]);
    const bool stereo = argc > 3;
    HarmonicTremoloModule *m = new HarmonicTremoloModule();
    m->Init(48000.0f);
    m->SetEnabled(true);
    m->SetParameterAsFloat(HarmonicTremoloModule::DEPTH, depth);
    m->SetTempo((uint32_t)bpm);
    const int B = 48, seconds = 12;
    int edges = 0, onBlocks = 0, total = 0;
    float prev = 0.0f, first = -1.0f, last = -1.0f;
    for (int b = 0; b < seconds * 48000 / B; b++) {
        for (int i = 0; i < B; i++) {
            const float x = 0.3f * sinf(0.05f * (b * B + i));
            if (stereo) m->ProcessStereo(x, x); else m->ProcessMono(x);
        }
        (void)m->GetBrightnessForLED(0);
        const float led = m->GetBrightnessForLED(1);
        if (led > 0.5f && prev <= 0.5f) {
            const float t = b * B / 48000.0f;
            if (edges >= 1) { if (first < 0) first = t; last = t; } // the first pulse is the one that is already on
            edges++;
        }
        onBlocks += led > 0.5f;
        total++;
        prev = led;
    }
    const float period = edges > 2 ? (last - first) / (edges - 2) : 0.0f;
    const float expected = 60.0f / bpm;
    const float duty = 100.0f * onBlocks / total;
    const bool ok = edges > 2 && fabsf(period - expected) < 0.03f * expected + 0.003f && fabsf(duty - 27.5f) < 4.0f;
    printf("%d BPM, depth %.1f%s: blink period %.3f s, expected %.3f s, on %.0f%% of the time (27.5%%), %d pulses  %s\n", bpm, depth, stereo ? ", stereo" : "", period, expected, duty, edges, ok ? "OK" : "WRONG");
    return ok ? 0 : 1;
}
