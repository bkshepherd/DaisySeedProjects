#pragma once
#ifndef NEPTUNE_MODULE_H
#define NEPTUNE_MODULE_H

#include "base_effect_module.h"
#include "daisysp.h"
#include <atomic>
#include <stdint.h>

#include "../dependencies/CloudSeed/AudioLib/MathDefs.h"
#include "../dependencies/CloudSeed/AudioLib/ValueTables.h"
#include "../dependencies/CloudSeed/Default.h"
#include "../dependencies/CloudSeed/FastSin.h"
#include "../dependencies/CloudSeed/ReverbController.h"

#ifdef __cplusplus

/** @file neptune_module.h */

namespace bkshepherd {

// Neptune: a stereo reverb and delay, adapted from the Neptune pedal of the FunBox project by GuitarML (MIT licence, see Neptune/README.md). The reverb is the CloudSeed
// engine in three modes (Chorus, Medium Space, RubiKa); the delay (up to 4 seconds) has normal, octave and reverse modes. The routing sets how the two are combined.
// Uses its own CloudSeed reverb instance (about 4.9 MB of the pool in cloudseed_pool.cpp, shared with the Jupiter and CloudSeed modules) and about 4.5 MB of SDRAM
// for the delay lines. Needs a sample rate of 48 kHz (the delay times are in samples).
class NeptuneModule : public BaseEffectModule {
  public:
    enum Param {
        DECAY = 0,
        MIX,
        DELAY_TIME,
        MOD_AMT,
        LOWPASS,
        DELAY_FDBK,
        REVERB_MODE,
        ROUTING,
        DELAY_MODE,
        STEREO_IN,
        PRE_DELAY,
        MOD_RATE,
        REV_DELAY_MIX,
        PARAM_COUNT
    };

    NeptuneModule();
    ~NeptuneModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;
    bool AlternateFootswitchForTempo() const override { return false; }

  private:
    // Pushes the current value of one control into the reverb engine (some of these recompute the engine's filters and delay lengths, so ParameterChanged only flags the
    // control and the audio path applies one flagged control per 48 samples)
    void ApplyParameter(int parameter_id);
    void ApplyPending(size_t n);
    void ApplyReverbMode(); // loads one of the three reverb presets (clears the reverb) and then applies the controls over it
    void ApplyDelayMode();
    void CalculateMix();
    void UpdateDelays();

    void ProcessSamples(const float *inL, const float *inR, float *outL, float *outR, size_t n);

    CloudSeed::ReverbController *m_reverb;
    std::atomic<uint32_t> m_pending; // one bit per parameter whose value has changed and has not been pushed to the engine yet
    std::atomic<bool> m_reverbModeChanged;
    int m_sinceApply;                // samples since a pending control was last pushed to the engine
    float m_wetMix, m_dryMix;        // dry/wet
    float m_delayMix, m_reverbMix;   // balance between the delay and the reverb
    float m_modeRamp;                // below 1 while the output is muted for a change of reverb mode
    bool m_modeTrigger;              // the new reverb mode is loaded when the ramp has passed 0.1
};
} // namespace bkshepherd
#endif
#endif
