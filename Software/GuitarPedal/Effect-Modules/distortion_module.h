#pragma once
#ifndef DISTORTION_MODULE_H
#define DISTORTION_MODULE_H

#include <stdint.h>

#include "base_effect_module.h"
#include "daisysp.h"

#ifdef __cplusplus

/** @file distortion_module.h */

using namespace daisysp;

namespace bkshepherd {

class DistortionModule : public BaseEffectModule {
  public:
    DistortionModule();
    ~DistortionModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    float ProcessTiltToneControl(float in);
    void InitializeFilters();

    float m_levelMin = 0.0f;
    float m_levelMax = 1.0f;

    float m_gainMin = 1.0f;
    float m_gainMax = 20.0f;

    Tone m_tone;

    bool m_oversampling;
};
} // namespace bkshepherd
#endif
#endif
