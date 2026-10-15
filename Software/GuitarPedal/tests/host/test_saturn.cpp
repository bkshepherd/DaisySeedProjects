// Saturn tests.
//  1. The block path and the per-sample path give the same output (defaults and changed controls, mono and stereo modes).
//  2. The same audio at 48- and at 256-sample blocks (the work is scheduled by sample count).
//  3. Signal path: with the shortest Time, Feedback 0 and Mix 1 a low sine comes out 2.25 x as loud (the transform's own gain of 0.75, as in the original, times the
//     original's constant gain of 3 on the wet signal) after the transform's latency; with Linear Time the sine is delayed by about what the settings say.
//  4. The controls do something (Time, Feedback, Filter, PreDelay, the modes, Mono, Stereo Spread, Drift) and Mono sends the same signal to both outputs.
#include "saturn_module.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace bkshepherd;

static int g_fails = 0;
static void check(bool ok, const char *what) { printf("  %-76s %s\n", what, ok ? "OK" : "FAIL"); if (!ok) g_fails++; }

static constexpr size_t kTotal = 48000 * 2;

struct Out { std::vector<float> l, r; };

static std::vector<float> noiseBurst(unsigned seed, float gain) {
    std::vector<float> v(kTotal, 0.0f);
    unsigned s = seed;
    for (size_t i = 0; i < kTotal; i++) {
        s = 1664525u * s + 1013904223u;
        const float noise = ((s >> 8) & 0xFFFF) / 32768.0f - 1.0f;
        const bool burst = i < 9600 || (i > 40000 && i < 42000);
        v[i] = burst ? gain * (noise + 0.5f * std::sin(0.03f * i)) : 0.0f;
    }
    return v;
}

// path: 0 = per-sample, 1 = block of 48, 2 = block of 256
// `before` sets controls before Init() (so the delays start at those settings), `cfg` after it
template <class Cfg, class Before> static Out run(Cfg cfg, const std::vector<float> &inL, const std::vector<float> &inR, int path, bool stereoInput, Before before) {
    static unsigned char mem[sizeof(SaturnModule)];
    std::memset(mem, 0, sizeof mem);
    SaturnModule *m = new (mem) SaturnModule();
    before(*m);
    m->Init(48000.0f);
    cfg(*m);
    const size_t total = inL.size();
    Out o{std::vector<float>(total), std::vector<float>(total)};
    const size_t bs = path == 2 ? 256 : 48;
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
    m->~SaturnModule();
    return o;
}

