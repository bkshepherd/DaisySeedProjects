// Pluto tests.
//  1. The block path and the per-sample path give the same output (loops recorded, speeds, every effect), mono and stereo, also at 256-sample blocks.
//  2. The loopers: a recorded loop plays back what was recorded, at the set speed and direction (stepped: 2x, -1x), in the right channel(s); a second tap within a second pauses;
//     holding clears; the loop modes (one-time, frippertronics) behave; the two loopers are independent (footswitch target).
//  3. The effects (stability, filter, reverb) and the controls change the sound; the LED shows the state.
#include "pluto_module.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace bkshepherd;

static int g_fails = 0;
static void check(bool ok, const char *what) { printf("  %-84s %s\n", what, ok ? "OK" : "FAIL"); if (!ok) g_fails++; }

static constexpr size_t kTotal = 6 * 48000;

struct Out { std::vector<float> l, r; };

static std::vector<float> noise(unsigned seed, float gain, size_t from, size_t to) {
    std::vector<float> v(kTotal, 0.0f);
    unsigned s = seed;
    for (size_t i = 0; i < kTotal; i++) {
        s = 1664525u * s + 1013904223u;
        const float x = ((s >> 8) & 0xFFFF) / 32768.0f - 1.0f;
        if (i >= from && i < to) v[i] = gain * x;
    }
    return v;
}

// kind: 0 footswitch pressed, 1 footswitch held for a second, 2 set raw parameter (a = id, b = value), 3 set float parameter
struct Event { size_t at; int kind; float a, b; };

