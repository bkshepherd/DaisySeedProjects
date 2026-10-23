// Tests of the Booster (clean, treble, mid and warm boost): what each type does to the frequency response (measured with a small signal, so
// that nothing clips), that Gain is the boost where the type works, what Tone and Level do, and the saturation of each type (the ceiling, the
// asymmetry of Treble and Warm, the clean types staying clean). The block path, bounds and silence are checked with the distortion family
// (test_distortion_family.cpp).
#include "booster_module.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
using namespace bkshepherd;
using B = BoosterModule;

static int fails = 0;
static void check(bool ok, const char *what) {
    fails += !ok;
    std::printf("  %-110s %s\n", what, ok ? "OK" : "FAILED");
}

static constexpr float kSr = 48000.0f;
static constexpr size_t N = 48000;

// A booster of the given type, Gain and Tone (0..1) set before Init
static std::unique_ptr<B> make(int type, float gain = 0.5f, float tone = 0.5f, float level = 0.8f) {
    auto m = std::make_unique<B>();
    m->SetParameterRaw(B::TYPE, type);
    m->SetParameterAsFloat(B::GAIN, gain);
    m->SetParameterAsFloat(B::TONE, tone);
    m->SetParameterAsFloat(B::LEVEL, level);
    m->Init(kSr);
    return m;
}

static std::vector<float> run(B &m, const std::vector<float> &in, size_t block = 48) {
    std::vector<float> l(in.size()), r(in.size());
    for (size_t o = 0; o < in.size(); o += block) m.ProcessMonoBlock(&in[o], &l[o], &r[o], std::min(block, in.size() - o));
    return l;
}

static std::vector<float> sine(float f, float peak) {
    std::vector<float> v(N);
    for (size_t i = 0; i < N; i++) v[i] = peak * std::sin(2.0f * 3.14159265f * f * i / kSr);
    return v;
}

// The amplitude at frequency f over the second half of the signal (a single-bin DFT; f is a multiple of 2 Hz, so whole periods)
static double amp(const std::vector<float> &x, float f) {
    const size_t from = x.size() / 2, n = x.size() - from;
    double re = 0, im = 0;
    for (size_t i = 0; i < n; i++) {
        const double w = 2.0 * 3.14159265358979 * f * (from + i) / kSr;
        re += x[from + i] * std::cos(w);
        im += x[from + i] * std::sin(w);
    }
    return 2.0 * std::sqrt(re * re + im * im) / n;
}

// The gain in dB of a small signal (-60 dBFS: far below any clipping) at frequency f
static double gainDb(int type, float gain, float tone, float f, float level = 0.8f) {
    auto m = make(type, gain, tone, level);
    const std::vector<float> x = sine(f, 0.001f);
    return 20.0 * std::log10(amp(run(*m, x), f) / amp(x, f));
}

