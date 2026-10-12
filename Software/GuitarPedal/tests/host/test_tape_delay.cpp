// Tape Delay: dumps output via per-sample or block path, for several modes/head configs, with parameter automation. argv: mode outfile
#include "tape_delay_module.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace bkshepherd;
static constexpr size_t kB = 48, kN = 500;
int main(int argc, char **argv) {
    bool block = !strcmp(argv[1], "block");
    bool stereoIn = argc > 3 && !strcmp(argv[3], "stereo");
    std::vector<float> inL(kB * kN), inR(kB * kN), outL(inL.size()), outR(inL.size()); srand(21);
    for (size_t i = 0; i < inL.size(); i++) {
        float burst = (i < 2400 || (i > 9600 && i < 10800)) ? 0.7f : 0.02f;
        inL[i] = burst * (((rand() / (float)RAND_MAX) - 0.5f) * 2) + 0.25f * sinf(6.2831853f * 196 * i / 48000.0f);
        inR[i] = burst * (((rand() / (float)RAND_MAX) - 0.5f) * 2) + 0.25f * sinf(6.2831853f * 262 * i / 48000.0f);
    }
    FILE *f = fopen(argv[2], "wb");
    // (mode, headCfg, tapDiv) combinations: fixed, multi, single (with all divisions), lofi
    int combos[][3] = {{0,0,0},{0,1,0},{0,2,0},{1,0,0},{1,1,0},{1,2,0},{2,0,0},{2,1,1},{2,2,2},{3,0,0},{3,1,0}};
    for (auto &c : combos) {
        TapeDelayModule *m = new TapeDelayModule(); m->Init(48000.0f);
        m->SetParameterRaw(TapeDelayModule::MODE, c[0]);
        m->SetParameterRaw(TapeDelayModule::HEAD_CONFIG, c[1]);
        m->SetParameterRaw(TapeDelayModule::TAP_DIV, c[2]);
        for (size_t b = 0; b < kN; b++) {
            if (b % 100 == 0) { // automation at block boundaries, identical for both paths
                float k = (b / 100) * 0.13f;
                const int ids[] = {TapeDelayModule::TIME, TapeDelayModule::MIX, TapeDelayModule::REPEATS, TapeDelayModule::WOW_FLUTTER,
                                   TapeDelayModule::TAPE_AGE, TapeDelayModule::TAPE_BIAS, TapeDelayModule::IMPERFECTIONS,
                                   TapeDelayModule::LOW_END_CONTOUR, TapeDelayModule::REVERB_MIX};
                const float base[] = {0.05f, 0.6f, 0.55f, 0.5f, 0.3f, 0.4f, 0.6f, 0.3f, 0.35f};
                for (int p = 0; p < 9; p++) m->SetParameterAsMagnitude(ids[p], std::min(1.0f, base[p] + k * ((p % 3) - 1) * 0.5f + k * 0.3f));
            }
            size_t o = b * kB;
            if (block) {
                if (stereoIn) m->ProcessStereoBlock(&inL[o], &inR[o], &outL[o], &outR[o], kB);
                else m->ProcessMonoBlock(&inL[o], &outL[o], &outR[o], kB);
            } else for (size_t i = 0; i < kB; i++) {
                if (stereoIn) m->ProcessStereo(inL[o + i], inR[o + i]); else m->ProcessMono(inL[o + i]);
                outL[o + i] = m->GetAudioLeft(); outR[o + i] = m->GetAudioRight();
            }
        }
        fwrite(outL.data(), 4, outL.size(), f); fwrite(outR.data(), 4, outR.size(), f);
        delete m;
    }
    fclose(f); return 0;
}
