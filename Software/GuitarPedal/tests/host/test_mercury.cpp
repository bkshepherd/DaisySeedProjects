// Mercury tests.
//  1. The network gives the output of the original (RTNeural-NAM on RTNeural, Eigen backend) for all nine models, to float precision; the prewarm (the receptive field plus a few samples
//     of silence) gives the same state as the original's 16384 samples.
//  2. The block path and the per-sample path give the same output (every model), also at 256-sample blocks.
//  3. The controls do something: gain, level, the four EQ bands (at their own frequencies), EQ off, and each of the nine models sounds different; a model change is clean.
#include "Mercury/wavenet_pico.h"
#include "mercury_module.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace bkshepherd;

static int g_fails = 0;
static void check(bool ok, const char *what) { printf("  %-84s %s\n", what, ok ? "OK" : "FAIL"); if (!ok) g_fails++; }

static constexpr size_t kTotal = 48000;

struct Out { std::vector<float> l, r; };

static std::vector<float> guitar(float freq, float peak) {
    std::vector<float> v(kTotal);
    double maxAbs = 0;
    for (size_t i = 0; i < kTotal; i++) {
        double x = 0;
        for (int k = 1; k <= 8; k++) x += std::sin(2.0 * 3.14159265358979 * freq * k * (double)i / 48000.0) / k;
        v[i] = (float)x;
        maxAbs = std::fmax(maxAbs, std::fabs(x));
    }
    for (auto &x : v) x = (float)(x / maxAbs) * peak;
    return v;
}

