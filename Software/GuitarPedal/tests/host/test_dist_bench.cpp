// Tests of the Dist Bench (up to four distortion stages in series):
//  1. one stage with its generic controls at 50% gives exactly the output of that effect on its own, for every type
//  2. the generic controls reach the ends of the effects' own ranges and step through their modes
//  3. with every stage Off the bench is a clean +6 dB
//  4. the block path and the per-sample path give identical output, with random settings (types included) changed while playing
//  5. the output stays finite and bounded, and changing a stage's type does not click
#include "dist_bench_module.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
using namespace bkshepherd;

static int fails = 0;
static void check(bool ok, const char *what) {
    fails += !ok;
    std::printf("  %-100s %s\n", what, ok ? "OK" : "FAILED");
}

static constexpr float kSr = 48000.0f;
static constexpr size_t N = 24000;
using DB = DistBenchModule;

static std::vector<float> guitar(float f, float peak, size_t n) { // a harmonic series, partial k at 1/k
    std::vector<float> v(n);
    for (size_t i = 0; i < n; i++) {
        double x = 0;
        for (int k = 1; k <= 8; k++) x += std::sin(2.0 * 3.14159265358979 * f * k * i / kSr) / k;
        v[i] = float(x) * peak / 2.0f;
    }
    return v;
}

// The mono block path, as the bench runs its stages
static std::vector<float> run(BaseEffectModule &m, const std::vector<float> &in, size_t block = 48) {
    std::vector<float> l(in.size()), r(in.size());
    for (size_t o = 0; o < in.size(); o += block) m.ProcessMonoBlock(&in[o], &l[o], &r[o], std::min(block, in.size() - o));
    return l;
}

static double maxDiff(const std::vector<float> &a, const std::vector<float> &b) {
    double d = 0;
    for (size_t i = 0; i < a.size(); i++) d = std::fmax(d, std::fabs(double(a[i]) - b[i]));
    return d;
}

// A bench with stage 1 of the given type and the other stages Off, the generic controls of stage 1 set before Init
static std::unique_ptr<DB> bench(int type, float drive = 0.5f, float tone = 0.5f, float chr = 0.5f) {
    auto b = std::make_unique<DB>();
    b->SetParameterRaw(DB::StageParamId(0, DB::TYPE), type);
    b->SetParameterAsFloat(DB::StageParamId(0, DB::DRIVE), drive);
    b->SetParameterAsFloat(DB::StageParamId(0, DB::TONE), tone);
    b->SetParameterAsFloat(DB::StageParamId(0, DB::CHAR), chr);
    b->Init(kSr);
    return b;
}

template <class M>
static std::unique_ptr<BaseEffectModule> alone(std::initializer_list<std::pair<int, float>> floats = {}, std::initializer_list<std::pair<int, int>> raws = {}) {
    auto m = std::make_unique<M>();
    m->Init(kSr);
    for (auto [id, v] : floats) m->SetParameterAsFloat(id, v);
    for (auto [id, v] : raws) m->SetParameterRaw(id, v);
    return m;
}

