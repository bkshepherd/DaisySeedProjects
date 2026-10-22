// The Delay starts from silence when it is switched on: what was played the last time it was on must not come back as echoes (it did in a
// chain, where a switched-off slot is not processed, so its delay lines were frozen). The lines are cleared a little per sample while the
// delay's own output is muted, so new playing during the clearing is kept; with Trails (the tail still ringing) nothing is cleared.
// The module keeps its delay lines in static memory, so the scenarios run one after another, each on a freshly initialised module.
#include "delay_module.h"
#include "effect_chain.h"
#include <cmath>
#include <cstdio>
#include <memory>
using namespace bkshepherd;

static int fails = 0;
static void check(bool ok, const char *what) {
    fails += !ok;
    std::printf("  %-100s %s\n", what, ok ? "OK" : "FAILED");
}

constexpr float kSr = 48000.0f;

static float tone(size_t i) { return 0.3f * std::sin(6.2831853f * 220.0f * i / kSr); }

// A delay with long, strong repeats (so that a stale echo is still loud seconds later), of the given type (Forward, Reverse, Octave, ...)
static void setup(BaseEffectModule &d, int type) {
    d.SetParameterAsMagnitude(DelayModule::DELAY_TIME, 0.3f);
    d.SetParameterAsMagnitude(DelayModule::D_FEEDBACK, 0.8f);
    d.SetParameterAsMagnitude(DelayModule::DELAY_MIX, 1.0f); // wet only: anything heard is from the delay lines
    d.SetParameterRaw(DelayModule::DELAY_TYPE, type);
}

// Plays the tone for `seconds` (or silence), returns the output peak
static float play(BaseEffectModule &d, float seconds, bool withTone) {
    float peak = 0;
    for (size_t i = 0; i < size_t(seconds * kSr); i++) {
        d.ProcessStereo(withTone ? tone(i) : 0.0f, withTone ? tone(i) : 0.0f);
        peak = std::fmax(peak, std::fmax(std::fabs(d.GetAudioLeft()), std::fabs(d.GetAudioRight())));
    }
    return peak;
}

int main() {
    const char *types[6] = {"Forward", "Reverse", "Octave", "ReverseOct", "Dual", "DualOct"};

    std::printf("On its own (switched off without Trails, so not processed, then on again):\n");
    for (int t = 0; t < 6; t++) {
        auto d = std::make_unique<DelayModule>();
        d->Init(kSr);
        setup(*d, t);
        d->SetEnabled(true);
        const float loud = play(*d, 2.0f, true);
        d->SetEnabled(false); // ... and not processed for a while
        d->SetEnabled(true);
        const float stale = play(*d, 4.0f, false);
        char what[128];
        std::snprintf(what, sizeof what, "%-10s: repeats while playing (peak %.3f), nothing of them after switching on again (peak %.1e)", types[t], loud, stale);
        check(loud > 0.05f && stale < 1e-6f, what);
    }

    std::printf("\nIn a chain (the slot switched off and on):\n");
    {
        auto *delay = new DelayModule();
        EffectChain chain("Test", {{"Dl", delay}}, {});
        chain.Init(kSr);
        setup(*delay, 0);
        chain.SetEnabled(true);
        const float loud = play(chain, 2.0f, true);
        chain.SetSlotOn(0, false);
        play(chain, 1.0f, true); // the slot fades out and is not processed any more, while the guitar goes on
        chain.SetSlotOn(0, true);
        float stale = 0;
        for (size_t i = 0; i < size_t(4 * kSr); i++) {
            chain.ProcessStereo(0.0f, 0.0f);
            stale = std::fmax(stale, std::fabs(chain.GetAudioLeft()));
        }
        std::printf("    peak while playing %.3f, after switching the slot on again with silence in %.1e\n", loud, stale);
        check(loud > 0.05f && stale < 1e-6f, "switching the slot on again brings back nothing of what was played before");
    }

    std::printf("\nWhat is kept:\n");
    {
        auto d = std::make_unique<DelayModule>();
        d->Init(kSr);
        setup(*d, 0);
        d->SetEnabled(true);
        play(*d, 2.0f, true);
        d->SetEnabled(false);
        d->SetTrailing(true); // the pedal is letting the tail ring out (Trails) and is switched on again meanwhile
        d->SetEnabled(true);
        d->SetTrailing(false);
        const float kept = play(*d, 1.0f, false);
        check(kept > 0.05f, "with Trails (the tail still ringing) the repeats are kept");
    }
    {
        auto d = std::make_unique<DelayModule>();
        d->Init(kSr);
        setup(*d, 0);
        d->SetEnabled(true);
        play(*d, 2.0f, true);
        d->SetEnabled(false);
        d->SetEnabled(true);
        const float during = play(*d, 0.05f, true); // a note played right away, while the lines are being cleared
        const float echo = play(*d, 3.0f, false);
        std::printf("    output while clearing %.1e, echoes of the note played meanwhile %.3f\n", during, echo);
        check(during < 1e-6f && echo > 0.02f, "the delay is muted while clearing (0.12 s), and a note played meanwhile still gives its echoes");
    }

    std::printf("\n%s\n", fails ? "DELAY CLEAR TESTS FAILED" : "ALL DELAY CLEAR TESTS PASSED");
    return fails ? 1 : 0;
}
