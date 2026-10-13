// Default-level test for the NAM module and each bundled amp model: gain at default parameters (IR cab Off).
// The IR cab options are reported for information. Every measurement uses a fresh module, which reloads and resets the model.
#include "level_common.h"
#include "nam_a2_module.h"
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

static std::unique_ptr<NamA2Module> make(int model, int irCab) {
    auto m = std::make_unique<NamA2Module>();
    m->Init(kSampleRate);                                  // every parameter at its metadata default
    m->SetParameterRaw(NamA2Module::MODEL, model);         // raw value is the zero-based bin index
    m->SetParameterRaw(NamA2Module::IR_CAB, irCab);
    return m;
}

int main() {
    // Project rule: +0..+6 dB at default settings, never attenuating. Amp models are distortion-like, so they aim for +3 dB.
    const Window w{0.0, 11.0, 9.0, 1.0};
    // Model count and names come from the module's own parameter metadata (the model table header defines non-inline functions, so it
    // can only be included once).
    NamA2Module probe;
    const int count = probe.GetParameterBinCount(NamA2Module::MODEL);
    const char **names = probe.GetParameterBinNames(NamA2Module::MODEL);
    bool ok = true;

    for (int model = 0; model < count; model++) {
        char title[96];
        std::snprintf(title, sizeof title, "NAM model %d: %s, default parameters, IR cab Off", model, names[model]);
        // The guitar-like signal is what the window is enforced on: amp models shape the tone strongly, so two pure tones can differ a lot.
        ok &= report(title, [model] { return make(model, 0); }, w, Signal::Guitar);
        report(title, [model] { return make(model, 0); }, w, Signal::Sine, /*enforce=*/false);
    }

    std::printf("\nIR cab options at otherwise-default settings (informational): guitar-like signal, mean RMS gain at the reference level, dB\n");
    std::printf("    %-10s %8s %8s %8s\n", "model", "Off", "Rhythm", "Lead");
    for (int model = 0; model < count; model++) {
        std::printf("    %-10s", names[model]);
        for (int ir = 0; ir < 3; ir++) std::printf(" %+8.2f", referenceGainDb([model, ir] { return make(model, ir); }, Signal::Guitar));
        std::printf("\n");
    }

    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
