// Level test for the Booster. It is not level-matched like the distortions (+6 dB at default settings): Gain is the boost where the type works,
// so the types differ in how much of a whole guitar signal they raise. At the default Gain (12 dB) the guitar-like signal (a harmonic series of
// a low note, most of its energy in the lows) comes out as follows, and the test keeps it there (and never attenuating, never reaching full scale):
//   Clean   +12 dB   it boosts everything
//   Treble  +3 dB    the bass is cut, so only the upper partials are boosted by 12 dB
//   Mid     +4 dB    only the bell around 1.1 kHz is boosted
//   Warm    +13 dB   everything, and 3 dB more of the lows
#include "booster_module.h"
#include "level_common.h"
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

static std::unique_ptr<BoosterModule> make(int type) {
    auto m = std::make_unique<BoosterModule>();
    m->Init(kSampleRate); // every parameter at its default (Gain 12 dB, Tone 50%, Level 0 dB)
    m->SetParameterRaw(BoosterModule::TYPE, type);
    return m;
}

int main() {
    struct Row {
        const char *name;
        int type;
        Window window;
    };
    const Row rows[] = {{"Booster, Clean", BoosterModule::CLEAN, {0.0, 16.0, 12.0, 0.5}},
                        {"Booster, Treble", BoosterModule::TREBLE, {0.0, 16.0, 2.7, 1.0}},
                        {"Booster, Mid", BoosterModule::MID, {0.0, 16.0, 3.7, 1.0}},
                        {"Booster, Warm", BoosterModule::WARM, {0.0, 16.0, 13.1, 1.0}}};
    bool ok = true;
    for (const Row &r : rows) {
        const int type = r.type;
        ok &= report(r.name, [type] { return make(type); }, r.window, Signal::Guitar);
    }
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
