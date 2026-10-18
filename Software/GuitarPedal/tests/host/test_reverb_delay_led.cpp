// The tempo LED of Reverb Delay blinks once every two delay times (like the Delay module). The pedal asks for the LED brightness twice per
// 48-sample audio block (once for each LED), so the blink must not depend on how often the LED is read.
// test_reverb_delay_led: one process per case (the module keeps its delay lines in static memory)   argv: <delay ms or "default"> <block|sample>
#include "reverb_delay_module.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace bkshepherd;
int main(int argc, char **argv) {
    ReverbDelayModule *m = new ReverbDelayModule();
    m->Init(48000.0f);
    m->SetEnabled(true);
    float delayMs = 400.0f; // the default
    if (strcmp(argv[1], "default") != 0) {
        delayMs = (float)atof(argv[1]);
        m->SetParameterAsFloat(ReverbDelayModule::DELAY_TIME, (delayMs - 50.0f) / (1500.0f - 50.0f)); // the parameter is 0..1 over 50..1500 ms
    }
    const bool block = strcmp(argv[2], "block") == 0;
    const int seconds = 12, B = 48;
    std::vector<float> in(B, 0.0f), l(B), r(B);
    int edges = 0, onBlocks = 0, total = 0;
    float prev = 0.0f, firstEdge = -1.0f, lastEdge = -1.0f;
    for (int b = 0; b < seconds * 48000 / B; b++) {
        if (block) m->ProcessMonoBlock(in.data(), l.data(), r.data(), B);
        else for (int i = 0; i < B; i++) m->ProcessMono(in[i]);
        (void)m->GetBrightnessForLED(0); // the firmware reads both LEDs every block
        const float led = m->GetBrightnessForLED(1);
        if (led > 0.5f && prev <= 0.5f) {
            const float t = b * B / 48000.0f;
            // The first pulse starts at 0, in the middle of its "on" part: only the pulses after it start at the same point of the cycle
            if (edges >= 1) {
                if (firstEdge < 0) firstEdge = t;
                lastEdge = t;
            }
            edges++;
        }
        onBlocks += led > 0.5f;
        total++;
        prev = led;
    }
    const float period = edges > 2 ? (lastEdge - firstEdge) / (edges - 2) : 0.0f;
    const float expected = 2.0f * delayMs / 1000.0f;
    const bool ok = edges > 2 && fabsf(period - expected) < 0.03f * expected + 0.003f;
    printf("delay %.0f ms (%s): blink period %.3f s, expected %.3f s, on %.0f%% of the time (expected 27.5%%), %d pulses  %s\n", delayMs, argv[2], period, expected,
           100.0f * onBlocks / total, edges, ok ? "OK" : "WRONG");
    return ok ? 0 : 1;
}
