// Default-level test for Saturn at default parameters. The wet signal arrives after the transform's latency, so this measures the last half second of the one
// second test signal, by which time the delay is established.
#include "level_common.h"
#include "saturn_module.h"
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

static std::unique_ptr<SaturnModule> make() {
    auto m = std::make_unique<SaturnModule>();
    m->Init(kSampleRate); // every parameter at its metadata default
    return m;
}

int main() {
    // Project rule: +0..+6 dB at default settings, never attenuating. A delay mixes the dry signal with its own, so it aims for the middle of the window.
    const Window w{0.0, 6.0, 3.0, 3.0};
    bool ok = true;
    ok &= report("Saturn, default parameters", [] { return make(); }, w, Signal::Guitar);
    report("Saturn, default parameters", [] { return make(); }, w, Signal::Sine, /*enforce=*/false);
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
