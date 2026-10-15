// Jupiter tests (the reverb engine itself is the repository's CloudSeed code, tested through the CloudSeed module):
// This test is built against a copy of the engine with its internal block size set to 1 (see run_tests.sh), like the CloudSeed comparison: the engine's
// feedback is delayed by its block size minus one, so only at block size 1 are the block and per-sample paths bit-exact.
//  1. The block path and the per-sample path give the same output, with the defaults and after the controls were changed (the changes are pushed to the engine
//     on the same sample in both).
//  2. The controls do something: Lush, Modulation, PreDelay, Decay, the shelves and Mix change the output; Mix 0 is the dry signal.
//  3. Stereo In off feeds the left input to both sides.
#include "jupiter_module.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace bkshepherd;

static int g_fails = 0;
static void check(bool ok, const char *what) { printf("  %-72s %s\n", what, ok ? "OK" : "FAIL"); if (!ok) g_fails++; }

extern size_t pool_index; // the reverb engine's memory pool never frees; each module instance takes about 4.9 MB

static constexpr size_t kBlock = 48;
static constexpr size_t kTotal = 48000;

static void makeInput(std::vector<float> &l, std::vector<float> &r) {
    l.assign(kTotal, 0.0f); r.assign(kTotal, 0.0f);
    unsigned s = 4242u;
    for (size_t i = 0; i < kTotal; i++) {
        s = 1664525u * s + 1013904223u;
        const float noise = ((s >> 8) & 0xFFFF) / 32768.0f - 1.0f;
        const bool burst = i < 4800 || (i > 20000 && i < 21000);
        l[i] = burst ? 0.4f * noise + 0.2f * std::sin(0.03f * i) : 0.0f;
        r[i] = burst ? 0.3f * noise - 0.2f * std::sin(0.05f * i) : 0.0f;
    }
}

struct Out { std::vector<float> l, r; };

// config: callback to set up parameters; path: 0 = per-sample, 1 = block (48), 2 = block (256)
template <class Cfg> static Out run(Cfg cfg, int path, bool stereoInput) {
    srand(7); // the engine's modulation phases come from rand()
    pool_index = 0;
    static unsigned char mem[sizeof(JupiterModule)];
    std::memset(mem, 0, sizeof mem);
    JupiterModule *m = new (mem) JupiterModule();
    m->Init(48000.0f);
    cfg(*m);
    std::vector<float> inL, inR;
    makeInput(inL, inR);
    Out o{std::vector<float>(kTotal), std::vector<float>(kTotal)};
    const size_t bs = path == 2 ? 256 : kBlock;
    for (size_t b = 0; b < kTotal; b += bs) {
        const size_t n = std::min(bs, kTotal - b);
        if (path == 0) {
            for (size_t i = b; i < b + n; i++) {
                if (stereoInput) m->ProcessStereo(inL[i], inR[i]); else m->ProcessMono(inL[i]);
                o.l[i] = m->GetAudioLeft(); o.r[i] = m->GetAudioRight();
            }
        } else if (stereoInput) {
            m->ProcessStereoBlock(&inL[b], &inR[b], &o.l[b], &o.r[b], n);
        } else {
            m->ProcessMonoBlock(&inL[b], &o.l[b], &o.r[b], n);
        }
    }
    return o;
}

static double maxDiff(const Out &a, const Out &b, double &peak) {
    double d = 0; peak = 0;
    for (size_t i = 0; i < kTotal; i++) {
        d = std::fmax(d, std::fmax(std::fabs(a.l[i] - b.l[i]), std::fabs(a.r[i] - b.r[i])));
        peak = std::fmax(peak, std::fmax(std::fabs(a.l[i]), std::fabs(a.r[i])));
    }
    return d;
}

static double tailEnergy(const Out &o, size_t from) { double e = 0; for (size_t i = from; i < kTotal; i++) e += o.l[i] * o.l[i] + o.r[i] * o.r[i]; return e; }

