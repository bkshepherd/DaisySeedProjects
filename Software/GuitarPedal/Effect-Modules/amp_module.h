#pragma once
#ifndef AMP_MODULE_H
#define AMP_MODULE_H

#include "ImpulseResponse/ImpulseResponse.h"
#include "base_effect_module.h"
#include "daisysp.h"
#include <RTNeural/RTNeural.h>
#include <stdint.h>

#ifdef __cplusplus

/** @file amp_module.h */

using namespace daisysp;

namespace bkshepherd {

class AmpModule : public BaseEffectModule {
  public:
    AmpModule();
    ~AmpModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void SelectModel();
    void SelectIR();
    void CalculateMix();
    void CalculateTone();
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

    float nnLevelAdjust;
    int m_currentModelindex = -1;

    float m_toneFreqMin;
    float m_toneFreqMax;

    Tone tone; // Low Pass
    // Balance bal;     // Balance for volume correction in filtering

    float m_cachedEffectMagnitudeValue;

    ImpulseResponse mIR;
    int m_currentIRindex = -1;
};
} // namespace bkshepherd
#endif
#endif
