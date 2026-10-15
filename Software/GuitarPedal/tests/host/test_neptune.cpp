// Neptune tests (the reverb engine itself is the repository's CloudSeed code, tested through the CloudSeed module):
//  1. The block path and the per-sample path give the same output (defaults and changed controls, mono and stereo in).
//  2. The delay: a normal echo arrives after the set time; octave mode raises the pitch of the echo by an octave; reverse mode plays the echo backwards; Delay Time at the
//     bottom switches the delay off; the feedback gives repeats.
//  3. The reverb mode change mutes the output for a moment (1000 samples) and then the sound is back; the routing, the reverb controls and the mix controls do something.
// Built against a copy of the engine with its internal block size set to 1 (see run_tests.sh), like the Jupiter test.
#include "neptune_module.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace bkshepherd;

static int g_fails = 0;
static void check(bool ok, const char *what) { printf("  %-78s %s\n", what, ok ? "OK" : "FAIL"); if (!ok) g_fails++; }

extern size_t pool_index; // the reverb engine's memory pool never frees; each module instance takes about 4.9 MB

static constexpr size_t kBlock = 48;

struct Out { std::vector<float> l, r; };

static std::vector<float> noiseBurst(size_t total, unsigned seed, float gain) {
    std::vector<float> v(total, 0.0f);
    unsigned s = seed;
    for (size_t i = 0; i < total; i++) {
        s = 1664525u * s + 1013904223u;
        const float noise = ((s >> 8) & 0xFFFF) / 32768.0f - 1.0f;
        const bool burst = i < 4800 || (i > 30000 && i < 31000);
        v[i] = burst ? gain * (noise + 0.5f * std::sin(0.03f * i)) : 0.0f;
    }
    return v;
}

