// Default-level test for the Crusher: the gain at default parameters (8 bits, 8000 Hz, 6000 Hz cutoff) against the project's rule. As a
// distortion-like effect it aims for +6 dB, the target of the Distortion, the Overdrive and the distortion family.
#include "crusher_module.h"
#include "level_common.h"
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

int main() {
    const Window w{0.0, 8.0, 6.0, 1.0};
    auto make = [] {
        auto m = std::make_unique<CrusherModule>();
        m->Init(kSampleRate); // every parameter at its metadata default
        return m;
    };
    bool ok = report("Crusher, default parameters", make, w);
    report("Crusher, default parameters", make, w, Signal::Guitar, /*enforce=*/false);
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "OUT OF WINDOW");
    return ok ? 0 : 1;
}
