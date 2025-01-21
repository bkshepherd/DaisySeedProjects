#pragma once
#ifndef MODULATED_TREMOLO_MODULE_H
#define MODULATED_TREMOLO_MODULE_H

#include "../Util/tremolo.h"
#include "base_effect_module.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file modulated_tremolo_module.h */

namespace bkshepherd {

class ModulatedTremoloModule : public BaseEffectModule {
  public:
    ModulatedTremoloModule();
    ~ModulatedTremoloModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void SetTempo(uint32_t bpm) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    daisysp_modified::Tremolo m_tremolo;
    float m_tremoloFreqMin;
    float m_tremoloFreqMax;

    daisysp_modified::Oscillator m_freqOsc;
    float m_freqOscFreqMin;
    float m_freqOscFreqMax;

    float m_cachedEffectMagnitudeValue;
};
} // namespace bkshepherd
#endif
#endif
