#pragma once
#ifndef CHORUS_MODULE_H
#define CHORUS_MODULE_H

#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file chorus_module.h */

using namespace daisysp;

namespace bkshepherd {

class ChorusModule : public BaseEffectModule {
  public:
    ChorusModule();
    ~ChorusModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    Chorus m_chorus;
    float m_lfoFreqMin;
    float m_lfoFreqMax;
};
} // namespace bkshepherd
#endif
#endif
