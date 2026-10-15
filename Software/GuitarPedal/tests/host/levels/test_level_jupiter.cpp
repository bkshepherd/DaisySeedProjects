// Default-level test for Jupiter at default parameters. The reverb builds up over the test signal, so this measures the last half second, by which time it
// is established.
#include "level_common.h"
#include "jupiter_module.h"
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

extern size_t pool_index; // the reverb engine's memory pool never frees; each instance takes about 4.9 MB

static std::unique_ptr<JupiterModule> make() {
    pool_index = 0;
    auto m = std::make_unique<JupiterModule>(); // (leaks the previous instance's engine objects; irrelevant for a test)
    m->Init(kSampleRate); // every parameter at its metadata default
    return m;
}

int main() {
    // Project rule: +0..+6 dB at default settings, never attenuating. A reverb mixes the dry signal with its own, so it aims for the middle of the window.
    const Window w{0.0, 6.0, 3.0, 3.0};
    bool ok = true;
    ok &= report("Jupiter, default parameters", [] { return make(); }, w, Signal::Guitar);
    report("Jupiter, default parameters", [] { return make(); }, w, Signal::Sine, /*enforce=*/false);
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
