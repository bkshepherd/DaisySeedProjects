#pragma once
#ifndef JUPITER_MODULE_H
#define JUPITER_MODULE_H

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

/** @file jupiter_module.h */

namespace bkshepherd {

// Jupiter: a stereo reverb built on the CloudSeed engine, adapted from the Jupiter pedal of the FunBox project by GuitarML (MIT licence, see Jupiter/README.md).
// Decay, a low and a high shelf (frequency and gain) after the reverb, and the dry/wet mix are controls; the reverb's lushness, modulation and pre-delay are
// three-way menu items. Uses its own CloudSeed reverb instance (about 4.9 MB of the pool in cloudseed_pool.cpp, shared with the CloudSeed module).
class JupiterModule : public BaseEffectModule {
  public:
    enum Param {
        DECAY = 0,
        MIX,
        LOW_GAIN,
        LOW_FREQ,
        HIGH_GAIN,
        HIGH_FREQ,
        LUSH,
        MODULATION,
        PRE_DELAY,
        STEREO_IN,
        PARAM_COUNT
    };

    JupiterModule();
    ~JupiterModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;
    bool AlternateFootswitchForTempo() const override { return false; }

  private:
    // Pushes the current value of one control into the reverb engine (some of these recompute the engine's filters and delay lengths, which is too much work to do
    // for every knob step inside one audio block, so ParameterChanged only flags the control and the audio path applies one flagged control per block)
    void ApplyParameter(int parameter_id);
    void ApplyPending(size_t n);
    void CalculateMix();

    // Runs the reverb over up to ReverbController::MaxBlockSize samples and applies the wet/dry mix
    void ProcessBlockInternal(const float *inL, const float *inR, float *outL, float *outR, size_t n);

    CloudSeed::ReverbController *m_reverb;
    std::atomic<uint32_t> m_pending; // one bit per parameter whose value has changed and has not been pushed to the engine yet
    int m_sinceApply; // samples since a pending control was last pushed to the engine
    float m_wetMix;
    float m_dryMix;
};
} // namespace bkshepherd
#endif
#endif
