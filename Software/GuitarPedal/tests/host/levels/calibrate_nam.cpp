// Measures every NAM model for update_models.sh --calibrate: the mean RMS gain in dB of the guitar-like test signal (110 Hz and 330 Hz
// harmonic series) at the reference input level, with the module at its default parameters and the IR cab Off, i.e. the number
// test_level_nam.cpp enforces. Prints one line per model, in the order of the MODEL choices: "<index> <name> <gain in dB>".
#include "level_common.h"
#include "nam_a2_module.h"
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

int main() {
    NamA2Module probe;
    const int count = probe.GetParameterBinCount(NamA2Module::MODEL);
    const char **names = probe.GetParameterBinNames(NamA2Module::MODEL);
    for (int model = 0; model < count; model++) {
        const double db = referenceGainDb(
            [model] {
                auto m = std::make_unique<NamA2Module>();
                m->Init(kSampleRate);
                m->SetParameterRaw(NamA2Module::MODEL, model);
                m->SetParameterRaw(NamA2Module::IR_CAB, 0);
                return m;
            },
            Signal::Guitar);
        std::printf("%d %s %.4f\n", model, names[model], db);
    }
    return 0;
}
