// SciFi: dumps output via per-sample or block path for several parameter sets, with automation. argv: mode outfile
#include "scifi_module.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace bkshepherd;
static constexpr size_t kB = 48, kN = 500;
int main(int argc, char **argv) {
    bool block = !strcmp(argv[1], "block");
    std::vector<float> in(kB * kN), outL(in.size()), outR(in.size()); srand(11);
    for (size_t i = 0; i < in.size(); i++) {
        float burst = (i < 4800 || (i > 9600 && i < 12000)) ? 0.7f : 0.03f;
        in[i] = burst * (((rand() / (float)RAND_MAX) - 0.5f) * 2) + 0.25f * sinf(6.2831853f * 196 * i / 48000.0f);
    }
    FILE *f = fopen(argv[2], "wb");
    for (int cfg = 0; cfg < 4; cfg++) {
        SciFiModule *m = new SciFiModule(); m->Init(48000.0f);
        for (size_t b = 0; b < kN; b++) {
            if (b % 100 == 0) { // parameter automation at block boundaries, identical for both paths
                float base[4][9] = {{0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f}, {1,1,1,1,1,1,1,1,1}, {0,0.9f,0.2f,0.8f,0.1f,0.3f,0.9f,0.4f,0.0f}, {0.3f,0,1,0.2f,0.9f,0.7f,0.1f,0.8f,1.0f}};
                for (int p = 0; p < 9; p++) m->SetParameterAsMagnitude(p, std::min(1.0f, std::max(0.0f, base[cfg][p] + 0.07f * (b / 100))));
            }
            size_t o = b * kB;
            if (block) m->ProcessStereoBlock(&in[o], &in[o], &outL[o], &outR[o], kB);
            else for (size_t i = 0; i < kB; i++) { m->ProcessStereo(in[o + i], in[o + i]); outL[o + i] = m->GetAudioLeft(); outR[o + i] = m->GetAudioRight(); }
        }
        fwrite(outL.data(), 4, outL.size(), f); fwrite(outR.data(), 4, outR.size(), f);
        delete m;
    }
    fclose(f); return 0;
}
