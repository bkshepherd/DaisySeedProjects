// Tests of the spring reverb (Util/spring_dsp.h and Effect-Modules/spring_reverb_module.h):
//  1. the dispersive allpass chain: its group delay against the formula and against the delay of a wave in a bar (the highs arrive first), unit gain,
//     stability; the fractional delay line
//  2. the module: the echoes of a burst (their spacing for each tank, the sweep of each echo and what Drip does to it), the decay time, Dwell, Tone, Mix,
//     the dry signal, the two springs being different, switching on from silence (and not with Trails)
//  3. the block path equals the per-sample path (bit-exact), the output stays finite and bounded for any setting, silence in gives silence out
#include "spring_reverb_module.h"
#include <cmath>
#include <complex>
#include <cstdio>
#include <memory>
#include <vector>
using namespace bkshepherd;
using M = SpringReverbModule;

static int fails = 0;
static void check(bool ok, const char *what) {
    fails += !ok;
    std::printf("  %-110s %s\n", what, ok ? "OK" : "FAILED");
}

static constexpr float kSr = 48000.0f;
static constexpr double kPi = 3.14159265358979;

// ---------------------------------------------------------------- the chain

using Chain = springdsp::DispersionChain<96, 6>;
static const int kCounts[4] = {53, 4, 13, 21};
static const float kCoefs[4] = {0.30f, 0.70f, 0.80f, 0.88f};

// The complex amplitude of x at frequency f over [from, from + n): n a whole number of periods
static std::complex<double> dft(const std::vector<float> &x, size_t from, size_t n, double f) {
    std::complex<double> s = 0;
    for (size_t i = 0; i < n; i++) s += double(x[from + i]) * std::polar(1.0, -2.0 * kPi * f * double(from + i) / kSr);
    return s * (2.0 / double(n));
}

// The steady-state response of a chain to a sine of frequency f (1 s to settle, 1 s to measure): the complex gain
static std::complex<double> chainGain(Chain &c, double f) {
    c.Reset();
    const size_t N = 96000;
    std::vector<float> in(N), out(N);
    for (size_t i = 0; i < N; i++) {
        in[i] = 0.5f * float(std::sin(2.0 * kPi * f * i / kSr));
        out[i] = c.Process(in[i]);
    }
    return dft(out, 48000, 48000, f) / dft(in, 48000, 48000, f);
}

// The group delay in seconds at f, from the phase of the response at f and at f + 8 Hz (the phase changes by less than pi)
static double measuredDelay(Chain &c, double f) {
    const double d = 8.0;
    const std::complex<double> g0 = chainGain(c, f), g1 = chainGain(c, f + d);
    double dphi = std::arg(g1) - std::arg(g0);
    while (dphi > kPi) dphi -= 2.0 * kPi;
    while (dphi < -kPi) dphi += 2.0 * kPi;
    return -dphi / (2.0 * kPi * d);
}

