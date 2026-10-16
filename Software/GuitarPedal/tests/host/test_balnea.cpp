// Balnea tests.
//  1. The block path and the per-sample path give the same output (defaults; every option on, with taps and a MIDI note), mono and stereo, also at 256-sample blocks.
//  2. The reverse delay line (a port without modulo divisions) gives the same output as the original by Adam Fulford.
//  3. The controls do something; the delay time is what it says; tap tempo sets the time, a held tap self-oscillates; a MIDI note is heard and dies away.
#include "Balnea/balnea_dsp.h"
#include "balnea/delayline_reverse_original.h"
#include "balnea_module.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace bkshepherd;

static int g_fails = 0;
static void check(bool ok, const char *what) { printf("  %-84s %s\n", what, ok ? "OK" : "FAIL"); if (!ok) g_fails++; }

static constexpr size_t kTotal = 192000;

struct Out { std::vector<float> l, r; };

static std::vector<float> noiseBurst(unsigned seed, float gain, size_t burstEnd = 12000) {
    std::vector<float> v(kTotal, 0.0f);
    unsigned s = seed;
    for (size_t i = 0; i < kTotal; i++) {
        s = 1664525u * s + 1013904223u;
        const float noise = ((s >> 8) & 0xFFFF) / 32768.0f - 1.0f;
        v[i] = i < burstEnd ? gain * (noise + 0.5f * std::sin(0.03f * i)) : 0.0f;
    }
    return v;
}

// Events happen before the block that starts at `at`. kind: 0 note on (a = note, b = velocity), 1 note off, 2 set raw parameter (a = id, b = value),
// 3 set float parameter, 4 footswitch down, 5 footswitch up
struct Event { size_t at; int kind; float a, b; };

