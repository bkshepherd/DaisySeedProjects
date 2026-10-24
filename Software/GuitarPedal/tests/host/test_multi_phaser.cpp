// Tests of the Multi Phaser (up to four phasers in series or in parallel, each with its own Rate, Depth, Feedback, Stages, Center, Wave and Phase):
// the frequency response of a phaser against the allpass formula, the notches (one for every two stages), the sweep (its rate), the routing,
// feedback, the dry signal, Stereo, switching a phaser or the routing while playing (no click), the block path and the bounds.
#include "multi_phaser_module.h"
#include <algorithm>
#include <complex>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
using namespace bkshepherd;
using M = MultiPhaserModule;
using cd = std::complex<double>;

static int fails = 0;
static void check(bool ok, const char *what) {
    fails += !ok;
    std::printf("  %-120s %s\n", what, ok ? "OK" : "FAILED");
}

static constexpr float kSr = 48000.0f;
static constexpr double kPi = 3.14159265358979;

// The stored values (0..1) for a frequency, a rate and a feedback
static float centerV(double hz) { return float(std::log(hz / 100.0) / std::log(40.0)); }
static float rateV(double hz) { return float(std::log(hz / 0.05) / std::log(200.0)); }
static float feedbackV(double fb) { return float((fb / 0.85 + 1.0) / 2.0); }

struct Unit {
    bool on = true;
    int stages = 4;      // 2..12
    double center = 800; // Hz
    double depth = 0.0;  // 0..1
    double rate = 0.5;   // Hz
    double feedback = 0; // -0.85..0.85
    int wave = M::SINE;
    float phase = 0.0f;
};

static std::unique_ptr<M> make(std::vector<Unit> units, int routing = M::SERIES, int output = M::MONO_OUT, float mix = 1.0f) {
    auto m = std::make_unique<M>();
    m->SetParameterAsFloat(M::MIX, mix);
    m->SetParameterRaw(M::ROUTING, routing);
    m->SetParameterRaw(M::OUTPUT, output);
    for (int u = 0; u < M::kUnits; u++) {
        const bool used = u < int(units.size());
        const Unit d = used ? units[u] : Unit();
        m->SetParameterRaw(M::UnitParamId(u, M::U_ON), used && d.on ? 1 : 0);
        m->SetParameterAsFloat(M::UnitParamId(u, M::U_RATE), rateV(d.rate));
        m->SetParameterAsFloat(M::UnitParamId(u, M::U_DEPTH), float(d.depth));
        m->SetParameterAsFloat(M::UnitParamId(u, M::U_FEEDBACK), feedbackV(d.feedback));
        m->SetParameterRaw(M::UnitParamId(u, M::U_STAGES), d.stages / 2 - 1);
        m->SetParameterAsFloat(M::UnitParamId(u, M::U_CENTER), centerV(d.center));
        m->SetParameterRaw(M::UnitParamId(u, M::U_WAVE), d.wave);
        m->SetParameterAsFloat(M::UnitParamId(u, M::U_PHASE), d.phase);
    }
    m->Init(kSr);
    return m;
}

static std::vector<float> run(M &m, const std::vector<float> &in, size_t block = 48) {
    std::vector<float> l(in.size()), r(in.size());
    for (size_t o = 0; o < in.size(); o += block) m.ProcessMonoBlock(&in[o], &l[o], &r[o], std::min(block, in.size() - o));
    return l;
}

static std::vector<float> sine(float f, float peak, size_t n) {
    std::vector<float> v(n);
    for (size_t i = 0; i < n; i++) v[i] = peak * float(std::sin(2.0 * kPi * f * i / kSr));
    return v;
}

// The complex gain at f (output over input) over the second half of the signal
static cd gainAt(M &m, float f, size_t n = 9600) {
    const std::vector<float> x = sine(f, 0.1f, n), y = run(m, x);
    const size_t from = n / 2;
    cd sx = 0, sy = 0;
    for (size_t i = from; i < n; i++) {
        const cd w = std::polar(1.0, -2.0 * kPi * f * i / kSr);
        sx += double(x[i]) * w;
        sy += double(y[i]) * w;
    }
    return sy / sx / double(M::kMakeup); // without the make-up gain of the phaser
}

