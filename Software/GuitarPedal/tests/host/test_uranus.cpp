// Uranus tests.
//  1. The block path and the per-sample path give the same output (defaults, the granular synth with a key held, the FM synth with a chord), also at 256-sample blocks.
//  2. The controls do something (size, mix, pitch, feedback, width, speed, grain mode, grain envelope, spread, LFO, hold).
//  3. MIDI: in granular synth mode a key opens the envelope (nothing is heard without one) and releasing it closes it; in FM mode the input is replaced by the synth, the pitch follows
//     the key, two keys are louder than one, and the notes die away after the release; a MIDI CC reaches the parameter mapped to it.
#include "uranus_module.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace bkshepherd;

static int g_fails = 0;
static void check(bool ok, const char *what) { printf("  %-80s %s\n", what, ok ? "OK" : "FAIL"); if (!ok) g_fails++; }

static constexpr size_t kTotal = 96000;

struct Out { std::vector<float> l, r; };

static std::vector<float> noiseBurst(unsigned seed, float gain, size_t burstEnd = 24000) {
    std::vector<float> v(kTotal, 0.0f);
    unsigned s = seed;
    for (size_t i = 0; i < kTotal; i++) {
        s = 1664525u * s + 1013904223u;
        const float noise = ((s >> 8) & 0xFFFF) / 32768.0f - 1.0f;
        v[i] = i < burstEnd ? gain * (noise + 0.5f * std::sin(0.03f * i)) : 0.0f;
    }
    return v;
}

// events: called before the block that starts at `at`
struct Event { size_t at; int kind; float a, b; }; // kind 0 = note on (a = note, b = velocity), 1 = note off, 2 = set raw parameter (a = id, b = value)

