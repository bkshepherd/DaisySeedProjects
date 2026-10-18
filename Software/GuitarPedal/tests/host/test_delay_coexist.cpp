// Delay and Reverb Delay in the same program: both headers are included (they used to define the same names) and both modules are linked
// (their delay lines used to have the same global names). Each module gets its own signal and must give exactly the output it gives alone.
// test_delay_coexist <both|delay|reverbdelay> <outfile>   (the modules keep their delay lines in static memory: one process per run)
#include "delay_module.h"
#include "reverb_delay_module.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace bkshepherd;
int main(int argc, char **argv) {
    const bool runDelay = strcmp(argv[1], "reverbdelay") != 0, runRd = strcmp(argv[1], "delay") != 0;
    const size_t n = 48000 * 2;
    DelayModule *d = new DelayModule(); d->Init(48000.0f);
    ReverbDelayModule *r = new ReverbDelayModule(); r->Init(48000.0f);
    d->SetParameterAsMagnitude(DelayModule::DELAY_TIME, 0.03f);
    r->SetParameterAsMagnitude(ReverbDelayModule::DELAY_TIME, 0.05f);
    std::vector<float> outD(n), outR(n);
    for (size_t i = 0; i < n; i++) {
        const float burst = (i % 24000) < 6000 ? 1.0f : 0.1f;
        const float inD = burst * 0.3f * sinf(6.2831853f * 220 * i / 48000.0f); // different signals for the two modules
        const float inR = burst * 0.3f * sinf(6.2831853f * 330 * i / 48000.0f);
        if (runDelay) { d->ProcessMono(inD); outD[i] = d->GetAudioLeft(); }
        if (runRd) { r->ProcessMono(inR); outR[i] = r->GetAudioLeft(); }
    }
    float pd = 0, pr = 0;
    for (size_t i = 0; i < n; i++) { pd = fmaxf(pd, fabsf(outD[i])); pr = fmaxf(pr, fabsf(outR[i])); }
    FILE *f = fopen(argv[2], "wb"); fwrite(outD.data(), 4, n, f); fwrite(outR.data(), 4, n, f); fclose(f);
    printf("delay peak %.4f, reverb delay peak %.4f\n", pd, pr);
    return 0;
}
