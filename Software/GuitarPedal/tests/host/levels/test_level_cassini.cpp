// Default-level test for Cassini at default parameters: no sample is recorded and no key is played, so it passes the input through at unity gain (0 dB).
#include "level_common.h"
#include "cassini_module.h"
#include <cstdlib>
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

static std::unique_ptr<CassiniModule> make() {
    auto m = std::make_unique<CassiniModule>();
    m->Init(kSampleRate); // every parameter at its metadata default
    return m;
}

int main() {
    // Project rule: +0..+6 dB at default settings, never attenuating. 
    const Window w{0.0, 6.0, 3.0, 3.0};
    bool ok = true;
    ok &= report("Cassini, default parameters", [] { return make(); }, w, Signal::Guitar);
    report("Cassini, default parameters", [] { return make(); }, w, Signal::Sine, /*enforce=*/false);
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
