// Tests of the distortion family (Preamp, Screamer, Muff Fuzz, Wavefolder, Exciter, Bass Dist, Octa Fuzz, Tape Sat, Chainsaw):
//  1. the block path and the per-sample path give identical output, for the defaults and for several settings
//  2. the output stays finite and bounded for any parameter setting, also when the parameters are changed while playing
//  3. silence in gives silence out
//  4. each effect does what it claims (tone controls, clean lows, octave, harmonics, saturation)
#include "bass_dist_module.h"
#include "chainsaw_module.h"
#include "exciter_module.h"
#include "muff_fuzz_module.h"
#include "octave_fuzz_module.h"
#include "preamp_module.h"
#include "screamer_module.h"
#include "tape_sat_module.h"
#include "wavefolder_module.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
using namespace bkshepherd;

static int fails = 0;
static void check(bool ok, const char *what) {
    fails += !ok;
    std::printf("  %-100s %s\n", what, ok ? "OK" : "FAILED");
}

static constexpr float kSr = 48000.0f;

template <class M>
static std::unique_ptr<M> make() {
    auto m = std::make_unique<M>();
    m->Init(kSr);
    return m;
}

static std::vector<float> sine(float f, float peak, size_t n) {
    std::vector<float> v(n);
    for (size_t i = 0; i < n; i++) v[i] = peak * std::sin(2.0f * 3.14159265f * f * i / kSr);
    return v;
}

// Runs the stereo block path (48 samples) like the pedal and returns the left output
static std::vector<float> run(BaseEffectModule &m, const std::vector<float> &in, size_t block = 48) {
    std::vector<float> l(in.size()), r(in.size());
    for (size_t o = 0; o < in.size(); o += block) {
        const size_t k = std::min(block, in.size() - o);
        m.ProcessStereoBlock(&in[o], &in[o], &l[o], &r[o], k);
    }
    return l;
}

// The amplitude of the component at frequency f over the second half of the signal (a Goertzel filter / single-bin DFT)
static double amp(const std::vector<float> &x, float f) {
    const size_t from = x.size() / 2, n = x.size() - from;
    double re = 0, im = 0;
    for (size_t i = 0; i < n; i++) {
        const double w = 2.0 * 3.14159265358979 * f * (from + i) / kSr;
        re += x[from + i] * std::cos(w);
        im += x[from + i] * std::sin(w);
    }
    return 2.0 * std::sqrt(re * re + im * im) / n;
}
static double db(double r) { return 20.0 * std::log10(r > 1e-12 ? r : 1e-12); }

static void setFloat(BaseEffectModule &m, int id, float v) { m.SetParameterAsMagnitude(id, v); }

// Random parameter values for any module: floats 0..1, bins within range, bools
static void randomize(BaseEffectModule &m, unsigned &s) {
    auto rnd = [&s] { s = 1664525u * s + 1013904223u; return ((s >> 8) & 0xFFFF) / 65535.0f; };
    for (int i = 0; i < m.GetParameterCount(); i++) {
        switch (m.GetParameterType(i)) {
        case ParameterValueType::Float: m.SetParameterAsMagnitude(i, rnd()); break;
        case ParameterValueType::Bool: m.SetParameterAsBool(i, rnd() > 0.5f); break;
        case ParameterValueType::Binned: m.SetParameterAsBinnedValue(i, 1 + int(rnd() * 0.999f * m.GetParameterBinCount(i))); break;
        default: break;
        }
    }
}