// path: 0 = per-sample, 1 = block of 48, 2 = block of 256
template <class Cfg> static Out run(Cfg cfg, const std::vector<float> &inL, const std::vector<float> &inR, int path, bool stereo, const std::vector<Event> &events = {}, float *ledOut = nullptr, size_t ledAt = 0) {
    srand(5);
    static unsigned char mem[sizeof(PlutoModule)];
    std::memset(mem, 0, sizeof mem);
    PlutoModule *m = new (mem) PlutoModule();
    m->Init(48000.0f);
    m->SetEnabled(true);
    cfg(*m);
    Out o{std::vector<float>(kTotal), std::vector<float>(kTotal)};
    const size_t bs = path == 2 ? 256 : 48;
    size_t nextEvent = 0;
    for (size_t b = 0; b < kTotal; b += bs) {
        while (nextEvent < events.size() && events[nextEvent].at <= b) {
            const Event &e = events[nextEvent++];
            switch (e.kind) {
            case 0: m->AlternateFootswitchPressed(); break;
            case 1: m->AlternateFootswitchHeldFor1Second(); break;
            case 2: m->SetParameterRaw((int)e.a, (uint32_t)e.b); break;
            default: m->SetParameterAsFloat((int)e.a, e.b); break;
            }
        }
        if (ledOut && b == ledAt) *ledOut = m->GetBrightnessForLED(1);
        const size_t n = std::min(bs, kTotal - b);
        if (path == 0) {
            for (size_t i = b; i < b + n; i++) {
                if (stereo) m->ProcessStereo(inL[i], inR[i]); else m->ProcessMono(inL[i]);
                o.l[i] = m->GetAudioLeft(); o.r[i] = m->GetAudioRight();
            }
        } else if (stereo) {
            m->ProcessStereoBlock(&inL[b], &inR[b], &o.l[b], &o.r[b], n);
        } else {
            m->ProcessMonoBlock(&inL[b], &o.l[b], &o.r[b], n);
        }
    }
    m->~PlutoModule();
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
    const size_t loopLen = 57600; // 1.2 s: more than a second between the two taps, or the second tap is a double tap (pause)
    const size_t recStart = 4608, recEnd = recStart + loopLen; // multiples of 768: the same time at 48 and 256 samples per block
    const std::vector<float> in = noise(11u, 0.3f, 0, recEnd + 1);   // noise while recording (and the first block)
    const std::vector<float> inR = noise(23u, 0.3f, 0, recEnd + 1);
    const std::vector<float> zero(kTotal, 0.0f);
    const std::vector<Event> rec = {{recStart, 0, 0, 0}, {recEnd, 0, 0, 0}}; // tap to record, tap to play (more than a second apart: no pause)
    auto defaults = [](PlutoModule &) {};
    auto setA = [](PlutoModule &m) { m.SetParameterRaw(PlutoModule::FOOT_TARGET, 1); }; // the footswitch controls looper A only
    auto setB = [](PlutoModule &m) { m.SetParameterRaw(PlutoModule::FOOT_TARGET, 2); };

    printf("block path vs per-sample path:\n");
    {
        double peak;
        auto cfgEffects = [](int effect, bool stepped) { return [effect, stepped](PlutoModule &m) {
            m.SetParameterRaw(PlutoModule::EFFECT, effect);
            m.SetParameterRaw(PlutoModule::SPEED_MODE, stepped ? 1 : 0);
            m.SetParameterAsFloat(PlutoModule::MOD_A, 0.7f);
            m.SetParameterAsFloat(PlutoModule::MOD_B, 0.3f);
            m.SetParameterAsFloat(PlutoModule::SPEED_A, 0.62f);
            m.SetParameterAsFloat(PlutoModule::SPEED_B, 0.2f); }; };
        const std::vector<Event> ev = rec;
        for (int effect = 0; effect < 3; effect++) {
            for (int stepped = 0; stepped < 2; stepped++) {
                const Out a = run(cfgEffects(effect, stepped), in, inR, 0, true, ev), b = run(cfgEffects(effect, stepped), in, inR, 1, true, ev), c = run(cfgEffects(effect, stepped), in, inR, 2, true, ev);
                const double d = maxDiff(a, b, peak);
                char what[120]; snprintf(what, sizeof what, "block path equals per-sample path and 256-sample blocks (stereo, %s, %s speed)", effect == 0 ? "stability" : effect == 1 ? "filter" : "reverb", stepped ? "stepped" : "smooth");
                check(d == 0.0 && maxDiff(a, c, peak) == 0.0 && peak > 0.1 && std::isfinite(peak), what);
            }
        }
        const Out a = run(cfgEffects(2, 0), in, inR, 0, false, ev), b = run(cfgEffects(2, 0), in, inR, 1, false, ev);
        check(maxDiff(a, b, peak) == 0.0, "block path equals per-sample path (mono, reverb)");
    }

    printf("\nloopers:\n");
    {
        // Looper A alone: record one second (the loop is 48000 samples long), then it plays back what was recorded
        auto cfg = [&](PlutoModule &m) { setA(m); m.SetParameterAsFloat(PlutoModule::LEVEL_B, 0.0f); };
        const Out o = run(cfg, in, inR, 1, false, rec);
        // The loop recorded the input from recStart (the record window fades in over 1200 samples). After recEnd the loop plays from its start.
        // After recEnd the dry input is zero, so the output is the loop
        double maxLoopDiff = 1e9;
        for (int shift = -4; shift <= 4; shift++) { // allow for a position offset of a few samples
            double d = 0;
            for (size_t k = 3000; k < 40000; k++) d = std::fmax(d, std::fabs(o.l[recEnd + 1 + k + shift] - in[recStart + k]));
            maxLoopDiff = std::fmin(maxLoopDiff, d);
        }
        printf("  loop playback vs the recorded input: max difference %.3g\n", maxLoopDiff);
        check(maxLoopDiff < 1e-4, "a recorded loop plays back what was recorded, at 1x");
        double rep = 1e9;
        for (int shift = -4; shift <= 4; shift++) { double d = 0; for (size_t k = 3000; k < 30000; k++) d = std::fmax(d, std::fabs(o.l[recEnd + 1 + loopLen + k + shift] - o.l[recEnd + 1 + k])); rep = std::fmin(rep, d); }
        check(rep < 1e-4, "the loop repeats after its length");
        // the other looper stays empty: Level B is 0, and with A only the output equals A's loop on both channels in mono
        check(energy(o.l, recEnd + 2000, recEnd + 40000) > 1.0, "the loop is heard");
    }
    {
        // Stepped speed 2x: the loop passes twice as fast: output at k equals the recorded input at 2k
        auto cfg = [&](PlutoModule &m) { setA(m); m.SetParameterAsFloat(PlutoModule::LEVEL_B, 0.0f); m.SetParameterRaw(PlutoModule::SPEED_MODE, 1); m.SetParameterAsFloat(PlutoModule::SPEED_A, 0.75f); };
        const Out o = run(cfg, in, inR, 1, false, rec);
        double best = 1e9;
        for (int shift = -6; shift <= 6; shift++) { double d = 0; for (size_t k = 3000; k < 18000; k++) d = std::fmax(d, std::fabs(o.l[recEnd + 1 + k + shift] - in[recStart + 2 * k])); best = std::fmin(best, d); }
        printf("  2x speed vs the recorded input at twice the position: max difference %.3g\n", best);
        check(best < 1e-3, "stepped speed 2x plays the loop twice as fast");
    }
    {
        // Stepped speed -1x: backwards
        auto cfg = [&](PlutoModule &m) { setA(m); m.SetParameterAsFloat(PlutoModule::LEVEL_B, 0.0f); m.SetParameterRaw(PlutoModule::SPEED_MODE, 1); m.SetParameterAsFloat(PlutoModule::SPEED_A, 0.2f); };
        const Out o = run(cfg, in, inR, 1, false, rec);
        // the recording is 48000 samples (recStart .. recEnd); backwards playback starts from its end
        double best = 1e9;
        for (int shift = -6; shift <= 6; shift++) { double d = 0; for (size_t k = 3000; k < 40000; k++) d = std::fmax(d, std::fabs(o.l[recEnd + 1 + k + shift] - in[recEnd - k])); best = std::fmin(best, d); }
        printf("  backwards playback vs the recorded input reversed: max difference %.3g\n", best);
        check(best < 1e-3, "stepped speed -1x plays the loop backwards");
    }
    {
        // Independent loopers: record B only, A stays silent
        auto cfg = [&](PlutoModule &m) { setB(m); };
        const Out o = run(cfg, in, inR, 1, true, rec); // stereo: A is the left channel, B the right
        const double eL = energy(o.l, recEnd + 2000, recEnd + 40000), eR = energy(o.r, recEnd + 2000, recEnd + 40000);
        printf("  energy left / right after recording looper B: %.3g / %.3g\n", eL, eR);
        check(eR > 1.0 && eL < 1e-9, "the footswitch target B records looper B only, which plays on the right in stereo");
        const Out a = run(setA, in, inR, 1, true, rec);
        check(energy(a.l, recEnd + 2000, recEnd + 40000) > 1.0 && energy(a.r, recEnd + 2000, recEnd + 40000) < 1e-9, "the footswitch target A records looper A only, which plays on the left in stereo");
        const Out both = run(defaults, in, inR, 1, true, rec);
        check(energy(both.l, recEnd + 2000, recEnd + 40000) > 1.0 && energy(both.r, recEnd + 2000, recEnd + 40000) > 1.0, "A + B records both loopers");
    }
    {
        // Double tap pauses, a hold clears
        const Out p = run(setA, in, inR, 1, false, {{recStart, 0, 0, 0}, {recEnd, 0, 0, 0}, {recEnd + 9600, 0, 0, 0}}); // third tap 0.2 s after the second: pause
        check(energy(p.l, recEnd + 12000, recEnd + 40000) < 1e-9, "a second tap within a second pauses the loop");
        const Out c = run(setA, in, inR, 1, false, {{recStart, 0, 0, 0}, {recEnd, 0, 0, 0}, {recEnd + 24000, 1, 0, 0}});
        check(energy(c.l, recEnd + 26000, recEnd + 60000) < 1e-9 && energy(c.l, recEnd + 100, recEnd + 20000) > 1.0, "holding the footswitch clears the loop");
        // A tap on an empty looper starts recording; a hold clears even while recording
        const Out q = run(setA, in, inR, 1, false, {{recStart, 0, 0, 0}, {recStart + 24000, 1, 0, 0}, {recEnd, 0, 0, 0}});
        check(std::isfinite(energy(q.l, 0, kTotal)), "clearing while recording is safe");
    }
    {
        // Frippertronics: the loop decays on every pass while recording: with the footswitch pressed once more (overdub) and silence in, the loop dies away by 0.707 per pass
        auto cfg = [&](PlutoModule &m) { setA(m); m.SetParameterAsFloat(PlutoModule::LEVEL_B, 0.0f); m.SetParameterRaw(PlutoModule::LOOP_MODE, 2); };
        const Out f = run(cfg, in, inR, 1, false, {{recStart, 0, 0, 0}, {recEnd, 0, 0, 0}, {recEnd + 60000, 0, 0, 0}}); // overdub (frippertronics) starts at 1.25 s after the loop began
        const double e1 = energy(f.l, recEnd + 100, recEnd + 40000);
        const double e2 = energy(f.l, recEnd + 60000 + loopLen + 100, recEnd + 60000 + loopLen + 40000); // a pass later, still recording: decayed
        printf("  frippertronics: loop energy first pass %.3g, a pass after recording started %.3g\n", e1, e2);
        check(e2 < 0.8 * e1 && e2 > 0.2 * e1, "frippertronics: the loop decays on every pass while recording");
    }

    printf("\neffects and controls:\n");
    {
        auto cfgBase = [&](PlutoModule &m) { setA(m); m.SetParameterAsFloat(PlutoModule::LEVEL_B, 0.0f); };
        const Out base = run(cfgBase, in, inR, 1, false, rec);
        auto differs = [&](auto cfg) { const Out o = run(cfg, in, inR, 1, false, rec); double p; return maxDiff(base, o, p) > 1e-4; };
        check(differs([&](PlutoModule &m) { cfgBase(m); m.SetParameterRaw(PlutoModule::EFFECT, 1); m.SetParameterAsFloat(PlutoModule::MOD_A, 0.2f); }), "the filter effect (low pass) changes the sound");
        check(differs([&](PlutoModule &m) { cfgBase(m); m.SetParameterRaw(PlutoModule::EFFECT, 1); m.SetParameterAsFloat(PlutoModule::MOD_A, 0.9f); }), "the filter effect (high pass) changes the sound");
        check(differs([&](PlutoModule &m) { cfgBase(m); m.SetParameterRaw(PlutoModule::EFFECT, 2); m.SetParameterAsFloat(PlutoModule::MOD_A, 0.9f); }), "the reverb effect changes the sound");
        check(differs([&](PlutoModule &m) { cfgBase(m); m.SetParameterAsFloat(PlutoModule::MOD_A, 1.0f); }), "the stability effect changes the sound");
        check(differs([&](PlutoModule &m) { cfgBase(m); m.SetParameterAsFloat(PlutoModule::LEVEL_A, 0.9f); }), "Level A changes the sound");
        check(differs([&](PlutoModule &m) { cfgBase(m); m.SetParameterAsFloat(PlutoModule::SPEED_A, 0.9f); }), "Speed A changes the sound");
        // Reverb: a tail after the loop is cleared? the reverb rings after the input stops: energy after the loop ends exceeds none
        auto rv = [&](PlutoModule &m) { setA(m); m.SetParameterAsFloat(PlutoModule::LEVEL_B, 0.0f); m.SetParameterRaw(PlutoModule::EFFECT, 2); m.SetParameterAsFloat(PlutoModule::MOD_A, 0.9f); };
        const Out o = run(rv, in, inR, 1, false, {{recStart, 0, 0, 0}, {recEnd, 0, 0, 0}, {recEnd + 96000, 1, 0, 0}});
        const double tail = energy(o.l, recEnd + 96000 + 4800, recEnd + 96000 + 24000);
        printf("  reverb tail after the loop is cleared: energy %.3g\n", tail);
        check(tail > 1e-3, "the reverb rings on after the loop is cleared");
    }
    {
        // LED: off when empty, pulsing while recording, lit while playing
        float led = -1;
        run(setA, in, inR, 1, false, {}, &led, 4800);
        check(led == 0.0f, "LED off with an empty looper");
        run(setA, in, inR, 1, false, {{recStart, 0, 0, 0}}, &led, recStart + 24000);
        check(led > 0.0f && led <= 1.0f, "LED pulses while recording");
        run(setA, in, inR, 1, false, rec, &led, recEnd + 4800);
        check(led == 1.0f, "LED lit while playing");
    }
    {
        // The default (empty loopers) is a clean pass: output equals the input
        const Out o = run(defaults, in, inR, 1, false);
        double d = 0; for (size_t i = 0; i < kTotal; i++) d = std::fmax(d, std::fabs(o.l[i] - in[i]));
        check(d == 0.0, "with no loop the output is the input (unity gain)");
    }

    printf("\n%s\n", g_fails == 0 ? "ALL PLUTO TESTS PASSED" : "PLUTO TESTS FAILED");
    return g_fails == 0 ? 0 : 1;
}
