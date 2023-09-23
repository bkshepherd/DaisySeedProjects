#pragma once
#ifndef MODULATED_TREMOLO_MODULE_H
#define MODULATED_TREMOLO_MODULE_H

#include <stdint.h>
#include "daisysp.h"
#include "base_effect_module.h"
#ifdef __cplusplus

/** @file modulated_tremolo_module.h */

using namespace daisysp;

namespace bkshepherd
{

class ModulatedTremoloModule : public BaseEffectModule
{
  public:
    ModulatedTremoloModule();
    ~ModulatedTremoloModule();

    void Init(float sample_rate) override;
    float ProcessMono(float in) override;
    float ProcessStereoLeft(float in) override;
    float ProcessStereoRight(float in) override;
    float GetOutputLEDBrightness() override;

  private:
    Tremolo m_tremolo;
    float m_tremoloFreqMin;
    float m_tremoloFreqMax;

    Oscillator m_freqOsc;
    float m_freqOscFreqMin;
    float m_freqOscFreqMax;

    float m_cachedEffectMagnitudeValue;
};
} // namespace bkshepherd
#endif
#endif
