// The left LED of CloudSeed in wet-only mode (second footswitch): a sine-curve pulse, one cycle per half second, measured in time (the
// pedal reads the LED twice per 48-sample audio block). test_cloudseed_led <block|sample>
#include "cloudseed_module.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace bkshepherd;
static int fails = 0;
static void check(bool ok, const char *what) { fails += !ok; std::printf("  %-100s %s\n", what, ok ? "OK" : "FAILED"); }
int main(int argc, char **argv) {
    const bool block = !strcmp(argv[1], "block");
    CloudSeedModule *m = new CloudSeedModule();
    m->Init(48000.0f);
    m->SetEnabled(true);
    const int B = 48;
    std::vector<float> in(B, 0.0f), l(B), r(B);
    auto run = [&](int blocks) {
        for (int b = 0; b < blocks; b++) {
            if (block) m->ProcessMonoBlock(in.data(), l.data(), r.data(), B);
            else for (int i = 0; i < B; i++) m->ProcessMono(in[i]);
        }
    };
    auto led = [&]() { (void)m->GetBrightnessForLED(0); return m->GetBrightnessForLED(1); };

    run(100);
    check(led() == 1.0f, "outside wet-only the LED is steady on");

    m->AlternateFootswitchPressed(); // wet-only on
    float mn = 1.0f, mx = 0.0f, maxErr = 0.0f, prev = led(), first = -1.0f, last = -1.0f;
    int edges = 0;
    check(std::fabs(prev - 1.0f) < 1e-6f, "the flash starts at full brightness");
    for (int b = 1; b <= 6 * 1000; b++) { // 6 s
        run(1);
        const float v = led();
        mn = fminf(mn, v); mx = fmaxf(mx, v);
        const float phase = std::fmod(b * B / 24000.0f, 1.0f); // one cycle per 24000 samples
        maxErr = fmaxf(maxErr, std::fabs(v - (0.5f + 0.5f * std::cos(6.2831853f * phase))));
        if (v > 0.5f && prev <= 0.5f) { const float t = b * B / 48000.0f; if (first < 0) first = t; last = t; edges++; }
        prev = v;
    }
    const float period = edges > 1 ? (last - first) / (edges - 1) : 0.0f;
    std::printf("  period %.4f s, range %.3f .. %.3f, largest difference from the sine curve %.2e, %d pulses\n", period, mn, mx, maxErr, edges);
    check(std::fabs(period - 0.5f) < 0.005f, "one cycle per half second");
    check(mn < 0.01f && mx > 0.99f, "the brightness goes from dark to full");
    check(maxErr < 5e-3f, "the brightness follows a sine curve (within the float rounding of the accumulated phase)");

    m->AlternateFootswitchPressed(); // wet-only off
    run(10);
    check(led() == 1.0f, "after the second press the LED is steady on again");
    std::printf(fails ? "CLOUDSEED LED TESTS FAILED\n" : "ALL CLOUDSEED LED TESTS PASSED\n");
    return fails ? 1 : 0;
}