// What one phaser without feedback does at f: half the sum of the input and the allpass chain (a at the corner frequency fc, `stages` stages)
static cd formula(double fc, int stages, double f) {
    const double t = std::tan(kPi * fc / kSr), a = (t - 1.0) / (t + 1.0);
    const cd z1 = std::polar(1.0, -2.0 * kPi * f / kSr);
    const cd stage = (a + z1) / (1.0 + a * z1);
    return 0.5 * (1.0 + std::pow(stage, stages));
}

int main() {
    std::printf("A phaser against the allpass formula (Depth 0%%: the corner frequency stays at the Center):\n");
    {
        double worst = 0;
        const double freqs[] = {150, 300, 500, 800, 1200, 2000, 3500, 6000};
        for (int stages : {2, 4, 8, 12}) {
            for (double center : {300.0, 800.0, 2000.0}) {
                Unit u;
                u.stages = stages;
                u.center = center;
                // the stored centre is on a 0..1 scale: the value actually used is what the parameter maps to
                auto m = make({u});
                for (double f : freqs) {
                    const cd g = gainAt(*m, float(f));
                    worst = std::fmax(worst, std::abs(std::abs(g) - std::abs(formula(center, stages, f))));
                }
            }
        }
        std::printf("    largest difference of the gain from the formula: %.4f\n", worst);
        check(worst < 0.01, "the gain at 8 frequencies, for 2, 4, 8 and 12 stages and three centres, is the formula 0.5 (1 + A^N) (within 0.01)");
    }

    std::printf("\nThe notches (one for every two stages), and the sweep:\n");
    {
        for (int stages : {2, 4, 8, 12}) {
            Unit u;
            u.stages = stages;
            u.center = 1000;
            auto m = make({u});
            int notches = 0;
            double prev2 = 1, prev1 = 1;
            for (double f = 60; f < 16000; f *= 1.03) {
                const double g = std::abs(gainAt(*m, float(f), 4800));
                if (prev1 < prev2 && prev1 < g && prev1 < 0.15) notches++;
                prev2 = prev1;
                prev1 = g;
            }
            char what[100];
            std::snprintf(what, sizeof what, "%2d stages: %d notches below 16 kHz (the gain below 15%%)", stages, notches);
            check(notches == stages / 2, what);
        }
        // The notch of two stages is at the corner frequency, which sweeps through the centre twice per cycle: a sine at the centre goes through a notch twice per cycle
        for (int wave : {int(M::SINE), int(M::TRIANGLE)}) {
            Unit u;
            u.stages = 2;
            u.center = 800;
            u.depth = 0.3;
            u.rate = 1.0;
            u.wave = wave;
            auto m = make({u});
            const std::vector<float> x = sine(800.0f, 0.1f, 48000 * 6), y = run(*m, x);
            std::vector<double> env;
            for (size_t i = 0; i + 480 <= y.size(); i += 240) {
                double s = 0;
                for (size_t k = 0; k < 480; k++) s += double(y[i + k]) * y[i + k];
                env.push_back(std::sqrt(s / 480));
            }
            int minima = 0;
            double mx = 0;
            for (double e : env) mx = std::fmax(mx, e);
            const size_t from = env.size() / 6; // after the first second
            for (size_t i = from + 1; i + 1 < env.size(); i++)
                if (env[i] < env[i - 1] && env[i] <= env[i + 1] && env[i] < 0.25 * mx) minima++;
            char what[140];
            std::snprintf(what, sizeof what, "Rate 1 Hz (%s): the notch crosses 800 Hz %d times in 5 s (two for every cycle: 10)", wave == M::SINE ? "sine" : "triangle", minima);
            check(minima >= 9 && minima <= 11, what);
        }
        Unit u;
        u.stages = 2;
        u.center = 800;
        u.depth = 0.5;
        u.rate = 1.0;
        u.wave = M::RANDOM;
        auto m = make({u});
        const std::vector<float> x = sine(800.0f, 0.1f, 48000 * 6), y = run(*m, x);
        double lo = 1e9, hi = 0;
        for (size_t i = 24000; i + 480 <= y.size(); i += 480) {
            double s = 0;
            for (size_t k = 0; k < 480; k++) s += double(y[i + k]) * y[i + k];
            lo = std::fmin(lo, std::sqrt(s / 480));
            hi = std::fmax(hi, std::sqrt(s / 480));
        }
        check(hi > 2.0 * lo && hi < 0.15, "Random: the notch wanders (the level of a sine at 800 Hz varies by more than 6 dB) and stays bounded");
    }

    std::printf("\nRouting and the dry signal:\n");
    {
        Unit a, b;
        a.stages = 4; a.center = 500;
        b.stages = 6; b.center = 1500;
        auto series = make({a, b}, M::SERIES), par = make({a, b}, M::PARALLEL), one = make({a}), two = make({b});
        double worstS = 0, worstP = 0;
        for (double f : {200.0, 400.0, 700.0, 1100.0, 1800.0, 3000.0, 5000.0}) {
            const cd g1 = gainAt(*one, float(f)), g2 = gainAt(*two, float(f));
            const cd gs = gainAt(*series, float(f)), gp = gainAt(*par, float(f));
            worstS = std::fmax(worstS, std::abs(gs - g1 * g2 * double(M::kMakeup))); // (the gains are without one make-up gain)
            worstP = std::fmax(worstP, std::abs(gp - 0.5 * (g1 + g2)));
        }
        std::printf("    Series against the product, Parallel against the mean: %.4f, %.4f\n", worstS, worstP);
        check(worstS < 0.01, "Series: the response of two phasers is the product of their responses");
        check(worstP < 0.01, "Parallel: the response is the mean of their responses (the notches of both)");

        auto off = make({}, M::SERIES);
        const std::vector<float> x = sine(440.0f, 0.3f, 9600), y = run(*off, x);
        check(x == y, "with every phaser off the output is the input, bit for bit");

        auto zero = make({a, b}, M::SERIES, M::MONO_OUT, 0.0f);
        const std::vector<float> yz = run(*zero, x);
        check(x == yz, "Mix 0%: the output is the input, bit for bit");

        auto half = make({a, b}, M::SERIES, M::MONO_OUT, 0.5f), full = make({a, b}, M::SERIES, M::MONO_OUT, 1.0f);
        double worst = 0;
        for (double f : {300.0, 900.0, 2500.0}) worst = std::fmax(worst, std::abs(double(M::kMakeup) * gainAt(*half, float(f)) - 0.5 * (1.0 + double(M::kMakeup) * gainAt(*full, float(f)))));
        check(worst < 0.01, "Mix 50%: the dry signal and the phased signal in equal parts");
    }

    std::printf("\nFeedback:\n");
    {
        auto peak = [](double fb, int stages) {
            Unit u;
            u.stages = stages;
            u.center = 1000;
            u.feedback = fb;
            auto m = make({u});
            double mx = 0;
            for (double f = 100; f < 10000; f *= 1.02) mx = std::fmax(mx, std::abs(gainAt(*m, float(f), 4800)));
            return mx;
        };
        const double p0 = peak(0.0, 4), pPos = peak(0.85, 4), pNeg = peak(-0.85, 4);
        std::printf("    the highest gain at 4 stages: %.2f with no feedback, %.2f at +85%%, %.2f at -85%%\n", p0, pPos, pNeg);
        check(p0 > 0.97 && p0 < 1.02, "no feedback: the gain never exceeds 1 (a notch filter)");
        check(pPos > 2.0 && pPos < 3.2 && pNeg > 1.5 && pNeg < 3.2, "feedback of 85% either way gives resonant peaks of 4 to 10 dB, no more (the loop is stable, the level is corrected)");
        Unit u;
        u.stages = 12;
        u.feedback = 0.85;
        u.depth = 1.0;
        u.rate = 5.0;
        auto m = make({u, u, u, u});
        std::vector<float> noise(48000 * 3);
        unsigned s = 5;
        for (float &v : noise) { s = 1664525u * s + 1013904223u; v = 2.0f * ((s >> 8) & 0xFFFF) / 65535.0f - 1.0f; }
        const std::vector<float> y = run(*m, noise);
        double mx = 0;
        bool finite = true;
        for (float v : y) { mx = std::fmax(mx, std::fabs(v)); finite = finite && std::isfinite(v); }
        std::printf("    four phasers of 12 stages at 85%% feedback, full-scale noise, fast and deep: the peak is %.1f\n", mx);
        check(finite && mx < 200.0, "the output is finite and bounded in the worst case (full-scale noise through four resonant phasers in series: 40 dB)");
    }

    std::printf("\nStereo:\n");
    {
        Unit u;
        u.stages = 4;
        u.depth = 0.8;
        u.rate = 1.0;
        std::vector<float> in = sine(500.0f, 0.2f, 48000);
        for (size_t i = 0; i < in.size(); i++) in[i] += 0.1f * float(std::sin(2.0 * kPi * 1700.0 * i / kSr));
        auto mono = make({u}, M::SERIES, M::MONO_OUT), st = make({u}, M::SERIES, M::STEREO_OUT);
        std::vector<float> ml(in.size()), mr(in.size()), sl(in.size()), sr(in.size());
        for (size_t o = 0; o < in.size(); o += 48) {
            mono->ProcessMonoBlock(&in[o], &ml[o], &mr[o], 48);
            st->ProcessMonoBlock(&in[o], &sl[o], &sr[o], 48);
        }
        double same = 0, leftDiff = 0, rightDiff = 0;
        for (size_t i = 0; i < in.size(); i++) {
            same = std::fmax(same, std::fabs(ml[i] - mr[i]));
            leftDiff = std::fmax(leftDiff, std::fabs(ml[i] - sl[i]));
            rightDiff = std::fmax(rightDiff, std::fabs(sl[i] - sr[i]));
        }
        check(same == 0.0, "Mono: both channels carry the same signal");
        check(leftDiff < 1e-6, "Stereo: the left channel is what Mono gives");
        check(rightDiff > 0.02, "Stereo: the right channel is the same phaser a quarter of a cycle later (different from the left)");
    }

    std::printf("\nSwitching while playing (no click):\n");
    {
        Unit a, b;
        a.stages = 4; a.center = 700; a.depth = 0.6; a.rate = 0.3;
        b.stages = 6; b.center = 1500; b.depth = 0.6; b.rate = 0.4;
        const std::vector<float> tone = [] { auto t = sine(330.0f, 0.2f, 96000); for (size_t i = 0; i < t.size(); i++) t[i] += 0.1f * float(std::sin(2.0 * kPi * 990.0 * i / kSr)); return t; }();
        // the largest step of the output in the 200 ms after a change made at sample 48000, against the usual step
        auto step = [&](M &m, auto change) {
            std::vector<float> l(tone.size()), r(tone.size());
            for (size_t o = 0; o < tone.size(); o += 48) {
                if (o == 48000) change(m);
                m.ProcessMonoBlock(&tone[o], &l[o], &r[o], 48);
            }
            // the usual step is the largest of that before the change, long after it and of the dry input (the phased signal passes through the dry one)
            double before = 0, after = 0;
            for (size_t i = 48000 - 4800; i < 48000; i++) before = std::fmax(before, std::fabs(l[i] - l[i - 1]));
            for (size_t i = 96000 - 4800; i < 96000; i++) before = std::fmax(before, std::fabs(l[i] - l[i - 1]));
            for (size_t i = 1; i < tone.size(); i++) before = std::fmax(before, std::fabs(tone[i] - tone[i - 1]) * 1.05);
            for (size_t i = 48000; i < 48000 + 9600; i++) after = std::fmax(after, std::fabs(l[i] - l[i - 1]));
            std::printf("      largest step %.4f before, %.4f after (%.2f)\n", before, after, after / before);
            return after / before;
        };
        auto m1 = make({a, b});
        check(step(*m1, [](M &m) { m.SetParameterRaw(M::UnitParamId(1, M::U_ON), 0); }) < 1.5, "switching a phaser off does not click");
        auto m2 = make({a});
        check(step(*m2, [](M &m) { m.SetParameterRaw(M::UnitParamId(1, M::U_ON), 1); }) < 1.5, "switching a phaser on does not click");
        auto m3 = make({a, b}, M::SERIES);
        check(step(*m3, [](M &m) { m.SetParameterRaw(M::ROUTING, M::PARALLEL); }) < 1.5, "switching the routing does not click");
        auto m4 = make({a, b}, M::SERIES, M::MONO_OUT);
        check(step(*m4, [](M &m) { m.SetParameterRaw(M::OUTPUT, M::STEREO_OUT); }) < 1.5, "switching to Stereo does not click");
        auto m5 = make({a, b});
        check(step(*m5, [](M &m) { m.SetParameterRaw(M::UnitParamId(0, M::U_STAGES), 5); m.SetParameterRaw(M::UnitParamId(1, M::U_STAGES), 0); }) < 1.5,
              "changing the number of stages does not click");

        // after the switch the new routing is the one that is heard
        Unit sa = a, sb = b;
        sa.depth = sb.depth = 0.0;
        auto r = make({sa, sb}, M::SERIES);
        r->SetParameterRaw(M::ROUTING, M::PARALLEL);
        run(*r, sine(440.0f, 0.1f, 9600)); // 200 ms: the switch is made
        auto ref = make({sa, sb}, M::PARALLEL);
        double worst = 0;
        for (double f : {300.0, 900.0, 2500.0}) worst = std::fmax(worst, std::abs(gainAt(*r, float(f)) - gainAt(*ref, float(f))));
        check(worst < 0.01, "after switching to Parallel the response is that of Parallel");
    }

    std::printf("\nBlock path, bounds, silence:\n");
    {
        auto randomize = [](BaseEffectModule &m, unsigned &s) {
            auto rnd = [&s] { s = 1664525u * s + 1013904223u; return ((s >> 8) & 0xFFFF) / 65535.0f; };
            for (int i = 0; i < m.GetParameterCount(); i++) {
                if (m.GetParameterType(i) == ParameterValueType::Binned)
                    m.SetParameterRaw(i, std::min(int(rnd() * m.GetParameterBinCount(i)), m.GetParameterBinCount(i) - 1));
                else
                    m.SetParameterAsFloat(i, rnd());
            }
        };
        const size_t total = 48000 * 4, every = 48 * 37 * 3;
        std::vector<float> x(total);
        for (size_t i = 0; i < total; i++) x[i] = 0.3f * float(std::sin(0.31 * i) + 0.5 * std::sin(0.023 * i)) * float((i / 4800) % 2);
        std::vector<float> sl(total), sr(total), l48(total), r48(total), l37(total), r37(total);
        auto a = std::make_unique<M>(), b = std::make_unique<M>(), c = std::make_unique<M>();
        a->Init(kSr), b->Init(kSr), c->Init(kSr);
        unsigned sa = 7, sb = 7, sc = 7;
        for (size_t i = 0; i < total; i++) {
            if (i % every == 0) randomize(*a, sa);
            a->ProcessMono(x[i]);
            sl[i] = a->GetAudioLeft();
            sr[i] = a->GetAudioRight();
        }
        for (size_t o = 0; o < total; o += 48) {
            if (o % every == 0) randomize(*b, sb);
            b->ProcessMonoBlock(&x[o], &l48[o], &r48[o], std::min<size_t>(48, total - o));
        }
        for (size_t o = 0; o < total; o += 37) {
            if (o % every == 0) randomize(*c, sc);
            c->ProcessMonoBlock(&x[o], &l37[o], &r37[o], std::min<size_t>(37, total - o));
        }
        bool equal = true;
        double mx = 0;
        bool finite = true;
        for (size_t i = 0; i < total; i++) {
            equal = equal && sl[i] == l48[i] && sr[i] == r48[i] && sl[i] == l37[i] && sr[i] == r37[i];
            mx = std::fmax(mx, std::fmax(std::fabs(sl[i]), std::fabs(sr[i])));
            finite = finite && std::isfinite(sl[i]) && std::isfinite(sr[i]);
        }
        check(equal, "the block paths (48 and 37 samples) equal the per-sample path, with random settings changed while playing");
        std::printf("    the largest output over the random settings: %.2f\n", mx);
        check(finite && mx < 20.0, "the output is finite and bounded for random settings, changed while playing");

        auto z = std::make_unique<M>();
        z->Init(kSr);
        double zmax = 0;
        for (int i = 0; i < 48000; i++) {
            z->ProcessMono(0.0f);
            zmax = std::fmax(zmax, std::fabs(z->GetAudioLeft()));
        }
        check(zmax == 0.0, "silence in gives silence out");

        // switching it on again starts from silence
        Unit u;
        u.stages = 12;
        u.feedback = 0.85;
        auto e = make({u});
        for (int i = 0; i < 4800; i++) e->ProcessMono(0.5f * float(std::sin(0.1 * i)));
        e->SetEnabled(false);
        e->SetEnabled(true);
        double peak = 0;
        for (int i = 0; i < 4800; i++) {
            e->ProcessMono(0.0f);
            peak = std::fmax(peak, std::fabs(e->GetAudioLeft()));
        }
        check(peak == 0.0, "switching it on again starts from silence: nothing of what was played before comes back");
    }

    std::printf("\n%s\n", fails ? "MULTI PHASER TESTS FAILED" : "ALL MULTI PHASER TESTS PASSED");
    return fails ? 1 : 0;
}
