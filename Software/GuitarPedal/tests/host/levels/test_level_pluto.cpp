// Default-level test for Pluto at default parameters: the loopers are empty, so it passes the input through at unity gain (0 dB). The dry signal is the whole output until a loop is recorded.
#include "level_common.h"
#include "pluto_module.h"
#include <cstdlib>
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

static std::unique_ptr<PlutoModule> make() {
    auto m = std::make_unique<PlutoModule>();
    m->Init(kSampleRate); // every parameter at its metadata default
    return m;
}

int main() {
    // Project rule: +0..+6 dB at default settings, never attenuating. 
    const Window w{0.0, 6.0, 3.0, 3.0};
    bool ok = true;
    ok &= report("Pluto, default parameters", [] { return make(); }, w, Signal::Guitar);
    report("Pluto, default parameters", [] { return make(); }, w, Signal::Sine, /*enforce=*/false);
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
