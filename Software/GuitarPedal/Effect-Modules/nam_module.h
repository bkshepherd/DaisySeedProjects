#pragma once
#ifndef NAM_MODULE_H
#define NAM_MODULE_H

#include <stdint.h>
#include "daisysp.h"
#include "base_effect_module.h"
#include <RTNeural/RTNeural.h>

#ifdef __cplusplus

/** @file nam_module.h */

using namespace daisysp;


namespace bkshepherd
{

class NamModule : public BaseEffectModule
{
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

    float wetMix;
    float dryMix;

    //float nnLevelAdjust;
    int   m_currentModelindex = -1;

    float m_toneFreqMin;    
    float m_toneFreqMax;

    //Tone tone;       // Low Pass


    float m_cachedEffectMagnitudeValue;

};
} // namespace bkshepherd
#endif
#endif
