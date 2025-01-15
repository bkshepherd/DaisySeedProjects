#pragma once
#ifndef POLYOCTAVE_MODULE_H
#define POLYOCTAVE_MODULE_H

#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file polyoctave_module.h */

//
// NOTE: This effect code was adapted from https://github.com/schult/terrarium-poly-octave
//       (Under the MIT License)

using namespace daisysp;

namespace bkshepherd {

class PolyOctaveModule : public BaseEffectModule {
  public:
    PolyOctaveModule();
    ~PolyOctaveModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    int bin_counter = 0;
    float buff[6];
    float buff_out[6];

    float m_tremoloFreqMin;
    float m_tremoloFreqMax;

    float m_freqOscFreqMin;
    float m_freqOscFreqMax;

    float m_cachedEffectMagnitudeValue;
};
} // namespace bkshepherd
#endif
#endif