// path: 0 = per-sample, 1 = block of 48, 2 = block of 256
template <class Cfg> static Out run(Cfg cfg, const std::vector<float> &inL, const std::vector<float> &inR, int path, bool stereoInput) {
    srand(7); // the engine's modulation phases come from rand()
    pool_index = 0;
    static unsigned char mem[sizeof(NeptuneModule)];
    std::memset(mem, 0, sizeof mem);
    NeptuneModule *m = new (mem) NeptuneModule();
    m->Init(48000.0f);
    cfg(*m);
    const size_t total = inL.size();
    Out o{std::vector<float>(total), std::vector<float>(total)};
    const size_t bs = path == 2 ? 256 : kBlock;
    for (size_t b = 0; b < total; b += bs) {
        const size_t n = std::min(bs, total - b);
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
    for (size_t i = 0; i < a.l.size(); i++) {
        d = std::fmax(d, std::fmax(std::fabs(a.l[i] - b.l[i]), std::fabs(a.r[i] - b.r[i])));
        peak = std::fmax(peak, std::fmax(std::fabs(a.l[i]), std::fabs(a.r[i])));
    }
    return d;
}

static double energy(const std::vector<float> &v, size_t from, size_t to) { double e = 0; for (size_t i = from; i < to && i < v.size(); i++) e += v[i] * v[i]; return e; }

// first and last sample where the 480-sample moving RMS exceeds `fraction` of the maximum over [from, to)
static void activeRegion(const std::vector<float> &v, size_t from, size_t to, double fraction, size_t &first, size_t &last) {
    std::vector<double> rms;
    double maxRms = 0;
    for (size_t i = from; i + 480 < to; i += 48) { rms.push_back(std::sqrt(energy(v, i, i + 480) / 480.0)); maxRms = std::fmax(maxRms, rms.back()); }
    first = last = from;
    bool found = false;
    for (size_t k = 0; k < rms.size(); k++) if (rms[k] > fraction * maxRms) { if (!found) { first = from + k * 48; found = true; } last = from + k * 48 + 480; }
}

int main() {
    const size_t total = 96000;
    const std::vector<float> inL = noiseBurst(total, 4242u, 0.3f), inR = noiseBurst(total, 777u, 0.25f);
    auto defaults = [](NeptuneModule &) {};
    auto changed = [](NeptuneModule &m) {
        m.SetParameterAsFloat(NeptuneModule::DECAY, 0.8f);
        m.SetParameterAsFloat(NeptuneModule::MIX, 0.6f);
        m.SetParameterAsFloat(NeptuneModule::DELAY_TIME, 0.15f);
        m.SetParameterAsFloat(NeptuneModule::MOD_AMT, 0.7f);
        m.SetParameterAsFloat(NeptuneModule::LOWPASS, 0.5f);
        m.SetParameterAsFloat(NeptuneModule::DELAY_FDBK, 0.5f);
        m.SetParameterRaw(NeptuneModule::REVERB_MODE, 2);
        m.SetParameterRaw(NeptuneModule::ROUTING, 0);
        m.SetParameterRaw(NeptuneModule::DELAY_MODE, 1);
        m.SetParameterAsFloat(NeptuneModule::PRE_DELAY, 0.1f);
        m.SetParameterAsFloat(NeptuneModule::MOD_RATE, 0.5f);
        m.SetParameterAsFloat(NeptuneModule::REV_DELAY_MIX, 0.3f);
    };

    printf("block path vs per-sample path:\n");
    for (int v = 0; v < 2; v++) for (int st = 0; st < 2; st++) {
        auto cfg = [&](NeptuneModule &m) { if (v) changed(m); if (st) m.SetParameterRaw(NeptuneModule::STEREO_IN, 1); };
        const Out a = run(cfg, inL, inR, 0, st), b = run(cfg, inL, inR, 1, st);
        double peak; const double d = maxDiff(a, b, peak);
        printf("  %s, %s input: max difference %.3g, peak %.3f\n", v ? "changed controls" : "defaults", st ? "stereo" : "mono", d, peak);
        check(d == 0.0 && peak > 0.05 && std::isfinite(peak), "block path equals per-sample path");
    }
    {
        const Out a = run(defaults, inL, inR, 1, true), b = run(defaults, inL, inR, 2, true);
        double peak; const double d = maxDiff(a, b, peak);
        check(d == 0.0 && peak > 0.05, "256-sample blocks give the same audio as 48-sample blocks (defaults)");
    }

    // ---- the delay on its own: Mix 1 (effect only), Rev/Delay 1 (delay only), Parallel routing, no feedback
    auto delayOnly = [](NeptuneModule &m) {
        m.SetParameterAsFloat(NeptuneModule::MIX, 1.0f);
        m.SetParameterAsFloat(NeptuneModule::REV_DELAY_MIX, 1.0f);
        m.SetParameterAsFloat(NeptuneModule::DELAY_FDBK, 0.0f);
        m.SetParameterRaw(NeptuneModule::ROUTING, 1);
    };
    printf("\ndelay (Mix 1, delay only):\n");
    {
        // a burst at the start, 0.2 Delay Time = 2400 + 0.2 * 189600 = 40320 samples
        std::vector<float> burst(total, 0.0f), zero(total, 0.0f);
        const std::vector<float> noise = noiseBurst(total, 99u, 0.4f);
        for (size_t i = 0; i < 2400; i++) burst[i] = noise[i];
        auto normal = [&](NeptuneModule &m) { delayOnly(m); m.SetParameterAsFloat(NeptuneModule::DELAY_TIME, 0.2f); };
        const Out o = run(normal, burst, zero, 1, false);
        size_t first, last; activeRegion(o.l, 0, total, 0.3, first, last);
        printf("  normal: echo from sample %zu to %zu (expected about 40320 + 2400)\n", first, last);
        check(first > 39000 && first < 42000, "normal mode: the echo arrives after the set time");
        check(energy(o.l, 0, 30000) < 0.01 * energy(o.l, first, last + 1), "normal mode: nothing before the echo (the delay is the only wet signal)");
        // feedback: repeats
        auto fb = [&](NeptuneModule &m) { normal(m); m.SetParameterAsFloat(NeptuneModule::DELAY_FDBK, 0.6f); };
        const Out f = run(fb, burst, zero, 1, false);
        check(energy(f.l, 80000, 96000) > 10 * energy(o.l, 80000, 96000), "feedback gives repeats");
        // delay off at the bottom of the range
        auto off = [&](NeptuneModule &m) { delayOnly(m); m.SetParameterAsFloat(NeptuneModule::DELAY_TIME, 0.0f); };
        const Out z = run(off, burst, zero, 1, false);
        check(energy(z.l, 0, total) < 1e-6, "Delay Time at the bottom switches the delay off");
    }
    {
        // octave: a 500 Hz sine burst comes back an octave up (counted by zero crossings in the echo)
        std::vector<float> sine(total, 0.0f), zero(total, 0.0f);
        for (size_t i = 0; i < 12000; i++) sine[i] = 0.3f * std::sin(2.0f * 3.14159265f * 500.0f * i / 48000.0f);
        auto cfg = [&](NeptuneModule &m) { delayOnly(m); m.SetParameterAsFloat(NeptuneModule::DELAY_TIME, 0.2f); m.SetParameterRaw(NeptuneModule::DELAY_MODE, 1); };
        const Out o = run(cfg, sine, zero, 1, false);
        size_t first, last; activeRegion(o.l, 0, total, 0.3, first, last);
        int crossings = 0; for (size_t i = first + 1; i < last; i++) if ((o.l[i - 1] < 0) != (o.l[i] < 0)) crossings++;
        const double freq = crossings / 2.0 / ((last - first) / 48000.0);
        printf("  octave: echo from %zu to %zu, about %.0f Hz (input 500 Hz)\n", first, last, freq);
        check(freq > 800 && freq < 1250, "octave mode: the echo is an octave up");
    }
    {
        // reverse: a burst whose level rises comes back falling
        std::vector<float> rise(total, 0.0f), zero(total, 0.0f);
        const std::vector<float> noise = noiseBurst(total, 5u, 0.5f);
        for (size_t i = 0; i < 9600; i++) rise[i] = noise[i % 4800] * (float)i / 9600.0f;
        auto cfg = [&](NeptuneModule &m) { delayOnly(m); m.SetParameterAsFloat(NeptuneModule::DELAY_TIME, 0.1f); m.SetParameterRaw(NeptuneModule::DELAY_MODE, 2); };
        const Out o = run(cfg, rise, zero, 1, false);
        size_t first, last; activeRegion(o.l, 0, total, 0.2, first, last);
        const size_t mid = (first + last) / 2;
        const double e1 = energy(o.l, first, mid), e2 = energy(o.l, mid, last);
        printf("  reverse: echo region %zu .. %zu, energy first half %.3g, second half %.3g\n", first, last, e1, e2);
        check(last > first && e1 > 1.5 * e2, "reverse mode: a rising burst comes back falling");
    }

    // ---- the reverb, the routing and the mix
    printf("\ncontrols:\n");
    const Out base = run(defaults, inL, inR, 1, true);
    auto differs = [&](const Out &o) { double p; return maxDiff(base, o, p) > 1e-4; };
    auto set = [](int p, float v) { return [p, v](NeptuneModule &m) { m.SetParameterAsFloat(p, v); }; };
    auto raw = [](int p, int v) { return [p, v](NeptuneModule &m) { m.SetParameterRaw(p, v); }; };
    check(differs(run(set(NeptuneModule::DECAY, 0.9f), inL, inR, 1, true)), "Decay changes the sound");
    check(differs(run(set(NeptuneModule::MOD_AMT, 0.9f), inL, inR, 1, true)), "Mod changes the sound");
    check(differs(run(set(NeptuneModule::MOD_RATE, 0.9f), inL, inR, 1, true)), "Mod Rate changes the sound");
    check(differs(run(set(NeptuneModule::LOWPASS, 0.3f), inL, inR, 1, true)), "LowPass changes the sound");
    check(differs(run(set(NeptuneModule::PRE_DELAY, 0.3f), inL, inR, 1, true)), "PreDelay changes the sound");
    check(differs(run(set(NeptuneModule::REV_DELAY_MIX, 0.9f), inL, inR, 1, true)), "Rev/Delay changes the sound");
    check(differs(run(raw(NeptuneModule::ROUTING, 0), inL, inR, 1, true)), "Routing Send differs from Parallel");
    check(differs(run(raw(NeptuneModule::ROUTING, 2), inL, inR, 1, true)), "Routing Series differs from Parallel");
    check(differs(run(raw(NeptuneModule::REVERB_MODE, 0), inL, inR, 1, true)), "Reverb mode Chorus differs from Medium Space");
    check(differs(run(raw(NeptuneModule::REVERB_MODE, 2), inL, inR, 1, true)), "Reverb mode RubiKa differs from Medium Space");
    {
        // Mix 0 passes the dry signal (times the output gain)
        const Out dry = run(set(NeptuneModule::MIX, 0.0f), inL, inR, 1, false);
        double num = 0, den = 0; for (size_t i = 0; i < total; i++) { num += dry.l[i] * inL[i]; den += inL[i] * inL[i]; }
        const double k = num / den;
        double d = 0; for (size_t i = 0; i < total; i++) d = std::fmax(d, std::fabs(dry.l[i] - k * inL[i]));
        printf("  Mix 0: output = %.3f x input, residual %.3g\n", k, d);
        check(d < 1e-5 && k > 0.5 && k < 3.0, "Mix 0 passes the dry signal (times the output gain)");
    }
    {
        // a reverb mode change mutes the output, then the sound comes back
        std::vector<float> tone(total, 0.0f), zero(total, 0.0f);
        for (size_t i = 0; i < total; i++) tone[i] = 0.2f * std::sin(0.05f * i);
        // The change must happen while running: use the module directly
        srand(7); pool_index = 0;
        static unsigned char mem[sizeof(NeptuneModule)];
        std::memset(mem, 0, sizeof mem);
        NeptuneModule *m = new (mem) NeptuneModule();
        m->Init(48000.0f);
        std::vector<float> l(total), r(total);
        for (size_t b = 0; b < total; b += kBlock) {
            if (b == 20016) m->SetParameterRaw(NeptuneModule::REVERB_MODE, 2);
            m->ProcessMonoBlock(&tone[b], &l[b], &r[b], kBlock);
        }
        check(energy(l, 20100, 20900) == 0.0 && energy(r, 20100, 20900) == 0.0, "a change of reverb mode mutes the output for a moment (both channels)");
        check(energy(l, 19000, 20000) > 0.1 && energy(l, 22000, 23000) > 0.1, "the sound is there before and after the change");
    }

    printf("\n%s\n", g_fails ? "NEPTUNE: FAILURES" : "NEPTUNE OK");
    return g_fails != 0;
}
