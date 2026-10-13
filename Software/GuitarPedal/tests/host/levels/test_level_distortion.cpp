// Default-level test for the Distortion effect: gain at default parameters, for every distortion type.
#include "distortion_module.h"
#include "level_common.h"
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

static std::unique_ptr<DistortionModule> make(int type) {
    auto m = std::make_unique<DistortionModule>();
    m->Init(kSampleRate); // every parameter at its metadata default
    if (type >= 0) m->SetParameterRaw(DistortionModule::DIST_TYPE, type); // otherwise keep the default type
    return m;
}

int main() {
    // Project rule: +0..+6 dB at default settings, never attenuating; distortion and compression aim for +3 dB.
    const Window w{0.0, 6.0, 3.0, 1.0};
    const char *types[] = {"Hard Clip", "Soft Clip", "Fuzz", "Tube", "Multi Stage", "Diode Clip"};
    bool ok = true;

    // The default configuration is what the rule applies to.
    ok &= report("Distortion, default parameters (Type = default)", [] { return make(-1); }, w);
    // Informational: the same on the guitar-like signal (a saturating effect can read differently on sines and on a peaky harmonic signal)
    report("Distortion, default parameters (Type = default)", [] { return make(-1); }, w, Signal::Guitar, /*enforce=*/false);

    // Informational: the other types at otherwise-default settings, so they can be kept consistent with the default one.
    std::printf("\nOther distortion types, other parameters at default (informational, same window shown):\n");
    for (int t = 0; t < 6; t++) {
        char title[64];
        std::snprintf(title, sizeof title, "Type %d: %s", t, types[t]);
        ok &= report(title, [t] { return make(t); }, w);
    }
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
