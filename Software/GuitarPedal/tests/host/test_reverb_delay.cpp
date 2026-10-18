// Dumps Reverb Delay output through one path for one parameter configuration (the module keeps its delay lines and reverb in static memory,
// so each run is its own process): test_reverb_delay <sample|mono|samplest|stereo> <config> <block size> <outfile>
//   sample    ProcessMono per sample          mono    ProcessMonoBlock       (mono input)
//   samplest  ProcessStereo per sample        stereo  ProcessStereoBlock     (different left and right input)
// The block path must give exactly the per-sample output (run_tests.sh compares the files).
#include "reverb_delay_module.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace bkshepherd;
using M = ReverbDelayModule;

static void configure(M &m, int cfg) {
    m.SetParameterAsMagnitude(M::DELAY_TIME, 0.02f); // about 6000 samples, so the echoes land inside the test signal
    m.SetParameterAsMagnitude(M::DELAY_MIX, 0.5f);
    m.SetParameterAsMagnitude(M::REVERB_MIX, 0.4f);
    m.SetParameterAsMagnitude(M::D_FEEDBACK, 0.5f);
    switch (cfg) {
    case 0: break;                                                                       // defaults
    case 1: m.SetParameterAsBool(M::REVERSE, true); break;                               // reverse
    case 2: m.SetParameterAsBool(M::OCTAVE, true); m.SetParameterAsMagnitude(M::D_FEEDBACK, 0.6f); break;
    case 3: m.SetParameterAsBool(M::DUAL_DELAY, true); m.SetParameterAsMagnitude(M::D_SPREAD, 0.6f); break;
    case 4: m.SetParameterAsMagnitude(M::D_SPREAD, 0.5f); break;                         // spread without dual delay
    case 5: m.SetParameterAsBool(M::SERIES_D_TO_R, true); break;                         // delay into reverb
    case 6: m.SetParameterAsBinnedValue(M::MOD_PARAM, 2); m.SetParameterAsMagnitude(M::MOD_AMT, 0.7f); m.SetParameterAsMagnitude(M::MOD_RATE, 0.5f); break; // delay time
    case 7: m.SetParameterAsBinnedValue(M::MOD_PARAM, 3); m.SetParameterAsBinnedValue(M::MOD_WAVE, 2); m.SetParameterAsMagnitude(M::MOD_AMT, 0.6f); m.SetParameterAsMagnitude(M::MOD_RATE, 0.8f); break; // delay level
    case 8: m.SetParameterAsBinnedValue(M::MOD_PARAM, 4); m.SetParameterAsBool(M::SYNC_MOD_F, true); m.SetParameterAsMagnitude(M::MOD_AMT, 0.8f); break; // reverb level, synced
    case 9: m.SetParameterAsBinnedValue(M::MOD_PARAM, 5); m.SetParameterAsBinnedValue(M::MOD_WAVE, 3); m.SetParameterAsMagnitude(M::MOD_AMT, 0.8f); m.SetParameterAsMagnitude(M::MOD_RATE, 0.4f); break; // delay pan
    case 10: m.SetParameterAsBinnedValue(M::DELAY_MODE, 2); m.SetParameterAsMagnitude(M::D_FEEDBACK, 0.6f); break; // triplett
    case 11: m.SetParameterAsBinnedValue(M::DELAY_MODE, 3); break;                       // dotted eighth
    case 12: // everything
        m.SetParameterAsBool(M::REVERSE, true); m.SetParameterAsBool(M::OCTAVE, true); m.SetParameterAsBool(M::DUAL_DELAY, true);
        m.SetParameterAsMagnitude(M::D_SPREAD, 0.3f); m.SetParameterAsBinnedValue(M::MOD_PARAM, 2); m.SetParameterAsMagnitude(M::MOD_AMT, 0.5f);
        break;
    }
}

int main(int argc, char **argv) {
    const char *mode = argv[1];
    const int cfg = atoi(argv[2]);
    const size_t block = (size_t)atoi(argv[3]);
    const size_t n = 48 * 3000; // 3 s
    std::vector<float> inL(n), inR(n), outL(n), outR(n);
    srand(11);
    for (size_t i = 0; i < n; i++) {
        const float burst = (i % 24000) < 6000 ? 1.0f : 0.15f; // notes with gaps, so the echoes and the reverb tail are audible
        inL[i] = burst * (0.3f * sinf(6.2831853f * 220 * i / 48000.0f) + 0.1f * ((rand() / (float)RAND_MAX) - 0.5f));
        inR[i] = burst * (0.25f * sinf(6.2831853f * 330 * i / 48000.0f) + 0.1f * ((rand() / (float)RAND_MAX) - 0.5f));
    }
    M *m = new M();
    m->Init(48000.0f);
    configure(*m, cfg);
    if (!strcmp(mode, "sample")) {
        for (size_t i = 0; i < n; i++) { m->ProcessMono(inL[i]); outL[i] = m->GetAudioLeft(); outR[i] = m->GetAudioRight(); }
    } else if (!strcmp(mode, "samplest")) {
        for (size_t i = 0; i < n; i++) { m->ProcessStereo(inL[i], inR[i]); outL[i] = m->GetAudioLeft(); outR[i] = m->GetAudioRight(); }
    } else {
        for (size_t o = 0; o < n; o += block) {
            const size_t k = (o + block <= n) ? block : n - o;
            if (!strcmp(mode, "mono")) m->ProcessMonoBlock(&inL[o], &outL[o], &outR[o], k);
            else m->ProcessStereoBlock(&inL[o], &inR[o], &outL[o], &outR[o], k);
        }
    }
    float peak = 0;
    for (size_t i = 0; i < n; i++) peak = fmaxf(peak, fmaxf(fabsf(outL[i]), fabsf(outR[i])));
    FILE *f = fopen(argv[4], "wb"); fwrite(outL.data(), 4, n, f); fwrite(outR.data(), 4, n, f); fclose(f);
    printf("peak %.4f\n", peak);
    return peak > 0.001f && std::isfinite(peak) ? 0 : 1;
}