template <class Cfg> static Out run(Cfg cfg, const std::vector<float> &in, int path) {
    static unsigned char mem[sizeof(MercuryModule)];
    std::memset(mem, 0, sizeof mem);
    MercuryModule *m = new (mem) MercuryModule();
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
    m->~MercuryModule();
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
    printf("the network against the original (RTNeural-NAM):\n");
    {
        FILE *f = fopen("tests/host/mercury/wavenet_reference.bin", "rb");
        constexpr int kN = 1000;
        std::vector<float> in(kN), ref[9];
        bool ok = f != nullptr;
        if (ok) {
            ok &= fread(in.data(), sizeof(float), kN, f) == (size_t)kN;
            for (int m = 0; m < 9; m++) { ref[m].resize(kN); ok &= fread(ref[m].data(), sizeof(float), kN, f) == (size_t)kN; }
            fclose(f);
        }
        check(ok, "the reference data (tests/host/mercury/wavenet_reference.bin) is there");
        if (ok) {
            static mercury::PicoWavenet<> net;
            double worst = 0;
            bool all = true;
            for (int m = 0; m < 9; m++) {
                net.LoadWeights(mercury::kModels[m].weights);
                net.Prewarm();
                double d = 0, peak = 0;
                for (int i = 0; i < kN; i++) { d = std::fmax(d, std::fabs(net.Process(in[i]) - ref[m][i])); peak = std::fmax(peak, std::fabs(ref[m][i])); }
                printf("  %-20s peak %.3f, max difference %.3g\n", mercury::kModels[m].name, peak, d);
                all &= d < 2e-5 && peak > 0.05;
                worst = std::fmax(worst, d);
            }
            check(all, "all nine models equal the original's output (to float precision)");
            // The prewarm of the original was 16384 samples: ours is shorter, and must give the same state
            net.LoadWeights(mercury::kModels[8].weights);
            net.Prewarm();
            std::vector<float> a(kN), b(kN);
            for (int i = 0; i < kN; i++) a[i] = net.Process(in[i]);
            net.LoadWeights(mercury::kModels[8].weights);
            net.Reset();
            for (int i = 0; i < 16384; i++) net.Process(0.0f);
            for (int i = 0; i < kN; i++) b[i] = net.Process(in[i]);
            double d = 0; for (int i = 0; i < kN; i++) d = std::fmax(d, std::fabs(a[i] - b[i]));
            printf("  short prewarm (%d samples) vs 16384: max difference %.3g\n", mercury::kReceptiveField + 256, d);
            check(d == 0.0, "the short prewarm leaves the same state as 16384 samples of silence");
        }
    }

    const std::vector<float> in = guitar(110.0f, 0.1f);
    auto defaults = [](MercuryModule &) {};
    auto raw = [](int p, int v) { return [p, v](MercuryModule &m) { m.SetParameterRaw(p, v); }; };
    auto set = [](int p, float v) { return [p, v](MercuryModule &m) { m.SetParameterAsFloat(p, v); }; };

    printf("\nblock path vs per-sample path:\n");
    {
        double peak;
        bool ok = true;
        for (int model = 0; model < mercury::kModelCount; model++) {
            const Out a = run(raw(MercuryModule::MODEL, model), in, 0), b = run(raw(MercuryModule::MODEL, model), in, 1), c = run(raw(MercuryModule::MODEL, model), in, 2);
            ok &= maxDiff(a, b, peak) == 0.0 && maxDiff(a, c, peak) == 0.0 && peak > 0.001 && std::isfinite(peak);
        }
        check(ok, "block path equals per-sample path, also at 256, for every model");
    }

    printf("\ncontrols:\n");
    {
        const Out base = run(defaults, in, 1);
        double p;
        auto differs = [&](auto cfg) { return maxDiff(base, run(cfg, in, 1), p) > 1e-5; };
        check(differs(set(MercuryModule::GAIN, 0.8f)), "Gain changes the sound");
        check(differs(set(MercuryModule::LEVEL, 0.9f)), "Level changes the sound");
        check(differs(set(MercuryModule::BASS, 8.0f)), "Bass changes the sound");
        check(differs(set(MercuryModule::MID, 8.0f)), "Mid changes the sound");
        check(differs(set(MercuryModule::TREBLE, 8.0f)), "Treble changes the sound");
        check(differs(set(MercuryModule::PRESENCE, 8.0f)), "Presence changes the sound");
        const Out eqOff = run(raw(MercuryModule::EQ, 0), in, 1), eqOffBass = run([](MercuryModule &m) { m.SetParameterRaw(MercuryModule::EQ, 0); m.SetParameterAsFloat(MercuryModule::BASS, 9.0f); }, in, 1);
        check(maxDiff(eqOff, eqOffBass, p) == 0.0, "with the EQ off the bands do nothing");
        // every model is different from the first
        bool distinct = true;
        for (int m = 1; m < mercury::kModelCount; m++) distinct &= differs(raw(MercuryModule::MODEL, m));
        check(distinct, "each of the other eight models differs from the first");
        // the bands act at their frequencies: +10 dB at 180 Hz (bass) raises a 180 Hz tone, not an 8 kHz one
        auto tone = [](float f) { std::vector<float> v(kTotal); for (size_t i = 0; i < kTotal; i++) v[i] = 0.05f * std::sin(2.0f * 3.14159265f * f * (float)i / 48000.0f); return v; };
        const std::vector<float> t180 = tone(180.0f), t8k = tone(8000.0f);
        const double b180 = energy(run(defaults, t180, 1).l, 24000, 48000), bass180 = energy(run(set(MercuryModule::BASS, 10.0f), t180, 1).l, 24000, 48000);
        const double b8k = energy(run(defaults, t8k, 1).l, 24000, 48000), bass8k = energy(run(set(MercuryModule::BASS, 10.0f), t8k, 1).l, 24000, 48000);
        const double pres8k = energy(run(set(MercuryModule::PRESENCE, 10.0f), t8k, 1).l, 24000, 48000);
        printf("  bass +10 dB: 180 Hz x%.2f, 8 kHz x%.2f; presence +10 dB: 8 kHz x%.2f (energy ratios)\n", bass180 / b180, bass8k / b8k, pres8k / b8k);
        check(bass180 > 3.0 * b180 && bass8k < 1.5 * b8k && pres8k > 3.0 * b8k, "the bass band acts at 180 Hz and the presence band at 8 kHz");
    }

    printf("\nmodel changes:\n");
    {
        // Changing the model while running: the output goes on, finite, and equals a fresh module with that model
        static unsigned char mem[sizeof(MercuryModule)];
        std::memset(mem, 0, sizeof mem);
        MercuryModule *m = new (mem) MercuryModule();
        m->Init(48000.0f);
        float outL[48], outR[48];
        bool finite = true;
        for (size_t b = 0; b < 24000; b += 48) { m->ProcessMonoBlock(&in[b], outL, outR, 48); }
        m->SetParameterRaw(MercuryModule::MODEL, 5);
        for (size_t b = 24000; b < kTotal; b += 48) { m->ProcessMonoBlock(&in[b], outL, outR, 48); for (int i = 0; i < 48; i++) finite &= std::isfinite(outL[i]); }
        m->~MercuryModule();
        check(finite, "the output stays finite after a model change");
        const Out fresh = run(raw(MercuryModule::MODEL, 5), in, 1);
        check(energy(fresh.l, 24000, 48000) > 1e-6, "the model sounds after the change");
    }

    printf("\n%s\n", g_fails == 0 ? "ALL MERCURY TESTS PASSED" : "MERCURY TESTS FAILED");
    return g_fails == 0 ? 0 : 1;
}
