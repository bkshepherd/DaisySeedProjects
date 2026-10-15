// Default-level test for the Compressor.
// Init() applies every parameter to the DaisySP compressor, so the state at startup matches what the menu shows. A regression check
// confirms that: touching every parameter away and back to its default must not change the measured gain.
#include "compressor_module.h"
#include "level_common.h"
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

static std::unique_ptr<CompressorModule> make(bool touchParameters) {
    auto m = std::make_unique<CompressorModule>();
    m->Init(kSampleRate); // every parameter at its metadata default
    if (touchParameters) {
        // SetParameter* only calls ParameterChanged when the value differs, so move each parameter away and back to its default
        for (int p : {CompressorModule::LEVEL, CompressorModule::RATIO, CompressorModule::THRESHOLD, CompressorModule::ATTACK,
                      CompressorModule::RELEASE}) {
            const float def = m->GetParameterDefaultValueAsFloat(p);
            m->SetParameterAsMagnitude(p, 0.0f);
            m->SetParameterAsFloat(p, def);
        }
    }
    return m;
}

int main() {
    // Project rule: +0..+6 dB at default settings, never attenuating; compression aims for +3 dB.
    const Window w{0.0, 6.0, 3.0, 1.0};
    bool ok = true;
    ok &= report("Compressor, default parameters", [] { return make(false); }, w, Signal::Guitar);
    report("Compressor, default parameters", [] { return make(false); }, w, Signal::Sine, /*enforce=*/false);

    // Startup state must equal the state after touching every parameter back to its default
    const double atInit = referenceGainDb([] { return make(false); }, Signal::Guitar);
    const double afterTouch = referenceGainDb([] { return make(true); }, Signal::Guitar);
    const bool same = std::fabs(atInit - afterTouch) < 0.01;
    std::printf("\nstartup state vs touched-to-default state: %+.3f dB vs %+.3f dB  =>  %s\n", atInit, afterTouch,
                same ? "OK (defaults are applied at startup)" : "MISMATCH (defaults are not applied at startup)");
    ok &= same;

    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