int main() {
    const std::vector<float> in = guitar(110.0f, 0.05f, N);

    { // 1. a stage at 50% is the effect at its defaults (the -6 dB trim and the +6 dB makeup cancel exactly)
        std::unique_ptr<BaseEffectModule> ref[DB::TYPE_COUNT] = {nullptr,
                                                                alone<PreampModule>(),
                                                                alone<ScreamerModule>(),
                                                                alone<MuffFuzzModule>(),
                                                                alone<WavefolderModule>(),
                                                                alone<ExciterModule>(),
                                                                alone<BassDistModule>(),
                                                                alone<OctaveFuzzModule>(),
                                                                alone<TapeSatModule>(),
                                                                alone<OverdriveModule>(),
                                                                alone<DistortionModule>({}, {{DistortionModule::OVERSAMP, 0}}), // the bench runs it without oversampling
                                                                alone<ChainsawModule>()};
        bool same = true;
        for (int t = 1; t < DB::TYPE_COUNT; t++) {
            const double d = maxDiff(run(*bench(t), in), run(*ref[t], in));
            if (d != 0.0) {
                std::printf("    type %d differs from the effect alone by %g\n", t, d);
                same = false;
            }
        }
        check(same, "one stage at 50% gives exactly the effect's own output at its defaults, for all eleven types (Distortion without oversampling)");
        auto def = std::make_unique<DB>();
        def->Init(kSr);
        check(maxDiff(run(*def, in), run(*alone<ScreamerModule>(), in)) == 0.0, "the default bench (stage 1 Screamer, the rest Off) is the Screamer at its defaults");
    }

    { // 2. the ends of the generic controls
        check(maxDiff(run(*bench(DB::SCREAMER, 1.0f, 0.0f), in), run(*alone<ScreamerModule>({{ScreamerModule::DRIVE, 1.0f}, {ScreamerModule::TONE, 0.0f}}), in)) == 0.0,
              "Drive 100% and Tone 0% are the Screamer's Drive at maximum and Tone at minimum");
        check(maxDiff(run(*bench(DB::PREAMP, 0.5f, 0.5f, 0.0f), in), run(*alone<PreampModule>({}, {{PreampModule::VOICING, 0}}), in)) == 0.0,
              "Preamp Char 0% is the Clean voicing");
        check(maxDiff(run(*bench(DB::PREAMP, 0.5f, 0.5f, 1.0f), in), run(*alone<PreampModule>({}, {{PreampModule::VOICING, 2}}), in)) == 0.0,
              "Preamp Char 100% is the Lead voicing");
        check(maxDiff(run(*bench(DB::SCREAMER, 0.5f, 0.5f, 0.75f), in), run(*alone<ScreamerModule>({}, {{ScreamerModule::MODE, 1}}), in)) == 0.0 &&
                  maxDiff(run(*bench(DB::SCREAMER, 0.5f, 0.5f, 1.0f), in), run(*alone<ScreamerModule>({}, {{ScreamerModule::MODE, 2}}), in)) == 0.0,
              "Screamer Char 75% is Asymmetric and 100% is Hard (Classic, the default, at 50% and below)");
        check(maxDiff(run(*bench(DB::EXCITER, 0.5f, 0.5f, 0.0f), in),
                      run(*alone<ExciterModule>({{ExciterModule::H2, 1.0f}, {ExciterModule::H3, 0.0f}}), in)) == 0.0,
              "Exciter Char 0% is the 2nd harmonic at full and no 3rd");
        check(maxDiff(run(*bench(DB::DISTORTION, 0.5f, 0.5f, 1.0f), in),
                      run(*alone<DistortionModule>({}, {{DistortionModule::OVERSAMP, 0}, {DistortionModule::DIST_TYPE, 5}}), in)) == 0.0,
              "Distortion Char 100% is Diode Clip (the last type)");
        check(maxDiff(run(*bench(DB::OVERDRIVE, 0.0f), in), run(*alone<OverdriveModule>({{OverdriveModule::DRIVE, 0.0f}}), in)) == 0.0,
              "Overdrive Drive 0% is the Overdrive's Drive at minimum");
    }

    { // two stages of the same type are two independent instances (the Distortion's filters used to be shared by all its instances)
        auto b = std::make_unique<DB>();
        b->SetParameterRaw(DB::StageParamId(0, DB::TYPE), DB::DISTORTION);
        b->SetParameterRaw(DB::StageParamId(1, DB::TYPE), DB::DISTORTION);
        b->Init(kSr);
        auto d1 = alone<DistortionModule>({}, {{DistortionModule::OVERSAMP, 0}}), d2 = alone<DistortionModule>({}, {{DistortionModule::OVERSAMP, 0}});
        std::vector<float> mid = run(*d1, in);
        for (float &v : mid) v *= 0.5f; // the -6 dB trim of stage 1
        std::vector<float> ref = run(*d2, mid);
        for (float &v : ref) v = v * 0.5f * 2.0f; // the trim of stage 2 and the +6 dB at the end
        check(maxDiff(run(*b, in), ref) == 0.0, "two Distortion stages equal two Distortions on their own in series (no shared state)");
    }

    { // 3. all stages Off
        auto b = bench(DB::OFF);
        const std::vector<float> y = run(*b, in);
        bool exact = true;
        for (size_t i = 0; i < N; i++) exact &= y[i] == 2.0f * in[i];
        check(exact, "with every stage Off the output is the input at +6 dB, unchanged otherwise");
    }

    { // 4. and 5. block vs per-sample with random settings changed while playing; bounds
        const size_t total = 48000 * 4, every = 48 * 37 * 3; // the changes fall on block boundaries of both block sizes
        const std::vector<float> x = guitar(146.8f, 0.3f, total);
        auto randomize = [](BaseEffectModule &m, unsigned &s) {
            auto rnd = [&s] { s = 1664525u * s + 1013904223u; return ((s >> 8) & 0xFFFF) / 65535.0f; };
            for (int i = 0; i < m.GetParameterCount(); i++) {
                if (m.GetParameterType(i) == ParameterValueType::Binned)
                    m.SetParameterRaw(i, std::min(int(rnd() * m.GetParameterBinCount(i)), m.GetParameterBinCount(i) - 1));
                else
                    m.SetParameterAsFloat(i, rnd());
            }
        };
        std::vector<float> ys(total), y48(total), y37(total), r(total);
        auto a = std::make_unique<DB>(), b = std::make_unique<DB>(), c = std::make_unique<DB>();
        a->Init(kSr), b->Init(kSr), c->Init(kSr);
        unsigned sa = 7, sb = 7, sc = 7;
        for (size_t i = 0; i < total; i++) {
            if (i % every == 0) randomize(*a, sa);
            a->ProcessMono(x[i]);
            ys[i] = a->GetAudioLeft();
        }
        for (size_t o = 0; o < total; o += 48) {
            if (o % every == 0) randomize(*b, sb);
            b->ProcessMonoBlock(&x[o], &y48[o], &r[o], std::min<size_t>(48, total - o));
        }
        for (size_t o = 0; o < total; o += 37) {
            if (o % every == 0) randomize(*c, sc);
            c->ProcessStereoBlock(&x[o], &x[o], &y37[o], &r[o], std::min<size_t>(37, total - o));
        }
        check(maxDiff(ys, y48) == 0.0 && maxDiff(ys, y37) == 0.0, "block paths (48 mono, 37 stereo) equal the per-sample path, random settings and types changed while playing");
        bool finite = true;
        double peak = 0;
        for (float v : ys) {
            finite &= std::isfinite(v);
            peak = std::fmax(peak, std::fabs(v));
        }
        std::printf("    output peak with random settings: %.2f (input peak 0.30)\n", peak);
        check(finite && peak < 8.0, "the output stays finite and bounded (below 8) with random settings");
    }

    { // 5. changing the type crossfades: the largest sample-to-sample step is not bigger than in the steady state of either type
        const size_t total = 72000, at = 24000;
        const std::vector<float> x = guitar(110.0f, 0.05f, total);
        auto b = bench(DB::SCREAMER);
        std::vector<float> y(total), r(total);
        for (size_t o = 0; o < total; o += 48) {
            if (o == at) b->SetParameterRaw(DB::StageParamId(0, DB::TYPE), DB::MUFF_FUZZ);
            b->ProcessMonoBlock(&x[o], &y[o], &r[o], 48);
        }
        auto maxStep = [&](size_t from, size_t to) {
            double m = 0;
            for (size_t i = from + 1; i < to; i++) m = std::fmax(m, std::fabs(double(y[i]) - y[i - 1]));
            return m;
        };
        const double before = maxStep(12000, at), during = maxStep(at, at + 4800), after = maxStep(at + 9600, total);
        std::printf("    largest step: Screamer %.4f, during the fade %.4f, Muff Fuzz %.4f\n", before, during, after);
        check(during <= 1.1 * std::fmax(before, after), "changing the type (Screamer to Muff Fuzz) does not click");
    }

    std::printf("\n%s\n", fails == 0 ? "ALL DIST BENCH TESTS PASSED" : "DIST BENCH TESTS FAILED");
    return fails == 0 ? 0 : 1;
}
