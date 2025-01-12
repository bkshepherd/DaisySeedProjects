#pragma once
#ifndef NOISEGATE_MODULE_H
#define NOISEGATE_MODULE_H

#include <stdint.h>

#include "base_effect_module.h"
#include "daisysp-lgpl.h"

#ifdef __cplusplus

/** @file noisegate_module.h */

using namespace daisysp;

namespace bkshepherd {

class NoiseGateModule : public BaseEffectModule {
  public:
    NoiseGateModule();
    ~NoiseGateModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;
};
} // namespace bkshepherd
#endif
#endif
