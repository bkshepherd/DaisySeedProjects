// Test Tone tests: the level (peak for the tones, the RMS of a sine of that level for the noise), the frequency, the waveforms, smooth changes of level, frequency and type,
// the input being ignored, and the block path equal to the per-sample path.
#include "test_tone_module.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace bkshepherd;

static int g_fails = 0;
static void check(bool ok, const char *what) { printf("  %-82s %s\n", what, ok ? "OK" : "FAIL"); if (!ok) g_fails++; }

static constexpr float kSr = 48000.0f;

struct Out { std::vector<float> l, r; };

// Runs the module for `total` samples in blocks of 48 (or per sample); `events` change a parameter before the block that starts at `at`.
struct Event { size_t at; int param; float value; bool raw; };
template <class Cfg> static Out run(Cfg cfg, size_t total, bool perSample = false, const std::vector<Event> &events = {}, const std::vector<float> *input = nullptr) {
    static unsigned char mem[sizeof(TestToneModule)];
    std::memset(mem, 0, sizeof mem);
    TestToneModule *m = new (mem) TestToneModule();
    m->Init(kSr);
    cfg(*m);
    std::vector<float> zero(total, 0.0f);
    const std::vector<float> &in = input ? *input : zero;
    Out o{std::vector<float>(total), std::vector<float>(total)};
    size_t nextEvent = 0;
    const size_t bs = 48;
    for (size_t b = 0; b < total; b += bs) {
        while (nextEvent < events.size() && events[nextEvent].at <= b) {
            const Event &e = events[nextEvent++];
            if (e.raw) m->SetParameterRaw(e.param, (uint32_t)e.value); else m->SetParameterAsFloat(e.param, e.value);
        }
        const size_t n = std::min(bs, total - b);
        if (perSample) {
            for (size_t i = b; i < b + n; i++) { m->ProcessMono(in[i]); o.l[i] = m->GetAudioLeft(); o.r[i] = m->GetAudioRight(); }
        } else {
            m->ProcessMonoBlock(&in[b], &o.l[b], &o.r[b], n);
        }
    }
    m->~TestToneModule();
    return o;
}

static double rms(const std::vector<float> &v, size_t from, size_t to) { double e = 0; for (size_t i = from; i < to; i++) e += (double)v[i] * v[i]; return std::sqrt(e / (to - from)); }
static double peak(const std::vector<float> &v, size_t from, size_t to) { double p = 0; for (size_t i = from; i < to; i++) p = std::fmax(p, std::fabs(v[i])); return p; }
static double mean(const std::vector<float> &v, size_t from, size_t to) { double s = 0; for (size_t i = from; i < to; i++) s += v[i]; return s / (to - from); }
static double freqByCrossings(const std::vector<float> &v, size_t from, size_t to) { int c = 0; for (size_t i = from + 1; i < to; i++) if ((v[i - 1] < 0) != (v[i] < 0)) c++; return c / 2.0 / ((to - from) / kSr); }
static double maxStep(const std::vector<float> &v, size_t from, size_t to) { double s = 0; for (size_t i = from + 1; i < to; i++) s = std::fmax(s, std::fabs(v[i] - v[i - 1])); return s; }
static double db(double x) { return 20.0 * std::log10(x); }

