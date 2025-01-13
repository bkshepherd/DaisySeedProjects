#pragma once
#ifndef MODAL_KEYS_MODULE_H
#define MODAL_KEYS_MODULE_H

#include <stdint.h>
#include "daisysp.h"
#include "base_effect_module.h"
#ifdef __cplusplus

/** @file modal_keys_module.h */
// Implements DaisySP ModalVoice class


using namespace daisysp;

namespace bkshepherd
{


class ModalKeysModule : public BaseEffectModule
{
  public:
    ModalKeysModule();
    ~ModalKeysModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void OnNoteOn(float notenumber, float velocity) override;
    void OnNoteOff(float notenumber, float velocity) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;

  private:

    ModalVoice   modalvoice;


    float m_freqMin;
    float m_freqMax;



    float m_cachedEffectMagnitudeValue;
};
} // namespace bkshepherd
#endif
#endif
