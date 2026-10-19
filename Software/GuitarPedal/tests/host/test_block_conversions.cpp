// The block paths of Crusher, Chopper, Modulated Tremolo and Harmonic Tremolo must give exactly the output of the per-sample paths (mono and stereo,
// block sizes 48 and 37), for the defaults and for random settings.
//   test_block_conversions                                    compares the paths inside the process (Crusher with pitch tracking off)
//   test_block_conversions tracking <sample|samplemono|mono|stereo> <config> <outfile>
//                                                             one run of the Crusher with pitch tracking on: its pitch detector is shared by all
//                                                             instances (static), so each run is its own process and the files are compared outside
// The modules are constructed in zeroed memory: the DaisySP oscillator does not initialise last_out_ (used by its polyBLEP triangle), so two instances
// would otherwise differ in their first samples.
#include "chopper_module.h"
#include "crusher_module.h"
#include "harmonic_tremolo_module.h"
#include "modulated_tremolo_module.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <type_traits>
#include <vector>
using namespace bkshepherd;

static int fails = 0;

template <class M>
struct Zeroed {
    alignas(M) unsigned char mem[sizeof(M)];
    M *m;
    Zeroed() {
        std::memset(mem, 0, sizeof(mem));
        m = new (mem) M();
    }
    ~Zeroed() { m->~M(); }
    Zeroed(const Zeroed &) = delete;
    M *operator->() { return m; }
    M &operator*() { return *m; }
};

static std::vector<float> makeInput(size_t n, float f, unsigned seed) {
    std::vector<float> v(n);
    unsigned s = seed;
    for (size_t i = 0; i < n; i++) {
        s = 1664525u * s + 1013904223u;
        const float noise = (((s >> 8) & 0xFFFF) / 32768.0f - 1.0f);
        const float burst = (i % 24000) < 12000 ? 1.0f : 0.1f;
        v[i] = burst * (0.3f * std::sin(6.2831853f * f * i / 48000.0f) + 0.04f * noise);
    }
    return v;
}

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
static void setup(M &m, int cfg, unsigned seed, int tracking /* -1 leave, 0 off, 1 on (Crusher) */) {
    m.Init(48000.0f);
    if (cfg > 0) randomize(m, seed);
    if constexpr (std::is_same<M, CrusherModule>::value) {
        if (tracking >= 0) m.SetParameterAsBool(CrusherModule::PITCH_TRACKING, tracking == 1);
    }
}

static bool sameBits(float a, float b) { return std::memcmp(&a, &b, sizeof a) == 0; }

template <class M>
static void test(const char *name, int configs) {
    bool same = true;
    unsigned seed = 4242;
    for (int cfg = 0; cfg < configs; cfg++) {
        const size_t n = 48 * 1500;
        const std::vector<float> inL = makeInput(n, 196.0f, 3 + cfg), inR = makeInput(n, 330.0f, 9 + cfg);
        Zeroed<M> refMono, refStereo;
        setup(*refMono, cfg, seed, std::is_same<M, CrusherModule>::value ? 0 : -1);
        setup(*refStereo, cfg, seed, std::is_same<M, CrusherModule>::value ? 0 : -1);
        std::vector<float> mono(n), stL(n), stR(n);
        for (size_t i = 0; i < n; i++) { refMono->ProcessMono(inL[i]); mono[i] = refMono->GetAudioLeft(); }
        for (size_t i = 0; i < n; i++) { refStereo->ProcessStereo(inL[i], inR[i]); stL[i] = refStereo->GetAudioLeft(); stR[i] = refStereo->GetAudioRight(); }
        for (size_t block : {size_t(48), size_t(37)}) {
            Zeroed<M> m1, m2;
            setup(*m1, cfg, seed, std::is_same<M, CrusherModule>::value ? 0 : -1);
            setup(*m2, cfg, seed, std::is_same<M, CrusherModule>::value ? 0 : -1);
            std::vector<float> l1(n), r1(n), l2(n), r2(n);
            for (size_t o = 0; o < n; o += block) {
                const size_t k = std::min(block, n - o);
                m1->ProcessMonoBlock(&inL[o], &l1[o], &r1[o], k);
                m2->ProcessStereoBlock(&inL[o], &inR[o], &l2[o], &r2[o], k);
            }
            size_t bad[4] = {0, 0, 0, 0}, first[4] = {0, 0, 0, 0};
            for (size_t i = 0; i < n; i++) {
                const bool ok[4] = {sameBits(l1[i], mono[i]), sameBits(r1[i], mono[i]), sameBits(l2[i], stL[i]), sameBits(r2[i], stR[i])};
                for (int k = 0; k < 4; k++) if (!ok[k]) { if (!bad[k]++) first[k] = i; }
            }
            for (int k = 0; k < 4; k++) {
                if (bad[k]) {
                    same = false;
                    std::printf("    %s config %d block %zu: %s differs in %zu samples (first at %zu)\n", name, cfg, block,
                                k == 0 ? "mono left" : k == 1 ? "mono right" : k == 2 ? "stereo left" : "stereo right", bad[k], first[k]);
                }
            }
        }
        seed = seed * 7 + 5;
    }
    std::printf("  %-20s block paths (48 and 37 samples) equal the per-sample paths, mono and stereo, %d configurations  %s\n", name, configs, same ? "OK" : "MISMATCH");
    fails += !same;
}

int main(int argc, char **argv) {
    if (argc == 5 && !std::strcmp(argv[1], "tracking")) {
        const char *mode = argv[2];
        const int cfg = std::atoi(argv[3]);
        const size_t n = 48 * 1500, block = !std::strcmp(mode, "stereo") ? 37 : 48;
        const std::vector<float> inL = makeInput(n, 196.0f, 3 + cfg), inR = makeInput(n, 330.0f, 9 + cfg);
        Zeroed<CrusherModule> m;
        unsigned seed = 4242;
        for (int c = 0; c < cfg; c++) seed = seed * 7 + 5;
        setup(*m, 1 + cfg, seed, 1);
        std::vector<float> l(n), r(n);
        if (!std::strcmp(mode, "sample")) {
            for (size_t i = 0; i < n; i++) { m->ProcessStereo(inL[i], inR[i]); l[i] = m->GetAudioLeft(); r[i] = m->GetAudioRight(); }
        } else if (!std::strcmp(mode, "samplemono")) {
            for (size_t i = 0; i < n; i++) { m->ProcessMono(inL[i]); l[i] = m->GetAudioLeft(); r[i] = m->GetAudioRight(); }
        } else {
            for (size_t o = 0; o < n; o += block) {
                const size_t k = std::min(block, n - o);
                if (!std::strcmp(mode, "mono")) m->ProcessMonoBlock(&inL[o], &l[o], &r[o], k);
                else m->ProcessStereoBlock(&inL[o], &inR[o], &l[o], &r[o], k);
            }
        }
        FILE *f = std::fopen(argv[4], "wb"); std::fwrite(l.data(), 4, n, f); std::fwrite(r.data(), 4, n, f); std::fclose(f);
        return 0;
    }

    test<CrusherModule>("Crusher", 8);
    test<ChopperModule>("Chopper", 8);
    test<ModulatedTremoloModule>("Modulated Tremolo", 8);
    test<HarmonicTremoloModule>("Harmonic Tremolo", 8);
    std::printf(fails ? "BLOCK CONVERSION TESTS FAILED\n" : "ALL BLOCK CONVERSION TESTS PASSED\n");
    return fails ? 1 : 0;
}
