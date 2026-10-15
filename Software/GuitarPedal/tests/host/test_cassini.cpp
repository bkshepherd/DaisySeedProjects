// Cassini tests.
//  1. The block path and the per-sample path give the same output (a recorded sample played by several keys, the looper, trims, banks).
//  2. The sampler: holding the footswitch records the bank's sample; a key plays it at its own pitch (middle C = unchanged, an octave up = twice the frequency), one-shot or looped,
//     trimmed, backwards when the start is after the end; the banks are separate; the velocity and the release.
//  3. The looper (footswitch mode Looper): records what the keys play and loops it after the keys are released; the filter and reverb change the sound.
#include "cassini_module.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace bkshepherd;

static int g_fails = 0;
static void check(bool ok, const char *what) { printf("  %-84s %s\n", what, ok ? "OK" : "FAIL"); if (!ok) g_fails++; }

static constexpr size_t kTotal = 8 * 48000;
static constexpr float kPi = 3.14159265358979f;

struct Out { std::vector<float> l, r; };

static std::vector<float> sine(float freq, float gain, size_t from, size_t to) {
    std::vector<float> v(kTotal, 0.0f);
    for (size_t i = from; i < to && i < kTotal; i++) v[i] = gain * std::sin(2.0f * kPi * freq * (float)(i - from) / 48000.0f);
    return v;
}

// kind: 0 footswitch pressed, 1 released, 2 held for a second, 3 note on (a = note, b = velocity), 4 note off, 5 set raw parameter (a = id, b = value), 6 set float parameter
struct Event { size_t at; int kind; float a, b; };

template <class Cfg> static Out run(Cfg cfg, const std::vector<float> &in, int path, const std::vector<Event> &events = {}, float *ledOut = nullptr, size_t ledAt = 0) {
    srand(5);
    static unsigned char mem[sizeof(CassiniModule)];
    std::memset(mem, 0, sizeof mem);
    CassiniModule *m = new (mem) CassiniModule();
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
            case 1: m->AlternateFootswitchReleased(); break;
            case 2: m->AlternateFootswitchHeldFor1Second(); break;
            case 3: m->OnNoteOn(e.a, e.b); break;
            case 4: m->OnNoteOff(e.a, e.b); break;
            case 5: m->SetParameterRaw((int)e.a, (uint32_t)e.b); break;
            default: m->SetParameterAsFloat((int)e.a, e.b); break;
            }
        }
        if (ledOut && b == ledAt) *ledOut = m->GetBrightnessForLED(1);
        const size_t n = std::min(bs, kTotal - b);
        if (path == 0) {
            for (size_t i = b; i < b + n; i++) { m->ProcessMono(in[i]); o.l[i] = m->GetAudioLeft(); o.r[i] = m->GetAudioRight(); }
        } else {
            m->ProcessMonoBlock(&in[b], &o.l[b], &o.r[b], n);
        }
    }
    m->~CassiniModule();
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
// the frequency from the positive-going zero crossings
static double freq(const std::vector<float> &v, size_t from, size_t to) {
    int c = 0;
    for (size_t i = from + 1; i < to; i++) if (v[i - 1] < 0.0f && v[i] >= 0.0f) c++;
    return c * 48000.0 / (double)(to - from);
}

