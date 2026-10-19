// Default-level test for the distortion family (Preamp, Screamer, Muff Fuzz, Wavefolder, Exciter, Bass Dist, Octa Fuzz, Tape Sat): the gain at default
// parameters, for every mode, against the project's rule. The distortion types aim for +6 dB (as the Distortion effect), the exciter and the tape
// saturator, which hardly change the level, for +3 dB.
#include "bass_dist_module.h"
#include "exciter_module.h"
#include "level_common.h"
#include "muff_fuzz_module.h"
#include "octave_fuzz_module.h"
#include "preamp_module.h"
#include "screamer_module.h"
#include "tape_sat_module.h"
#include "wavefolder_module.h"
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

template <class M>
static std::unique_ptr<M> make(int modeParam = -1, int mode = 0) {
    auto m = std::make_unique<M>();
    m->Init(kSampleRate); // every parameter at its metadata default
    if (modeParam >= 0) m->SetParameterRaw(modeParam, mode);
    return m;
}

int main() {
    const Window loud{0.0, 8.0, 6.0, 1.0};
    const Window gentle{0.0, 6.0, 3.0, 1.0};
    bool ok = true;

    for (int v = 0; v < 3; v++) {
        char t[64];
        std::snprintf(t, sizeof t, "Preamp, voicing %d", v);
        ok &= report(t, [v] { return make<PreampModule>(PreampModule::VOICING, v); }, loud);
    }
    for (int m = 0; m < 3; m++) {
        char t[64];
        std::snprintf(t, sizeof t, "Screamer, mode %d", m);
        ok &= report(t, [m] { return make<ScreamerModule>(ScreamerModule::MODE, m); }, loud);
    }
    for (int m = 0; m < 3; m++) {
        char t[64];
        std::snprintf(t, sizeof t, "Muff Fuzz, mode %d", m);
        ok &= report(t, [m] { return make<MuffFuzzModule>(MuffFuzzModule::MODE, m); }, loud);
    }
    ok &= report("Wavefolder", [] { return make<WavefolderModule>(); }, loud);
    ok &= report("Exciter", [] { return make<ExciterModule>(); }, gentle);
    for (int c = 0; c < 3; c++) {
        char t[64];
        std::snprintf(t, sizeof t, "Bass Dist, clip %d", c);
        ok &= report(t, [c] { return make<BassDistModule>(BassDistModule::CLIP, c); }, loud);
    }
    ok &= report("Octa Fuzz", [] { return make<OctaveFuzzModule>(); }, loud);
    for (int m = 0; m < 2; m++) {
        char t[64];
        std::snprintf(t, sizeof t, "Tape Sat, mode %d", m);
        ok &= report(t, [m] { return make<TapeSatModule>(TapeSatModule::MODE, m); }, gentle);
    }

    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
