// Earth tests: the reverb's SDRAM arena is big enough, the octave produces an octave, the freeze holds the reverb tail, the overdrive swell works, the
// output stays finite, and the block path equals the per-sample path.
#include "earth_module.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

using namespace bkshepherd;

static int g_fails = 0;
static void check(bool ok, const char *what) { printf("  %-74s %s\n", what, ok ? "OK" : "FAIL"); if (!ok) g_fails++; }

static constexpr size_t kBlock = 48;
static constexpr float kPi = 3.14159265f;

struct Run {
    std::vector<float> l, r;
};

// A fresh module with the given settings; plays `signal(i)` for `seconds`, optionally pressing the footswitch for a stretch, returns the stereo output.
template <class Signal>
static Run play(const std::vector<std::pair<int, float>> &floatParams, const std::vector<std::pair<int, uint32_t>> &rawParams, double seconds, Signal signal,
                bool blockPath = true, double pressFrom = -1, double pressTo = -1) {
    static unsigned char mem[sizeof(EarthModule)];
    std::memset(mem, 0, sizeof mem);
    EarthModule *m = new (mem) EarthModule();
    for (auto &p : floatParams) m->SetParameterAsFloat(p.first, p.second); // before Init(), so the smoothed values start at these settings
    for (auto &p : rawParams) m->SetParameterRaw(p.first, p.second);
    m->Init(48000.0f);
    const size_t total = (size_t)(seconds * 48000);
    std::vector<float> in(total);
    for (size_t i = 0; i < total; i++) in[i] = signal(i);
    Run out;
    out.l.resize(total);
    out.r.resize(total);
    bool pressed = false;
    for (size_t o = 0; o < total; o += kBlock) {
        const double t = o / 48000.0;
        if (pressFrom >= 0 && !pressed && t >= pressFrom && t < pressTo) { m->AlternateFootswitchPressed(); pressed = true; }
        if (pressed && t >= pressTo) { m->AlternateFootswitchReleased(); pressed = false; }
        if (blockPath) m->ProcessMonoBlock(&in[o], &out.l[o], &out.r[o], kBlock);
        else for (size_t i = 0; i < kBlock; i++) { m->ProcessMono(in[o + i]); out.l[o + i] = m->GetAudioLeft(); out.r[o + i] = m->GetAudioRight(); }
    }
    m->~EarthModule();
    return out;
}

static double rms(const std::vector<float> &v, size_t a, size_t b) {
    double s = 0;
    for (size_t i = a; i < b && i < v.size(); i++) s += double(v[i]) * v[i];
    return std::sqrt(s / (b - a));
}

// magnitude of the DFT of v[a..b) at `freq`
static double magAt(const std::vector<float> &v, size_t a, size_t b, double freq) {
    double re = 0, im = 0;
    for (size_t i = a; i < b; i++) { const double w = 2 * kPi * freq * i / 48000.0; re += v[i] * std::cos(w); im -= v[i] * std::sin(w); }
    return std::sqrt(re * re + im * im) / (b - a);
}

static bool finite(const Run &r) {
    for (size_t i = 0; i < r.l.size(); i++) if (!std::isfinite(r.l[i]) || !std::isfinite(r.r[i])) return false;
    return true;
}

