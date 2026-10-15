// Tape Delay defaults regression test.
//  1. At startup (only Init() has run, as on the pedal) the wet/dry balance must be right: echo present, output finite, and independent of what the
//     module's memory contained (the module is built with placement new over memory pre-filled with a pattern to expose uninitialised reads).
//  2. The startup state must equal the state after touching Mix away and back to its default (which is what fires ParameterChanged).
//  3. A single jump of Mix must reach the target mix, not stay one change behind.
// Past bug: Init() derived the wet/dry balance from an uninitialised smoothed mix, and the balance was only recomputed on a Mix change, from the
// still-trailing smoothed value.
#include "tape_delay_module.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>
using namespace bkshepherd;

// Plays a short noise burst then silence and returns the RMS of the echo window and of the dry burst window, both on the left output.
static void run(TapeDelayModule &m, double &dryRms, double &echoRms) {
    const size_t total = 48000 * 3, B = 48;
    std::vector<float> in(total, 0.0f), out(total), outR(total);
    srand(3);
    for (size_t i = 0; i < 9600; i++) in[i] = 0.4f * ((rand() / (float)RAND_MAX) - 0.5f) * 2.0f; // 0.2 s burst
    for (size_t o = 0; o < total; o += B) m.ProcessStereoBlock(&in[o], &in[o], &out[o], &outR[o], B);
    auto rms = [&](size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; i++) s += double(out[i]) * out[i]; return std::sqrt(s / (b - a)); };
    dryRms = rms(2000, 8000);
    echoRms = rms(48000 + 4000, 48000 + 12000); // the first echo lands about 1.04 s after the burst starts (default Time 0.25)
}

// Runs the same amount of silence as settleTouch(), so runs with and without a Mix touch have the same wow/flutter and glitch generator phase
static void idle(TapeDelayModule &m) {
    std::vector<float> z(9600, 0.0f), o(9600), o2(9600);
    for (size_t i = 0; i < 9600; i += 48) m.ProcessStereoBlock(&z[i], &z[i], &o[i], &o2[i], 48);
}

// Moves Mix away from its default and back, running silence in between so the smoothed value settles each time
static void settleTouch(TapeDelayModule &m) {
    std::vector<float> z(4800, 0.0f), o(4800), o2(4800);
    const float def = m.GetParameterDefaultValueAsFloat(TapeDelayModule::MIX);
    m.SetParameterAsFloat(TapeDelayModule::MIX, 0.9f);
    for (size_t i = 0; i < 4800; i += 48) m.ProcessStereoBlock(&z[i], &z[i], &o[i], &o2[i], 48);
    m.SetParameterAsFloat(TapeDelayModule::MIX, def);
    for (size_t i = 0; i < 4800; i += 48) m.ProcessStereoBlock(&z[i], &z[i], &o[i], &o2[i], 48);
}

// Echo RMS after a single jump of Mix to 'target', versus after two nudges that let the smoothed value catch up in between
static double echoAfter(float target, bool twoStep) {
    alignas(16) static unsigned char mem2[sizeof(TapeDelayModule)];
    std::memset(mem2, 0x00, sizeof mem2);
    TapeDelayModule *m = new (mem2) TapeDelayModule();
    m->Init(48000.0f);
    settleTouch(*m);
    std::vector<float> z(4800, 0.0f), o(4800), o2(4800);
    m->SetParameterAsFloat(TapeDelayModule::MIX, target);
    if (twoStep) {
        for (size_t i = 0; i < 4800; i += 48) m->ProcessStereoBlock(&z[i], &z[i], &o[i], &o2[i], 48);
        m->SetParameterAsFloat(TapeDelayModule::MIX, target + 0.0001f);
    }
    double dry, echo; run(*m, dry, echo);
    return echo;
}

static int g_fails = 0;
static void check(bool ok, const char *what) { printf("  %-62s %s\n", what, ok ? "OK" : "FAIL"); if (!ok) g_fails++; }
static bool near(double a, double b, double relTol) { return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) <= relTol * std::fmax(std::fabs(a), std::fabs(b)); }

int main() {
    const unsigned char patterns[] = {0x00, 0xA5, 0xFF};
    const char *names[] = {"zero memory", "0xA5 garbage memory", "0xFF (NaN) memory"};
    alignas(16) static unsigned char mem[sizeof(TapeDelayModule)];
    double startupEcho[3];
    printf("Startup state (Init only):\n");
    for (int p = 0; p < 3; p++) {
        std::memset(mem, patterns[p], sizeof mem);
        TapeDelayModule *m = new (mem) TapeDelayModule();
        m->Init(48000.0f);
        idle(*m);
        double dry, echo; run(*m, dry, echo);
        startupEcho[p] = echo;
        printf("  %-22s dry %.4f  echo %.4f\n", names[p], dry, echo);
        check(std::isfinite(dry) && std::isfinite(echo) && echo > 0.05, "output is finite and the echo is present");
    }
    check(near(startupEcho[0], startupEcho[1], 0.001) && near(startupEcho[0], startupEcho[2], 0.001), "startup result does not depend on the memory contents");

    // Reference: Mix touched away and back, with time for the smoothing to settle in between
    std::memset(mem, 0x00, sizeof mem);
    TapeDelayModule *m = new (mem) TapeDelayModule();
    m->Init(48000.0f);
    settleTouch(*m);
    double dry, touched; run(*m, dry, touched);
    printf("\nTouched to default: dry %.4f  echo %.4f\n", dry, touched);
    check(near(startupEcho[0], touched, 0.02), "startup echo equals the echo after touching Mix back to its default");

    const double single = echoAfter(0.9f, false), two = echoAfter(0.9f, true);
    printf("\nMix 0.4 -> 0.9 in one step: echo %.4f; re-applied later: echo %.4f\n", single, two);
    check(near(single, two, 0.03), "a single jump of Mix reaches the target mix");

    printf("\n%s\n", g_fails ? "TAPE DELAY DEFAULTS: FAILURES" : "TAPE DELAY DEFAULTS OK");
    return g_fails != 0;
}
