// Default-level test for Venus at default parameters. The wet signal arrives after the STFT latency (about 107 ms) and then builds up, so this measures the
// last half second of the one second test signal, by which time the reverb is established.
#include "level_common.h"
#include "venus_module.h"
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

static std::unique_ptr<VenusModule> make() {
    auto m = std::make_unique<VenusModule>();
    m->Init(kSampleRate); // every parameter at its metadata default
    return m;
}

int main() {
    // Project rule: +0..+6 dB at default settings, never attenuating. A reverb mixes the dry signal with its own, so it aims for the middle of the window.
    const Window w{0.0, 6.0, 3.0, 3.0};
    bool ok = true;
    ok &= report("Venus, default parameters", [] { return make(); }, w, Signal::Guitar);
    report("Venus, default parameters", [] { return make(); }, w, Signal::Sine, /*enforce=*/false);
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
