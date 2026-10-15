// Default-level test for Mercury and each of its nine models: gain at default parameters (EQ flat). Amp models are distortion-like: they aim for +3 dB.
#include "level_common.h"
#include "mercury_module.h"
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

static std::unique_ptr<MercuryModule> make(int model) {
    auto m = std::make_unique<MercuryModule>();
    m->Init(kSampleRate); // every parameter at its metadata default
    m->SetParameterRaw(MercuryModule::MODEL, model); // raw value is the zero-based bin index
    return m;
}

int main() {
    const Window w{0.0, 6.0, 3.0, 3.0};
    MercuryModule probe;
    const int count = probe.GetParameterBinCount(MercuryModule::MODEL);
    const char **names = probe.GetParameterBinNames(MercuryModule::MODEL);
    bool ok = true;
    for (int model = 0; model < count; model++) {
        char title[96];
        std::snprintf(title, sizeof title, "Mercury model %d: %s, default parameters", model, names[model]);
        // The guitar-like signal is what the window is enforced on: amp models shape the tone strongly, so two pure tones can differ a lot.
        ok &= report(title, [model] { return make(model); }, w, Signal::Guitar);
        report(title, [model] { return make(model); }, w, Signal::Sine, /*enforce=*/false);
    }
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