int main() {
    // A 220 Hz sine is recorded for a second (the footswitch is held), at 4608 .. 4608 + 48000 (multiples of 768 again)
    const size_t recLen = 57600;                                // 1.2 s
    const size_t recStart = 4608, recEnd = recStart + recLen;
    const size_t noteAt = 69888;                                 // multiples of 768 everywhere: the same time at 48 and 256 samples per block
    const std::vector<float> in = sine(220.0f, 0.3f, recStart, recEnd);
    const std::vector<Event> rec = {{recStart, 0, 0, 0}, {recEnd, 1, 0, 0}};
    auto noReverb = [](CassiniModule &m) { m.SetParameterAsFloat(CassiniModule::REVERB, 0.0f); };
    auto withKeys = [](std::vector<Event> e, size_t at, float note) { e.push_back({at, 3, note, 127}); return e; };

    printf("block path vs per-sample path:\n");
    {
        std::vector<Event> ev = rec;
        ev.push_back({noteAt, 3, 60, 127}); ev.push_back({noteAt, 3, 67, 100}); ev.push_back({noteAt + 38400, 3, 76, 90}); ev.push_back({noteAt + 88320, 4, 60, 0});
        auto cfg = [](CassiniModule &m) { m.SetParameterAsFloat(CassiniModule::TRIM_START, 0.1f); m.SetParameterAsFloat(CassiniModule::TRIM_END, 0.8f); m.SetParameterRaw(CassiniModule::SPEED_MODE, 1); };
        double peak;
        const Out a = run(cfg, in, 0, ev), b = run(cfg, in, 1, ev), c = run(cfg, in, 2, ev);
        const double d = maxDiff(a, b, peak);
        printf("  max difference %.3g, peak %.3f\n", d, peak);
        check(d == 0.0 && peak > 0.1 && std::isfinite(peak), "block path equals per-sample path");
        check(maxDiff(a, c, peak) == 0.0, "256-sample blocks give the same audio");
        // with the looper on the footswitch
        std::vector<Event> ev2 = {{4608, 5, CassiniModule::FOOT_MODE, 1}, {6144, 0, 0, 0}, {noteAt, 0, 0, 0}};
        ev2.push_back({768, 3, 64, 127}); ev2.push_back({30720, 4, 64, 0});
        const std::vector<Event> evl = {{0, 5, CassiniModule::FOOT_MODE, 1}, {768, 3, 64, 127}, {768, 3, 67, 127}, {6144, 0, 0, 0}, {30720, 4, 64, 0}, {30720, 4, 67, 0}, {noteAt, 0, 0, 0}};
        auto cfgL = [](CassiniModule &m) { m.SetParameterRaw(CassiniModule::SPEED_MODE, 0); m.SetParameterAsFloat(CassiniModule::LOOP_SPEED, 0.7f); };
        const Out l0 = run(cfgL, in, 0, evl), l1 = run(cfgL, in, 1, evl);
        check(maxDiff(l0, l1, peak) == 0.0, "block path equals per-sample path (looper)");
    }

    printf("\nthe sampler:\n");
    {
        // Nothing recorded: the keys are silent, the output is the input
        const Out o = run(noReverb, in, 1, {{recEnd + 4608, 3, 60, 127}});
        double d = 0; for (size_t i = 0; i < kTotal; i++) d = std::fmax(d, std::fabs(o.l[i] - in[i]));
        check(d == 0.0, "with an empty bank the keys are silent");
        // A held footswitch records: the bank has the sine; middle C plays it unchanged (220 Hz), an octave up at twice the frequency, an octave down at half
        const size_t from = noteAt + 24000, to = from + 48000;
        for (float note : {48.0f, 60.0f, 72.0f}) {
            std::vector<Event> ev = rec; ev.push_back({noteAt, 3, note, 127});
            const Out p = run(noReverb, in, 1, ev);
            const double f = freq(p.l, from, to), expect = 220.0 * std::pow(2.0, (note - 60.0) / 12.0);
            printf("  note %.0f: %.1f Hz (expected %.1f)\n", note, f, expect);
            check(std::fabs(f - expect) < 0.03 * expect, "a key plays the sample at its own pitch");
        }
    }
    {
        // One-shot plays the sample once: it ends after a second; looped repeats while the key is held
        auto oneShot = [](CassiniModule &m) { m.SetParameterAsFloat(CassiniModule::REVERB, 0.0f); m.SetParameterRaw(CassiniModule::SAMPLE_MODE, 0); };
        std::vector<Event> ev = rec; ev.push_back({noteAt, 3, 60, 127});
        const Out o1 = run(oneShot, in, 1, ev), o2 = run(noReverb, in, 1, ev);
        const double e1 = energy(o1.l, noteAt + 20000, noteAt + 40000), e1late = energy(o1.l, noteAt + 80000, noteAt + 100000);
        const double e2late = energy(o2.l, noteAt + 80000, noteAt + 100000);
        printf("  one-shot: energy while the sample plays %.3g, after it ended %.3g; looped, after %.3g\n", e1, e1late, e2late);
        check(e1 > 1.0 && e1late < 1e-3 * e1 && e2late > 0.5 * e1, "one-shot plays once, looped repeats");
    }
    {
        // Trim: the start at 0.5 and the end at 1.0 plays only the second half; the end before the start plays backwards (same pitch measured)
        std::vector<Event> ev = rec; ev.push_back({noteAt, 3, 60, 127});
        auto half = [](CassiniModule &m) { m.SetParameterAsFloat(CassiniModule::REVERB, 0.0f); m.SetParameterRaw(CassiniModule::SAMPLE_MODE, 0); m.SetParameterAsFloat(CassiniModule::TRIM_START, 0.5f); };
        const Out h = run(half, in, 1, ev);
        // the half plays for 0.5 s (plus the attack of the envelope) and then stops
        const double eEarly = energy(h.l, noteAt + 10000, noteAt + 20000), eLate = energy(h.l, noteAt + 40000, noteAt + 60000);
        check(eEarly > 1.0 && eLate < 1e-3 * eEarly, "Trim Start 0.5 plays only the second half (one-shot)");
        auto back = [](CassiniModule &m) { m.SetParameterAsFloat(CassiniModule::REVERB, 0.0f); m.SetParameterAsFloat(CassiniModule::TRIM_START, 1.0f); m.SetParameterAsFloat(CassiniModule::TRIM_END, 0.0f); };
        const Out bk = run(back, in, 1, ev);
        const double f = freq(bk.l, noteAt + 24000, noteAt + 72000);
        printf("  backwards: %.1f Hz\n", f);
        check(std::fabs(f - 220.0) < 8.0 && energy(bk.l, noteAt + 24000, noteAt + 72000) > 1.0, "Trim Start after Trim End plays backwards");
    }
    {
        // Banks are separate: record into bank 1, switch to bank 2 (silent), back to bank 1 (sound)
        std::vector<Event> ev = rec; ev.push_back({noteAt, 5, CassiniModule::BANK, 1}); ev.push_back({noteAt, 3, 60, 127});
        const Out b2 = run(noReverb, in, 1, ev);
        check(energy(b2.l, noteAt + 24000, noteAt + 48000) < 1e-9, "bank 2 is empty while bank 1 has the sample");
        std::vector<Event> ev3 = {{recStart, 5, CassiniModule::BANK, 2}, {recStart, 0, 0, 0}, {recEnd, 1, 0, 0}, {recEnd + 768, 5, CassiniModule::BANK, 0}, {noteAt, 3, 60, 127}};
        const Out b1 = run(noReverb, in, 1, ev3);
        check(energy(b1.l, noteAt + 24000, noteAt + 48000) < 1e-9, "a sample recorded into bank 3 is not heard from bank 1");
        std::vector<Event> ev4 = {{recStart, 5, CassiniModule::BANK, 2}, {recStart, 0, 0, 0}, {recEnd, 1, 0, 0}, {noteAt, 3, 60, 127}};
        const Out b3 = run(noReverb, in, 1, ev4);
        check(energy(b3.l, noteAt + 24000, noteAt + 48000) > 1.0, "...and heard from bank 3");
    }
    {
        // Velocity and release: a quiet key is quieter; after the release the sound dies away
        std::vector<Event> ev = rec; ev.push_back({noteAt, 3, 60, 127});
        std::vector<Event> evq = rec; evq.push_back({noteAt, 3, 60, 30});
        const Out loud = run(noReverb, in, 1, ev), quiet = run(noReverb, in, 1, evq);
        const double el = energy(loud.l, noteAt + 24000, noteAt + 48000), eq = energy(quiet.l, noteAt + 24000, noteAt + 48000);
        check(el > 8.0 * eq, "a lower velocity is quieter");
        ev.push_back({noteAt + 38400, 4, 60, 0});
        const Out rel = run(noReverb, in, 1, ev);
        printf("  after the release: %.3g (while playing %.3g)\n", energy(rel.l, noteAt + 38400 + 48000, noteAt + 38400 + 96000), el);
        check(energy(rel.l, noteAt + 38400 + 144000, noteAt + 38400 + 192000) < 1e-4 * el, "the sound dies away after the key is released");
    }

    printf("\nthe looper, filter, reverb:\n");
    {
        // Looper footswitch mode: tap to record while a key plays, release the key, tap to loop: the loop goes on
        const std::vector<Event> ev = {{recStart, 0, 0, 0}, {recEnd, 1, 0, 0}, {noteAt, 3, 60, 127}, {99840, 0, 0, 0}, {99840 - 1536, 5, CassiniModule::FOOT_MODE, 1}};
        auto cfg = noReverb;
        // record the sample first (Sample mode), then switch to Looper mode, tap at 99840 - 1536 + ... : build by hand
        const std::vector<Event> evl = {{recStart, 0, 0, 0}, {recEnd, 1, 0, 0}, {noteAt, 5, CassiniModule::FOOT_MODE, 1}, {noteAt + 768, 3, 60, 127}, {noteAt + 768, 0, 0, 0}, // tap: start the loop
                                        {noteAt + 768 + 72960, 4, 60, 0}, {noteAt + 768 + 76800, 0, 0, 0}};                                                            // key up, tap (1.6 s after the first): play
        (void)ev;
        const Out o = run(cfg, in, 1, evl);
        const size_t end = noteAt + 768 + 76800;
        const double during = energy(o.l, end - 20000, end - 5000);
        const double after = energy(o.l, end + 3 * 48000 + 20000, end + 3 * 48000 + 35000); // long after the key is released: only the loop
        printf("  energy while the key plays %.3g, in the loop much later %.3g\n", during, after);
        check(during > 1.0 && after > 0.1 * during, "the looper keeps playing what was played after the key is released");
        float led = -1;
        run(cfg, in, 1, evl, &led, end + 4608);
        check(led == 1.0f, "LED lit while the loop plays");
        float led2 = -1;
        run(cfg, in, 1, {{0, 0, 0, 0}}, &led2, 24000);
        check(led2 > 0.0f, "LED shows the recording of the sample");
    }
    {
        std::vector<Event> ev = rec; ev.push_back({noteAt, 3, 60, 127});
        const Out base = run(noReverb, in, 1, ev);
        auto differs = [&](auto cfg) { const Out o = run(cfg, in, 1, ev); double p; return maxDiff(base, o, p) > 1e-4; };
        check(differs([](CassiniModule &m) { m.SetParameterAsFloat(CassiniModule::REVERB, 0.9f); }), "Reverb changes the sound");
        check(differs([&](CassiniModule &m) { noReverb(m); m.SetParameterAsFloat(CassiniModule::FILTER, 0.1f); }), "the filter (low pass) changes the sound");
        check(differs([&](CassiniModule &m) { noReverb(m); m.SetParameterAsFloat(CassiniModule::FILTER, 0.9f); }), "the filter (high pass) changes the sound");
        check(differs([&](CassiniModule &m) { noReverb(m); m.SetParameterAsFloat(CassiniModule::LEVEL, 0.9f); }), "Level changes the sound");
        // the filter: a low pass at 0.1 (510 Hz) takes a 5 kHz tone down a lot
        const std::vector<Event> evh = rec;
        const std::vector<float> high = sine(5000.0f, 0.3f, recStart, recEnd);
        std::vector<Event> evh2 = evh; evh2.push_back({noteAt, 3, 60, 127});
        auto lp = [&](CassiniModule &m) { noReverb(m); m.SetParameterAsFloat(CassiniModule::FILTER, 0.1f); };
        const Out open = run(noReverb, high, 1, evh2), closed = run(lp, high, 1, evh2);
        const double eOpen = energy(open.l, noteAt + 24000, noteAt + 48000), eClosed = energy(closed.l, noteAt + 24000, noteAt + 48000);
        printf("  5 kHz sample: open %.3g, low pass at 0.1: %.3g\n", eOpen, eClosed);
        check(eClosed < 0.1 * eOpen && eOpen > 1.0, "the low pass takes a 5 kHz tone down");
    }

    printf("\n%s\n", g_fails == 0 ? "ALL CASSINI TESTS PASSED" : "CASSINI TESTS FAILED");
    return g_fails == 0 ? 0 : 1;
}
