// Default-level test for DuckCompressor at default parameters: a mono input is its own key (the usual -37 dBFS playing level is below the -30 dB threshold), so the audio only gets the +3 dB make-up gain.
#include "level_common.h"
#include "duck_compressor_module.h"
#include <cstdlib>
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

static std::unique_ptr<DuckCompressorModule> make() {
    auto m = std::make_unique<DuckCompressorModule>();
    m->Init(kSampleRate); // every parameter at its metadata default
    return m;
}

int main() {
    // Project rule: +0..+6 dB at default settings, never attenuating; compression aims for +3 dB.
    const Window w{0.0, 6.0, 3.0, 3.0};
    bool ok = true;
    ok &= report("DuckCompressor, default parameters", [] { return make(); }, w, Signal::Guitar);
    report("DuckCompressor, default parameters", [] { return make(); }, w, Signal::Sine, /*enforce=*/false);
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
