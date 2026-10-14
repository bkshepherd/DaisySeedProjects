// Venus tests.
//  1. SlicedFourier + SpectralReverb must produce exactly what the one-shot soundmath::Fourier produces with the same processing, delayed by the
//     extra `stride` samples (bit-exact), for several parameter sets. This is what makes it safe to spread each frame's work over many blocks.
//  2. The sliced pipeline is driven the way the module drives it (kStepsPerHop stages per hop, by sample count), at 48- and 256-sample blocks, and the audio
//     is the same for both block sizes.
//  3. VenusModule: the block path and the per-sample path give the same output (constant parameters).
//  4. Informational: the worst per-block time of the sliced pipeline against the one-shot one (host speed, so only the ratio means something).
#include "venus_module.h"
#include "Venus/cmsis_fft.h"
#include "../../Util/STFT/fourier.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace venus;
using namespace soundmath;

static int g_fails = 0;
static void check(bool ok, const char *what) { printf("  %-70s %s\n", what, ok ? "OK" : "FAIL"); if (!ok) g_fails++; }

static constexpr size_t kBlock = 48;
static constexpr size_t kSlotsSliced = 2 * kLaps + 1;
// Must match kStepsPerHop in venus_module.cpp: the stages are issued at 3/2 of the rate they need
static constexpr int kStepsPerHop = (int)((kChunks + 2) * 3 / 2);
static_assert(kStepsPerHop >= (int)(kChunks + 2), "enough stage steps per hop");

static Wave<float> g_hann([](float phase) -> float { return 0.5f * (1.0f - std::cos(2.0f * 3.14159265358979f * phase)); });

static SpectralReverb *g_ref;
static void refProcessor(const float *in, float *out) {
    SpectralReverb::BeginFrame(g_ref);
    g_ref->ProcessBins(in, out, 0, kBins);
}

static std::vector<float> makeInput(size_t total) {
    std::vector<float> v(total);
    unsigned s = 987654321u;
    for (size_t i = 0; i < total; i++) {
        s = 1664525u * s + 1013904223u;
        const float noise = ((s >> 8) & 0xFFFF) / 32768.0f - 1.0f;
        v[i] = (i < 24000 ? 0.3f * noise : 0.0f) + 0.2f * std::sin(2.0f * 3.14159265f * 220.0f * i / 48000.0f) * (i < 60000 ? 1.0f : 0.0f);
    }
    return v;
}

enum class Run { OneShot, SyncStages, Sliced };

