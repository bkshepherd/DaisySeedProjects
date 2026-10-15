// Mars tests.
//  1. The GRU network gives the output of the original (RTNeural, Eigen backend) for the three models, with exact tanh/sigmoid (to float precision) and with the fast ones the module uses
//     (within the approximation's error); the approximations themselves are within their documented error.
//  2. The block path and the per-sample path give the same output (every model, cabinet and delay mode), also at 256-sample blocks.
//  3. The controls do something; the delay time is what it says, the second tap lands at 3/4 or 2/3 of the delay time after the first, the delay stops taking input below 1%.
#include "Mars/mars_dsp.h"
#include "mars_module.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace bkshepherd;

static int g_fails = 0;
static void check(bool ok, const char *what) { printf("  %-84s %s\n", what, ok ? "OK" : "FAIL"); if (!ok) g_fails++; }

static constexpr size_t kTotal = 144000;

struct Out { std::vector<float> l, r; };

static std::vector<float> noiseBurst(unsigned seed, float gain, size_t from, size_t to) {
    std::vector<float> v(kTotal, 0.0f);
    unsigned s = seed;
    for (size_t i = 0; i < kTotal; i++) {
        s = 1664525u * s + 1013904223u;
        const float x = ((s >> 8) & 0xFFFF) / 32768.0f - 1.0f;
        if (i >= from && i < to) v[i] = gain * (x + 0.5f * std::sin(0.03f * i));
    }
    return v;
}

template <class Cfg> static Out run(Cfg cfg, const std::vector<float> &in, int path) {
    static unsigned char mem[sizeof(MarsModule)];
    std::memset(mem, 0, sizeof mem);
    MarsModule *m = new (mem) MarsModule();
    m->Init(48000.0f);
    cfg(*m);
    Out o{std::vector<float>(kTotal), std::vector<float>(kTotal)};
    const size_t bs = path == 2 ? 256 : 48;
    for (size_t b = 0; b < kTotal; b += bs) {
        const size_t n = std::min(bs, kTotal - b);
        if (path == 0) {
            for (size_t i = b; i < b + n; i++) { m->ProcessMono(in[i]); o.l[i] = m->GetAudioLeft(); o.r[i] = m->GetAudioRight(); }
        } else {
            m->ProcessMonoBlock(&in[b], &o.l[b], &o.r[b], n);
        }
    }
    m->~MarsModule();
    return o;
}

static double maxDiff(const Out &a, const Out &b, double &peak) {
    double d = 0; peak = 0;
    for (size_t i = 0; i < a.l.size(); i++) {
        d = std::fmax(d, std::fmax(std::fabs(a.l[i] - b.l[i]), std::fabs(a.r[i] - b.r[i])));
        peak = std::fmax(peak, std::fmax(std::fabs(a.l[i]), std::fabs(a.r[i])));
    }
    return d;
}
static double energy(const std::vector<float> &v, size_t from, size_t to) { double e = 0; for (size_t i = from; i < to && i < v.size(); i++) e += v[i] * v[i]; return e; }