int main() {
    auto noiseBurst = [](size_t i) { static unsigned s = 1; s = 1664525u * s + 1013904223u; return i < 4800 ? 0.3f * (((s >> 8) & 0xFFFF) / 32768.0f - 1.0f) : 0.0f; };
    auto tone220 = [](size_t i) { return 0.3f * std::sin(2 * kPi * 220.0f * i / 48000.0f); };
    const std::vector<std::pair<int, float>> none;
    const std::vector<std::pair<int, uint32_t>> noneRaw;

    printf("Reverb memory:\n");
    {
        std::vector<float> arena(524288);
        InterpDelayArena::set(arena.data(), arena.size());
        { auto *d = new Dattorro(48000.0f, 16.0f, 4.0f, 33600); delete d; }
        printf("  arena used by the engine at the largest Size and a 700 ms pre-delay: %zu of %zu floats (%.0f%%)\n", InterpDelayArena::used(), arena.size(),
               100.0 * InterpDelayArena::used() / arena.size());
        check(!InterpDelayArena::exhausted() && InterpDelayArena::used() < arena.size(), "the engine fits in the arena");
        InterpDelayArena::set(nullptr, 0);
    }

    printf("\nReverb:\n");
    {
        const Run r = play({{EarthModule::MIX, 0.8f}, {EarthModule::DECAY, 0.9f}}, noneRaw, 3.0, noiseBurst);
        check(finite(r), "the output is finite");
        check(rms(r.l, 48000, 72000) > 1e-3, "a reverb tail follows the input burst");
        check(std::fabs(rms(r.l, 48000, 72000) - rms(r.r, 48000, 72000)) / rms(r.l, 48000, 72000) < 0.5 && r.l != r.r, "the left and right outputs are different (stereo reverb)");
    }

    printf("\nPre-delay (0 vs 0.5 = 350 ms): energy in the first 200 ms after the burst:\n");
    {
        const Run a = play({{EarthModule::MIX, 1.0f}, {EarthModule::PRE_DELAY, 0.0f}}, noneRaw, 2.0, noiseBurst);
        const Run b = play({{EarthModule::MIX, 1.0f}, {EarthModule::PRE_DELAY, 0.5f}}, noneRaw, 2.0, noiseBurst);
        printf("  no pre-delay: %.4f, 350 ms pre-delay: %.4f\n", rms(a.l, 0, 9600), rms(b.l, 0, 9600));
        check(rms(a.l, 0, 9600) > 5.0 * rms(b.l, 0, 9600), "the pre-delay holds the reverb back");
        check(rms(b.l, 24000, 48000) > 1e-3, "and the reverb arrives after it");
    }

    printf("\nOctave up on a 220 Hz tone (up and down also adds the lower octaves):\n");
    {
        const Run off = play({{EarthModule::MIX, 1.0f}}, {{EarthModule::OCTAVE, 0}}, 2.0, tone220);
        const Run up = play({{EarthModule::MIX, 1.0f}}, {{EarthModule::OCTAVE, 1}}, 2.0, tone220);
        const Run both = play({{EarthModule::MIX, 1.0f}}, {{EarthModule::OCTAVE, 2}}, 2.0, tone220);
        const size_t a = 48000, b = 96000;
        printf("  level at 440 Hz: none %.5f, up %.5f, up+down %.5f;  at 110 Hz: none %.5f, up %.5f, up+down %.5f\n", magAt(off.l, a, b, 440), magAt(up.l, a, b, 440),
               magAt(both.l, a, b, 440), magAt(off.l, a, b, 110), magAt(up.l, a, b, 110), magAt(both.l, a, b, 110));
        check(magAt(up.l, a, b, 440) > 4.0 * magAt(off.l, a, b, 440), "octave up adds a component an octave above");
        check(magAt(both.l, a, b, 110) > 4.0 * magAt(up.l, a, b, 110), "octave up and down adds a component an octave below");
        check(finite(up) && finite(both), "the output is finite");
    }

    printf("\nFreeze (secondary footswitch, FS Action = Freeze), burst then silence, footswitch held from 0.6 s to 3.0 s:\n");
    {
        const std::vector<std::pair<int, float>> p = {{EarthModule::MIX, 1.0f}, {EarthModule::DECAY, 0.6f}};
        const Run free = play(p, {{EarthModule::FS_ACTION, 0}}, 5.0, noiseBurst);
        const Run frozen = play(p, {{EarthModule::FS_ACTION, 0}}, 5.0, noiseBurst, true, 0.6, 3.0);
        printf("  tail level between 2.4 s and 2.9 s: not frozen %.5f, frozen %.5f\n", rms(free.l, 115200, 139200), rms(frozen.l, 115200, 139200));
        check(rms(frozen.l, 115200, 139200) > 5.0 * rms(free.l, 115200, 139200), "freezing holds the tail instead of letting it decay");
        check(rms(frozen.l, 192000, 240000) < rms(frozen.l, 115200, 139200), "and the tail decays again after release");
        check(finite(frozen), "the output is finite");
    }

    printf("\nOverdrive swell (FS Action = Overdrive), held for 3 s over a steady tone:\n");
    {
        const std::vector<std::pair<int, float>> p = {{EarthModule::MIX, 1.0f}, {EarthModule::DECAY, 0.8f}};
        const Run base = play(p, {{EarthModule::FS_ACTION, 1}}, 6.0, tone220);
        const Run od = play(p, {{EarthModule::FS_ACTION, 1}}, 6.0, tone220, true, 1.0, 4.0);
        const double harmonicsBase = magAt(base.l, 192000, 240000, 660) + magAt(base.l, 192000, 240000, 880);
        const double harmonicsOd = magAt(od.l, 192000, 240000, 660) + magAt(od.l, 192000, 240000, 880);
        printf("  level of the 3rd and 4th harmonics at 4.0 s..5.0 s: without %.6f, with the swell held %.6f\n", harmonicsBase, harmonicsOd);
        check(finite(od), "the output is finite");
        check(rms(od.l, 144000, 192000) > 1e-3 && rms(od.l, 144000, 192000) < 5.0, "the swell stays at a sensible level");
    }

    printf("\nBlock path vs per-sample path (settings that do not change over time):\n");
    for (int variant = 0; variant < 3; variant++) {
        std::vector<std::pair<int, float>> p = {{EarthModule::MIX, 0.6f}};
        std::vector<std::pair<int, uint32_t>> raw;
        const char *names[] = {"defaults", "octave up and down, big, no input diffusion", "octave up, small, octave-only"};
        if (variant == 1) { raw = {{EarthModule::OCTAVE, 2}, {EarthModule::SIZE, 2}, {EarthModule::IN_DIFFUSION, 0}}; p.push_back({EarthModule::MOD_DEPTH, 0.7f}); }
        if (variant == 2) { raw = {{EarthModule::OCTAVE, 1}, {EarthModule::SIZE, 0}, {EarthModule::OCT_ONLY, 1}}; p.push_back({EarthModule::FILTER, 0.6f}); }
        const Run a = play(p, raw, 2.0, tone220, true), b = play(p, raw, 2.0, tone220, false);
        double d = 0, peak = 0;
        for (size_t i = 0; i < a.l.size(); i++) { d = std::fmax(d, std::fabs(a.l[i] - b.l[i])); d = std::fmax(d, std::fabs(a.r[i] - b.r[i])); peak = std::fmax(peak, std::fabs(a.l[i])); }
        printf("  %-46s max difference %.3g, peak %.3f\n", names[variant], d, peak);
        check(d == 0.0 && peak > 0.01, "block path equals per-sample path");
    }

    printf("\n%s\n", g_fails ? "EARTH: FAILURES" : "EARTH OK");
    return g_fails != 0;
}