// path: 0 = per-sample, 1 = block of 48, 2 = block of 256; stereo uses the second input
template <class Cfg> static Out run(Cfg cfg, const std::vector<float> &inL, const std::vector<float> &inR, int path, bool stereo, const std::vector<Event> &events = {}) {
    static unsigned char mem[sizeof(BalneaModule)];
    std::memset(mem, 0, sizeof mem);
    BalneaModule *m = new (mem) BalneaModule();
    m->Init(48000.0f);
    cfg(*m);
    Out o{std::vector<float>(kTotal), std::vector<float>(kTotal)};
    const size_t bs = path == 2 ? 256 : 48;
    size_t nextEvent = 0;
    for (size_t b = 0; b < kTotal; b += bs) {
        while (nextEvent < events.size() && events[nextEvent].at <= b) {
            const Event &e = events[nextEvent++];
            switch (e.kind) {
            case 0: m->OnNoteOn(e.a, e.b); break;
            case 1: m->OnNoteOff(e.a, e.b); break;
            case 2: m->SetParameterRaw((int)e.a, (uint32_t)e.b); break;
            case 3: m->SetParameterAsFloat((int)e.a, e.b); break;
            case 4: m->AlternateFootswitchPressed(); break;
            default: m->AlternateFootswitchReleased(); break;
            }
        }
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
    m->~BalneaModule();
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

// The reverse line against the original: the same writes must give the same reads
static void reverseLineTest() {
    printf("reverse delay line vs the original:\n");
    constexpr size_t kSize = 8192;
    static balnea::DelayLineReverse mine;
    static float buf[kSize];
    static DelayLineReverse<float, kSize> orig;
    mine.Init(buf, kSize);
    orig.Init();
    unsigned s = 99;
    double maxd = 0;
    float delays[4] = {5000.0f, 3000.0f, 2600.0f, 4000.0f};
    for (size_t i = 0; i < 60000; i++) {
        if (i % 7500 == 0) { mine.SetDelay1(delays[(i / 7500) % 4]); orig.SetDelay1(delays[(i / 7500) % 4]); }
        s = 1664525u * s + 1013904223u;
        const float x = ((s >> 8) & 0xFFFF) / 32768.0f - 1.0f;
        mine.Write(x);
        orig.Write(x);
        maxd = std::fmax(maxd, std::fabs(mine.ReadRev() - orig.ReadRev()));
    }
    printf("  max difference %.3g\n", maxd);
    check(maxd == 0.0, "the reverse line equals the original");
}

int main() {
    const std::vector<float> in = noiseBurst(4242u, 0.3f);
    const std::vector<float> inR = noiseBurst(777u, 0.3f);
    const std::vector<float> zero(kTotal, 0.0f);
    auto defaults = [](BalneaModule &) {};
    auto allOptions = [](BalneaModule &m) {
        m.SetParameterAsFloat(BalneaModule::INT1, 0.8f);
        m.SetParameterAsFloat(BalneaModule::INT2, 0.2f);
        m.SetParameterAsFloat(BalneaModule::MOD_RATE, 0.5f);
        m.SetParameterAsFloat(BalneaModule::MOD_DEPTH, 0.5f);
        m.SetParameterAsFloat(BalneaModule::GLIDE, 0.5f);
        m.SetParameterRaw(BalneaModule::SCALE, 1);
        m.SetParameterRaw(BalneaModule::INT1_STAGE, 1);
        m.SetParameterRaw(BalneaModule::REVERSE, 1);
    };
    const std::vector<Event> taps = {{4608, 4, 0, 0}, {5376, 5, 0, 0}, {19968, 4, 0, 0}, {20736, 5, 0, 0}, {33792, 4, 0, 0}, {34560, 5, 0, 0}, {39936, 0, 60, 100}, {59904, 1, 60, 0}, {90624, 4, 0, 0}, {110592, 5, 0, 0}}; // multiples of 768: the same time at 48 and 256 samples per block

    printf("block path vs per-sample path:\n");
    {
        double peak;
        const Out a = run(defaults, in, inR, 0, false), b = run(defaults, in, inR, 1, false), c = run(defaults, in, inR, 2, false);
        double d = maxDiff(a, b, peak);
        printf("  defaults, mono: max difference %.3g, peak %.3f\n", d, peak);
        check(d == 0.0 && peak > 0.05 && std::isfinite(peak), "block path equals per-sample path (defaults, mono)");
        check(maxDiff(a, c, peak) == 0.0, "256-sample blocks give the same audio (defaults, mono)");
        const Out s0 = run(defaults, in, inR, 0, true), s1 = run(defaults, in, inR, 1, true);
        check(maxDiff(s0, s1, peak) == 0.0, "block path equals per-sample path (defaults, stereo)");
        double dl = 0; for (size_t i = 0; i < kTotal; i++) dl = std::fmax(dl, std::fabs(s1.l[i] - s1.r[i]));
        check(dl > 0.01, "the two inputs of a stereo signal stay apart");
        const Out o0 = run(allOptions, in, inR, 0, true, taps), o1 = run(allOptions, in, inR, 1, true, taps), o2 = run(allOptions, in, inR, 2, false, taps);
        d = maxDiff(o0, o1, peak);
        printf("  all options, stereo, taps, a note: max difference %.3g, peak %.3f\n", d, peak);
        check(d == 0.0 && peak > 0.05 && std::isfinite(peak), "block path equals per-sample path (reverse, modulation, glide, scale, taps, a note)");
        const Out o3 = run(allOptions, in, inR, 0, false, taps);
        check(maxDiff(o3, o2, peak) == 0.0, "256-sample blocks give the same audio (all options, mono)");
    }
    printf("\n");
    reverseLineTest();

    printf("\ncontrols:\n");
    const Out base = run(defaults, in, inR, 1, false);
    auto differs = [&](const Out &o) { double p; return maxDiff(base, o, p) > 1e-4; };
    auto set = [](int p, float v) { return [p, v](BalneaModule &m) { m.SetParameterAsFloat(p, v); }; };
    auto raw = [](int p, int v) { return [p, v](BalneaModule &m) { m.SetParameterRaw(p, v); }; };
    check(differs(run(set(BalneaModule::MIX, 0.9f), in, inR, 1, false)), "Mix changes the sound");
    check(differs(run(set(BalneaModule::TONE, 0.3f), in, inR, 1, false)), "Tone changes the sound");
    check(differs(run(set(BalneaModule::REGEN, 0.9f), in, inR, 1, false)), "Regen changes the sound");
    check(differs(run(set(BalneaModule::TIME, 1500.0f), in, inR, 1, false)), "Time changes the sound");
    check(differs(run(set(BalneaModule::INT1, 0.9f), in, inR, 1, false)), "Interval 1 changes the sound (second stage)");
    check(differs(run(set(BalneaModule::INT2, 0.1f), in, inR, 1, false)), "Interval 2 changes the sound (third stage)");
    check(differs(run(raw(BalneaModule::SCALE, 2), in, inR, 1, false)) || differs(run([](BalneaModule &m) { m.SetParameterAsFloat(BalneaModule::INT1, 0.8f); m.SetParameterRaw(BalneaModule::SCALE, 2); }, in, inR, 1, false)), "Scale changes the intervals");
    check(differs(run(raw(BalneaModule::REVERSE, 1), in, inR, 1, false)), "Reverse changes the sound");
    check(differs(run([](BalneaModule &m) { m.SetParameterAsFloat(BalneaModule::MOD_DEPTH, 0.6f); m.SetParameterAsFloat(BalneaModule::MOD_RATE, 0.6f); }, in, inR, 1, false)), "Mod Depth changes the sound");

    printf("\nthe delay time:\n");
    {
        // Mix 1: only the repeat. The first repeat starts after about the delay time, for several times (the delay line has a few lengths, the speed makes up the rest)
        for (float ms : {250.0f, 500.0f, 1000.0f, 2000.0f, 3500.0f}) {
            auto cfg = [ms](BalneaModule &m) { m.SetParameterAsFloat(BalneaModule::MIX, 1.0f); m.SetParameterAsFloat(BalneaModule::REGEN, 0.0f); m.SetParameterAsFloat(BalneaModule::TIME, ms); };
            const Out o = run(cfg, in, inR, 1, false);
            double mx = 0; for (size_t i = 0; i < kTotal; i++) mx = std::fmax(mx, std::fabs(o.l[i]));
            size_t first = 0; for (size_t i = 0; i < kTotal; i++) if (std::fabs(o.l[i]) > 0.1 * mx) { first = i; break; }
            const double expect = ms * 48.0;
            printf("  Time %.0f ms: the repeat starts at sample %zu (expected about %.0f)\n", ms, first, expect);
            check(std::fabs((double)first - expect) < 0.12 * expect + 600, "the repeat starts after the delay time");
        }
    }

    printf("\ntap tempo, self-oscillation, MIDI:\n");
    {
        // three taps set the time from the last two: the first interval (300 ms here) is dropped, the second (501 ms) is used
        BalneaModule *m = new BalneaModule();
        m->Init(48000.0f);
        float o1[48], o2[48];
        const float zero48[48] = {0};
        auto tap = [&](int blocksBefore) { for (int i = 0; i < blocksBefore; i++) m->ProcessMonoBlock(zero48, o1, o2, 48); m->AlternateFootswitchPressed(); m->ProcessMonoBlock(zero48, o1, o2, 48); m->AlternateFootswitchReleased(); };
        const float defaultTime = m->GetParameterAsFloat(BalneaModule::TIME);
        tap(100);
        tap(299);
        check(m->GetParameterAsFloat(BalneaModule::TIME) == defaultTime, "the first two taps do not change the delay time");
        tap(500);
        // the second and third tap were 501 blocks apart: 24048 samples = 501 ms
        printf("  Time after three taps, the last two 24048 samples apart: %.1f ms\n", m->GetParameterAsFloat(BalneaModule::TIME));
        check(std::fabs(m->GetParameterAsFloat(BalneaModule::TIME) - 501.0f) < 2.0f, "three taps set the delay time from the last two");
        // a tap after a pause starts a new series: it does not change the time
        const float setTime = m->GetParameterAsFloat(BalneaModule::TIME);
        tap(6 * 1000);
        tap(200);
        check(m->GetParameterAsFloat(BalneaModule::TIME) == setTime, "after a pause two taps do not change the delay time");
        // a hold of 300+ ms is not a tap
        const float before = m->GetParameterAsFloat(BalneaModule::TIME);
        m->AlternateFootswitchPressed(); for (int i = 0; i < 400; i++) m->ProcessMonoBlock(zero48, o1, o2, 48); m->AlternateFootswitchReleased();
        check(m->GetParameterAsFloat(BalneaModule::TIME) == before, "holding the footswitch is not a tap");
        delete m;

        // Self-oscillation: while the footswitch is held, a burst keeps circulating, so there is more energy late than with the same burst and no hold
        auto cfg = [](BalneaModule &m) { m.SetParameterAsFloat(BalneaModule::MIX, 0.8f); m.SetParameterAsFloat(BalneaModule::REGEN, 0.2f); };
        const Out calm = run(cfg, in, inR, 1, false);
        const Out osc = run(cfg, in, inR, 1, false, {{20000, 4, 0, 0}, {150000, 5, 0, 0}});
        const double eCalm = energy(calm.l, 100000, 150000), eOsc = energy(osc.l, 100000, 150000);
        printf("  energy late in the tail: regen 0.2: %.3g, held: %.3g\n", eCalm, eOsc);
        check(eOsc > 5.0 * eCalm, "holding the footswitch makes the repeats keep going (self-oscillation)");

        // MIDI: a note is heard without any input, and dies away after the release
        auto dryOnly = [](BalneaModule &m) { m.SetParameterAsFloat(BalneaModule::MIX, 0.0f); };
        const Out nt = run(dryOnly, zero, zero, 1, false, {{4800, 0, 69, 100}, {40000, 1, 69, 0}});
        const double eBefore = energy(nt.l, 0, 4800), eDuring = energy(nt.l, 10000, 38000), eAfter = energy(nt.l, 80000, 100000);
        printf("  synth energy before / during / long after the note: %.3g / %.3g / %.3g\n", eBefore, eDuring, eAfter);
        check(eBefore == 0.0 && eDuring > 1e-3 && eAfter < 1e-6 * eDuring, "a MIDI note is heard and dies away after the release");
    }

    printf("\ntone, feedback and stability:\n");
    {
        auto big = [](BalneaModule &m) { m.SetParameterAsFloat(BalneaModule::REGEN, 1.0f); m.SetParameterAsFloat(BalneaModule::MIX, 0.6f); };
        const Out o = run(big, in, inR, 1, false);
        double peak = 0; bool finite = true;
        for (size_t i = 0; i < kTotal; i++) { peak = std::fmax(peak, std::fabs(o.l[i])); finite &= std::isfinite(o.l[i]); }
        printf("  Regen 1.0, 4 seconds: peak %.3f\n", peak);
        check(finite && peak < 4.0, "full feedback stays finite and bounded");
    }

    printf("\n%s\n", g_fails == 0 ? "ALL BALNEA TESTS PASSED" : "BALNEA TESTS FAILED");
    return g_fails == 0 ? 0 : 1;
}