int main() {
    std::printf("The dispersive allpass chain (the first spring's: 53 sections of 0.30, 4 of 0.70, 13 of 0.80, 21 of 0.88; stretch 6):\n");
    {
        Chain c;
        c.Set(kCounts, kCoefs, 4);
        bool formulaOk = true;
        double worst = 0;
        for (double f : {100.0, 500.0, 1000.0, 2000.0, 3000.0, 3500.0}) {
            const double m = measuredDelay(c, f), a = c.GroupDelay(float(f + 4.0), kSr);
            worst = std::fmax(worst, std::fabs(m - a) / a);
            formulaOk = formulaOk && std::fabs(m - a) < 0.04 * a;
        }
        char what[160];
        std::snprintf(what, sizeof what, "the group delay measured through the chain is the formula's (100 Hz to 3.5 kHz; the largest difference %.1f%%)", worst * 100);
        check(formulaOk, what);

        // The delay above its value at DC is that of a low-pass ladder below its transition frequency (4 kHz): 15.5 ms * (1 / sqrt(1 - (f / fC)^2) - 1)
        const double dc = c.GroupDelay(0.0f, kSr);
        bool ladder = true;
        const double fs[8] = {600, 1000, 1500, 2000, 2500, 3000, 3300, 3600};
        double ms[8];
        for (int i = 0; i < 8; i++) {
            ms[i] = 1000.0 * (c.GroupDelay(float(fs[i]), kSr) - dc);
            const double x = fs[i] / 4000.0, target = 15.5 * (1.0 / std::sqrt(1.0 - x * x) - 1.0);
            ladder = ladder && std::fabs(ms[i] - target) < 0.15 * target + 0.15;
        }
        std::printf("    delay above DC in ms at 600, 1000, 1500, 2000, 2500, 3000, 3300, 3600 Hz: %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f (DC: %.1f ms)\n", ms[0], ms[1], ms[2], ms[3], ms[4],
                    ms[5], ms[6], ms[7], 1000.0 * dc);
        check(ladder, "the delay follows 15.5 ms * (1 / sqrt(1 - (f / 4 kHz)^2) - 1) within 15%: the lows of an echo arrive first, the highs last");
        check(ms[0] < ms[1] && ms[1] < ms[2] && ms[2] < ms[3] && ms[3] < ms[4] && ms[4] < ms[5] && ms[5] < ms[6] && ms[6] < ms[7] && ms[0] < 0.5, "and it rises steadily with the frequency, from nothing at the lows");
        check(c.GroupDelay(4000.0f, kSr) > 3.0 * c.GroupDelay(3000.0f, kSr), "to a maximum at the 4 kHz transition frequency (stretch 6 at 48 kHz)");

        bool unity = true;
        for (double f : {60.0, 300.0, 1000.0, 3000.0, 3900.0, 6000.0}) unity = unity && std::fabs(std::abs(chainGain(c, f)) - 1.0) < 2e-3;
        check(unity, "it is an allpass: the gain is 1 at every frequency (60 Hz to 6 kHz)");

        c.Reset();
        double tail = 0, early = 0;
        for (int i = 0; i < 96000; i++) {
            const float y = c.Process(i == 0 ? 1.0f : 0.0f);
            if (i < 4800) early = std::fmax(early, std::fabs(y));
            if (i >= 90000) tail = std::fmax(tail, std::fabs(y));
        }
        check(early > 0.01 && tail < 1e-6, "an impulse spreads out and dies away (stable)");

        Chain z;
        const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        z.Set(kCounts, zeros, 4);
        check(std::fabs(z.GroupDelay(500.0f, kSr) - 91.0 * 6 / kSr) < 1e-7, "with the coefficients at 0 (Drip 0%) it is a plain delay of 91 sections of 6 samples");
        const int one[1] = {10};
        const float big[1] = {5.0f};
        z.Set(one, big, 1);
        check(z.a[0] > 0.0f && z.a[0] < 1.0f, "a coefficient is kept inside the stable range");

        // Drip: the amount of dispersion, the extra delay of the highs grows about in proportion to it (up to some 75%)
        auto sweepMs = [](float drip) {
            Chain d;
            float cf[4];
            for (int g = 0; g < 4; g++) cf[g] = springdsp::DripCoefficient(kCoefs[g], drip);
            d.Set(kCounts, cf, 4);
            return 1000.0 * (d.GroupDelay(3600.0f, kSr) - d.GroupDelay(500.0f, kSr));
        };
        const double full = sweepMs(1.0f);
        std::printf("    extra delay of 3.6 kHz over 500 Hz in ms at Drip 0, 25, 50, 75, 100%%: %.1f %.1f %.1f %.1f %.1f\n", sweepMs(0.0f), sweepMs(0.25f), sweepMs(0.5f), sweepMs(0.75f), full);
        check(springdsp::DripCoefficient(0.88f, 0.0f) == 0.0f && std::fabs(springdsp::DripCoefficient(0.88f, 1.0f) - 0.88f) < 1e-6 && springdsp::DripCoefficient(-0.5f, 0.5f) < 0.0f,
              "Drip 0 is a plain delay (coefficient 0) and Drip 100% the full coefficient (the sign is kept)");
        bool linear = std::fabs(sweepMs(0.0f)) < 0.01 && sweepMs(0.25f) < sweepMs(0.5f) && sweepMs(0.5f) < sweepMs(0.75f) && sweepMs(0.75f) < sweepMs(1.0f) * 1.02;
        for (float d : {0.25f, 0.5f, 0.75f}) linear = linear && std::fabs(sweepMs(d) / full - d) < 0.25;
        check(linear, "the sweep grows steadily with Drip, about in proportion (within 25% of the full sweep)");
        check(full > 15.0, "the full sweep (Drip 100%) is more than 15 ms between 500 Hz and 3.6 kHz");
    }

    std::printf("\nThe delay line:\n");
    {
        springdsp::DelayLine<64> d;
        for (int i = 1; i <= 20; i++) d.Write(float(i));
        check(d.Read(1.0f) == 20.0f && d.Read(5.0f) == 16.0f, "an integer delay gives the sample written that many writes ago");
        check(std::fabs(d.Read(5.25f) - 15.75f) < 1e-5, "a fractional delay is interpolated");
        for (int i = 0; i < 100; i++) d.Write(0.0f);
        check(d.Read(3.0f) == 0.0f, "it wraps around");
    }

    // ---------------------------------------------------------------- the module
    auto make = [](float dwell = 0.5f, float decay = 0.5f, float tone = 0.5f, float mix = 1.0f, float drip = 1.0f, int tank = 1, int output = 0, int springs = 0) {
        auto m = std::make_unique<M>();
        m->SetParameterAsFloat(M::DWELL, dwell);
        m->SetParameterAsFloat(M::DECAY, decay);
        m->SetParameterAsFloat(M::TONE, tone);
        m->SetParameterAsFloat(M::MIX, mix);
        m->SetParameterAsFloat(M::DRIP, drip);
        m->SetParameterRaw(M::TANK, tank);
        m->SetParameterRaw(M::OUTPUT, output);
        m->SetParameterRaw(M::SPRINGS, springs); // 0: two springs
        m->Init(kSr);
        return m;
    };
    struct Out {
        std::vector<float> l, r;
    };
    auto run = [](M &m, const std::vector<float> &inL, const std::vector<float> &inR, size_t block = 48) {
        Out o;
        o.l.resize(inL.size());
        o.r.resize(inL.size());
        for (size_t p = 0; p < inL.size(); p += block) m.ProcessStereoBlock(&inL[p], &inR[p], &o.l[p], &o.r[p], std::min(block, inL.size() - p));
        return o;
    };
    // The reverb is added to the dry signal: the wet part of an output is what is left when the input is taken away
    auto wetOf = [](Out o, const std::vector<float> &in) {
        for (size_t i = 0; i < in.size(); i++) {
            o.l[i] -= in[i];
            o.r[i] -= in[i];
        }
        return o;
    };
    // A Hann-windowed burst of a sine at the start of a silent signal
    auto burst = [](size_t n, float f, size_t len, float peak) {
        std::vector<float> v(n, 0.0f);
        for (size_t i = 0; i < len && i < n; i++) v[i] = peak * float(std::sin(2.0 * kPi * f * i / kSr) * (0.5 - 0.5 * std::cos(2.0 * kPi * i / len)));
        return v;
    };
    // The envelope: the RMS in a window of w samples
    auto envelope = [](const std::vector<float> &y, int w) {
        std::vector<double> e(y.size());
        for (size_t i = 0; i < y.size(); i++) {
            double s = 0;
            int c = 0;
            for (int k = -w / 2; k <= w / 2; k++)
                if (i + k < y.size() && int(i) + k >= 0) {
                    s += double(y[i + k]) * y[i + k];
                    c++;
                }
            e[i] = std::sqrt(s / c);
        }
        return e;
    };
    // The first `count` peaks of an envelope: local maxima above a fraction of the largest, at least min_gap samples apart (in ms)
    auto peaks = [](const std::vector<double> &e, size_t count, double minGapMs) {
        double mx = 0;
        for (double v : e) mx = std::fmax(mx, v);
        std::vector<double> t;
        const size_t gap = size_t(minGapMs * kSr / 1000.0);
        size_t last = 0;
        for (size_t i = 1; i + 1 < e.size() && t.size() < count; i++)
            if (e[i] > 0.25 * mx && e[i] >= e[i - 1] && e[i] > e[i + 1] && (t.empty() || i - last > gap)) {
                t.push_back(1000.0 * i / kSr);
                last = i;
            }
        return t;
    };

    std::printf("\nThe echoes of a burst (a 1 kHz burst of 6 ms, the envelope of each spring's wet signal):\n");
    {
        const double expected[3][2] = {{24.0, 29.0}, {34.0, 41.0}, {46.0, 56.0}};
        const char *tankNames[3] = {"Short", "Medium", "Long"};
        for (int tank = 0; tank < 3; tank++) {
            auto m = make(0.5f, 0.7f, 0.9f, 1.0f, 1.0f, tank);
            const std::vector<float> in = burst(48000, 1000.0f, 288, 0.05f);
            const Out o = wetOf(run(*m, in, in), in);
            bool ok = true;
            char what[160];
            double sp[2] = {0, 0};
            for (int ch = 0; ch < 2; ch++) {
                const std::vector<double> p = peaks(envelope(ch == 0 ? o.l : o.r, 48), 4, 10.0);
                if (p.size() < 4) {
                    ok = false;
                    continue;
                }
                sp[ch] = (p[3] - p[1]) / 2.0;
                for (size_t k = 2; k < 4; k++) ok = ok && std::fabs((p[k] - p[k - 1]) - expected[tank][ch]) < 1.0; // (the first peak is the burst itself)
            }
            std::snprintf(what, sizeof what, "%-6s tank: the echoes come every %.1f ms (left spring) and %.1f ms (right), as designed (%.0f and %.0f)", tankNames[tank], sp[0], sp[1],
                          expected[tank][0], expected[tank][1]);
            check(ok, what);
        }
    }

    std::printf("\nThe sweep of an echo, and Drip:\n");
    {
        // The time of the first echo of a burst at 300 Hz (20 ms long) and of one at 3.4 kHz (3 ms long), the burst's own centre taken away
        auto arrival = [&](float drip, float f, size_t len) {
            auto m = make(0.5f, 0.5f, 0.9f, 1.0f, drip, 1);
            const std::vector<float> in = burst(24000, f, len, 0.05f);
            const Out o = wetOf(run(*m, in, in), in);
            const std::vector<double> e = envelope(o.l, 240);
            size_t best = 0;
            for (size_t i = 0; i < size_t(0.07 * kSr); i++)
                if (e[i] > e[best]) best = i;
            return 1000.0 * best / kSr - 1000.0 * (len / 2.0) / kSr;
        };
        const double lowFull = arrival(1.0f, 300.0f, 960), highFull = arrival(1.0f, 3400.0f, 144);
        const double lowFlat = arrival(0.0f, 300.0f, 960), highFlat = arrival(0.0f, 3400.0f, 144);
        std::printf("    first echo (ms after the centre of the burst): Drip 100%%: 300 Hz %.1f, 3.4 kHz %.1f; Drip 0%%: 300 Hz %.1f, 3.4 kHz %.1f\n", lowFull, highFull, lowFlat, highFlat);
        check(highFull - lowFull > 8.0, "Drip 100%: the highs of an echo arrive more than 8 ms after the lows (the upward sweep, the \"boing\")");
        check(std::fabs(lowFlat - highFlat) < 3.0, "Drip 0%: the lows and the highs arrive together (a plain echo)");
    }

    std::printf("\nDoes it sound like a delay? (the repetition of the echoes of a pick, left plus right as in mono):\n");
    {
        // The strength of the repetition: the largest normalised autocorrelation, at lags of 10 to 80 ms, of the detrended log envelope (1 ms steps)
        // of the wet signal from 60 ms on. About 1 for a clean delay, near 0 for a wash. A pick is a 3 ms burst of noise.
        auto repetition = [&](float drip, int tank) {
            auto m = make(0.5f, 0.5f, 0.5f, 1.0f, drip, tank);
            std::vector<float> in(48000, 0.0f);
            unsigned s = 11;
            for (int i = 0; i < 144; i++) {
                s = 1664525u * s + 1013904223u;
                in[i] = 0.1f * (((s >> 8) & 0xFFFF) / 32768.0f - 1.0f);
            }
            const Out o = run(*m, in, in);
            std::vector<double> db;
            for (size_t i = 60 * 48; i + 96 < 700 * 48; i += 48) {
                double e = 0;
                for (int k = 0; k < 96; k++) e += 0.25 * double(o.l[i + k] + o.r[i + k]) * (o.l[i + k] + o.r[i + k]);
                db.push_back(10.0 * std::log10(e / 96 + 1e-20));
            }
            std::vector<double> d(db.size());
            for (size_t i = 0; i < db.size(); i++) {
                double sum = 0;
                int c = 0;
                for (int k = -75; k <= 75; k++)
                    if (int(i) + k >= 0 && i + k < db.size()) {
                        sum += db[i + k];
                        c++;
                    }
                d[i] = db[i] - sum / c;
            }
            double e0 = 0;
            for (double v : d) e0 += v * v;
            double best = 0;
            for (int lag = 10; lag <= 80; lag++) {
                double sum = 0;
                for (size_t i = 0; i + lag < d.size(); i++) sum += d[i] * d[i + lag];
                best = std::fmax(best, sum / e0);
            }
            return best;
        };
        const double flat = repetition(0.0f, 1), full = repetition(1.0f, 1), half = repetition(0.5f, 1), fullShort = repetition(1.0f, 0), fullLong = repetition(1.0f, 2);
        std::printf("    repetition: Drip 0%% %.2f; Drip 100%% Medium %.2f, Short %.2f, Long %.2f; Drip 50%% Medium %.2f\n", flat, full, fullShort, fullLong, half);
        check(flat > 0.4, "without dispersion (Drip 0%) the echoes repeat like a delay: the measure sees it (more than 0.4)");
        check(full < flat - 0.1 && fullShort < flat - 0.05 && fullLong < flat - 0.05, "with the full dispersion (the default) the echoes repeat less than without it in every tank (a real spring's echoes stay regular, but are smeared)");
        check(half < flat, "and half the dispersion lies in between");
    }

    std::printf("\nThe decay time (a 1 kHz tone for 0.4 s, then silence; the amplitude at 1 kHz in windows of 100 ms):\n");
    {
        double measured[3];
        const float knobs[3] = {0.3f, 0.5f, 0.7f};
        for (int k = 0; k < 3; k++) {
            auto m = make(0.5f, knobs[k], 0.9f, 1.0f, 1.0f, 1);
            const size_t N = 48000 * 6, on = 19200;
            std::vector<float> in(N, 0.0f);
            for (size_t i = 0; i < on; i++) in[i] = 0.05f * float(std::sin(2.0 * kPi * 1000.0 * i / kSr));
            const Out o = run(*m, in, in);
            std::vector<double> t, db;
            for (size_t w = on + 4800; w + 4800 <= N && t.size() < 20; w += 4800) {
                const double a = std::abs(dft(o.l, w, 4800, 1000.0));
                if (a < 1e-7) break;
                t.push_back((w + 2400 - on) / double(kSr));
                db.push_back(20.0 * std::log10(a));
            }
            double st = 0, sd = 0, stt = 0, std_ = 0;
            const size_t n = t.size();
            for (size_t i = 0; i < n; i++) {
                st += t[i];
                sd += db[i];
                stt += t[i] * t[i];
                std_ += t[i] * db[i];
            }
            measured[k] = -60.0 / ((n * std_ - st * sd) / (n * stt - st * st));
        }
        bool ok = true;
        for (int k = 0; k < 3; k++) {
            const double target = 0.5 * std::pow(12.0, knobs[k]);
            ok = ok && measured[k] > 0.8 * target && measured[k] < 1.2 * target;
        }
        std::printf("    Decay 1.05, 1.73 and 2.85 s: measured %.2f, %.2f, %.2f s\n", measured[0], measured[1], measured[2]);
        check(ok, "the tail dies away by 60 dB in the time Decay says (within 20%, at 1 kHz)");
        check(measured[0] < measured[1] && measured[1] < measured[2], "a longer Decay gives a longer tail");
    }

    std::printf("\nDwell, Tone and Mix:\n");
    {
        auto noise = [](size_t n, float peak, unsigned seed) {
            std::vector<float> v(n);
            unsigned s = seed;
            for (size_t i = 0; i < n; i++) {
                s = 1664525u * s + 1013904223u;
                v[i] = peak * (((s >> 8) & 0xFFFF) / 32768.0f - 1.0f);
            }
            return v;
        };
        auto wetRms = [&](float dwell, float peak) { // Mix 100%, the RMS of the wet part of the left output over the last second of 1.5 s of noise
            auto m = make(dwell, 0.5f, 0.9f, 1.0f, 1.0f, 1);
            const std::vector<float> in = noise(72000, peak, 7);
            const Out o = run(*m, in, in);
            double s = 0;
            for (size_t i = 24000; i < 72000; i++) s += double(o.l[i] - in[i]) * (o.l[i] - in[i]);
            return std::sqrt(s / 48000.0);
        };
        const double quiet0 = wetRms(0.0f, 0.001f), quiet1 = wetRms(1.0f, 0.001f);
        const double step = 20.0 * std::log10(quiet1 / quiet0);
        check(std::fabs(step - 36.0) < 3.0, "Dwell from 0 to 10 raises the wet signal of a quiet input by 36 dB (-12 to +24 dB of drive)");
        const double loud1 = wetRms(1.0f, 0.3f), loud0 = wetRms(0.0f, 0.3f);
        const double rise = 20.0 * std::log10(0.3 / 0.001); // the input is this much louder
        const double grow1 = 20.0 * std::log10(loud1 / quiet1), grow0 = 20.0 * std::log10(loud0 / quiet0);
        std::printf("    an input %.1f dB louder (0.001 to 0.3): the wet signal grows by %.1f dB at Dwell 0 and by %.1f dB at Dwell 10\n", rise, grow0, grow1);
        check(std::fabs(grow0 - rise) < 1.5, "at the lowest Dwell the tank is linear (the wet signal grows as much as the input)");
        check(grow1 < rise - 6.0, "at the highest Dwell it saturates: the tank is driven into a clip, so the wet signal grows more than 6 dB less than the input");

        auto toneAmp = [&](float tone, float f) {
            auto m = make(0.5f, 0.5f, tone, 1.0f, 1.0f, 1);
            std::vector<float> in(72000);
            for (size_t i = 0; i < in.size(); i++) in[i] = 0.05f * float(std::sin(2.0 * kPi * f * i / kSr));
            const Out o = wetOf(run(*m, in, in), in);
            return std::abs(dft(o.l, 24000, 48000, f));
        };
        const double hi = 20.0 * std::log10(toneAmp(1.0f, 3000.0) / toneAmp(0.0f, 3000.0)), lo = 20.0 * std::log10(toneAmp(1.0f, 300.0) / toneAmp(0.0f, 300.0));
        std::printf("    Tone 100%% against 0%%: %+.1f dB at 3 kHz, %+.1f dB at 300 Hz\n", hi, lo);
        check(hi > 8.0 && std::fabs(lo) < 1.0, "Tone is a high cut: it opens the highs (3 kHz) by more than 8 dB and leaves 300 Hz alone");

        // Mix: the dry signal exactly at 0%; the reverb is added to the dry signal, so at 50% the wet part is half of what it is at 100%
        std::vector<float> inL = noise(24000, 0.3f, 3), inR = noise(24000, 0.3f, 4);
        const Out o0 = run(*make(0.5f, 0.5f, 0.5f, 0.0f), inL, inR), o1 = run(*make(0.5f, 0.5f, 0.5f, 1.0f), inL, inR), o5 = run(*make(0.5f, 0.5f, 0.5f, 0.5f), inL, inR);
        bool dryExact = true;
        double worst = 0;
        for (size_t i = 0; i < inL.size(); i++) {
            dryExact = dryExact && o0.l[i] == inL[i] && o0.r[i] == inR[i];
            const double expect = inL[i] + 0.5 * (o1.l[i] - inL[i]); // the dry signal plus half of the wet part at 100%
            worst = std::fmax(worst, std::fabs(o5.l[i] - expect));
        }
        check(dryExact, "at Mix 0% the output is the input, bit for bit (stereo: the left and right inputs stay apart)");
        check(worst < 1e-3, "at Mix 50% the output is the dry signal plus half of the wet part at 100% (the reverb is added: the dry level never changes)");
    }

    std::printf("\nThe level of the return:\n");
    {
        // A note (a harmonic series, 0.3 s) on each of the six open strings, with the default settings (Mix 30%): the energy of what the reverb adds in the
        // 3 s that follow against the energy of the dry note. The first version had the wet signal at a level that made the return LOUDER than the dry
        // note (crossfading the Mix would otherwise have taken the level down), and that sounded like an echo effect, not a spring reverb.
        const float strings[6] = {82.41f, 110.0f, 146.83f, 196.0f, 246.94f, 329.63f};
        double sumW = 0, sumD = 0;
        for (float f : strings) {
            std::vector<float> in(48000 * 3, 0.0f);
            for (size_t i = 0; i < 14400; i++) {
                double x = 0;
                for (int k = 1; k <= 8; k++) x += std::sin(2.0 * kPi * f * k * i / kSr) / k;
                in[i] = 0.05f * float(x);
            }
            auto m = make(0.5f, 0.5f, 0.5f, 0.3f, 1.0f, 1);
            const Out o = wetOf(run(*m, in, in), in);
            for (size_t i = 0; i < in.size(); i++) {
                sumW += double(o.l[i]) * o.l[i];
                sumD += double(in[i]) * in[i];
            }
        }
        const double db = 10.0 * std::log10(sumW / sumD);
        std::printf("    the energy the reverb adds against the dry note, six strings: %.1f dB (left output)\n", db);
        check(db < -6.0 && db > -15.0, "at the default Mix the return is 6 to 15 dB below the dry note (audible, but a reverb, not a second note)");
    }

    std::printf("\nThe two springs, and switching on:\n");
    {
        auto m = make();
        const std::vector<float> in = burst(24000, 1500.0f, 144, 0.05f);
        const Out o = wetOf(run(*m, in, in), in);
        double diff = 0, e = 0;
        for (size_t i = 0; i < in.size(); i++) {
            diff += double(o.l[i] - o.r[i]) * (o.l[i] - o.r[i]);
            e += double(o.l[i]) * o.l[i];
        }
        check(diff > 0.5 * e, "the left and right outputs are different springs: a mono input gives a different tail in each (a stereo tank)");

        auto play = [&](M &mod, float seconds, float peak) {
            float p = 0;
            for (size_t i = 0; i < size_t(seconds * kSr); i++) {
                const float x = peak * float(std::sin(2.0 * kPi * 440.0 * i / kSr));
                mod.ProcessStereo(x, x);
                p = std::fmax(p, std::fabs(mod.GetAudioRight() - x)); // the wet part
            }
            return p;
        };
        auto a = make(0.5f, 0.5f, 0.5f, 0.3f);
        a->SetEnabled(true);
        play(*a, 1.0f, 0.2f);
        a->SetEnabled(false); // a chain slot that is switched off is not processed: its tank still holds this
        a->SetEnabled(true);
        float silentPeak = 0;
        for (int i = 0; i < 96000; i++) {
            a->ProcessStereo(0.0f, 0.0f);
            silentPeak = std::fmax(silentPeak, std::fmax(std::fabs(a->GetAudioLeft()), std::fabs(a->GetAudioRight())));
        }
        check(silentPeak < 1e-9f, "switching it on again starts from silence: nothing of what was played before comes back");
        auto b = make(0.5f, 0.5f, 0.5f, 0.3f);
        b->SetEnabled(true);
        play(*b, 1.0f, 0.2f);
        b->SetEnabled(false);
        b->SetTrailing(true); // the pedal lets the tail ring out (Trails) and the effect is switched on again meanwhile
        b->SetEnabled(true);
        b->SetTrailing(false);
        float keptPeak = 0;
        for (int i = 0; i < 4800; i++) {
            b->ProcessStereo(0.0f, 0.0f);
            keptPeak = std::fmax(keptPeak, std::fabs(b->GetAudioLeft()));
        }
        check(keptPeak > 1e-3f, "but not while the tail is still ringing out (Trails): then it is kept");
    }

    std::printf("\nOutput: stereo or mono, and the number of springs:\n");
    {
        auto noiseBurst = [](size_t n, unsigned seed) {
            std::vector<float> v(n, 0.0f);
            unsigned s = seed;
            for (size_t i = 0; i < n / 2; i++) {
                s = 1664525u * s + 1013904223u;
                v[i] = 0.2f * (((s >> 8) & 0xFFFF) / 32768.0f - 1.0f);
            }
            return v;
        };
        const std::vector<float> in = noiseBurst(48000, 21);
        for (int springs : {0, 2, 4}) {
            const Out st = wetOf(run(*make(0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1, 0, springs), in, in), in), mo = wetOf(run(*make(0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1, 1, springs), in, in), in);
            // Two channels of equal weight (an even count of springs): the mono output is the sum of the two, scaled to the level of one
            double sameErr = 0, sumErr = 0, rms = 0, differs = 0;
            for (size_t i = 0; i < in.size(); i++) {
                sameErr = std::fmax(sameErr, std::fabs(mo.l[i] - mo.r[i]));
                const double expect = (st.l[i] + st.r[i]) / std::sqrt(2.0);
                sumErr = std::fmax(sumErr, std::fabs(mo.l[i] - expect));
                rms += double(mo.l[i]) * mo.l[i];
                differs = std::fmax(differs, std::fabs(st.l[i] - st.r[i]));
            }
            char what[200];
            std::snprintf(what, sizeof what, "%d springs: Stereo gives two different tails, and Mono the same tail on both channels, the sum of them (to 1e-4 of the peak)", springs + 2);
            check(differs > 1e-3 && sameErr < 1e-6 && sumErr < 1e-4 && rms > 0, what);
        }
        // More springs: the repetition of the echoes drops (a denser tank), at a similar level
        auto level = [&](int springs) {
            const Out o = wetOf(run(*make(0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1, 1, springs), in, in), in);
            double e = 0;
            for (size_t i = 0; i < in.size(); i++) e += double(o.l[i]) * o.l[i];
            return 10.0 * std::log10(e / in.size());
        };
        const double l2 = level(0), l4 = level(2), l6 = level(4);
        std::printf("    the wet level in Mono with 2, 4 and 6 springs: %.1f, %.1f, %.1f dB\n", l2, l4, l6);
        check(std::fabs(l2 - l4) < 1.5 && std::fabs(l4 - l6) < 1.5, "the level of the return does not depend on the number of springs (within 1.5 dB)");

        // From mono back to stereo: nothing stale, and the tails go on
        auto m = make(0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1, 1);
        std::vector<float> loud = noiseBurst(48000, 5), zeros(72000, 0.0f);
        run(*m, loud, loud);
        m->SetParameterRaw(M::OUTPUT, 0);
        const Out after = run(*m, zeros, zeros);
        double leftTail = 0, rightTail = 0;
        for (size_t i = 24000; i < 48000; i++) { // from 0.5 s after the switch
            leftTail = std::fmax(leftTail, std::fabs(after.l[i]));
            rightTail = std::fmax(rightTail, std::fabs(after.r[i]));
        }
        std::printf("    0.5 to 1 s after switching mono -> stereo, with silence in: the left tail %.1e, the right %.1e\n", leftTail, rightTail);
        check(leftTail > 1e-4 && rightTail > 1e-4, "going back to stereo: both channels' tails go on (all the springs ran)");

        // Fewer springs while playing: the dropped springs fade out, no click, and they come back from silence
        auto fs = make(0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1, 1, 4);
        std::vector<float> tn(96000);
        for (size_t i = 0; i < tn.size(); i++) tn[i] = 0.2f * float(std::sin(2.0 * kPi * 440.0 * i / kSr) + 0.5 * std::sin(2.0 * kPi * 660.0 * i / kSr));
        std::vector<float> fl(tn.size()), fr(tn.size());
        for (size_t o = 0; o < tn.size(); o += 48) {
            if (o == 24000) fs->SetParameterRaw(M::SPRINGS, 0);
            if (o == 60000) fs->SetParameterRaw(M::SPRINGS, 4);
            fs->ProcessStereoBlock(&tn[o], &tn[o], &fl[o], &fr[o], 48);
        }
        double b1 = 0, a1 = 0, a2 = 0;
        for (size_t i = 24000 - 4800; i < 24000; i++) b1 = std::fmax(b1, std::fabs(fl[i] - fl[i - 1]));
        for (size_t i = 24000; i < 36000; i++) a1 = std::fmax(a1, std::fabs(fl[i] - fl[i - 1]));
        for (size_t i = 60000; i < 72000; i++) a2 = std::fmax(a2, std::fabs(fl[i] - fl[i - 1]));
        std::printf("    largest step: %.4f before, %.4f when springs are dropped, %.4f when they come back\n", b1, a1, a2);
        check(a1 < 1.5 * b1 && a2 < 1.5 * b1, "changing the number of springs while playing does not click");

        // Switching while playing: no click. The right channel's largest step across the switch against its largest step in the 100 ms before it
        auto sw = make(0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 1, 0);
        std::vector<float> tone(48000);
        for (size_t i = 0; i < tone.size(); i++) tone[i] = 0.2f * float(std::sin(2.0 * kPi * 440.0 * i / kSr) + 0.5 * std::sin(2.0 * kPi * 660.0 * i / kSr));
        std::vector<float> l(tone.size()), r(tone.size());
        for (size_t o = 0; o < tone.size(); o += 48) {
            if (o == 24000) sw->SetParameterRaw(M::OUTPUT, 1);
            sw->ProcessStereoBlock(&tone[o], &tone[o], &l[o], &r[o], 48);
        }
        double before = 0, across = 0;
        for (size_t i = 24000 - 4800; i < 24000; i++) before = std::fmax(before, std::fabs(r[i] - r[i - 1]));
        for (size_t i = 24000; i < 24000 + 2400; i++) across = std::fmax(across, std::fabs(r[i] - r[i - 1]));
        std::printf("    the right channel's largest step: %.4f before the switch to mono, %.4f across it\n", before, across);
        check(across < 1.5 * before, "switching to mono while playing does not click (the right channel crossfades to the first spring)");
        // ... and back
        auto sw2 = make(0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 1, 1);
        for (size_t o = 0; o < tone.size(); o += 48) {
            if (o == 24000) sw2->SetParameterRaw(M::OUTPUT, 0);
            sw2->ProcessStereoBlock(&tone[o], &tone[o], &l[o], &r[o], 48);
        }
        double before2 = 0, across2 = 0;
        for (size_t i = 24000 - 4800; i < 24000; i++) before2 = std::fmax(before2, std::fabs(r[i] - r[i - 1]));
        for (size_t i = 24000; i < 24000 + 2400; i++) across2 = std::fmax(across2, std::fabs(r[i] - r[i - 1]));
        std::printf("    ... and back to stereo: %.4f before, %.4f across\n", before2, across2);
        check(across2 < 1.5 * before2, "and switching back to stereo does not click either");
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
        const size_t total = 48000 * 3, every = 48 * 37 * 3; // the changes fall on block boundaries of both block sizes
        std::vector<float> x(total);
        for (size_t i = 0; i < total; i++) x[i] = 0.3f * float(std::sin(0.31 * i) + 0.5 * std::sin(0.023 * i)) * float((i / 4800) % 2);
        std::vector<float> sl(total), sr(total), l48(total), r48(total), l37(total), r37(total);
        auto a = std::make_unique<M>(), b = std::make_unique<M>(), c = std::make_unique<M>();
        a->Init(kSr), b->Init(kSr), c->Init(kSr);
        unsigned sa = 5, sb = 5, sc = 5;
        for (size_t i = 0; i < total; i++) {
            if (i % every == 0) randomize(*a, sa);
            a->ProcessStereo(x[i], x[i]);
            sl[i] = a->GetAudioLeft();
            sr[i] = a->GetAudioRight();
        }
        for (size_t o = 0; o < total; o += 48) {
            if (o % every == 0) randomize(*b, sb);
            b->ProcessStereoBlock(&x[o], &x[o], &l48[o], &r48[o], std::min<size_t>(48, total - o));
        }
        for (size_t o = 0; o < total; o += 37) {
            if (o % every == 0) randomize(*c, sc);
            c->ProcessStereoBlock(&x[o], &x[o], &l37[o], &r37[o], std::min<size_t>(37, total - o));
        }
        bool same = true;
        for (size_t i = 0; i < total; i++) same = same && sl[i] == l48[i] && sr[i] == r48[i] && sl[i] == l37[i] && sr[i] == r37[i];
        check(same, "the block paths (48 and 37 samples) equal the per-sample path, with random settings changed while playing");

        // the mono entry points
        auto d = make(), e = make();
        std::vector<float> ml(4800), mr(4800), ref(4800);
        const std::vector<float> in = burst(4800, 700.0f, 400, 0.2f);
        for (size_t o = 0; o < 4800; o += 48) d->ProcessMonoBlock(&in[o], &ml[o], &mr[o], 48);
        for (size_t i = 0; i < 4800; i++) {
            e->ProcessMono(in[i]);
            ref[i] = e->GetAudioLeft();
        }
        bool monoSame = true;
        for (size_t i = 0; i < 4800; i++) monoSame = monoSame && ml[i] == ref[i];
        check(monoSame, "the mono block path equals ProcessMono");

        bool finite = true;
        double worst = 0;
        unsigned s = 99;
        for (int cfg = 0; cfg < 12; cfg++) {
            auto m = std::make_unique<M>();
            m->Init(kSr);
            for (int round = 0; round < 3; round++) {
                randomize(*m, s);
                std::vector<float> loud(24000), sine(9600);
                for (size_t i = 0; i < loud.size(); i++) {
                    s = 1664525u * s + 1013904223u;
                    loud[i] = 1.2f * (((s >> 8) & 0xFFFF) / 32768.0f - 1.0f);
                }
                for (size_t i = 0; i < sine.size(); i++) sine[i] = float(std::sin(2.0 * kPi * (100.0 + 400.0 * cfg) * i / kSr));
                for (const auto &sig : {loud, sine}) {
                    const Out o = run(*m, sig, sig);
                    for (size_t i = 0; i < sig.size(); i++) {
                        finite = finite && std::isfinite(o.l[i]) && std::isfinite(o.r[i]);
                        worst = std::fmax(worst, std::fmax(std::fabs(o.l[i]), std::fabs(o.r[i])));
                    }
                }
            }
        }
        std::printf("    largest output over the random settings with full-scale noise and sines: %.2f\n", worst);
        check(finite && worst < 20.0, "the output is finite and bounded (below 20) for random settings, changed while playing");

        auto z = make();
        const std::vector<float> zeros(48000, 0.0f);
        const Out oz = run(*z, zeros, zeros);
        double mx = 0;
        for (size_t i = 0; i < zeros.size(); i++) mx = std::fmax(mx, std::fmax(std::fabs(oz.l[i]), std::fabs(oz.r[i])));
        check(mx == 0.0, "silence in gives silence out");
    }

    std::printf("\n%s\n", fails ? "SPRING REVERB TESTS FAILED" : "ALL SPRING REVERB TESTS PASSED");
    return fails ? 1 : 0;
}
