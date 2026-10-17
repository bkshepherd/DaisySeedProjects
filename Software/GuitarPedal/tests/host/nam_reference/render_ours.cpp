// Renders a float32 mono file through our NAM effect: render_ours <model index> <in.f32> <out.f32>
//   built with -DPLAYER: nam_a2_daisy::A2Player alone (the network); otherwise NamA2Module's block path with Gain and Level at unity
//   (the parameter value 0.5 gives a factor of 1), EQ and cab IR off (the output still has the model's outputGain applied: compare.py divides it out).
// The runtime header defines non-inline functions, so the player build does not link the module. The model is a process-wide static: one process per run.
#include <cstdio>
#include <cstdlib>
#include <vector>
#ifdef PLAYER
#include "Nam/model_data_nam_a2.h"
#include "Nam/nam_a2_runtime.h"
#else
#include "nam_a2_module.h"
using namespace bkshepherd;
#endif
int main(int argc, char **argv) {
    const int idx = atoi(argv[1]);
    std::vector<float> in;
    { FILE *f = fopen(argv[2], "rb"); float x; while (fread(&x, 4, 1, f) == 1) in.push_back(x); fclose(f); }
    std::vector<float> out(in.size(), 0.0f);
    const int B = 48;
#ifdef PLAYER
    static nam_a2_daisy::A2Player player;
    player.load_weights(nam_a2_models::kNamA2Models[idx].weights, nam_a2_daisy::kA2WeightCount);
    for (size_t o = 0; o + B <= in.size(); o += B) player.process_block_48(&in[o], &out[o]);
#else
    std::vector<float> dummy(in.size());
    NamA2Module *m = new NamA2Module();
    m->Init(48000.0f);
    m->SetParameterAsBinnedValue(NamA2Module::MODEL, idx + 1);
    m->SetParameterAsFloat(NamA2Module::GAIN, 0.5f);
    m->SetParameterAsFloat(NamA2Module::LEVEL, 0.5f);
    m->SetParameterRaw(NamA2Module::EQ, 0);
    m->SetParameterRaw(NamA2Module::IR_CAB, 0);
    for (size_t o = 0; o + B <= in.size(); o += B) m->ProcessMonoBlock(&in[o], &out[o], &dummy[o], B);
#endif
    FILE *f = fopen(argv[3], "wb"); fwrite(out.data(), 4, out.size(), f); fclose(f);
    return 0;
}
