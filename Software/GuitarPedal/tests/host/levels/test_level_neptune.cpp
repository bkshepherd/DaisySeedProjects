// Default-level test for Neptune at default parameters. The reverb and the delay build up over the test signal, so this measures the last half second.
#include "level_common.h"
#include "neptune_module.h"
#include <cstdlib>
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

extern size_t pool_index; // the reverb engine's memory pool never frees; each instance takes about 4.9 MB

static std::unique_ptr<NeptuneModule> make() {
    pool_index = 0;
    auto m = std::make_unique<NeptuneModule>(); // (leaks the previous instance's engine objects; irrelevant for a test)
    m->Init(kSampleRate); // every parameter at its metadata default
    return m;
}

int main() {
    // Project rule: +0..+6 dB at default settings, never attenuating. A reverb and delay mix the dry signal with their own, so it aims for the middle of the window.
    const Window w{0.0, 6.0, 3.0, 3.0};
    bool ok = true;
    ok &= report("Neptune, default parameters", [] { return make(); }, w, Signal::Guitar);
    report("Neptune, default parameters", [] { return make(); }, w, Signal::Sine, /*enforce=*/false);
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
