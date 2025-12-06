#pragma once
#ifndef FLANGER_MODULE_H
#define FLANGER_MODULE_H

#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file flanger_module.h */

using namespace daisysp;

namespace bkshepherd {

class FlangerModule : public BaseEffectModule {
  public:
    FlangerModule();
    ~FlangerModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    Flanger m_flanger;
    float m_lfoFreqMin;
    float m_lfoFreqMax;

    // Smoothing state
    float m_mixSm = 0.0f;
    float m_delaySm = 0.0f;
    float m_rateSm = 0.0f;
    float m_depthSm = 0.0f;
    float m_fbSm = 0.0f;

    inline float Smooth(float cur, float tgt, float alpha) { return cur + alpha * (tgt - cur); }
};

} // namespace bkshepherd
#endif
#endif
