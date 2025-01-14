#pragma once
#ifndef CLOUDSEED_MODULE_H
#define CLOUDSEED_MODULE_H

#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>

#include "CloudSeed/AudioLib/MathDefs.h"
#include "CloudSeed/AudioLib/ValueTables.h"
#include "CloudSeed/Default.h"
#include "CloudSeed/FastSin.h"
#include "CloudSeed/ReverbController.h"

#ifdef __cplusplus

/** @file cloudseed_module.h */

// using namespace daisysp;

namespace bkshepherd {

class CloudSeedModule : public BaseEffectModule {
  public:
    CloudSeedModule();
    ~CloudSeedModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void changePreset();
    void CalculateMix();
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    CloudSeed::ReverbController *reverb = 0;

    float m_gainMin;
    float m_gainMax;
    int throttle_counter = 0;

    float wetMix;
    float dryMix;

    float m_cachedEffectMagnitudeValue;
};
} // namespace bkshepherd
#endif
#endif