int main() {
    const size_t total = 96000, from = 24000; // measure after the first half second
    auto tone = [](int type, float level, float freq) {
        return [=](TestToneModule &m) {
            m.SetParameterRaw(TestToneModule::TYPE, type);
            m.SetParameterAsFloat(TestToneModule::LEVEL, level);
            m.SetParameterAsFloat(TestToneModule::FREQUENCY, freq);
        };
    };
    auto settled = [&](const Out &o) { return o; };
    (void)settled;

    printf("the waveforms (level -10 dB, 1 kHz):\n");
    const float lv = std::pow(10.0f, -10.0f / 20.0f);
    {
        const Out o = run(tone(TestToneModule::SINE, -10.0f, 1000.0f), total);
        printf("  sine: peak %.4f (expected %.4f), RMS %.4f (%.4f), %.1f Hz\n", peak(o.l, from, total), lv, rms(o.l, from, total), lv / std::sqrt(2.0f), freqByCrossings(o.l, from, total));
        check(std::fabs(peak(o.l, from, total) - lv) < 0.01 * lv, "sine: the peak is the level");
        check(std::fabs(rms(o.l, from, total) - lv / std::sqrt(2.0)) < 0.01 * lv, "sine: the RMS is the peak / sqrt(2)");
        check(std::fabs(freqByCrossings(o.l, from, total) - 1000.0) < 5.0, "sine: the frequency is 1000 Hz");
        check(std::fabs(mean(o.l, from, total)) < 0.002 * lv, "sine: no DC");
        double d = 0; for (size_t i = 0; i < total; i++) d = std::fmax(d, std::fabs(o.l[i] - o.r[i]));
        check(d == 0.0, "both outputs carry the same signal");
    }
    {
        const Out o = run(tone(TestToneModule::SQUARE, -10.0f, 1000.0f), total);
        printf("  square: peak %.4f, RMS %.4f, %.1f Hz\n", peak(o.l, from, total), rms(o.l, from, total), freqByCrossings(o.l, from, total));
        check(peak(o.l, from, total) > 0.99 * lv && peak(o.l, from, total) < 1.03 * lv, "square: the peak is the level");
        check(std::fabs(rms(o.l, from, total) - lv) < 0.03 * lv, "square: the RMS is the peak");
        check(std::fabs(freqByCrossings(o.l, from, total) - 1000.0) < 5.0, "square: the frequency is 1000 Hz");
    }
    {
        const Out o = run(tone(TestToneModule::TRIANGLE, -10.0f, 1000.0f), total);
        printf("  triangle: peak %.4f, RMS %.4f (expected %.4f), %.1f Hz\n", peak(o.l, from, total), rms(o.l, from, total), lv / std::sqrt(3.0f), freqByCrossings(o.l, from, total));
        check(std::fabs(peak(o.l, from, total) - lv) < 0.01 * lv, "triangle: the peak is the level");
        check(std::fabs(rms(o.l, from, total) - lv / std::sqrt(3.0)) < 0.03 * lv, "triangle: the RMS is the peak / sqrt(3)");
        check(std::fabs(freqByCrossings(o.l, from, total) - 1000.0) < 5.0, "triangle: the frequency is 1000 Hz");
    }
    {
        const Out a = run(tone(TestToneModule::NOISE, -10.0f, 1000.0f), total), b = run(tone(TestToneModule::NOISE, -10.0f, 5000.0f), total);
        printf("  noise: RMS %.4f (expected %.4f), mean %.5f\n", rms(a.l, from, total), lv / std::sqrt(2.0f), mean(a.l, from, total));
        check(std::fabs(rms(a.l, from, total) - lv / std::sqrt(2.0)) < 0.03 * lv, "noise: the RMS equals that of a sine at the level");
        check(std::fabs(mean(a.l, from, total)) < 0.005 * lv, "noise: no DC");
        bool same = true; for (size_t i = 1000; i < total; i++) if (a.l[i] != b.l[i]) same = false; // (the first 5 ms are the fade out of the sine that is playing at the start)
        check(same, "noise: the frequency does not matter");
        // white: the RMS of the first difference of white noise is sqrt(2) x the RMS (a sine at 1 kHz would be much lower)
        double dd = 0; for (size_t i = from + 1; i < total; i++) dd += (double)(a.l[i] - a.l[i - 1]) * (a.l[i] - a.l[i - 1]);
        dd = std::sqrt(dd / (total - from - 1));
        check(std::fabs(dd / rms(a.l, from, total) - std::sqrt(2.0)) < 0.05, "noise: it is white (no correlation between samples)");
    }

    printf("\nlevel and frequency:\n");
    for (float level : {0.0f, -6.0f, -20.0f, -60.0f}) {
        const Out o = run(tone(TestToneModule::SINE, level, 440.0f), total);
        const double want = std::pow(10.0, level / 20.0);
        printf("  level %+.0f dB: peak %.5f = %.2f dB (expected %.2f dB)\n", level, peak(o.l, from, total), db(peak(o.l, from, total)), level);
        check(std::fabs(db(peak(o.l, from, total)) - level) < 0.1, "the peak level in dB follows the Level setting");
        (void)want;
    }
    for (float f : {20.0f, 100.0f, 440.0f, 5000.0f, 15000.0f}) {
        const Out o = run(tone(TestToneModule::SINE, -10.0f, f), total);
        const double got = freqByCrossings(o.l, from, total);
        printf("  %.0f Hz: measured %.1f Hz\n", f, got);
        check(std::fabs(got - f) < 0.02 * f + 1.0, "the frequency follows the Frequency setting");
    }

    printf("\nsmooth changes:\n");
    {
        const float amp = std::pow(10.0f, -10.0f / 20.0f);
        const double natural = 2.0 * 3.14159265 * 1000.0 / kSr * amp; // the largest step of a sine of that amplitude and frequency
        // level jumps from -10 to -30 dB, then back
        const Out o = run(tone(TestToneModule::SINE, -10.0f, 1000.0f), total, false, {{30000, TestToneModule::LEVEL, -30.0f, false}, {60000, TestToneModule::LEVEL, -10.0f, false}});
        printf("  largest sample to sample step across level changes: %.4f (a steady sine: %.4f)\n", maxStep(o.l, 24000, total), natural);
        check(maxStep(o.l, 24000, total) < 1.2 * natural, "a change of Level does not click");
        check(peak(o.l, 40000, 55000) < 0.12 * amp && peak(o.l, 80000, total) > 0.97 * amp, "and the level goes to the new value");
        // frequency jumps from 500 Hz to 3 kHz
        const Out f = run(tone(TestToneModule::SINE, -10.0f, 500.0f), total, false, {{30000, TestToneModule::FREQUENCY, 3000.0f, false}});
        const double naturalHigh = 2.0 * 3.14159265 * 3000.0 / kSr * amp;
        check(maxStep(f.l, 24000, total) < 1.05 * naturalHigh, "a change of Frequency does not click (the phase is continuous)");
        check(std::fabs(freqByCrossings(f.l, 60000, total) - 3000.0) < 20.0, "and the frequency goes to the new value");
        // type switches: the level goes down to zero and up again over 5 ms, so there is a near-silent moment at the switch (no step between two waveforms)
        for (int to : {TestToneModule::SQUARE, TestToneModule::TRIANGLE, TestToneModule::NOISE}) {
            const Out t = run(tone(TestToneModule::SINE, -10.0f, 1000.0f), total, false, {{30000, TestToneModule::TYPE, (float)to, true}});
            double quietest = 1e9; // the smallest peak over 12 consecutive samples in the 10 ms after the event
            for (size_t i = 30000; i + 12 < 30480; i++) quietest = std::fmin(quietest, peak(t.l, i, i + 12));
            printf("  sine -> %s: the quietest 12 samples around the switch peak at %.5f (the tone: %.3f)\n", to == 1 ? "square" : (to == 2 ? "triangle" : "noise"), quietest, amp);
            check(quietest < 0.05 * amp, "a change of Type goes through silence (no click)");
            check(peak(t.l, 40000, 48000) > 0.4 * amp, "and the new type is heard afterwards");
        }
    }

    printf("\nthe input and the paths:\n");
    {
        std::vector<float> loud(total);
        unsigned s = 1; for (size_t i = 0; i < total; i++) { s = 1664525u * s + 1013904223u; loud[i] = (((s >> 8) & 0xFFFF) / 32768.0f - 1.0f); }
        const Out a = run(tone(TestToneModule::SINE, -10.0f, 1000.0f), total, false, {}, &loud), b = run(tone(TestToneModule::SINE, -10.0f, 1000.0f), total);
        bool same = true; for (size_t i = 0; i < total; i++) if (a.l[i] != b.l[i]) same = false;
        check(same, "the audio input is ignored");
        for (int type = 0; type < 4; type++) {
            const Out blk = run(tone(type, -12.0f, 777.0f), total), per = run(tone(type, -12.0f, 777.0f), total, true);
            bool eq = true; for (size_t i = 0; i < total; i++) if (blk.l[i] != per.l[i] || blk.r[i] != per.r[i]) eq = false;
            check(eq, "the block path equals the per-sample path");
        }
    }

    printf("\n%s\n", g_fails ? "TEST TONE: FAILURES" : "TEST TONE OK");
    return g_fails != 0;
}
