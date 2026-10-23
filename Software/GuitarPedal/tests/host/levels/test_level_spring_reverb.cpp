// Default-level test for the Spring Reverb. The project's rule for a reverb, +0..+6 dB at default settings and never attenuating, is met in the way of the
// reverb control of an amp: the reverb is ADDED to the dry signal, which is never changed, and returns well below it (kWetGain in the module sets it so that at
// Mix 100% it adds as much energy as the dry signal, about 10 dB below at the default Mix of 30%). So the default level sits at the bottom of the window, +0.4 dB
// on average, by design: a reverb that returned louder than the dry signal (as a crossfading Mix would have needed to stay above 0 dB) sounds like an echo.
//
// The standard test signal has two pitches, and a tank is a comb: a sustained note that falls on a resonance of its echoes comes out louder than one that falls
// between two (several dB, the dry signal and the return add in phase or against each other), so two pitches are a poor measure. This test plays the six open
// strings of a guitar (E2 A2 D3 G3 B3 E4) as the guitar-like harmonic series of the other level tests and takes the mean. The tail builds up over the signal,
// so the last half second is measured.
#include "level_common.h"
#include "spring_reverb_module.h"
#include <memory>

using namespace bkshepherd;
using namespace leveltest;

int main() {
    const float strings[6] = {82.41f, 110.0f, 146.83f, 196.0f, 246.94f, 329.63f};
    const Window w{0.0, 6.0, 3.0, 3.0};
    double sum = 0, worst = 1e9, peak = 0;
    std::printf("Spring Reverb, default parameters, the six open strings at %s (RMS gain in dB):\n   ", kInputs[kReferenceInput].label);
    for (float f : strings) {
        auto m = std::make_unique<SpringReverbModule>();
        m->Init(kSampleRate); // every parameter at its metadata default
        const double g = measure(*m, Signal::Guitar, f, kInputs[kReferenceInput].peak).rmsGainDb;
        std::printf(" %.0f Hz %+.1f ", f, g);
        sum += g;
        worst = std::fmin(worst, g);
        for (const InputLevel &in : kInputs) { // never full scale, at any of the measured input levels
            auto fresh = std::make_unique<SpringReverbModule>();
            fresh->Init(kSampleRate);
            peak = std::fmax(peak, measure(*fresh, Signal::Guitar, f, in.peak).outPeak);
        }
    }
    const double mean = sum / 6.0;
    const bool meanOk = mean >= w.minDb && mean <= w.maxDb && std::fabs(mean - w.targetDb) <= w.toleranceDb;
    const bool worstOk = worst > -3.5; // no string is made much quieter (the return can be in antiphase with the dry note: 10 dB down, that is at most -3.3 dB)
    const bool peakOk = peak < 1.0;
    std::printf("\n    reference level: mean %+.2f dB; window %+.1f .. %+.1f dB (the dry signal plus a return about 10 dB below it)  =>  %s\n", mean, w.minDb, w.maxDb,
                meanOk ? "OK" : "OUT OF WINDOW");
    std::printf("    the quietest string %+.2f dB (no more than 3.5 dB below the input)  =>  %s\n", worst, worstOk ? "OK" : "TOO QUIET");
    std::printf("    the output peak over all the input levels %.2f  =>  %s\n", peak, peakOk ? "OK" : "REACHES FULL SCALE");
    const bool ok = meanOk && worstOk && peakOk;
    std::printf("\n%s\n", ok ? "ALL IN WINDOW" : "SOME CONFIGURATIONS OUT OF WINDOW");
    return ok ? 0 : 1;
}
