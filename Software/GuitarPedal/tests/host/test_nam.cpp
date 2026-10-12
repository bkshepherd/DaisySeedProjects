// Dumps NAM module output through one path. The model object is a process-wide static with recurrent state, so each path/config runs
// in its own process (argv: mode eq irCab outfile).
#include "nam_a2_module.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace bkshepherd;
static constexpr size_t kB = 48, kN = 120;
int main(int argc, char **argv) {
    const char *mode = argv[1]; int eq = atoi(argv[2]), ir = atoi(argv[3]);
    std::vector<float> in(kB * kN), outL(in.size()), outR(in.size()); srand(5);
    for (size_t i = 0; i < in.size(); i++) in[i] = 0.3f * sinf(6.2831853f * 110 * i / 48000.0f) + 0.2f * ((rand() / (float)RAND_MAX) - 0.5f);
    NamA2Module *m = new NamA2Module(); m->Init(48000.0f);
    m->SetParameterAsMagnitude(NamA2Module::GAIN, 0.6f);
    m->SetParameterAsMagnitude(NamA2Module::LEVEL, 0.5f);
    m->SetParameterRaw(NamA2Module::EQ, eq);
    m->SetParameterRaw(NamA2Module::IR_CAB, ir);
    for (size_t o = 0; o < in.size(); o += kB) {
        if (!strcmp(mode, "sample")) for (size_t i = 0; i < kB; i++) { m->ProcessMono(in[o + i]); outL[o + i] = m->GetAudioLeft(); outR[o + i] = m->GetAudioRight(); }
        else if (!strcmp(mode, "mono")) m->ProcessMonoBlock(&in[o], &outL[o], &outR[o], kB);
        else m->ProcessStereoBlock(&in[o], &in[o], &outL[o], &outR[o], kB);
    }
    FILE *f = fopen(argv[4], "wb"); fwrite(outL.data(), 4, outL.size(), f); fwrite(outR.data(), 4, outR.size(), f); fclose(f);
    printf("led=%.6f\n", m->GetBrightnessForLED(1));
    return 0;
}
