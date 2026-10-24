// Level test for the Multi Phaser. A phaser adds the dry signal to the phase-shifted one in equal parts, so it takes some level off a signal with energy in the
// notches (for noise 3 dB for every phaser in series), and gives it back at the peaks between them; the feedback of a phaser is level-corrected (its output is
// scaled so that its energy for noise is that of a phaser without feedback), and every phaser makes up for what the sum takes off (kMakeup in the module). The test
// plays the guitar-like signal through the default settings (two phasers in series), a single phaser, four phasers in series and in parallel, and the extremes of the
// feedback. The rule of the project, the default level +0 to +3 dB and never attenuating, holds for the default and for one phaser; more phasers in series add
// their make-up (four are about +5 dB), in parallel they average, and a strong positive feedback takes some level off (the resonance is narrow: -2.5 dB) while a
// negative one adds some (+2.5 dB).
#include "level_common.h"
#include "multi_phaser_module.h"
#include <memory>

using namespace bkshepherd;
using namespace leveltest;
using M = MultiPhaserModule;

static std::unique_ptr<M> make(int units, int routing, float feedback) {
    auto m = std::make_unique<M>();
    m->SetParameterRaw(M::ROUTING, routing);
    for (int u = 0; u < M::kUnits; u++) {
        m->SetParameterRaw(M::UnitParamId(u, M::U_ON), u < units ? 1 : 0);
        if (feedback >= 0.0f) m->SetParameterAsFloat(M::UnitParamId(u, M::U_FEEDBACK), feedback);
    }
    m->Init(kSampleRate); // the rest at the metadata defaults
    return m;
}

int main() {
    struct Row {
        const char *name;
        int units, routing;
        float feedback; // stored value; -1: the default
        Window window;
    };
    const Row rows[] = {{"Multi Phaser, default (two phasers in series)", 2, M::SERIES, -1.0f, {0.0, 3.0, 2.0, 1.0}},
                        {"Multi Phaser, one phaser", 1, M::SERIES, -1.0f, {0.0, 3.0, 1.5, 1.5}},
                        {"Multi Phaser, four phasers in series", 4, M::SERIES, -1.0f, {0.0, 7.0, 5.4, 1.5}},
                        {"Multi Phaser, four phasers in parallel", 4, M::PARALLEL, -1.0f, {-1.5, 3.0, 0.0, 1.5}},
                        {"Multi Phaser, two phasers, feedback +85%", 2, M::SERIES, 1.0f, {-4.0, 0.0, -2.5, 1.5}},
                        {"Multi Phaser, two phasers, feedback -85%", 2, M::SERIES, 0.0f, {0.0, 4.0, 2.5, 1.5}}};
    bool ok = true;
    for (const Row &r : rows) {
        const Row row = r;
        ok &= report(r.name, [row] { return make(row.units, row.routing, row.feedback); }, r.window, Signal::Guitar);
    }
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
