#pragma once
#ifndef STRING_KEYS_MODULE_H
#define STRING_KEYS_MODULE_H

#include <stdint.h>
#include "daisysp.h"
#include "base_effect_module.h"
#ifdef __cplusplus

/** @file modal_keys_module.h */
// Implements DaisySP StringVoice class


using namespace daisysp;

namespace bkshepherd
{


class StringKeysModule : public BaseEffectModule
{
  public:
    StringKeysModule();
    ~StringKeysModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void OnNoteOn(float notenumber, float velocity) override;
    void OnNoteOff(float notenumber, float velocity) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;

  private:

    StringVoice   modalvoice;


    float m_freqMin;
    float m_freqMax;



    float m_cachedEffectMagnitudeValue;
};
} // namespace bkshepherd
#endif
#endif