template <class Cfg> static Out run(Cfg cfg, const std::vector<float> &inL, const std::vector<float> &inR, int path, bool stereoInput) {
    return run(cfg, inL, inR, path, stereoInput, [](SaturnModule &) {});
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
    const std::vector<float> inL = noiseBurst(4242u, 0.3f), inR = noiseBurst(777u, 0.25f);
    auto defaults = [](SaturnModule &) {};
    auto changed = [](SaturnModule &m) { // everything but Drift, which changes the delay settings over time (see below)
        m.SetParameterAsFloat(SaturnModule::TIME, 0.8f);
        m.SetParameterAsFloat(SaturnModule::FEEDBACK, 0.6f);
        m.SetParameterAsFloat(SaturnModule::FILTER, 0.3f);
        m.SetParameterAsFloat(SaturnModule::PREDELAY, 0.2f);
        m.SetParameterRaw(SaturnModule::TIME_MODE, 2);
        m.SetParameterRaw(SaturnModule::FDBK_MODE, 1);
        m.SetParameterRaw(SaturnModule::STEREO_SPREAD, 1);
        m.SetParameterRaw(SaturnModule::PRE_FDBK, 1);
    };

    printf("block path vs per-sample path:\n");
    for (int v = 0; v < 2; v++) for (int mode = 0; mode < 3; mode++) {
        // mode 0: MISO (mono in, stereo out), 1: true stereo in, 2: mono
        auto cfg = [&](SaturnModule &m) { if (v) changed(m); if (mode == 1) m.SetParameterRaw(SaturnModule::STEREO_IN, 1); if (mode == 2) m.SetParameterRaw(SaturnModule::MONO, 1); };
        const Out a = run(cfg, inL, inR, 0, mode == 1), b = run(cfg, inL, inR, 1, mode == 1);
        double peak; const double d = maxDiff(a, b, peak);
        printf("  %s, %s: max difference %.3g, peak %.3f\n", v ? "changed controls" : "defaults", mode == 0 ? "MISO" : (mode == 1 ? "stereo" : "mono"), d, peak);
        check(d == 0.0 && peak > 0.05 && std::isfinite(peak), "block path equals per-sample path");
    }

    // With Drift the delay settings change over time. The stages of a frame run when the samples reach them, which differs a little between one-sample and 48-sample
    // calls (a frame that completes late in a group of 48 samples is picked up by the same group's stage steps only when the samples come one at a time), so the
    // delay settings a frame sees can differ by a drift step: close, not identical.
    {
        auto drift = [](SaturnModule &m) { m.SetParameterRaw(SaturnModule::DRIFT, 2); m.SetParameterAsFloat(SaturnModule::DRIFT_RATE, 0.5f); };
        const Out a = run(drift, inL, inR, 0, false), b = run(drift, inL, inR, 1, false);
        double peak; const double d = maxDiff(a, b, peak);
        printf("  drift: max difference %.3g, peak %.3f\n", d, peak);
        check(d < 0.02 * peak && peak > 0.05, "with Drift the block path stays close to the per-sample path");
    }

    printf("\n256-sample blocks vs 48-sample blocks:\n");
    {
        const Out a = run(defaults, inL, inR, 1, true), b = run(defaults, inL, inR, 2, true);
        double peak; const double d = maxDiff(a, b, peak);
        printf("  defaults: max difference %.3g\n", d);
        check(d == 0.0 && peak > 0.05, "identical audio at 256-sample blocks (defaults)");
        const Out c = run(changed, inL, inR, 2, true);
        double p2; maxDiff(c, c, p2);
        check(p2 > 0.05 && std::isfinite(p2), "256-sample blocks with changed controls: finite output");
    }

    printf("\nsignal path (440 Hz sine, Mix 1, Feedback 0; Time 0 means the shortest delay, one frame of 256 samples):\n");
    {
        std::vector<float> sine(48000, 0.0f), zero(48000, 0.0f);
        for (size_t i = 0; i < 12000; i++) sine[i] = 0.2f * std::sin(2.0f * 3.14159265f * 440.0f * i / 48000.0f); // 0.25 s of sine, then silence
        auto base = [](SaturnModule &m) { m.SetParameterAsFloat(SaturnModule::MIX, 1.0f); m.SetParameterAsFloat(SaturnModule::FEEDBACK, 0.0f); };
        auto timeZero = [&](SaturnModule &m) { base(m); m.SetParameterAsFloat(SaturnModule::TIME, 0.0f); };
        auto timeHalf = [&](SaturnModule &m) { base(m); m.SetParameterAsFloat(SaturnModule::TIME, 0.5f); };
        const Out z = run([](SaturnModule &) {}, sine, zero, 1, false, timeZero);
        const Out h = run([](SaturnModule &) {}, sine, zero, 1, false, timeHalf);
        // steady-state level of the wet signal relative to the input, over a stretch where the sine is certainly present
        const double rIn = std::sqrt(energy(sine, 4000, 8000) / 4000.0), rZ = std::sqrt(energy(z.l, 7000, 12000) / 5000.0); // the output starts about 3900 samples late
        // the module's fixed output gain, from the dry path (Mix 0)
        const Out dry = run([](SaturnModule &) {}, sine, zero, 1, false, [](SaturnModule &m) { m.SetParameterAsFloat(SaturnModule::MIX, 0.0f); });
        const double outputGain = std::sqrt(energy(dry.l, 4000, 8000) / 4000.0) / rIn;
        printf("  output gain (dry path) %.3f; Time 0: output / input level = %.3f (0.75 from the windowing of the transform x the original's wet gain of 3 x the output gain)\n", outputGain, rZ / rIn);
        check(std::fabs(rZ / rIn - 2.25 * outputGain) < 0.1 * outputGain, "a low sine passes with the transform's gain of 0.75 x the wet gain of 3 (x the output gain)");
        // when does the sine start: the first sample where a 48-sample window of the output exceeds 20% of its steady-state level
        auto onset = [&](const Out &o, double level) { for (size_t i = 0; i + 48 < o.l.size(); i++) if (std::sqrt(energy(o.l, i, i + 48) / 48.0) > 0.5 * level) return (double)i; return -1.0; };
        const double t0 = onset(z, rZ);
        double rH = 0; for (size_t i = 0; i < h.l.size(); i++) rH = std::fmax(rH, std::fabs(h.l[i]));
        const double t1 = onset(h, rH / std::sqrt(2.0));
        // 440 Hz is in bin 9 (46.9 Hz per bin): the Linear delay is Time * 4 s * 9 / 175 = 103 ms at Time 0.5, against 5.3 ms (one frame) at Time 0
        const double expectedMs = (0.5 * 4.0 * 187.5 * 9.0 / 175.0 - 1.0) * 256.0 / 48.0;
        printf("  onset: Time 0 at %.0f samples, Time 0.5 at %.0f samples, difference %.1f ms (expected about %.0f ms)\n", t0, t1, (t1 - t0) / 48.0, expectedMs);
        check(t0 > 500 && t0 < 4000 && std::fabs((t1 - t0) / 48.0 - expectedMs) < 25.0, "Linear time mode delays the sine by about Time x 4 s x bin / 175");
    }

    printf("\ncontrols:\n");
    const Out base = run(defaults, inL, inR, 1, true);
    auto differs = [&](const Out &o) { double p; return maxDiff(base, o, p) > 1e-4; };
    auto set = [](int p, float v) { return [p, v](SaturnModule &m) { m.SetParameterAsFloat(p, v); }; };
    auto raw = [](int p, int v) { return [p, v](SaturnModule &m) { m.SetParameterRaw(p, v); }; };
    check(differs(run(set(SaturnModule::TIME, 0.9f), inL, inR, 1, true)), "Time changes the sound");
    check(differs(run(set(SaturnModule::FEEDBACK, 0.8f), inL, inR, 1, true)), "Feedback changes the sound");
    check(differs(run(set(SaturnModule::FILTER, 0.2f), inL, inR, 1, true)), "Filter (low-pass) changes the sound");
    check(differs(run(set(SaturnModule::FILTER, 0.9f), inL, inR, 1, true)), "Filter (high-pass) changes the sound");
    check(differs(run(set(SaturnModule::PREDELAY, 0.5f), inL, inR, 1, true)), "PreDelay changes the sound");
    check(differs(run(raw(SaturnModule::TIME_MODE, 1), inL, inR, 1, true)), "Time Mode Sine differs from Linear");
    check(differs(run(raw(SaturnModule::TIME_MODE, 2), inL, inR, 1, true)), "Time Mode Random differs from Linear");
    check(differs(run(raw(SaturnModule::FDBK_MODE, 1), inL, inR, 1, true)), "Fdbk Mode Linear differs from Same");
    check(differs(run(raw(SaturnModule::FDBK_MODE, 2), inL, inR, 1, true)), "Fdbk Mode Random differs from Same");
    check(differs(run([](SaturnModule &m) { m.SetParameterRaw(SaturnModule::DRIFT, 0); m.SetParameterAsFloat(SaturnModule::DRIFT_RATE, 1.0f); }, inL, inR, 1, true)), "Drift changes the sound");
    check(differs(run(raw(SaturnModule::STEREO_SPREAD, 1), inL, inR, 1, true)), "Stereo Spread changes the sound");
    {
        const Out noPre = run(set(SaturnModule::MIX, 1.0f), inL, inR, 1, true);
        const Out preFb = run([](SaturnModule &m) { m.SetParameterAsFloat(SaturnModule::MIX, 1.0f); m.SetParameterAsFloat(SaturnModule::PREDELAY, 0.05f); m.SetParameterRaw(SaturnModule::PRE_FDBK, 1); }, inL, inR, 1, true);
        check(energy(preFb.l, 20000, 48000) > energy(noPre.l, 20000, 48000), "PreFdbk leaves more energy in the tail");
    }
    {
        const Out mono = run(raw(SaturnModule::MONO, 1), inL, inR, 1, true);
        double d = 0; for (size_t i = 0; i < mono.l.size(); i++) d = std::fmax(d, std::fabs(mono.l[i] - mono.r[i]));
        check(d == 0.0, "Mono sends the same signal to both outputs");
        check(differs(mono), "Mono changes the sound (twice the bins)");
    }
    {
        // MISO (Stereo In off) ignores the right input
        const Out miso = run(defaults, inL, inR, 1, true);
        std::vector<float> zero(kTotal, 0.0f);
        const Out miso2 = run(defaults, inL, zero, 1, true);
        double p; check(maxDiff(miso, miso2, p) == 0.0, "Stereo In off ignores the right input");
        check(differs(run(raw(SaturnModule::STEREO_IN, 1), inL, inR, 1, true)), "Stereo In on uses the right input");
    }

    printf("\n%s\n", g_fails ? "SATURN: FAILURES" : "SATURN OK");
    return g_fails != 0;
}