template <class M>
static void generic(const char *name) {
    std::printf("\n%s:\n", name);
    // 1. block path == per-sample path (defaults, then three random settings)
    bool same = true;
    unsigned seed = 12345;
    for (int cfg = 0; cfg < 4; cfg++) {
        auto a = make<M>(), b = make<M>(), c = make<M>();
        if (cfg > 0) {
            unsigned s1 = seed, s2 = seed, s3 = seed;
            randomize(*a, s1); randomize(*b, s2); randomize(*c, s3);
            seed = s1 * 7 + 3;
        }
        std::vector<float> in = sine(220.0f, 0.3f, 48000);
        for (size_t i = 0; i < in.size(); i++) in[i] += 0.2f * std::sin(0.31f * i) * ((i / 4800) % 2);
        std::vector<float> ref(in.size());
        for (size_t i = 0; i < in.size(); i++) { a->ProcessMono(in[i]); ref[i] = a->GetAudioLeft(); }
        std::vector<float> o1 = run(*b, in, 48), o2 = run(*c, in, 37);
        for (size_t i = 0; i < in.size(); i++) same = same && ref[i] == o1[i] && ref[i] == o2[i];
    }
    check(same, "the block path (48 and 37 samples) equals the per-sample path, defaults and random settings");

    // 2. finite and bounded for any setting, with the parameters changed while playing
    bool finite = true;
    double worst = 0;
    seed = 777;
    for (int cfg = 0; cfg < 12; cfg++) {
        auto m = make<M>();
        std::vector<float> loud(kSr / 2), quiet = sine(110.0f, 0.001f, kSr / 4);
        unsigned s = seed;
        for (size_t i = 0; i < loud.size(); i++) { s = 1664525u * s + 1013904223u; loud[i] = 1.2f * (((s >> 8) & 0xFFFF) / 32768.0f - 1.0f); } // full-scale noise
        for (int round = 0; round < 3; round++) {
            randomize(*m, s);
            for (const auto &sig : {loud, quiet, sine(1000.0f, 1.0f, 9600)}) {
                for (float y : run(*m, sig)) {
                    finite = finite && std::isfinite(y);
                    worst = std::max(worst, (double)std::fabs(y));
                }
            }
        }
        seed = s;
    }
    std::printf("  largest output over the random settings: %.2f\n", worst);
    check(finite && worst < 20.0, "the output is finite and bounded (< 20) for random settings, changed while playing");

    // 3. silence in, silence out
    auto m = make<M>();
    std::vector<float> zeros(48000, 0.0f);
    double mx = 0;
    for (float y : run(*m, zeros)) mx = std::max(mx, (double)std::fabs(y));
    check(mx < 1e-4, "silence in gives silence out");
}

