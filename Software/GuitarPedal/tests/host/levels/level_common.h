// Shared helpers for the default-level tests: play a test signal through an effect module with its default parameters and measure the
// output level relative to the input level.
//
// Gain is measured on the steady-state RMS (the last half second of a one second signal), at a few input levels and frequencies, because
// nonlinear effects (distortion, compression, amp models) have a gain that depends on how hard you play.
//
// Two test signals:
//   Sine   - pure tones at 220 Hz and 1 kHz. Simple, good for clean/level effects and distortion.
//   Guitar - a harmonic series (partial k at amplitude 1/k, up to the 16th) with fundamentals of 110 Hz and 330 Hz. Closer to a plucked
//            string; better for amp models and other tone-shaping effects, where two pure tones can differ a lot in gain.
#pragma once
#include "base_effect_module.h"
#include <cmath>
#include <cstdio>
#include <vector>

namespace leveltest {

constexpr float kSampleRate = 48000.0f;
constexpr size_t kBlock = 48;
constexpr size_t kTotal = 48000;       // 1 s
constexpr size_t kMeasureFrom = 24000; // measure the last 0.5 s

enum class Signal { Sine, Guitar };

// Input levels as peak amplitude (full scale = 1.0). The reference level is the one the pass/fail window applies to.
struct InputLevel {
    float peak;
    const char *label;
};
constexpr InputLevel kInputs[] = {{0.06f, "-24 dBFS pk"}, {0.25f, "-12 dBFS pk"}, {0.5f, "-6 dBFS pk"}};
constexpr int kReferenceInput = 1; // -12 dBFS peak: a typical strummed guitar into a line input

inline const float *frequencies(Signal s) {
    static const float sine[2] = {220.0f, 1000.0f};
    static const float guitar[2] = {110.0f, 330.0f};
    return s == Signal::Sine ? sine : guitar;
}
inline const char *signalName(Signal s) { return s == Signal::Sine ? "sine tones" : "guitar-like harmonic series"; }

inline double toDb(double ratio) { return 20.0 * std::log10(ratio > 1e-12 ? ratio : 1e-12); }

// Generates kTotal samples of the signal with the given peak amplitude.
inline std::vector<float> makeSignal(Signal kind, float freq, float peak) {
    std::vector<float> v(kTotal);
    const double w = 2.0 * 3.14159265358979 * freq / kSampleRate;
    const int partials = (kind == Signal::Sine) ? 1 : 16;
    double maxAbs = 0;
    for (size_t i = 0; i < kTotal; i++) {
        double x = 0;
        for (int k = 1; k <= partials; k++) x += std::sin(w * k * double(i)) / k;
        v[i] = float(x);
        maxAbs = std::fmax(maxAbs, std::fabs(x));
    }
    for (auto &x : v) x = float(x / maxAbs) * peak; // peak-normalise so the input level means the same for both signals
    return v;
}

struct Result {
    double rmsGainDb;  // output RMS / input RMS
    double peakGainDb; // output peak / input peak
    double outPeak;    // output peak, full scale = 1.0
};

// Runs `m` (already Init'ed and configured) on the signal, using the stereo block path like the pedal does (left = right). Returns the
// gain measured on the left output.
inline Result measure(bkshepherd::BaseEffectModule &m, Signal kind, float freq, float peak) {
    std::vector<float> in = makeSignal(kind, freq, peak), outL(kTotal), outR(kTotal);
    for (size_t o = 0; o < kTotal; o += kBlock) m.ProcessStereoBlock(&in[o], &in[o], &outL[o], &outR[o], kBlock);
    double si = 0, so = 0, pi = 0, po = 0;
    for (size_t i = kMeasureFrom; i < kTotal; i++) {
        si += double(in[i]) * in[i]; so += double(outL[i]) * outL[i];
        pi = std::fmax(pi, std::fabs(in[i])); po = std::fmax(po, std::fabs(outL[i]));
    }
    const double n = double(kTotal - kMeasureFrom);
    return {toDb(std::sqrt(so / n) / std::sqrt(si / n)), toDb(po / pi), po};
}

// Allowed window for the gain at the reference input level, in dB (RMS).
struct Window {
    double minDb, maxDb, targetDb;
    double toleranceDb; // how far from the target the reference gain may be
};

// Prints a gain table for one configuration. Returns true if the mean RMS gain at the reference level (over the two test frequencies) is
// inside the window and near the target, and the output never reaches full scale at any measured level. With enforce = false the window is only printed as information (always returns true).
template <class Make> // Make: () -> std::unique_ptr<Module> configured with the settings under test (fresh instance per measurement)
bool report(const char *title, Make make, const Window &w, Signal kind = Signal::Sine, bool enforce = true) {
    std::printf("%s [%s]\n", title, signalName(kind));
    const float *freqs = frequencies(kind);
    std::printf("    RMS gain in dB (peak gain in brackets)\n    %-12s", "input");
    for (int fi = 0; fi < 2; fi++) std::printf("  %9.0f Hz     ", freqs[fi]);
    std::printf("\n");
    bool clipped = false;
    double refSum = 0, refMin = 1e9, refMax = -1e9;
    for (int li = 0; li < int(sizeof(kInputs) / sizeof(kInputs[0])); li++) {
        std::printf("    %-12s", kInputs[li].label);
        for (int fi = 0; fi < 2; fi++) {
            auto m = make();
            Result r = measure(*m, kind, freqs[fi], kInputs[li].peak);
            std::printf("  %+7.2f (%+6.2f)", r.rmsGainDb, r.peakGainDb);
            if (r.outPeak >= 1.0) clipped = true;
            if (li == kReferenceInput) {
                refSum += r.rmsGainDb;
                refMin = std::fmin(refMin, r.rmsGainDb);
                refMax = std::fmax(refMax, r.rmsGainDb);
            }
        }
        std::printf("%s\n", li == kReferenceInput ? "   <- reference" : "");
    }
    // The gain of a tone-shaping effect depends on pitch, so the rule is applied to the mean over the two test frequencies.
    const double mean = refSum / 2.0;
    const bool ok = mean >= w.minDb && mean <= w.maxDb && std::fabs(mean - w.targetDb) <= w.toleranceDb;
    std::printf("    reference level: mean %+.2f dB (spread %.1f dB); window %+.1f .. %+.1f dB, target %+.1f +/- %.1f dB  =>  %s%s%s\n", mean,
                refMax - refMin, w.minDb, w.maxDb, w.targetDb, w.toleranceDb, ok ? "OK" : "OUT OF WINDOW",
                clipped ? "; OUTPUT REACHES FULL SCALE" : "", enforce ? "" : " (informational)");
    return !enforce || (ok && !clipped);
}

// Mean RMS gain over the test frequencies at the reference input level, for a quick one-line comparison between configurations.
template <class Make>
double referenceGainDb(Make make, Signal kind = Signal::Sine) {
    double sum = 0;
    const float *freqs = frequencies(kind);
    for (int fi = 0; fi < 2; fi++) {
        auto m = make();
        sum += measure(*m, kind, freqs[fi], kInputs[kReferenceInput].peak).rmsGainDb;
    }
    return sum / 2.0;
}

} // namespace leveltest
