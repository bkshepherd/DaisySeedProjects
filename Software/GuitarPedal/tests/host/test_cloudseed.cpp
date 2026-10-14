// Dumps CloudSeedModule output for all 8 presets, either via the per-sample path or the block path.
#include "cloudseed_module.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace bkshepherd;
static constexpr size_t kBlock = 48, kBlocks = 1000;
int main(int argc, char **argv) {
    bool block = argc > 2 && !strcmp(argv[1], "block");
    FILE *f = fopen(argv[argc - 1], "wb");
    std::vector<float> inL(kBlock * kBlocks), inR(kBlock * kBlocks), outL(inL.size()), outR(inL.size());
    srand(99);
    for (size_t i = 0; i < inL.size(); i++) {
        float t = i / 48000.0f;
        float burst = (i < 2400 || (i > 12000 && i < 13000)) ? 0.7f : 0.0f; // bursts, then silence so the tails are what's left
        inL[i] = burst * ((rand() / (float)RAND_MAX) - 0.5f) * 2 + (burst > 0 ? 0.2f * sinf(6.2831853f * 220 * t) : 0);
        inR[i] = burst * ((rand() / (float)RAND_MAX) - 0.5f) * 2 + (burst > 0 ? 0.2f * sinf(6.2831853f * 330 * t) : 0);
    }
    for (int stereo = 0; stereo < 2; stereo++) {
        for (int preset = 0; preset < 8; preset++) {
            CloudSeedModule *m = new CloudSeedModule();
            m->Init(48000.0f);
            // Select the preset by going through a fixed other preset first, so ParameterChanged always loads it, and always via the same sequence.
            // (Setting it straight to the module's default value would keep the preset Init() loaded; the original code had a Preset default that
            // did not match Init(), so one preset was never loaded. Presets also do not fully reset the controller, so the sequence must be the same
            // in the original and the new code for the comparison to be exact.)
            m->SetParameterRaw(CloudSeedModule::PRESET, preset == 0 ? 1 : 0);
            m->SetParameterRaw(CloudSeedModule::PRESET, preset);
            m->SetParameterAsMagnitude(CloudSeedModule::MIX, 0.6f);
            if (stereo) m->SetParameterRaw(CloudSeedModule::STEREO_IN, 1);
            for (size_t b = 0; b < kBlocks; b++) {
                size_t o = b * kBlock;
                if (block) {
                    if (stereo) m->ProcessStereoBlock(&inL[o], &inR[o], &outL[o], &outR[o], kBlock);
                    else m->ProcessMonoBlock(&inL[o], &outL[o], &outR[o], kBlock);
                } else {
                    for (size_t i = 0; i < kBlock; i++) {
                        if (stereo) m->ProcessStereo(inL[o + i], inR[o + i]); else m->ProcessMono(inL[o + i]);
                        outL[o + i] = m->GetAudioLeft(); outR[o + i] = m->GetAudioRight();
                    }
                }
            }
            fwrite(outL.data(), 4, outL.size(), f);
            fwrite(outR.data(), 4, outR.size(), f);
            delete m; // pool allocator never frees; the pool is reset below
            extern size_t pool_index; pool_index = 0;
        }
    }
    fclose(f);
    return 0;
}
