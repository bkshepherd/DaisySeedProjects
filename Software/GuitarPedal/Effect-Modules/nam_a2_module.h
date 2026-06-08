#pragma once
#ifndef NAM_A2_MODULE_H
#define NAM_A2_MODULE_H

#include "base_effect_module.h"
#include <stdint.h>

// Forward declaration only — the full runtime header (with NAM_A2_NOINLINE functions)
// is included exclusively in nam_a2_module.cpp to avoid ODR violations when
// nam_a2_runtime.h is pulled into multiple translation units.
namespace nam_a2_daisy { class A2Player; }

#ifdef __cplusplus

/** @file nam_a2_module.h */

namespace bkshepherd {

class NamA2Module : public BaseEffectModule {
  public:
    enum Param {
        GAIN = 0,
        LEVEL,
        MODEL,
        BASS,
        MID,
        TREBLE,
        EQ,
        PARAM_COUNT
    };

    void SelectModel();

    NamA2Module();
    ~NamA2Module();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;

    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    // Recompute the cached gain/level/EQ values from the current parameters.
    // Called from Init() and ParameterChanged() so ProcessMono() doesn't have to.
    void UpdateCachedParameters();

    static constexpr int kBlockSize = 48; // nam_a2_daisy::kBlockSize

    float m_inputBuffer[kBlockSize];
    float m_outputBuffer[kBlockSize];
    int m_bufferIndex;

    float m_gainMin;
    float m_gainMax;
    float m_levelMin;
    float m_levelMax;

    // Cached parameter values, recomputed in UpdateCachedParameters() on change
    // rather than re-read/re-scaled per sample in ProcessMono().
    float m_gain;
    float m_level;
    bool m_eqEnabled;

    float m_cachedEffectMagnitudeValue;

    int m_currentModelIndex;
    float m_currentModelGain;
    // Written on the main loop in SelectModel(), read in the audio callback
    // (ProcessMono). volatile so the callback observes the mute before/while
    // load_weights() + prewarm() rewrite the shared weights and state.
    volatile bool m_muteOutput;

    // Pointer to the static A2Player instance owned by nam_a2_module.cpp.
    // Keeping the full type out of this header avoids ODR violations from the
    // NAM_A2_NOINLINE functions in nam_a2_runtime.h.
    nam_a2_daisy::A2Player* m_model;
};

} // namespace bkshepherd

#endif
#endif