int main() {
    generic<PreampModule>("Preamp");
    generic<ScreamerModule>("Screamer");
    generic<MuffFuzzModule>("Muff Fuzz");
    generic<WavefolderModule>("Wavefolder");
    generic<ExciterModule>("Exciter");
    generic<BassDistModule>("Bass Dist");
    generic<OctaveFuzzModule>("Octa Fuzz");
    generic<TapeSatModule>("Tape Sat");
    generic<ChainsawModule>("Chainsaw");

    std::printf("\nBehaviour:\n");
    const size_t N = 48000;

    { // Preamp: the tone stack
        auto gainAt = [&](int param, float v, float f) {
            auto m = make<PreampModule>();
            m->SetParameterRaw(PreampModule::VOICING, 0);
            setFloat(*m, PreampModule::GAIN, 0.0f); // low gain: the tone stack is what is measured
            setFloat(*m, param, v);
            return db(amp(run(*m, sine(f, 0.01f, N)), f) / 0.01);
        };
        check(gainAt(PreampModule::BASS, 1.0f, 100.0f) - gainAt(PreampModule::BASS, 0.5f, 100.0f) > 6.0, "Bass boosts 100 Hz by more than 6 dB");
        check(gainAt(PreampModule::TREBLE, 1.0f, 6000.0f) - gainAt(PreampModule::TREBLE, 0.5f, 6000.0f) > 4.0, "Treble boosts 6 kHz by more than 4 dB");
        check(gainAt(PreampModule::MID, 1.0f, 500.0f) - gainAt(PreampModule::MID, 0.0f, 500.0f) > 10.0, "Mid sweeps 500 Hz over more than 10 dB");
        auto thd = [&](int voicing) {
            auto m = make<PreampModule>();
            m->SetParameterRaw(PreampModule::VOICING, voicing);
            setFloat(*m, PreampModule::GAIN, 0.7f);
            const std::vector<float> y = run(*m, sine(220.0f, 0.05f, N));
            return amp(y, 660.0f) / amp(y, 220.0f); // the 3rd harmonic against the fundamental
        };
        check(thd(2) > thd(0), "the Lead voicing distorts more than Clean at the same Gain");
    }

    { // Screamer: the mid hump (more gain in the mids than in the lows when clipping) and the unclipped clean path
        auto small = [&](float f, float drive) {
            auto m = make<ScreamerModule>();
            setFloat(*m, ScreamerModule::DRIVE, drive);
            return db(amp(run(*m, sine(f, 0.002f, N)), f) / 0.002);
        };
        check(small(1000.0f, 1.0f) - small(100.0f, 1.0f) > 12.0, "at full Drive the mids get more than 12 dB more gain than the lows (the mid hump)");
        const double midRise = small(1000.0f, 1.0f) - small(1000.0f, 0.0f), lowRise = small(100.0f, 1.0f) - small(100.0f, 0.0f);
        std::printf("  Drive 0 to 10 raises 1 kHz by %.1f dB and 100 Hz by %.1f dB\n", midRise, lowRise);
        check(midRise - lowRise > 6.0, "Drive raises the mids more than 6 dB more than the lows (the lows are only boosted by the high-passed path)");
    }

    { // Muff Fuzz: the tone control sweeps from dark to bright
        auto level = [&](float tone, float f) {
            auto m = make<MuffFuzzModule>();
            setFloat(*m, MuffFuzzModule::TONE, tone);
            return amp(run(*m, sine(f, 0.01f, N)), f);
        };
        check(db(level(0.0f, 200.0f) / level(0.0f, 4000.0f)) > 6.0, "Tone at 0 is dark: 200 Hz is more than 6 dB above 4 kHz");
        check(db(level(1.0f, 4000.0f) / level(1.0f, 200.0f)) > 6.0, "Tone at 1 is bright: 4 kHz is more than 6 dB above 200 Hz");
    }

    { // Wavefolder: harmonics with Fold, and a clean pass-through at Mix 0
        auto h3 = [&](float fold) {
            auto m = make<WavefolderModule>();
            setFloat(*m, WavefolderModule::FOLD, fold);
            const std::vector<float> y = run(*m, sine(300.0f, 0.5f, N));
            return amp(y, 900.0f) / amp(y, 300.0f);
        };
        check(h3(0.9f) > 5.0 * h3(0.2f), "more Fold gives much more 3rd harmonic");
        auto m = make<WavefolderModule>();
        setFloat(*m, WavefolderModule::MIX, 0.0f);
        const std::vector<float> y = run(*m, sine(300.0f, 0.5f, N));
        check(amp(y, 900.0f) / amp(y, 300.0f) < 0.002, "Mix at 0 is clean (no harmonics)");
    }

    { // Exciter: the harmonics are added when they are asked for and absent when they are not
        auto h2 = [&](float level2, float level3) {
            auto m = make<ExciterModule>();
            setFloat(*m, ExciterModule::H2, level2);
            setFloat(*m, ExciterModule::H3, level3);
            setFloat(*m, ExciterModule::MIX, 1.0f);
            setFloat(*m, ExciterModule::DRIVE, 0.9f);
            setFloat(*m, ExciterModule::FOCUS, 0.0f);
            const std::vector<float> y = run(*m, sine(1000.0f, 0.05f, N));
            return std::make_pair(amp(y, 2000.0f), amp(y, 3000.0f));
        };
        const auto on2 = h2(1.0f, 0.0f), on3 = h2(0.0f, 1.0f), off = h2(0.0f, 0.0f);
        check(db(on2.first / (off.first + 1e-9)) > 20.0, "the 2nd harmonic control adds the 2nd harmonic (more than 20 dB over zero)");
        check(db(on3.second / (off.second + 1e-9)) > 20.0, "the 3rd harmonic control adds the 3rd harmonic (more than 20 dB over zero)");
        check(on2.second < 0.1 * on2.first, "the 2nd harmonic control does not add much 3rd harmonic");
    }

    { // Bass Dist: the lows stay clean, the highs are distorted
        auto thd = [&](float f) {
            auto m = make<BassDistModule>();
            // Drive 0.9: at full Drive the top boost gives enough gain to clip the rest of a 50 Hz tone that the crossover lets through
            // (about 7% 3rd harmonic against 30% at 1 kHz); raise the Crossover for clean lows at full Drive
            setFloat(*m, BassDistModule::DRIVE, 0.9f);
            setFloat(*m, BassDistModule::CROSSOVER, 0.4f); // about 130 Hz
            const std::vector<float> y = run(*m, sine(f, 0.1f, N));
            return amp(y, 3.0f * f) / amp(y, f);
        };
        check(thd(1000.0f) > 10.0 * thd(50.0f), "a 50 Hz tone is distorted at least 10 times less than a 1 kHz tone");
    }

    { // Octa Fuzz: the octave
        auto m = make<OctaveFuzzModule>();
        setFloat(*m, OctaveFuzzModule::FUZZ, 0.0f);
        setFloat(*m, OctaveFuzzModule::OCTAVE, 1.0f);
        setFloat(*m, OctaveFuzzModule::TONE, 1.0f);
        const std::vector<float> y = run(*m, sine(300.0f, 0.02f, N));
        check(amp(y, 600.0f) > 3.0 * amp(y, 300.0f), "with Octave at 100% the 600 Hz octave is more than 3 times the 300 Hz fundamental");
        auto m2 = make<OctaveFuzzModule>();
        setFloat(*m2, OctaveFuzzModule::FUZZ, 0.0f);
        setFloat(*m2, OctaveFuzzModule::OCTAVE, 0.0f);
        const std::vector<float> y2 = run(*m2, sine(300.0f, 0.02f, N));
        check(amp(y2, 300.0f) > 5.0 * amp(y2, 600.0f), "with Octave at 0% the fundamental stays");
    }

    { // Tape Sat: unity gain for small signals (apart from the level makeup), compression for large ones, more low-end saturation with Warmth
        auto gain = [&](float peak, float f) {
            auto m = make<TapeSatModule>();
            setFloat(*m, TapeSatModule::TONE, 1.0f);
            setFloat(*m, TapeSatModule::WARMTH, 0.0f);
            return db(amp(run(*m, sine(f, peak, N)), f) / peak);
        };
        const double small = gain(0.0005f, 1000.0f), large = gain(0.5f, 1000.0f);
        std::printf("  small-signal gain %+.2f dB, gain at 0.5 peak %+.2f dB\n", small, large);
        check(small > 2.0 && small < 4.0, "the small-signal gain is the level makeup (about +3 dB)");
        check(small - large > 6.0, "a loud signal is compressed by more than 6 dB");
        auto h3 = [&](float warmth) {
            auto m = make<TapeSatModule>();
            setFloat(*m, TapeSatModule::WARMTH, warmth);
            setFloat(*m, TapeSatModule::TONE, 1.0f);
            const std::vector<float> y = run(*m, sine(60.0f, 0.05f, N));
            return amp(y, 180.0f) / amp(y, 60.0f);
        };
        check(h3(1.0f) > 1.5 * h3(0.0f), "Warmth makes the lows saturate more (more 3rd harmonic of a 60 Hz tone)");
    }

    { // Chainsaw: the EQ after the clipping (a fixed clipped level, so a band's boost shows directly in the output), the dead band, the saturation
        auto fund = [&](float f, int param, float v) {
            auto m = make<ChainsawModule>();
            setFloat(*m, param, v);
            return amp(run(*m, sine(f, 0.02f, N)), f);
        };
        const double high1k = db(fund(1100.0f, ChainsawModule::HIGH, 1.0f) / fund(1100.0f, ChainsawModule::HIGH, 0.0f));
        const double high200 = db(fund(200.0f, ChainsawModule::HIGH, 1.0f) / fund(200.0f, ChainsawModule::HIGH, 0.0f));
        const double low87 = db(fund(87.0f, ChainsawModule::LOW, 1.0f) / fund(87.0f, ChainsawModule::LOW, 0.0f));
        const double low1k = db(fund(1000.0f, ChainsawModule::LOW, 1.0f) / fund(1000.0f, ChainsawModule::LOW, 0.0f));
        std::printf("  High max vs min: %+.1f dB at 1.1 kHz, %+.1f dB at 200 Hz; Low max vs min: %+.1f dB at 87 Hz, %+.1f dB at 1 kHz\n", high1k, high200, low87, low1k);
        check(high1k > 35.0 && std::fabs(high200) < 3.0, "High is a big peak in the upper mids (two stagger-tuned peaks, more than 35 dB from min to max) and leaves 200 Hz alone");
        check(low87 > 25.0 && std::fabs(low1k) < 1.0, "Low is a peak at 87 Hz (more than 25 dB from min to max) and leaves 1 kHz alone");

        auto rms = [](const std::vector<float> &y) {
            double s = 0;
            for (size_t i = y.size() / 2; i < y.size(); i++) s += double(y[i]) * y[i];
            return std::sqrt(s / (y.size() / 2));
        };
        auto at = [&](float dist, float peak) {
            auto m = make<ChainsawModule>();
            setFloat(*m, ChainsawModule::DIST, dist);
            return rms(run(*m, sine(220.0f, peak, N)));
        };
        std::printf("  Dist 0: output RMS %g for a -70 dBFS tone, %g for -40 dBFS\n", at(0.0f, 0.0003f), at(0.0f, 0.01f));
        check(at(0.0f, 0.0003f) < 1e-9 && at(0.0f, 0.01f) > 1e-3, "the dead band gates a -70 dBFS tone at Dist 0 (silent), and a -40 dBFS tone passes");
        const double sat = db(at(0.8f, 0.25f) / at(0.8f, 0.0056f));
        std::printf("  output level change at the default Dist for an input change from -45 to -12 dBFS: %+.1f dB\n", sat);
        check(sat < 2.0, "at the default Dist the output hardly grows with the input (fully saturated from about -45 dBFS)");
    }

    std::printf("\n%s\n", fails ? "DISTORTION FAMILY TESTS FAILED" : "ALL DISTORTION FAMILY TESTS PASSED");
    return fails ? 1 : 0;
}
