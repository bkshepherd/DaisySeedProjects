// Host-side equivalence test: per-sample path vs block path of the real effect modules.
#include "reverb_module.h"
#include "dattorro_reverb_module.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <vector>

using namespace bkshepherd;

static constexpr size_t kBlock = 48;
static constexpr size_t kBlocks = 600; // 0.6 s at 48 kHz

struct Signal { std::vector<float> l, r; };

static Signal makeInput() {
    Signal s; s.l.resize(kBlock * kBlocks); s.r.resize(kBlock * kBlocks);
    srand(1234);
    for (size_t i = 0; i < s.l.size(); i++) {
        float t = i / 48000.0f;
        float burst = (i < 4800 || (i > 20000 && i < 22000)) ? 0.8f : 0.05f; // loud bursts then quiet noise
        float noise = (rand() / (float)RAND_MAX - 0.5f) * 2.0f;
        s.l[i] = burst * noise + 0.2f * sinf(2 * 3.14159265f * 220.0f * t);
        s.r[i] = burst * ((rand() / (float)RAND_MAX - 0.5f) * 2.0f) + 0.2f * sinf(2 * 3.14159265f * 330.0f * t);
    }
    return s;
}

// Change a parameter at fixed block boundaries so both paths see the same parameter at the same time.
static size_t g_automationShift = 0; // negative control: delay the automation in one path
static void automate(BaseEffectModule &m, size_t block, const std::vector<int> &params) {
    if (block >= g_automationShift && (block - g_automationShift) % 100 == 50) {
        block -= g_automationShift;
        for (size_t k = 0; k < params.size(); k++) {
            float v = ((block / 100 + k) % 2) ? 0.2f + 0.1f * k : 0.9f - 0.1f * k;
            m.SetParameterAsMagnitude(params[k], v);
        }
    }
}

enum PathMode { PER_SAMPLE, BLOCK, BLOCK_INPLACE };

template <class M>
static Signal runPath(PathMode mode, bool stereo, const Signal &in, const std::vector<int> &params) {
    M m;
    m.Init(48000.0f);
    Signal out; out.l.resize(in.l.size()); out.r.resize(in.l.size());
    Signal work = in; // used for the in-place variant
    for (size_t b = 0; b < kBlocks; b++) {
        automate(m, b, params);
        const size_t o = b * kBlock;
        if (mode == PER_SAMPLE) {
            for (size_t i = 0; i < kBlock; i++) {
                if (stereo) m.ProcessStereo(in.l[o + i], in.r[o + i]);
                else m.ProcessMono(in.l[o + i]);
                out.l[o + i] = m.GetAudioLeft();
                out.r[o + i] = m.GetAudioRight();
            }
        } else if (mode == BLOCK) {
            if (stereo) m.ProcessStereoBlock(&in.l[o], &in.r[o], &out.l[o], &out.r[o], kBlock);
            else m.ProcessMonoBlock(&in.l[o], &out.l[o], &out.r[o], kBlock);
        } else {
            if (stereo) m.ProcessStereoBlock(&work.l[o], &work.r[o], &work.l[o], &work.r[o], kBlock);
            else m.ProcessMonoBlock(&work.l[o], &work.l[o], &work.r[o], kBlock);
            if (!stereo) { /* mono writes outL in place, outR to separate buffer */ }
            out.l[o] = 0; // filled below
        }
    }
    if (mode == BLOCK_INPLACE) { out = work; }
    return out;
}

struct Stats { double maxDiff = 0; double peak = 0; bool finite = true; };
static Stats compare(const Signal &a, const Signal &b) {
    Stats s;
    for (size_t i = 0; i < a.l.size(); i++) {
        s.maxDiff = std::max<double>(s.maxDiff, fabs(a.l[i] - b.l[i]));
        s.maxDiff = std::max<double>(s.maxDiff, fabs(a.r[i] - b.r[i]));
        s.peak = std::max<double>(s.peak, std::max(fabs(a.l[i]), fabs(a.r[i])));
        if (!std::isfinite(a.l[i]) || !std::isfinite(b.l[i]) || !std::isfinite(a.r[i]) || !std::isfinite(b.r[i])) s.finite = false;
    }
    return s;
}

template <class M>
static int test(const char *name, const std::vector<int> &params, double tol) {
    int fails = 0;
    Signal in = makeInput();
    for (int stereo = 0; stereo < 2; stereo++) {
        Signal a = runPath<M>(PER_SAMPLE, stereo, in, params);
        Signal b = runPath<M>(BLOCK, stereo, in, params);
        Stats s = compare(a, b);
        bool ok = s.finite && s.peak > 0.01 && s.maxDiff <= tol;
        printf("%-10s %-6s block vs per-sample: maxDiff=%.3g peak=%.3f %s\n", name, stereo ? "stereo" : "mono", s.maxDiff, s.peak, ok ? (s.maxDiff == 0.0 ? "OK (bit-exact)" : "OK (within tolerance)") : "MISMATCH");
        fails += !ok;
        if (stereo) {
            Signal c = runPath<M>(BLOCK_INPLACE, true, in, params);
            Stats s2 = compare(a, c);
            // in-place output for the last sample of each block isn't zeroed, compare everything
            bool ok2 = s2.finite && s2.maxDiff <= tol;
            printf("%-10s %-6s in-place   vs per-sample: maxDiff=%.3g %s\n", name, "stereo", s2.maxDiff, ok2 ? (s2.maxDiff == 0.0 ? "OK (bit-exact)" : "OK (within tolerance)") : "MISMATCH");
            fails += !ok2;
        }
    }
    return fails;
}

int main() {
    int fails = 0;
    // Reverb: tolerance of 1e-6 (about -120 dB) because its block path uses float where the old per-sample path used double for 1.0 - mix
    fails += test<ReverbModule>("Reverb", {ReverbModule::TIME, ReverbModule::DAMP, ReverbModule::MIX}, 1e-6);
    fails += test<DattorroReverbModule>("Dattorro", {DattorroReverbModule::MIX, DattorroReverbModule::DECAY, DattorroReverbModule::TONE}, 0.0);

    // Negative control: automation applied at different times in the two paths must be detected
    {
        Signal in = makeInput();
        std::vector<int> p = {DattorroReverbModule::MIX, DattorroReverbModule::DECAY, DattorroReverbModule::TONE};
        Signal a = runPath<DattorroReverbModule>(PER_SAMPLE, true, in, p);
        g_automationShift = 3;
        Signal b = runPath<DattorroReverbModule>(BLOCK, true, in, p);
        g_automationShift = 0;
        Stats s = compare(a, b);
        bool detected = s.maxDiff > 1e-3;
        printf("negative control (shifted automation): maxDiff=%.3g %s\n", s.maxDiff, detected ? "detected (harness is sensitive)" : "NOT DETECTED - harness is blind");
        fails += !detected;
    }
    printf(fails ? "FAILURES: %d\n" : "ALL OK\n", fails);
    return fails != 0;
}
