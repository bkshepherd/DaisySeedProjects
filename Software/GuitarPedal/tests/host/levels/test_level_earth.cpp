// Default-level test for Earth at default parameters (octave off). The reverb builds up over the first moments of the signal, so this measures the last half
// second of the one second test signal.
#include "earth_module.h"
#include "level_common.h"
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

static std::unique_ptr<EarthModule> make() {
    auto m = std::make_unique<EarthModule>();
    m->Init(kSampleRate); // every parameter at its metadata default
    return m;
}

int main() {
    // Project rule: +0..+6 dB at default settings, never attenuating. A reverb mixes the dry signal with its own, so it aims for the lower-middle of the window.
    const Window w{0.0, 6.0, 2.0, 2.0};
    bool ok = true;
    ok &= report("Earth, default parameters", [] { return make(); }, w, Signal::Guitar);
    report("Earth, default parameters", [] { return make(); }, w, Signal::Sine, /*enforce=*/false);
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
