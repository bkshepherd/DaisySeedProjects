#pragma once
#ifndef OVERDRIVE_MODULE_H
#define OVERDRIVE_MODULE_H

#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file overdrive_module.h */

using namespace daisysp;

namespace bkshepherd {

class OverdriveModule : public BaseEffectModule {
  public:
    OverdriveModule();
    ~OverdriveModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    Overdrive m_overdriveLeft;
    Overdrive m_overdriveRight;
    float m_driveMin;
    float m_driveMax;
    float m_levelMin;
    float m_levelMax;
};
} // namespace bkshepherd
#endif
#endif
