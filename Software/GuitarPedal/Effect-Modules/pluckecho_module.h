#pragma once
#ifndef PLUCKEECHO_MODULE_H
#define PLUCKECHO_MODULE_H

#include <stdint.h>
#include "daisysp.h"
#include "base_effect_module.h"
#ifdef __cplusplus

#define NUM_VOICES 32
#define MAX_DELAY_PLUCKECHO ((size_t)(10.0f * 48000.0f))

/** @file pluckecho_module.h */


using namespace daisysp;

namespace bkshepherd
{


class PluckEchoModule : public BaseEffectModule
{
  public:
    PluckEchoModule();
    ~PluckEchoModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void OnNoteOn(float notenumber, float velocity) override;
    void OnNoteOff(float notenumber, float velocity) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;

  private:

    // Synthesis
    PolyPluck<NUM_VOICES> synth;
    // 10 second delay line on the external SDRAM
    //ReverbSc                                  verb;

    // Persistent filtered Value for smooth delay time changes.
    float smooth_time;

    float m_freqMin;
    float m_freqMax;
    //float m_verbMin;
    //float m_verbMax;
    float nn;
    float trig;


    float m_cachedEffectMagnitudeValue;
};
} // namespace bkshepherd
#endif
#endif
