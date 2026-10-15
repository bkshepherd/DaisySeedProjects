// Default-level test for Balnea at default parameters. The delay time is 500 ms and the repeats are filtered, so this measures the last half second of the one second test signal.
#include "level_common.h"
#include "balnea_module.h"
#include <cstdlib>
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

static std::unique_ptr<BalneaModule> make() {
    auto m = std::make_unique<BalneaModule>();
    m->Init(kSampleRate); // every parameter at its metadata default
    return m;
}

int main() {
    // Project rule: +0..+6 dB at default settings, never attenuating. A delay mixes the dry signal with its own, so it aims for the middle of the window.
    const Window w{0.0, 6.0, 3.0, 3.0};
    bool ok = true;
    ok &= report("Balnea, default parameters", [] { return make(); }, w, Signal::Guitar);
    report("Balnea, default parameters", [] { return make(); }, w, Signal::Sine, /*enforce=*/false);
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
