// The block paths of Distortion and Overdrive must give exactly the output of the per-sample path.
//  Distortion keeps its filters in file-level variables, so each run is its own process:
//    test_distortion_block <sample|mono|stereo> <type 0..5> <oversampling 0|1> <block size> <outfile>
//  Overdrive: test_distortion_block overdrive    (compares the paths in the process; the module's instances are independent)
#include "distortion_module.h"
#include "overdrive_module.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace bkshepherd;

static std::vector<float> makeInput(size_t n, float f, unsigned seed) {
    std::vector<float> v(n);
    unsigned s = seed;
    for (size_t i = 0; i < n; i++) {
        s = 1664525u * s + 1013904223u;
        const float noise = (((s >> 8) & 0xFFFF) / 32768.0f - 1.0f);
        const float burst = (i % 24000) < 12000 ? 1.0f : 0.1f;
        v[i] = burst * (0.25f * std::sin(6.2831853f * f * i / 48000.0f) + 0.05f * noise);
    }
    return v;
}

static int overdriveTest() {
    int fails = 0;
    for (int cfg = 0; cfg < 3; cfg++) {
        const float drive = cfg == 0 ? 0.5f : (cfg == 1 ? 1.0f : 0.1f), level = cfg == 0 ? 0.5f : (cfg == 1 ? 0.9f : 0.2f);
        auto setup = [&](OverdriveModule &m) {
            m.Init(48000.0f);
            m.SetParameterAsMagnitude(OverdriveModule::DRIVE, drive);
            m.SetParameterAsMagnitude(OverdriveModule::LEVEL, level);
        };
        const size_t n = 48 * 1000;
        const std::vector<float> inL = makeInput(n, 220.0f, 5), inR = makeInput(n, 330.0f, 9);
        // per-sample references: mono (ProcessMono) and stereo (ProcessStereo)
        OverdriveModule a, b, c, d;
        setup(a); setup(b); setup(c); setup(d);
        std::vector<float> monoRef(n), stL(n), stR(n);
        for (size_t i = 0; i < n; i++) { a.ProcessMono(inL[i]); monoRef[i] = a.GetAudioLeft(); }
        for (size_t i = 0; i < n; i++) { b.ProcessStereo(inL[i], inR[i]); stL[i] = b.GetAudioLeft(); stR[i] = b.GetAudioRight(); }
        // block versions, also with a block size that does not divide the signal
        bool same = true;
        for (size_t block : {size_t(48), size_t(37)}) {
            OverdriveModule m1, m2;
            setup(m1); setup(m2);
            std::vector<float> l1(n), r1(n), l2(n), r2(n);
            for (size_t o = 0; o < n; o += block) {
                const size_t k = std::min(block, n - o);
                m1.ProcessMonoBlock(&inL[o], &l1[o], &r1[o], k);
                m2.ProcessStereoBlock(&inL[o], &inR[o], &l2[o], &r2[o], k);
            }
            for (size_t i = 0; i < n; i++) same = same && l1[i] == monoRef[i] && r1[i] == monoRef[i] && l2[i] == stL[i] && r2[i] == stR[i];
        }
        std::printf("  Overdrive drive %.1f level %.1f: block paths (48 and 37 samples) equal the per-sample paths, mono and stereo  %s\n", drive, level, same ? "OK" : "MISMATCH");
        fails += !same;
    }
    return fails ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "overdrive")) return overdriveTest();
    const char *mode = argv[1];
    const int type = atoi(argv[2]), os = atoi(argv[3]);
    const size_t block = (size_t)atoi(argv[4]);
    const size_t n = 48 * 1500;
    DistortionModule *m = new DistortionModule();
    m->Init(48000.0f);
    m->SetParameterRaw(DistortionModule::DIST_TYPE, type);
    m->SetParameterRaw(DistortionModule::OVERSAMP, os);
    m->SetParameterAsMagnitude(DistortionModule::GAIN, 0.6f);
    m->SetParameterAsMagnitude(DistortionModule::INTENSITY, 0.35f);
    m->SetParameterAsMagnitude(DistortionModule::TONE, 0.4f);
    m->SetParameterAsMagnitude(DistortionModule::LEVEL, 0.7f);
    const std::vector<float> inL = makeInput(n, 220.0f, 3), inR = makeInput(n, 330.0f, 7);
    std::vector<float> outL(n), outR(n);
    if (!strcmp(mode, "sample")) {
        for (size_t i = 0; i < n; i++) { m->ProcessStereo(inL[i], inR[i]); outL[i] = m->GetAudioLeft(); outR[i] = m->GetAudioRight(); }
    } else {
        for (size_t o = 0; o < n; o += block) {
            const size_t k = std::min(block, n - o);
            if (!strcmp(mode, "mono")) m->ProcessMonoBlock(&inL[o], &outL[o], &outR[o], k);
            else m->ProcessStereoBlock(&inL[o], &inR[o], &outL[o], &outR[o], k);
        }
    }
    float peak = 0;
    for (size_t i = 0; i < n; i++) peak = std::fmax(peak, std::fmax(std::fabs(outL[i]), std::fabs(outR[i])));
    FILE *f = fopen(argv[5], "wb"); fwrite(outL.data(), 4, n, f); fwrite(outR.data(), 4, n, f); fclose(f);
    std::printf("peak %.4f\n", peak);
    return peak > 0.0005f && std::isfinite(peak) ? 0 : 1;
}