int main() {
    printf("the network against the original (RTNeural):\n");
    {
        FILE *f = fopen("tests/host/mars/gru_reference.bin", "rb");
        constexpr int kN = 4800;
        std::vector<float> in(kN), ref[3];
        bool ok = f != nullptr;
        if (ok) {
            ok &= fread(in.data(), sizeof(float), kN, f) == (size_t)kN;
            for (int m = 0; m < 3; m++) { ref[m].resize(kN); ok &= fread(ref[m].data(), sizeof(float), kN, f) == (size_t)kN; }
            fclose(f);
        }
        check(ok, "the reference data (tests/host/mars/gru_reference.bin) is there");
        if (ok) {
            for (int m = 0; m < 3; m++) {
                mars::Gru<mars::ExactMath> exact; exact.SetModel(&mars::kModels[m]);
                mars::Gru<mars::FastMath> fast; fast.SetModel(&mars::kModels[m]);
                double dExact = 0, dFast = 0, peak = 0;
                for (int i = 0; i < kN; i++) {
                    dExact = std::fmax(dExact, std::fabs(exact.Process(in[i]) - ref[m][i]));
                    dFast = std::fmax(dFast, std::fabs(fast.Process(in[i]) - ref[m][i]));
                    peak = std::fmax(peak, std::fabs(ref[m][i]));
                }
                printf("  %s: peak %.3f, max difference exact %.3g, fast %.3g\n", mars::kModels[m].name, peak, dExact, dFast);
                check(dExact < 2e-5 && peak > 0.05, "the network equals the original's output (exact maths)");
                check(dFast < 0.02 * peak, "the network with the fast maths is within 2% of the peak of the original's output");
            }
        }
        double et = 0, es = 0;
        for (float x = -8.0f; x <= 8.0f; x += 0.001f) {
            et = std::fmax(et, std::fabs(mars::FastMath::tanh(x) - std::tanh(x)));
            es = std::fmax(es, std::fabs(mars::FastMath::sigmoid(x) - 1.0f / (1.0f + std::exp(-x))));
        }
        printf("  fast tanh max error %.3g, fast sigmoid %.3g\n", et, es);
        check(et < 5e-4 && es < 3e-4, "the fast tanh and sigmoid are within 5e-4 and 3e-4");
    }

    const std::vector<float> in = noiseBurst(4242u, 0.3f, 0, 48000);
    auto defaults = [](MarsModule &) {};
    auto raw = [](int p, int v) { return [p, v](MarsModule &m) { m.SetParameterRaw(p, v); }; };

    printf("\nblock path vs per-sample path:\n");
    {
        double peak;
        for (int model = 0; model < 3; model++) {
            auto cfg = [model](MarsModule &m) { m.SetParameterRaw(MarsModule::MODEL, model); m.SetParameterRaw(MarsModule::IR, model); m.SetParameterRaw(MarsModule::DELAY_MODE, model); m.SetParameterAsFloat(MarsModule::FILTER, model == 1 ? 0.3f : model == 2 ? 0.95f : 0.8f); };
            const Out a = run(cfg, in, 0), b = run(cfg, in, 1), c = run(cfg, in, 2);
            const double d = maxDiff(a, b, peak);
            char what[100]; snprintf(what, sizeof what, "block path equals per-sample path, also at 256 (model %d, cabinet %d, delay mode %d)", model + 1, model + 1, model + 1);
            check(d < 1e-6 && maxDiff(a, c, peak) < 1e-6 && peak > 0.02 && std::isfinite(peak), what);
        }
    }

    printf("\ncontrols:\n");
    {
        const Out base = run(defaults, in, 1);
        double p;
        auto differs = [&](auto cfg) { return maxDiff(base, run(cfg, in, 1), p) > 1e-4; };
        auto set = [](int param, float v) { return [param, v](MarsModule &m) { m.SetParameterAsFloat(param, v); }; };
        check(differs(set(MarsModule::GAIN, 0.8f)), "Gain changes the sound");
        check(differs(set(MarsModule::MIX, 0.9f)), "Mix changes the sound");
        check(differs(set(MarsModule::LEVEL, 0.9f)), "Level changes the sound");
        check(differs(set(MarsModule::FILTER, 0.3f)), "the filter (low pass) changes the sound");
        check(differs(set(MarsModule::FILTER, 1.0f)), "the filter (high pass) changes the sound");
        check(differs(set(MarsModule::DELAY_TIME, 0.6f)), "Delay Time changes the sound");
        check(differs(set(MarsModule::DELAY_FEEDBACK, 0.8f)), "Delay Feedback changes the sound");
        check(differs(raw(MarsModule::MODEL, 1)), "the second model differs from the first");
        check(differs(raw(MarsModule::MODEL, 2)), "the third model differs from the first");
        check(differs(raw(MarsModule::IR, 1)), "the second cabinet differs from the first");
        check(differs(raw(MarsModule::IR, 2)), "the third cabinet differs from the first");
        check(differs(raw(MarsModule::AMP_ON, 0)), "switching the amp off changes the sound");
        check(differs(raw(MarsModule::IR_ON, 0)), "switching the cabinet off changes the sound");
        check(differs(raw(MarsModule::DELAY_MODE, 1)), "the dotted eighth mode changes the sound");
        check(differs(raw(MarsModule::DELAY_MODE, 2)), "the triplet mode changes the sound");
    }

    printf("\nthe delay:\n");
    {
        // Only the delay (no amp, no cabinet, Mix 1 = only the delayed signal), a burst at 60000, the delay set to 24000 samples (control 0.3553)
        const std::vector<float> burst = noiseBurst(99u, 0.3f, 60000, 61200);
        auto delayOnly = [](float knob, int mode, float feedback) { return [=](MarsModule &m) {
            m.SetParameterRaw(MarsModule::AMP_ON, 0); m.SetParameterRaw(MarsModule::IR_ON, 0);
            m.SetParameterAsFloat(MarsModule::MIX, 1.0f); m.SetParameterAsFloat(MarsModule::FILTER, 0.79f);
            m.SetParameterAsFloat(MarsModule::DELAY_TIME, knob); m.SetParameterAsFloat(MarsModule::DELAY_FEEDBACK, feedback);
            m.SetParameterRaw(MarsModule::DELAY_MODE, mode); }; };
        const float knob = (24000.0f - 2400.0f) / 60800.0f;
        // the delay time glides from 2400 samples to the target over a few thousand samples, so the burst is sent after that
        const Out o = run(delayOnly(knob, 0, 0.0f), burst, 1);
        auto onset = [&](const Out &x, size_t from) { double mx = 0; for (size_t i = from; i < kTotal; i++) mx = std::fmax(mx, std::fabs(x.l[i])); for (size_t i = from; i < kTotal; i++) if (std::fabs(x.l[i]) > 0.1 * mx) return (long)i; return -1L; };
        const long first = onset(o, 60100);
        printf("  the echo starts at sample %ld (expected about %d)\n", first, 60000 + 24000);
        check(std::labs(first - (60000 + 24000)) < 1500, "the delay time is what it says");
        // dotted eighth: a second echo at 3/4 of the delay time after the first echo's start... the second tap is read 0.75 x the delay further back, i.e. it comes 0.75 x the delay EARLIER? It reads
        // older audio, so it is heard later: at 1.75 x the delay time
        const Out dotted = run(delayOnly(knob, 1, 0.0f), burst, 1);
        const double eNormal = energy(o.l, 60000 + 42000 - 1500, 60000 + 42000 + 3000), eDotted = energy(dotted.l, 60000 + 42000 - 1500, 60000 + 42000 + 3000);
        printf("  energy at 1.75 x the delay: normal %.3g, dotted eighth %.3g\n", eNormal, eDotted);
        check(eNormal < 1e-6 && eDotted > 0.1, "the dotted eighth mode adds a second echo at 1.75 x the delay time");
        const Out trip = run(delayOnly(knob, 2, 0.0f), burst, 1);
        const double eTrip = energy(trip.l, 60000 + 40000 - 1500, 60000 + 40000 + 3000);
        check(eTrip > 0.1, "the triplet mode adds a second echo at 1.667 x the delay time");
        // feedback repeats the echo
        const Out fb = run(delayOnly(knob, 0, 0.7f), burst, 1);
        check(energy(fb.l, 60000 + 48000 - 1500, 60000 + 48000 + 3000) > 0.05, "feedback repeats the echo");
        check(energy(o.l, 60000 + 48000 - 1500, 60000 + 48000 + 3000) < 1e-6, "...and no feedback does not");
        // below 1% the delay takes no new input
        const Out off = run(delayOnly(0.0f, 0, 0.0f), burst, 1);
        check(energy(off.l, 60100 + 2000, kTotal) < 1e-6, "with Delay Time at 0 nothing is delayed");
    }

    printf("\nstability:\n");
    {
        auto loud = [](MarsModule &m) { m.SetParameterAsFloat(MarsModule::GAIN, 1.0f); m.SetParameterAsFloat(MarsModule::DELAY_FEEDBACK, 0.95f); m.SetParameterAsFloat(MarsModule::LEVEL, 1.0f); };
        const Out o = run(loud, noiseBurst(7u, 1.0f, 0, 96000), 1);
        double peak = 0; bool finite = true;
        for (size_t i = 0; i < kTotal; i++) { peak = std::fmax(peak, std::fabs(o.l[i])); finite &= std::isfinite(o.l[i]); }
        printf("  hot input, full gain and feedback: peak %.2f\n", peak);
        check(finite && peak < 50.0, "a hot input stays finite");
    }

    printf("\n%s\n", g_fails == 0 ? "ALL MARS TESTS PASSED" : "MARS TESTS FAILED");
    return g_fails == 0 ? 0 : 1;
}
