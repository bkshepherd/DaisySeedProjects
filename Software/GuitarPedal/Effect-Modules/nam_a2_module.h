#pragma once
#ifndef NAM_A2_MODULE_H
#define NAM_A2_MODULE_H

#include "base_effect_module.h"
#include <stdint.h>

// Forward declaration only — the full A2 header (with NAM_A2_NOINLINE functions)
// is included exclusively in nam_a2_module.cpp to avoid ODR violations when
// NamA2JCM2000Daisy48.h is pulled into multiple translation units.
namespace nam_a2_daisy { class A2Jcm2000Daisy48; }

#ifdef __cplusplus

/** @file nam_a2_module.h */

namespace bkshepherd {

class NamA2Module : public BaseEffectModule {
  public:
    enum Param {
        GAIN = 0,
        LEVEL,
        BASS,
        MID,
        TREBLE,
        EQ,
        PARAM_COUNT
    };

    NamA2Module();
    ~NamA2Module();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;

    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    static constexpr int kBlockSize = 48; // nam_a2_daisy::kBlockSize

    float m_inputBuffer[kBlockSize];
    float m_outputBuffer[kBlockSize];
    int m_bufferIndex;

    float m_gainMin;
    float m_gainMax;
    float m_levelMin;
    float m_levelMax;

    float m_cachedEffectMagnitudeValue;

    // Pointer to static instance owned by nam_a2_module.cpp.
    // Keeping the full type out of this header avoids ODR violations from
    // the NAM_A2_NOINLINE functions defined in NamA2JCM2000Daisy48.h.
    nam_a2_daisy::A2Jcm2000Daisy48* m_model;
};

} // namespace bkshepherd

#endif
#endif
