// Default-level test for Uranus at default parameters (the synth mode is off: audio in). The grains are cut from the last half second of the input, so this measures the last half
// second of the one second test signal, by which time the buffer is filled.
#include "level_common.h"
#include "uranus_module.h"
#include <cstdlib>
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

static std::unique_ptr<UranusModule> make() {
    srand(1); // the grain players pick start positions with rand()
    auto m = std::make_unique<UranusModule>();
    m->Init(kSampleRate); // every parameter at its metadata default
    return m;
}

int main() {
    // Project rule: +0..+6 dB at default settings, never attenuating. A delay mixes the dry signal with its own, so it aims for the middle of the window.
    const Window w{0.0, 6.0, 3.0, 3.0};
    bool ok = true;
    ok &= report("Uranus, default parameters", [] { return make(); }, w, Signal::Guitar);
    report("Uranus, default parameters", [] { return make(); }, w, Signal::Sine, /*enforce=*/false);
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