int main() {
    // The frequency where each type's boost is measured: everything (Clean), the highs (Treble), the centre of the bell (Mid), the mids (Warm)
    const float ref[B::TYPE_COUNT] = {1000.0f, 3000.0f, 1132.0f, 1000.0f};
    const char *names[B::TYPE_COUNT] = {"Clean", "Treble", "Mid", "Warm"};

    std::printf("Gain is the boost where the type works:\n");
    for (int t = 0; t < B::TYPE_COUNT; t++) {
        bool ok = true;
        double worst = 0;
        for (float g : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
            const double err = gainDb(t, g, 0.5f, ref[t]) - 24.0 * g;
            worst = std::fmax(worst, std::fabs(err));
            ok = ok && std::fabs(err) < 0.6;
        }
        char what[128];
        std::snprintf(what, sizeof what, "%-6s: the gain at %.0f Hz is the Gain control from 0 to 24 dB (largest error %.2f dB)", names[t], ref[t], worst);
        check(ok, what);
    }

    std::printf("\nClean: flat, with a tilt for Tone:\n");
    {
        const double lo = gainDb(B::CLEAN, 0.5f, 0.5f, 100.0f), mid = gainDb(B::CLEAN, 0.5f, 0.5f, 1000.0f), hi = gainDb(B::CLEAN, 0.5f, 0.5f, 8000.0f);
        std::printf("    gain at 100 Hz, 1 kHz, 8 kHz: %.2f, %.2f, %.2f dB\n", lo, mid, hi);
        check(std::fabs(lo - mid) < 0.5 && std::fabs(hi - mid) < 0.5, "at Tone 50% the response is flat (100 Hz and 8 kHz within 0.5 dB of 1 kHz)");
        const double brightHi = gainDb(B::CLEAN, 0.5f, 1.0f, 8000.0f) - gainDb(B::CLEAN, 0.5f, 1.0f, 1000.0f);
        const double brightLo = gainDb(B::CLEAN, 0.5f, 1.0f, 60.0f) - gainDb(B::CLEAN, 0.5f, 1.0f, 1000.0f);
        const double darkHi = gainDb(B::CLEAN, 0.5f, 0.0f, 8000.0f) - gainDb(B::CLEAN, 0.5f, 0.0f, 1000.0f);
        const double darkLo = gainDb(B::CLEAN, 0.5f, 0.0f, 60.0f) - gainDb(B::CLEAN, 0.5f, 0.0f, 1000.0f);
        std::printf("    Tone 100%%: highs %+.1f dB, lows %+.1f dB; Tone 0%%: highs %+.1f dB, lows %+.1f dB (against 1 kHz)\n", brightHi, brightLo, darkHi, darkLo);
        check(brightHi > 3.0 && brightLo < -3.0, "Tone up tilts it bright: the highs up and the lows down by more than 3 dB");
        check(darkHi < -3.0 && darkLo > 3.0, "Tone down tilts it dark: the highs down and the lows up by more than 3 dB");
    }

    std::printf("\nTreble: the bass is cut, the highs are boosted:\n");
    {
        const double pass = gainDb(B::TREBLE, 0.5f, 0.5f, 3000.0f);
        const double at100 = gainDb(B::TREBLE, 0.5f, 0.5f, 100.0f) - pass, at800 = gainDb(B::TREBLE, 0.5f, 0.5f, 800.0f) - pass;
        const double at8k = gainDb(B::TREBLE, 0.5f, 0.5f, 8000.0f) - pass;
        std::printf("    against 3 kHz: 100 Hz %+.1f dB, 800 Hz (the corner at Tone 50%%) %+.1f dB, 8 kHz %+.1f dB\n", at100, at800, at8k);
        check(at100 < -25.0, "the bass is cut hard: 100 Hz is more than 25 dB below the highs");
        check(at800 > -4.0 && at800 < -2.0, "the corner is at 800 Hz at Tone 50% (3 dB down)");
        check(at8k > -4.0, "the top stays open: 8 kHz is within 4 dB of the highs");
        const double high800 = gainDb(B::TREBLE, 0.5f, 1.0f, 800.0f) - gainDb(B::TREBLE, 0.5f, 1.0f, 8000.0f);
        const double low800 = gainDb(B::TREBLE, 0.5f, 0.0f, 800.0f) - gainDb(B::TREBLE, 0.5f, 0.0f, 3000.0f);
        std::printf("    800 Hz against the highs: Tone 100%% %+.1f dB, Tone 0%% %+.1f dB\n", high800, low800);
        check(high800 < -15.0 && low800 > -1.0, "Tone moves the corner: up cuts more of the lows (3.2 kHz), down lets the lows through (200 Hz)");
    }

    std::printf("\nMid: a bell at the centre frequency:\n");
    {
        const double peak = gainDb(B::MID, 0.5f, 0.5f, 1132.0f), low = gainDb(B::MID, 0.5f, 0.5f, 100.0f), high = gainDb(B::MID, 0.5f, 0.5f, 8000.0f);
        std::printf("    gain at 100 Hz, 1.1 kHz, 8 kHz: %.2f, %.2f, %.2f dB\n", low, peak, high);
        check(peak - low > 10.0 && peak - high > 10.0, "the boost is a bell: the centre is more than 10 dB above 100 Hz and 8 kHz");
        check(low < 1.5 && high < 1.5, "the rest is left alone (100 Hz and 8 kHz within 1.5 dB of unity at 12 dB Gain)");
        const double c400 = gainDb(B::MID, 0.5f, 0.0f, 400.0f), c1132 = gainDb(B::MID, 0.5f, 0.0f, 1132.0f), c3200 = gainDb(B::MID, 0.5f, 1.0f, 3200.0f);
        std::printf("    Tone 0%%: 400 Hz %.1f dB, 1.1 kHz %.1f dB; Tone 100%%: 3.2 kHz %.1f dB\n", c400, c1132, c3200);
        check(std::fabs(c400 - 12.0) < 0.6 && c400 - c1132 > 6.0, "Tone 0% moves the bell down to 400 Hz");
        check(std::fabs(c3200 - 12.0) < 0.6, "Tone 100% moves the bell up to 3.2 kHz");
        check(std::fabs(gainDb(B::MID, 0.0f, 0.5f, 1132.0f)) < 0.1, "with Gain at 0 dB it is not there");
    }

    std::printf("\nWarm: a bass lift and a high cut:\n");
    {
        const double mid = gainDb(B::WARM, 0.5f, 0.5f, 1000.0f), lows = gainDb(B::WARM, 0.5f, 0.5f, 60.0f) - mid, top = gainDb(B::WARM, 0.5f, 0.5f, 12000.0f) - mid;
        std::printf("    against 1 kHz: 60 Hz %+.1f dB, 12 kHz %+.1f dB\n", lows, top);
        check(lows > 2.0 && lows < 4.0, "the lows are lifted by about 3 dB");
        check(top < -6.0, "the highs are rolled off (12 kHz more than 6 dB down at Tone 50%)");
        const double open = gainDb(B::WARM, 0.5f, 1.0f, 8000.0f), dark = gainDb(B::WARM, 0.5f, 0.0f, 8000.0f);
        std::printf("    8 kHz: Tone 100%% %.1f dB, Tone 0%% %.1f dB\n", open, dark);
        check(open - dark > 15.0, "Tone opens and closes the highs (8 kHz differs by more than 15 dB between 100% and 0%)");
    }

    std::printf("\nLevel:\n");
    {
        const double d0 = gainDb(B::CLEAN, 0.0f, 0.5f, 1000.0f), dMin = gainDb(B::CLEAN, 0.0f, 0.5f, 1000.0f, 0.0f), dMax = gainDb(B::CLEAN, 0.0f, 0.5f, 1000.0f, 1.0f);
        std::printf("    Gain 0 dB: Level 80%% %.2f dB, Level 0%% %.2f dB, Level 100%% %.2f dB\n", d0, dMin, dMax);
        check(std::fabs(d0) < 0.1 && std::fabs(dMin + 24.0) < 0.1 && std::fabs(dMax - 6.0) < 0.1, "Level goes from -24 to +6 dB and is 0 dB at its default");
    }

    std::printf("\nSaturation:\n");
    {
        // Hard driven (24 dB): the output stays at the ceiling of its type: the largest excursion of the clip (Treble: 0.73, with the asymmetry of
        // its bias; Warm: 0.81), plus up to 12% for the overshoot of the filter after the clip and the DC blocker's start-up
        const double ceiling[B::TYPE_COUNT] = {1.0, 0.729, 0.8, 0.81};
        for (int t = 0; t < B::TYPE_COUNT; t++) {
            const std::vector<float> y = run(*make(t, 1.0f), sine(1000.0f, 0.3f));
            double peak = 0;
            for (float v : y) peak = std::fmax(peak, std::fabs(v));
            char what[128];
            std::snprintf(what, sizeof what, "%-6s hard driven (24 dB, a -10 dBFS note): the output peak %.2f stays within 12%% of its ceiling %.2f", names[t], peak, ceiling[t]);
            check(peak <= ceiling[t] * 1.12 && peak > ceiling[t] * 0.9, what);
        }
        // The same note a little below the clip: a symmetric clip makes no even harmonics, the germanium and tube clips (Treble, Warm) do
        double second[B::TYPE_COUNT];
        for (int t = 0; t < B::TYPE_COUNT; t++) {
            const std::vector<float> y = run(*make(t, 0.5f), sine(1000.0f, 0.15f));
            second[t] = amp(y, 2000.0f) / amp(y, 1000.0f);
        }
        std::printf("    2nd harmonic against the fundamental: Clean %.1e, Treble %.1e, Mid %.1e, Warm %.1e\n", second[0], second[1], second[2], second[3]);
        check(second[B::CLEAN] < 1e-4 && second[B::MID] < 1e-4, "Clean and Mid clip symmetrically: no 2nd harmonic");
        check(second[B::TREBLE] > 0.02 && second[B::WARM] > 0.02, "Treble and Warm clip asymmetrically: more than 2% 2nd harmonic when pushed (germanium, tube)");
        // A normal level through the clean boost: no audible distortion
        const std::vector<float> y = run(*make(B::CLEAN, 0.5f), sine(1000.0f, 0.03f));
        const double thd3 = amp(y, 3000.0f) / amp(y, 1000.0f);
        std::printf("    Clean at 12 dB with a -30 dBFS note: 3rd harmonic %.1e\n", thd3);
        check(thd3 < 0.002, "the clean boost stays clean at normal levels (3rd harmonic below 0.2%)");
    }

    std::printf("\n%s\n", fails ? "BOOSTER TESTS FAILED" : "ALL BOOSTER TESTS PASSED");
    return fails ? 1 : 0;
}
