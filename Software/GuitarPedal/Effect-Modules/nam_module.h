#pragma once
#ifndef NAM_MODULE_H
#define NAM_MODULE_H

#include "base_effect_module.h"
#include <stdint.h>

#ifdef __cplusplus

/** @file nam_module.h */

namespace bkshepherd {

class NamModule : public BaseEffectModule {
  public:
    NamModule();
    ~NamModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void SelectModel();

    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    float m_gainMin;
    float m_gainMax;

    float m_levelMin;
    float m_levelMax;

    float wetMix;
    float dryMix;

    int m_currentModelindex = -1;

    float m_cachedEffectMagnitudeValue;

    // Used for nicely switching models
    bool m_muteOutput = false;
};
} // namespace bkshepherd
#endif
#endif
