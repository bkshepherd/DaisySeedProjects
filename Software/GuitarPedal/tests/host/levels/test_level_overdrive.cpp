// Default-level test for the Overdrive effect at default parameters.
#include "level_common.h"
#include "overdrive_module.h"
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

static std::unique_ptr<OverdriveModule> make() {
    auto m = std::make_unique<OverdriveModule>();
    m->Init(kSampleRate); // every parameter at its metadata default
    return m;
}

int main() {
    // Project rule: +0..+6 dB at default settings, never attenuating; overdrive is distortion-like, so it aims for +3 dB.
    const Window w{0.0, 6.0, 3.0, 1.0};
    bool ok = true;
    ok &= report("Overdrive, default parameters", [] { return make(); }, w, Signal::Guitar);
    report("Overdrive, default parameters", [] { return make(); }, w, Signal::Sine, /*enforce=*/false);
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