// Runs a pipeline over the test input and returns its output; `worstMs` receives the slowest block. With a second parameter set, it takes effect at
// sample `switchAt` (chosen so that every frame sees the same parameters in all three runs).
static uint32_t g_lateFinishes = 0; // frames that were not finished in time in the last Sliced run
static std::vector<float> runPipeline(Run kind, const FrameParams &p0, const FrameParams *p1, size_t switchAt, double &worstMs, size_t blockSize = kBlock) {
    const size_t total = 48000 * 3;
    const std::vector<float> in = makeInput(total);
    std::vector<float> output(total);
    static SpectralReverb rev;
    rev.Init();
    rev.SetParams(p0);
    worstMs = 0;
    auto timed = [&](auto &&body) {
        const auto t0 = std::chrono::steady_clock::now();
        body();
        worstMs = std::fmax(worstMs, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    };
    ShyFFT<float, kN, RotationPhasor> fft;
    fft.Init();
    if (kind == Run::OneShot) {
        g_ref = &rev;
        std::vector<float> bi(kN * kLaps * 2, 0.0f), bm(kN * kLaps * 2, 0.0f), bo(kN * kLaps * 2, 0.0f);
        Fourier<float, kN> stft(refProcessor, &fft, &g_hann, kLaps, bi.data(), bm.data(), bo.data());
        for (size_t o = 0; o < total; o += blockSize)
            timed([&] { for (size_t i = o; i < std::min(o + blockSize, total); i++) { if (p1 && i == switchAt) rev.SetParams(*p1); stft.write(in[i]); output[i] = stft.read(); } });
    } else {
        std::vector<float> bi(kN * kSlotsSliced, 0.0f), bm(kN, 0.0f), bo(kN, 0.0f); // the work buffers are shared by all slots
        SlicedFourier<float, kN> stft(&SpectralReverb::ProcessChunk, &SpectralReverb::BeginFrame, &rev, kChunks, &fft, &g_hann, kLaps, bi.data(), bm.data(), bo.data());
        int stepAccum = 0;
        for (size_t o = 0; o < total; o += blockSize)
            timed([&] {
                if (kind == Run::Sliced) {
                    stepAccum += (int)blockSize * kStepsPerHop; // the module's schedule, by sample count
                    while (stepAccum >= (int)(kN / kLaps)) { stepAccum -= (int)(kN / kLaps); stft.Step(); }
                }
                for (size_t i = o; i < std::min(o + blockSize, total); i++) {
                    if (p1 && i == switchAt) rev.SetParams(*p1);
                    output[i] = stft.Process(in[i]);
                    if (kind == Run::SyncStages) while (stft.Busy()) stft.Step(); // all stages straight away, as if there were no deadline
                }
            });
        g_lateFinishes = stft.lateFinishes();
    }
    return output;
}

static double maxDiff(const std::vector<float> &a, size_t offsetA, const std::vector<float> &b, size_t offsetB, size_t count, double &peak) {
    double d = 0;
    peak = 0;
    for (size_t t = 0; t < count; t++) {
        d = std::fmax(d, std::fabs(a[offsetA + t] - b[offsetB + t]));
        peak = std::fmax(peak, std::fabs(a[offsetA + t]));
    }
    return d;
}

int main() {
    constexpr size_t kStride = kN / kLaps;
    constexpr size_t kTotal = 48000 * 3;
    // Frame k completes at sample kN - 1 + k * kStride. A parameter change halfway between two completions, at least a block after the last one, is seen
    // by the same frames in every run.
    constexpr size_t kSwitchAt = kN - 1 + 10 * kStride + kStride / 2;
    struct Cfg { const char *name; FrameParams p0; bool hasSwitch; FrameParams p1; };
    std::vector<Cfg> cfgs;
    { FrameParams p; p.decayFactor = 1.0f / 50.0f; p.damp = 0.4f; cfgs.push_back({"plain reverb", p, false, {}}); }
    { FrameParams p; p.decayFactor = 1.0f / 80.0f; p.damp = 0.8f; p.shimmerMode = 1; p.shimmerDouble = 0.05f; p.shimmerTriple = 0.02f; p.shimmerRemainder = 0.93f; cfgs.push_back({"shimmer up", p, false, {}}); }
    { FrameParams p; p.decayFactor = 1.0f / 80.0f; p.damp = 0.8f; p.shimmerMode = 0; p.shimmerDouble = 0.05f; p.shimmerRemainder = 0.95f; cfgs.push_back({"shimmer down", p, false, {}}); }
    { FrameParams p; p.decayFactor = 1.0f / 60.0f; p.damp = 0.6f; p.detuneMode = 2; p.detuneMultiplier = 1; p.detuneDouble = 0.2f; p.detuneRemainder = 0.8f; cfgs.push_back({"detune up", p, false, {}}); }
    { FrameParams p; p.decayFactor = 1.0f / 60.0f; p.damp = 0.6f; p.detuneMode = 0; p.detuneMultiplier = -1; p.detuneDouble = 0.2f; p.detuneRemainder = 0.8f; cfgs.push_back({"detune down", p, false, {}}); }
    { FrameParams p; p.decayFactor = 1.0f / 60.0f; p.damp = 0.6f; FrameParams q = p; q.freeze = true; cfgs.push_back({"freeze (switched on)", p, true, q}); }
    { FrameParams p; p.decayFactor = 1.0f / 40.0f; p.damp = 0.5f; FrameParams q = p; q.decayFactor = 1.0f / 150.0f; q.damp = 0.9f; q.shimmerDouble = 0.04f; q.shimmerRemainder = 0.96f; cfgs.push_back({"parameters switched", p, true, q}); }

    printf("Spreading the stages over blocks changes nothing (stages run straight away vs one stage per block, every frame, exact):\n");
    double worstOneShot = 0, worstSliced = 0;
    for (auto &c : cfgs) {
        double w1, w2, w3, peak;
        const FrameParams *p1 = c.hasSwitch ? &c.p1 : nullptr;
        const auto sync = runPipeline(Run::SyncStages, c.p0, p1, kSwitchAt, w1);
        const auto sliced = runPipeline(Run::Sliced, c.p0, p1, kSwitchAt, w2);
        check(g_lateFinishes == 0, "48-sample blocks: every frame was finished by the staged schedule (no fallback inside Process)");
        const double d = maxDiff(sync, 0, sliced, 0, kTotal, peak);
        printf("  %-22s max difference %.3g\n", c.name, d);
        check(d == 0.0, "sliced output equals the run with all stages immediately, exactly");
        check(peak > 1e-3 && std::isfinite(peak), "the output is not silent and is finite");
        worstSliced = std::fmax(worstSliced, w2);
        double w4;
        const auto sliced256 = runPipeline(Run::Sliced, c.p0, p1, kSwitchAt, w4, 256);
        check(maxDiff(sync, 0, sliced256, 0, kTotal, peak) == 0.0 && g_lateFinishes == 0, "256-sample blocks: the same audio, and no frame left unfinished");

        if (!c.hasSwitch) {
            // The original Fourier, whole frames at once: identical up to its ninth frame (see sliced_fourier.h), with the sliced one a hop later
            const auto oneShot = runPipeline(Run::OneShot, c.p0, nullptr, 0, w3);
            const size_t exactUntil = kN - 1 + 8 * kStride - 2; // the original's ninth frame completes here and drifts one sample from ours
            double peak2;
            const double d2 = maxDiff(oneShot, 0, sliced, kStride, exactUntil, peak2);
            printf("  %-22s vs original Fourier, first %zu samples: max difference %.3g\n", "", exactUntil, d2);
            check(d2 == 0.0 && peak2 > 1e-3, "matches the original Fourier (delayed by a hop) before its slot reuse");
            worstOneShot = std::fmax(worstOneShot, w3);
        }
    }
    printf("\nWorst block time on this host: one-shot Fourier %.3f ms, sliced %.3f ms (ratio %.1fx; host speed, only the ratio means something)\n",
           worstOneShot, worstSliced, worstOneShot / std::fmax(worstSliced, 1e-9));

    // The module's FFT (CmsisRealFft, here on the host's stand-in for CMSIS) instead of ShyFFT in the same pipeline: the same audio up to rounding
    printf("\nCmsisRealFft instead of ShyFFT in the sliced pipeline (the real CMSIS library is checked in test_cmsis_fft.cpp):\n");
    for (auto &c : cfgs) {
        if (c.hasSwitch) continue;
        double w, peak;
        const auto shy = runPipeline(Run::Sliced, c.p0, nullptr, 0, w);
        static SpectralReverb rev2;
        rev2.Init(); rev2.SetParams(c.p0);
        static CmsisRealFft<kN> cfft; cfft.Init();
        std::vector<float> bi(kN * kSlotsSliced, 0.0f), bm(kN, 0.0f), bo(kN, 0.0f), out2(kTotal), in = makeInput(kTotal);
        SlicedFourier<float, kN, CmsisRealFft<kN>> stft(&SpectralReverb::ProcessChunk, &SpectralReverb::BeginFrame, &rev2, kChunks, &cfft, &g_hann, kLaps, bi.data(), bm.data(), bo.data());
        int stepAccum = 0;
        for (size_t o = 0; o < kTotal; o += kBlock) {
            stepAccum += (int)kBlock * kStepsPerHop;
            while (stepAccum >= (int)kStride) { stepAccum -= (int)kStride; stft.Step(); }
            for (size_t i = o; i < o + kBlock; i++) out2[i] = stft.Process(in[i]);
        }
        const double d = maxDiff(shy, 0, out2, 0, kTotal, peak);
        printf("  %-22s max difference %.3g (peak %.3f)\n", c.name, d, peak);
        check(d < 2e-4 * peak && peak > 1e-3 && stft.lateFinishes() == 0, "same audio as with ShyFFT up to rounding");
    }

    printf("\nVenusModule: block path vs per-sample path:\n");
    for (int variant = 0; variant < 3; variant++) {
        auto build = [&](int mode, bool blockPath) {
            static unsigned char mem[sizeof(bkshepherd::VenusModule)];
            std::memset(mem, 0, sizeof mem);
            bkshepherd::VenusModule *m = new (mem) bkshepherd::VenusModule();
            m->Init(48000.0f);
            if (mode == 1) { m->SetParameterAsFloat(bkshepherd::VenusModule::SHIMMER, 0.8f); m->SetParameterAsFloat(bkshepherd::VenusModule::SHIMMER_TONE, 0.6f); m->SetParameterAsFloat(bkshepherd::VenusModule::DETUNE, 0.9f); }
            if (mode == 2) { m->SetParameterRaw(bkshepherd::VenusModule::LOFI, 2); m->SetParameterRaw(bkshepherd::VenusModule::SHIMMER_MODE, 0); m->SetParameterAsFloat(bkshepherd::VenusModule::DECAY, 0.8f); }
            const size_t total = 48000 * 2;
            std::vector<float> inp = makeInput(total), l(total), r(total);
            for (size_t o = 0; o < total; o += kBlock) {
                if (blockPath) m->ProcessMonoBlock(&inp[o], &l[o], &r[o], kBlock);
                else for (size_t i = 0; i < kBlock; i++) { m->ProcessMono(inp[o + i]); l[o + i] = m->GetAudioLeft(); r[o + i] = m->GetAudioRight(); }
            }
            m->~VenusModule();
            return l;
        };
        const auto a = build(variant, true), b = build(variant, false);
        double d = 0, peak = 0;
        for (size_t i = 0; i < a.size(); i++) { d = std::fmax(d, std::fabs(a[i] - b[i])); peak = std::fmax(peak, std::fabs(a[i])); }
        const char *names[] = {"defaults", "shimmer + detune", "lofi heavy, octave down, long decay"};
        printf("  %-36s max difference %.3g, peak %.3f\n", names[variant], d, peak);
        check(d == 0.0 && peak > 0.01, "block path equals per-sample path");
    }

    printf("\n%s\n", g_fails ? "VENUS: FAILURES" : "VENUS OK");
    return g_fails != 0;
}
