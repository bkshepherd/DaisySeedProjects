// Duck Compressor (side-chain compressor) tests.
//  1. The side chain: the left input is compressed by the level of the right input, which is not heard. With a quiet key the audio only gets the make-up gain, a loud key turns it
//     down (more with a higher ratio and a lower threshold), and the gain comes back with the release time. With the key silent the output is silent.
//  2. The audio stays clean while it is turned down (no distortion from the gain moving at audio rate).
//  3. The ratio never makes the audio louder than the make-up gain (the imported compressor expands when given a ratio above 1: the old module passed 1..40 straight in).
//  4. Block path = per-sample path; mono input and Sidechain off make a normal compressor; the defaults are applied at startup.
#include "duck_compressor_module.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace bkshepherd;

static int g_fails = 0;
static void check(bool ok, const char *what) { printf("  %-84s %s\n", what, ok ? "OK" : "FAIL"); if (!ok) g_fails++; }

static constexpr size_t kRate = 48000;
static constexpr size_t kTotal = 4 * kRate;
static constexpr float kPi = 3.14159265358979f;

struct Out { std::vector<float> l, r; };

static std::vector<float> sine(float freq, float amp, size_t from, size_t to) {
    std::vector<float> v(kTotal, 0.0f);
    for (size_t i = from; i < to && i < kTotal; i++) v[i] = amp * std::sin(2.0f * kPi * freq * (float)i / (float)kRate);
    return v;
}

// path: 0 per sample, 1 blocks of 48, 2 blocks of 256; stereo false = ProcessMono of the first input only
template <class Cfg> static Out run(Cfg cfg, const std::vector<float> &a, const std::vector<float> &k, int path, bool stereo = true) {
    static unsigned char mem[sizeof(DuckCompressorModule)];
    std::memset(mem, 0, sizeof mem);
    DuckCompressorModule *m = new (mem) DuckCompressorModule();
    m->Init((float)kRate);
    cfg(*m);
    Out o{std::vector<float>(kTotal), std::vector<float>(kTotal)};
    const size_t bs = path == 2 ? 256 : 48;
    for (size_t b = 0; b < kTotal; b += bs) {
        const size_t n = std::min(bs, kTotal - b);
        if (path == 0) {
            for (size_t i = b; i < b + n; i++) {
                if (stereo) m->ProcessStereo(a[i], k[i]); else m->ProcessMono(a[i]);
                o.l[i] = m->GetAudioLeft(); o.r[i] = m->GetAudioRight();
            }
        } else if (stereo) {
            m->ProcessStereoBlock(&a[b], &k[b], &o.l[b], &o.r[b], n);
        } else {
            m->ProcessMonoBlock(&a[b], &o.l[b], &o.r[b], n);
        }
    }
    m->~DuckCompressorModule();
    return o;
}

static double rms(const std::vector<float> &v, size_t from, size_t to) { double e = 0; for (size_t i = from; i < to; i++) e += (double)v[i] * v[i]; return std::sqrt(e / (double)(to - from)); }
static double db(double x) { return 20.0 * std::log10(std::fmax(x, 1e-12)); }
// the part of v that is not the sine of this frequency (a fit of its amplitude and phase), as a fraction of v's rms
static double distortion(const std::vector<float> &v, float freq, size_t from, size_t to) {
    double sc = 0, ss = 0;
    for (size_t i = from; i < to; i++) { const double ph = 2.0 * 3.14159265358979 * freq * (double)i / (double)kRate; sc += v[i] * std::cos(ph); ss += v[i] * std::sin(ph); }
    const double n = (double)(to - from);
    sc *= 2.0 / n; ss *= 2.0 / n;
    double res = 0;
    for (size_t i = from; i < to; i++) { const double ph = 2.0 * 3.14159265358979 * freq * (double)i / (double)kRate; const double d = v[i] - (sc * std::cos(ph) + ss * std::sin(ph)); res += d * d; }
    return std::sqrt(res / n) / rms(v, from, to);
}
static double maxDiff(const Out &a, const Out &b) { double d = 0; for (size_t i = 0; i < a.l.size(); i++) d = std::fmax(d, std::fmax(std::fabs(a.l[i] - b.l[i]), std::fabs(a.r[i] - b.r[i]))); return d; }