int main() {
    auto defaults = [](JupiterModule &) {};
    auto changed = [](JupiterModule &m) {
        m.SetParameterAsFloat(JupiterModule::DECAY, 0.8f);
        m.SetParameterAsFloat(JupiterModule::LOW_GAIN, 0.3f);
        m.SetParameterAsFloat(JupiterModule::HIGH_FREQ, 0.3f);
        m.SetParameterRaw(JupiterModule::LUSH, 2);
        m.SetParameterRaw(JupiterModule::MODULATION, 0);
        m.SetParameterRaw(JupiterModule::PRE_DELAY, 1);
    };

    printf("block path vs per-sample path:\n");
    for (int v = 0; v < 2; v++) for (int st = 0; st < 2; st++) {
        auto cfg = [&](JupiterModule &m) { if (v) changed(m); if (st) m.SetParameterRaw(JupiterModule::STEREO_IN, 1); };
        const Out a = run(cfg, 0, st), b = run(cfg, 1, st);
        double peak; const double d = maxDiff(a, b, peak);
        printf("  %s, %s input: max difference %.3g, peak %.3f\n", v ? "changed controls" : "defaults", st ? "stereo" : "mono", d, peak);
        check(d == 0.0 && peak > 0.05 && std::isfinite(peak), "block path equals per-sample path");
    }

    // (With changed controls the block size does matter: a changed control is pushed to the engine one per block, so the six changes take 6 blocks, 288 or 1536 samples.)
    printf("\n256-sample blocks give the same audio as 48-sample blocks (defaults):\n");
    {
        const Out a = run(defaults, 1, 1), b = run(defaults, 2, 1);
        double peak; const double d = maxDiff(a, b, peak);
        printf("  max difference %.3g\n", d);
        check(d == 0.0 && peak > 0.05, "identical audio at 256-sample blocks");
        const Out c = run(changed, 2, 1);
        double peak2; maxDiff(c, c, peak2);
        check(peak2 > 0.05 && std::isfinite(peak2), "256-sample blocks with changed controls: finite output");
    }

    printf("\ncontrols:\n");
    const Out base = run(defaults, 1, 1);
    auto differs = [&](const Out &o) { double p; return maxDiff(base, o, p) > 1e-4; };
    check(differs(run([](JupiterModule &m) { m.SetParameterRaw(JupiterModule::LUSH, 0); }, 1, 1)), "Lush Low differs from Medium");
    check(differs(run([](JupiterModule &m) { m.SetParameterRaw(JupiterModule::LUSH, 2); }, 1, 1)), "Lush High differs from Medium");
    check(differs(run([](JupiterModule &m) { m.SetParameterRaw(JupiterModule::MODULATION, 2); }, 1, 1)), "Modulation High differs from Medium");
    check(differs(run([](JupiterModule &m) { m.SetParameterAsFloat(JupiterModule::LOW_GAIN, 0.0f); }, 1, 1)), "Low Gain changes the sound");
    check(differs(run([](JupiterModule &m) { m.SetParameterAsFloat(JupiterModule::HIGH_GAIN, 0.0f); }, 1, 1)), "High Gain changes the sound");
    check(differs(run([](JupiterModule &m) { m.SetParameterAsFloat(JupiterModule::LOW_FREQ, 0.9f); m.SetParameterAsFloat(JupiterModule::LOW_GAIN, 0.2f); }, 1, 1)), "Low Freq changes the sound");

    // PreDelay: the first wet signal arrives later. Input starts at sample 0, so compare the early reverb energy.
    {
        const Out none = run([](JupiterModule &) {}, 1, 1);
        const Out late = run([](JupiterModule &m) { m.SetParameterRaw(JupiterModule::PRE_DELAY, 2); }, 1, 1);
        double e0 = 0, e1 = 0;
        for (size_t i = 5000; i < 5000 + 4800; i++) { e0 += none.l[i] * none.l[i]; e1 += late.l[i] * late.l[i]; }
        check(e1 < e0, "PreDelay 200 ms delays the reverb (less wet energy right after the first burst)");
    }
    // Decay: a longer decay leaves more energy in the tail
    {
        const Out shortDecay = run([](JupiterModule &m) { m.SetParameterAsFloat(JupiterModule::DECAY, 0.1f); }, 1, 1);
        const Out longDecay = run([](JupiterModule &m) { m.SetParameterAsFloat(JupiterModule::DECAY, 0.95f); }, 1, 1);
        check(tailEnergy(longDecay, 30000) > 2.0 * tailEnergy(shortDecay, 30000), "a longer Decay leaves a longer tail");
    }
    // Mix 0: dry only
    {
        const Out dry = run([](JupiterModule &m) { m.SetParameterAsFloat(JupiterModule::MIX, 0.0f); }, 1, 1);
        std::vector<float> inL, inR; makeInput(inL, inR);
        // the output is the dry signal times the module's fixed output gain: find the gain, then check the residual
        double num = 0, den = 0; for (size_t i = 0; i < kTotal; i++) { num += dry.l[i] * inL[i]; den += inL[i] * inL[i]; }
        const double k = num / den;
        double d = 0; for (size_t i = 0; i < kTotal; i++) d = std::fmax(d, std::fabs(dry.l[i] - k * inL[i]));
        printf("  Mix 0: output = %.3f x input, residual %.3g\n", k, d);
        check(d < 1e-5 && k > 0.5 && k < 3.0, "Mix 0 passes the dry signal (times the output gain)");
    }
    // Stereo In off (mono in): the right input is ignored
    {
        // run() feeds different left and right signals; with Stereo In off the output must equal that of a run with right = left
        const Out off = run([](JupiterModule &) {}, 1, 1);
        srand(7);
        pool_index = 0;
        static unsigned char mem2[sizeof(JupiterModule)];
        std::memset(mem2, 0, sizeof mem2);
        JupiterModule *m = new (mem2) JupiterModule();
        m->Init(48000.0f);
        std::vector<float> inL, inR; makeInput(inL, inR);
        Out o{std::vector<float>(kTotal), std::vector<float>(kTotal)};
        for (size_t b = 0; b < kTotal; b += kBlock) m->ProcessStereoBlock(&inL[b], &inL[b], &o.l[b], &o.r[b], kBlock);
        double p; const double d = maxDiff(off, o, p);
        check(d == 0.0, "Stereo In off ignores the right input");
        const Out on = run([](JupiterModule &m2) { m2.SetParameterRaw(JupiterModule::STEREO_IN, 1); }, 1, 1);
        check(differs(on), "Stereo In on uses the right input");
    }

    printf("\n%s\n", g_fails ? "JUPITER: FAILURES" : "JUPITER OK");
    return g_fails != 0;
}
