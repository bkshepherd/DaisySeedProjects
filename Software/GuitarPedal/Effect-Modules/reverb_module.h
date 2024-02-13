#pragma once
#ifndef REVERB_MODULE_H
#define REVERB_MODULE_H

#include <stdint.h>
#include "daisysp.h"
#include "base_effect_module.h"
#ifdef __cplusplus

/** @file reverb_module.h */

using namespace daisysp;

namespace bkshepherd
{

class ReverbModule : public BaseEffectModule
{
  public:
    ReverbModule();
    ~ReverbModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) override;

  private:
    ReverbSc *m_reverbStereo;
    float m_timeMin;
    float m_timeMax;
    float m_lpFreqMin;
    float m_lpFreqMax;
};
} // namespace bkshepherd
#endif
#endif