// path: 0 = per-sample, 1 = block of 48, 2 = block of 256
template <class Cfg> static Out run(Cfg cfg, const std::vector<float> &in, int path, const std::vector<Event> &events = {}) {
    srand(7); // the grain players pick their start positions with rand()
    static unsigned char mem[sizeof(UranusModule)];
    std::memset(mem, 0, sizeof mem);
    UranusModule *m = new (mem) UranusModule();
    m->Init(48000.0f);
    cfg(*m);
    Out o{std::vector<float>(kTotal), std::vector<float>(kTotal)};
    const size_t bs = path == 2 ? 256 : 48;
    size_t nextEvent = 0;
    for (size_t b = 0; b < kTotal; b += bs) {
        while (nextEvent < events.size() && events[nextEvent].at <= b) {
            const Event &e = events[nextEvent++];
            if (e.kind == 0) m->OnNoteOn(e.a, e.b);
            else if (e.kind == 1) m->OnNoteOff(e.a, e.b);
            else m->SetParameterRaw((int)e.a, (uint32_t)e.b);
        }
        const size_t n = std::min(bs, kTotal - b);
        if (path == 0) {
            for (size_t i = b; i < b + n; i++) { m->ProcessMono(in[i]); o.l[i] = m->GetAudioLeft(); o.r[i] = m->GetAudioRight(); }
        } else {
            m->ProcessMonoBlock(&in[b], &o.l[b], &o.r[b], n);
        }
    }
    m->~UranusModule();
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
    const std::vector<float> in = noiseBurst(4242u, 0.3f);
    const std::vector<float> zero(kTotal, 0.0f);
    auto defaults = [](UranusModule &) {};
    auto synthGranular = [](UranusModule &m) { m.SetParameterRaw(UranusModule::SYNTH_MODE, 1); };
    auto synthFm = [](UranusModule &m) { m.SetParameterRaw(UranusModule::SYNTH_MODE, 2); };

    printf("block path vs per-sample path:\n");
    {
        const Out a = run(defaults, in, 0), b = run(defaults, in, 1), c = run(defaults, in, 2);
        double peak; double d = maxDiff(a, b, peak);
        printf("  defaults: max difference %.3g, peak %.3f\n", d, peak);
        check(d == 0.0 && peak > 0.05 && std::isfinite(peak), "block path equals per-sample path (defaults)");
        check(maxDiff(a, c, peak) == 0.0, "256-sample blocks give the same audio (defaults)");
        const std::vector<Event> keys = {{4800, 0, 60, 100}, {30000, 1, 60, 0}};
        const Out g0 = run(synthGranular, in, 0, keys), g1 = run(synthGranular, in, 1, keys);
        d = maxDiff(g0, g1, peak);
        printf("  granular synth: max difference %.3g, peak %.3f\n", d, peak);
        check(d == 0.0 && peak > 0.05, "block path equals per-sample path (granular synth with a key)");
        const std::vector<Event> chord = {{4800, 0, 60, 100}, {4800, 0, 64, 100}, {4800, 0, 67, 100}, {50000, 1, 64, 0}};
        const Out f0 = run(synthFm, zero, 0, chord), f1 = run(synthFm, zero, 1, chord);
        d = maxDiff(f0, f1, peak);
        printf("  FM synth: max difference %.3g, peak %.3f\n", d, peak);
        check(d == 0.0 && peak > 0.01, "block path equals per-sample path (FM synth with a chord)");
    }

    printf("\ncontrols:\n");
    const Out base = run(defaults, in, 1);
    auto differs = [&](const Out &o) { double p; return maxDiff(base, o, p) > 1e-4; };
    auto set = [](int p, float v) { return [p, v](UranusModule &m) { m.SetParameterAsFloat(p, v); }; };
    auto raw = [](int p, int v) { return [p, v](UranusModule &m) { m.SetParameterRaw(p, v); }; };
    check(differs(run(set(UranusModule::SIZE, 0.8f), in, 1)), "Size changes the sound");
    check(differs(run(set(UranusModule::MIX, 0.9f), in, 1)), "Mix changes the sound");
    check(differs(run(set(UranusModule::PITCH, 0.9f), in, 1)), "Pitch changes the sound");
    check(differs(run(set(UranusModule::FEEDBACK, 0.9f), in, 1)), "Feedback changes the sound");
    check(differs(run(set(UranusModule::WIDTH, 0.9f), in, 1)), "Width changes the sound");
    check(differs(run(set(UranusModule::SPEED, 0.3f), in, 1)), "Speed changes the sound");
    check(differs(run(raw(UranusModule::GRAIN_MODE, 1), in, 1)), "Grain Mode Oct Down differs from Oct Up");
    check(differs(run(raw(UranusModule::GRAIN_MODE, 2), in, 1)), "Grain Mode Up+Down differs from Oct Up");
    check(differs(run(raw(UranusModule::GRAIN_ENV, 1), in, 1)), "Grain Env SlowAtk differs from Cos");
    check(differs(run(raw(UranusModule::GRAIN_ENV, 2), in, 1)), "Grain Env FastAtk differs from Cos");
    check(differs(run(set(UranusModule::SPREAD, 0.9f), in, 1)), "Spread changes the sound");
    check(differs(run([](UranusModule &m) { m.SetParameterAsFloat(UranusModule::LFO_DEPTH, 1.0f); m.SetParameterAsFloat(UranusModule::LFO_RATE, 0.8f); }, in, 1)), "the pitch LFO changes the sound");
    {
        // Mix 0 passes the dry signal (times the output gain)
        const Out dry = run(set(UranusModule::MIX, 0.0f), in, 1);
        double num = 0, den = 0; for (size_t i = 0; i < kTotal; i++) { num += dry.l[i] * in[i]; den += in[i] * in[i]; }
        const double k = num / den;
        double d = 0; for (size_t i = 0; i < kTotal; i++) d = std::fmax(d, std::fabs(dry.l[i] - k * in[i]));
        printf("  Mix 0: output = %.3f x input, residual %.3g\n", k, d);
        check(d < 1e-5 && k > 0.5 && k < 3.0, "Mix 0 passes the dry signal (times the output gain)");
    }
    {
        // Hold: after the burst (24000 samples) the buffer keeps its content, so the grains play on; without hold the buffer is overwritten by silence
        auto wet = [](UranusModule &m) { m.SetParameterAsFloat(UranusModule::MIX, 1.0f); m.SetParameterAsFloat(UranusModule::FEEDBACK, 0.5f); };
        const std::vector<Event> hold = {{24000, 2, (float)UranusModule::HOLD, 1}};
        const Out held = run(wet, in, 1, hold), free = run(wet, in, 1);
        const double eHeld = energy(held.l, 60000, 96000), eFree = energy(free.l, 60000, 96000);
        printf("  late energy with hold %.3g, without %.3g\n", eHeld, eFree);
        check(eHeld > 5.0 * eFree && eHeld > 1.0, "Hold keeps the grains playing after the input stops");
    }

    printf("\nMIDI:\n");
    {
        auto wetSynth = [](UranusModule &m) { m.SetParameterRaw(UranusModule::SYNTH_MODE, 1); m.SetParameterAsFloat(UranusModule::MIX, 1.0f); };
        const std::vector<Event> keys = {{30000, 0, 60, 100}, {60000, 1, 60, 0}};
        const Out o = run(wetSynth, in, 1, keys);
        printf("  granular synth: energy before the key %.3g, while held %.3g, long after release %.3g\n", energy(o.l, 5000, 30000), energy(o.l, 36000, 60000), energy(o.l, 80000, 96000));
        check(energy(o.l, 5000, 30000) == 0.0, "granular synth: nothing is heard until a key is pressed");
        check(energy(o.l, 36000, 60000) > 0.0, "granular synth: the grains are heard while a key is held");
        check(energy(o.l, 80000, 96000) < 1e-3 * energy(o.l, 36000, 60000), "granular synth: and stop after the release");
    }
    {
        // FM: the input is replaced by the synth: loud noise in, silence out without a key
        auto fmWet = [](UranusModule &m) { m.SetParameterRaw(UranusModule::SYNTH_MODE, 2); m.SetParameterAsFloat(UranusModule::MIX, 0.0f); }; // dry only: the synth itself
        const Out silent = run(fmWet, in, 1);
        check(energy(silent.l, 0, kTotal) == 0.0, "FM synth: the audio input is not heard (the synth replaces it)");
        const std::vector<Event> one = {{4800, 0, 69, 100}, {60000, 1, 69, 0}};
        const Out o = run(fmWet, zero, 1, one);
        // pitch: autocorrelation of 0.2 s of the held note, lag of the highest peak between 60 and 400 samples (A4 = 440 Hz is a period of 109 samples)
        int bestLag = 0; double best = -1e30;
        for (int lag = 60; lag < 400; lag++) {
            double c = 0; for (size_t i = 20000; i < 30000; i++) c += o.l[i] * o.l[i + lag];
            if (c > best) { best = c; bestLag = lag; }
        }
        printf("  FM synth: A4 has a period of %d samples (440 Hz = 109)\n", bestLag);
        check(std::abs(bestLag - 109) <= 2, "FM synth: the pitch follows the key");
        printf("  FM synth: energy while held %.3g, long after the release %.3g\n", energy(o.l, 20000, 30000), energy(o.l, 80000, 96000));
        check(energy(o.l, 80000, 96000) < 1e-2 * energy(o.l, 20000, 30000) && energy(o.l, 90000, 96000) < energy(o.l, 80000, 86000), "FM synth: the note dies away after the key is released (release 0.2 s)");
        const std::vector<Event> two = {{4800, 0, 69, 100}, {4800, 0, 76, 100}, {60000, 1, 69, 0}, {60000, 1, 76, 0}};
        const Out p = run(fmWet, zero, 1, two);
        check(energy(p.l, 20000, 30000) > 1.3 * energy(o.l, 20000, 30000), "FM synth: two keys are louder than one (polyphony)");
    }
    {
        // a MIDI CC reaches the parameter mapped to it
        static unsigned char mem[sizeof(UranusModule)];
        std::memset(mem, 0, sizeof mem);
        UranusModule *m = new (mem) UranusModule();
        m->Init(48000.0f);
        m->MidiCCValueNotification(14, 127);
        m->MidiCCValueNotification(31, 64);
        m->MidiCCValueNotification(22, 0);
        check(m->GetParameterAsFloat(UranusModule::SIZE) > 0.99f, "CC 14 sets Size");
        check(std::fabs(m->GetParameterAsFloat(UranusModule::ENV_RELEASE) - 64.0f / 127.0f) < 0.01f, "CC 31 sets the envelope release");
        check(m->GetParameterAsFloat(UranusModule::FM_MOD_LEVEL) < 0.01f, "CC 22 sets the FM modulator level");
    }

    printf("\n%s\n", g_fails ? "URANUS: FAILURES" : "URANUS OK");
    return g_fails != 0;
}