int main() {
    const float levelGain = std::pow(10.0f, 3.0f / 20.0f); // the default Level of +3 dB
    // the audio: 220 Hz at -10 dBFS throughout; the key: silent, then a loud 1 kHz tone from 1 s to 2.5 s (at -6 dBFS), then silent
    const std::vector<float> audio = sine(220.0f, 0.3f, 0, kTotal);
    const std::vector<float> key = sine(1000.0f, 0.5f, kRate, kRate * 5 / 2);
    auto defaults = [](DuckCompressorModule &) {};
    auto set = [](int p, float v) { return [p, v](DuckCompressorModule &m) { m.SetParameterAsFloat(p, v); }; };

    printf("the side chain:\n");
    const Out base = run(defaults, audio, key, 1);
    const double quiet = db(rms(base.l, kRate / 4, kRate * 3 / 4) / rms(audio, kRate / 4, kRate * 3 / 4));
    const double ducked = db(rms(base.l, kRate * 3 / 2, kRate * 2) / rms(audio, kRate * 3 / 2, kRate * 2));
    const double after = db(rms(base.l, kRate * 7 / 2, kRate * 4) / rms(audio, kRate * 7 / 2, kRate * 4));
    printf("  gain with a quiet key %+.2f dB, with the loud key %+.2f dB, 1 s after it %+.2f dB (Level +3 dB)\n", quiet, ducked, after);
    check(std::fabs(quiet - 3.0) < 0.05, "with a quiet key the audio only gets the make-up gain");
    check(ducked < quiet - 12.0, "a loud key turns the audio down by more than 12 dB");
    check(std::fabs(after - 3.0) < 0.2, "the gain comes back after the key stops (release)");
    check(maxDiff(base, base) == 0.0 && std::fabs(base.l[1000] - base.r[1000]) < 1e-9, "both outputs carry the same (audio) signal");
    {
        // the key is not in the output: with silent audio and a loud key the output is silent
        const std::vector<float> silent(kTotal, 0.0f);
        const Out o = run(defaults, silent, key, 1);
        check(rms(o.l, 0, kTotal) == 0.0 && rms(o.r, 0, kTotal) == 0.0, "the key is not audible: silent audio stays silent whatever the key does");
    }
    {
        // the ratio and the threshold set how much: more ratio, lower threshold, more reduction
        double prev = 1e9;
        bool mono = true;
        for (float ratio : {1.0f, 2.0f, 4.0f, 8.0f, 20.0f, 40.0f}) {
            const Out o = run(set(DuckCompressorModule::RATIO, ratio), audio, key, 1);
            const double g = db(rms(o.l, kRate * 3 / 2, kRate * 2) / rms(audio, kRate * 3 / 2, kRate * 2));
            printf("  ratio %4.0f:1 -> %+.2f dB\n", ratio, g);
            mono &= g <= prev + 1e-6;
            prev = g;
        }
        check(mono, "a higher ratio turns the audio down more");
        const Out hi = run(set(DuckCompressorModule::THRESHOLD, -10.0f), audio, key, 1), lo = run(set(DuckCompressorModule::THRESHOLD, -50.0f), audio, key, 1);
        check(rms(lo.l, kRate * 3 / 2, kRate * 2) < rms(hi.l, kRate * 3 / 2, kRate * 2), "a lower threshold turns it down more");
        const Out above = run(set(DuckCompressorModule::THRESHOLD, 0.0f), audio, key, 1);
        check(std::fabs(db(rms(above.l, kRate * 3 / 2, kRate * 2) / rms(audio, kRate * 3 / 2, kRate * 2)) - 3.0) < 0.05, "a key below the threshold changes nothing");
    }
    {
        // attack and release times: the reduction arrives faster with a short attack and leaves slower with a long release
        auto reductionAt = [&](const Out &o, size_t from, size_t to) { return db(rms(o.l, from, to) / rms(audio, from, to)); };
        const Out fast = run(set(DuckCompressorModule::ATTACK, 1.0f), audio, key, 1), slow = run(set(DuckCompressorModule::ATTACK, 100.0f), audio, key, 1);
        const size_t t = kRate + kRate / 100; // 10 ms after the key starts
        check(reductionAt(fast, t, t + 240) < reductionAt(slow, t, t + 240) - 3.0, "a short attack turns the audio down sooner than a long one");
        const Out quick = run(set(DuckCompressorModule::RELEASE, 20.0f), audio, key, 1), lazy = run(set(DuckCompressorModule::RELEASE, 1000.0f), audio, key, 1);
        const size_t e = kRate * 5 / 2 + kRate / 5; // 0.2 s after the key stops
        check(reductionAt(quick, e, e + 480) > reductionAt(lazy, e, e + 480) + 6.0, "a short release gets the audio back sooner than a long one");
    }

    printf("\nclean audio while turned down:\n");
    {
        const double d0 = distortion(base.l, 220.0f, kRate / 4, kRate * 3 / 4), d1 = distortion(base.l, 220.0f, kRate * 3 / 2, kRate * 2);
        printf("  distortion (residual of a sine fit): quiet key %.4f, loud key %.4f\n", d0, d1);
        check(d0 < 1e-4 && d1 < 0.02, "the audio is a clean sine while it is turned down (under 2% residual)");
        // the worst case for the gain following the waveform: a short release with a low key
        const std::vector<float> lowKey = sine(60.0f, 0.5f, kRate, kRate * 5 / 2);
        const Out o = run([](DuckCompressorModule &m) { m.SetParameterAsFloat(DuckCompressorModule::RELEASE, 10.0f); m.SetParameterAsFloat(DuckCompressorModule::ATTACK, 1.0f); }, audio, lowKey, 1);
        printf("  60 Hz key with the shortest times: residual %.3f (the gain follows the key's waveform: expected)\n", distortion(o.l, 220.0f, kRate * 3 / 2, kRate * 2));
    }

    printf("\nthe ratio never boosts:\n");
    {
        bool ok = true;
        double worst = -1e9;
        for (float ratio : {1.0f, 1.5f, 2.0f, 10.0f, 40.0f}) {
            const Out o = run(set(DuckCompressorModule::RATIO, ratio), audio, key, 1);
            double peak = 0;
            for (size_t i = 0; i < kTotal; i++) peak = std::fmax(peak, std::fabs(o.l[i]));
            worst = std::fmax(worst, db(peak / (0.3 * levelGain)));
            ok &= peak <= 0.3 * levelGain * 1.001;
        }
        printf("  highest output peak relative to the make-up gain at the ratios 1 to 40: %+.3f dB\n", worst);
        check(ok, "the output never exceeds input x make-up gain, whatever the ratio");
    }

    printf("\nblock path, mono, startup:\n");
    {
        const Out a = run(defaults, audio, key, 0), b = run(defaults, audio, key, 1), c = run(defaults, audio, key, 2);
        check(maxDiff(a, b) == 0.0 && maxDiff(a, c) == 0.0, "block path equals per-sample path (side chain), also at 256");
        // mono input: the audio is its own key: a loud tone is compressed, a quiet one is not
        const std::vector<float> loud = sine(220.0f, 0.5f, 0, kTotal), soft = sine(220.0f, 0.005f, 0, kTotal), none(kTotal, 0.0f);
        const Out ml = run(defaults, loud, none, 1, false), ms = run(defaults, soft, none, 1, false), m0 = run(defaults, loud, none, 0, false);
        const double gl = db(rms(ml.l, kRate, kTotal) / rms(loud, kRate, kTotal)), gs = db(rms(ms.l, kRate, kTotal) / rms(soft, kRate, kTotal));
        printf("  mono: loud input %+.2f dB, quiet input %+.2f dB\n", gl, gs);
        check(gl < 0.0 && std::fabs(gs - 3.0) < 0.05, "a mono input is its own key: the loud one is compressed, the quiet one is not");
        check(maxDiff(ml, m0) == 0.0, "mono: block path equals per-sample path");
        // Sidechain off on a stereo input: the right input is ignored
        const Out off = run([](DuckCompressorModule &m) { m.SetParameterRaw(DuckCompressorModule::SIDECHAIN, 0); }, loud, key, 1);
        check(maxDiff(off, ml) == 0.0, "with Sidechain off the right input is ignored (the audio is its own key)");
        // the defaults are applied at startup: touching every parameter away and back changes nothing
        const Out touched = run([](DuckCompressorModule &m) {
            for (int p : {DuckCompressorModule::LEVEL, DuckCompressorModule::RATIO, DuckCompressorModule::THRESHOLD, DuckCompressorModule::ATTACK, DuckCompressorModule::RELEASE}) {
                const float def = m.GetParameterDefaultValueAsFloat(p);
                m.SetParameterAsMagnitude(p, 0.0f);
                m.SetParameterAsFloat(p, def);
            } }, audio, key, 1);
        check(maxDiff(base, touched) < 1e-6, "the defaults are applied at startup");
    }

    printf("\n%s\n", g_fails == 0 ? "ALL DUCK COMPRESSOR TESTS PASSED" : "DUCK COMPRESSOR TESTS FAILED");
    return g_fails == 0 ? 0 : 1;
}
