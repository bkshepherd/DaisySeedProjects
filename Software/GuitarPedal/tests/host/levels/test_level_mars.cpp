// Default-level test for Mars at default parameters (amp model 1 with its cabinet, the delay at 0.35 with 30% feedback and a 0.3 mix). A distortion-like effect: it aims for +3 dB.
// The other models and cabinets are reported for information.
#include "level_common.h"
#include "mars_module.h"
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

static std::unique_ptr<MarsModule> make(int model, int cab) {
    auto m = std::make_unique<MarsModule>();
    m->Init(kSampleRate); // every parameter at its metadata default
    m->SetParameterRaw(MarsModule::MODEL, model);
    m->SetParameterRaw(MarsModule::IR, cab);
    return m;
}

int main() {
    const Window w{0.0, 6.0, 3.0, 3.0};
    bool ok = true;
    ok &= report("Mars, default parameters", [] { return make(0, 0); }, w, Signal::Guitar);
    report("Mars, default parameters", [] { return make(0, 0); }, w, Signal::Sine, /*enforce=*/false);
    std::printf("\nother models and cabinets (informational): guitar-like signal, mean RMS gain at the reference level, dB\n");
    std::printf("    %-10s %10s %10s %10s\n", "model", "Proteus", "US Deluxe", "Vox Bright");
    for (int model = 0; model < 3; model++) {
        std::printf("    %-10d", model + 1);
        for (int cab = 0; cab < 3; cab++) std::printf(" %+10.2f", referenceGainDb([model, cab] { return make(model, cab); }, Signal::Guitar));
        std::printf("\n");
    }
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
