// Default-level test for the distortion family (Preamp, Screamer, Muff Fuzz, Wavefolder, Exciter, Bass Dist, Octa Fuzz, Tape Sat, Chainsaw): the gain at default
// parameters, for every mode, against the project's rule. The distortion types aim for +6 dB (as the Distortion effect), the exciter and the tape
// saturator, which hardly change the level, for +3 dB.
#include "bass_dist_module.h"
#include "chainsaw_module.h"
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

// For an effect that is already saturated at the reference level: its level is matched to the Preamp's Crunch at the loudest test level
// (-12 dBFS peak, where both are saturated and only their output ceilings count), within 1 dB over both signals and both test frequencies, and it
// must never reach full scale (the report at the reference level is then only informational, so that check is done here).
template <class Make>
static bool matchedToCrunch(const char *name, Make make) {
    constexpr int kHot = 3; // -12 dBFS peak
    double diff = 0, peak = 0;
    for (Signal s : {Signal::Sine, Signal::Guitar})
        for (int fi = 0; fi < 2; fi++) {
            const float f = frequencies(s)[fi];
            auto crunch = ::make<PreampModule>(PreampModule::VOICING, 1);
            auto m = make();
            diff += measure(*m, s, f, kInputs[kHot].peak).rmsGainDb - measure(*crunch, s, f, kInputs[kHot].peak).rmsGainDb;
            for (const InputLevel &in : kInputs) {
                auto fresh = make();
                peak = std::fmax(peak, measure(*fresh, s, f, in.peak).outPeak);
            }
        }
    diff /= 4.0;
    const bool ok = std::fabs(diff) <= 1.0 && peak < 1.0;
    std::printf("%s vs Preamp Crunch at %s (both saturated): mean %+.2f dB, target 0 +/- 1 dB; output peak %.2f  =>  %s\n", name, kInputs[kHot].label, diff,
                peak, ok ? "OK" : "OUT OF WINDOW");
    return ok;
}

int main() {
    const Window loud{0.0, 8.0, 6.0, 1.0};
    const Window gentle{0.0, 6.0, 3.0, 1.0};
    bool ok = true;

    // Preamp: Clean and Crunch follow the rule at the reference level. Lead does not: it has so much gain that it is already saturated at the
    // reference level, while Crunch is still nearly clean there, so matching the two at -37 dBFS made Lead about 10 dB quieter than Crunch
    // when played with a guitar (above about -20 dBFS both are saturated and only their output ceilings count). Lead is therefore matched to
    // Crunch at the loudest test level (-12 dBFS peak, both saturated), as a channel switch on an amp would be; its gain at the reference level
    // (about +16 dB) is printed for information. Lead is louder than Crunch on soft playing and the tail of a note, as a high-gain voicing is.
    for (int v = 0; v < 3; v++) {
        char t[64];
        std::snprintf(t, sizeof t, "Preamp, voicing %d%s", v, v == 2 ? " (matched to Crunch at -12 dBFS, see below)" : "");
        ok &= report(t, [v] { return make<PreampModule>(PreampModule::VOICING, v); }, loud, Signal::Sine, v != 2);
    }
    ok &= matchedToCrunch("Preamp Lead", [] { return make<PreampModule>(PreampModule::VOICING, 2); });
    for (int m = 0; m < 3; m++) {
        char t[64];
        std::snprintf(t, sizeof t, "Screamer, mode %d", m);
        ok &= report(t, [m] { return make<ScreamerModule>(ScreamerModule::MODE, m); }, loud);
    }
    // Muff Fuzz: like the Lead and the Chainsaw, matched to Crunch when saturated. Its clipping saturates at any Sustain from about -45 dBFS, so
    // matched to +6 dB at the reference level it was about 11 dB quieter than Crunch when played hard. Every mode is checked (they differ only
    // in the tone corner and come out at the same level, so they share one makeup).
    for (int m = 0; m < 3; m++) {
        char t[64];
        std::snprintf(t, sizeof t, "Muff Fuzz, mode %d (matched to Crunch at -12 dBFS, see below)", m);
        report(t, [m] { return make<MuffFuzzModule>(MuffFuzzModule::MODE, m); }, loud, Signal::Sine, false);
        std::snprintf(t, sizeof t, "Muff Fuzz, mode %d", m);
        ok &= matchedToCrunch(t, [m] { return make<MuffFuzzModule>(MuffFuzzModule::MODE, m); });
    }
    ok &= report("Wavefolder", [] { return make<WavefolderModule>(); }, loud);
    ok &= report("Exciter", [] { return make<ExciterModule>(); }, gentle);
    for (int c = 0; c < 3; c++) {
        char t[64];
        std::snprintf(t, sizeof t, "Bass Dist, clip %d", c);
        ok &= report(t, [c] { return make<BassDistModule>(BassDistModule::CLIP, c); }, loud);
    }
    ok &= report("Octa Fuzz", [] { return make<OctaveFuzzModule>(); }, loud);
    // Chainsaw: like the Preamp's Lead, matched to Crunch when saturated, not at the reference level. At its default Dist it is fully saturated
    // from about -45 dBFS, so its output is a fixed ceiling; matched to +6 dB at -37 dBFS that ceiling was 10 to 14 dB below Crunch, Lead and the
    // Distortion as soon as a guitar was played harder than the reference level (it sounded far too quiet on the pedal). Its gain at the reference
    // level is printed for information. (Its High boost is a peak around 1 kHz, so the 1 kHz test tone is louder than the 220 Hz one.)
    report("Chainsaw (matched to Crunch at -12 dBFS, see below)", [] { return make<ChainsawModule>(); }, loud, Signal::Sine, false);
    ok &= matchedToCrunch("Chainsaw", [] { return make<ChainsawModule>(); });
    for (int m = 0; m < 2; m++) {
        char t[64];
        std::snprintf(t, sizeof t, "Tape Sat, mode %d", m);
        ok &= report(t, [m] { return make<TapeSatModule>(TapeSatModule::MODE, m); }, gentle);
    }

    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
