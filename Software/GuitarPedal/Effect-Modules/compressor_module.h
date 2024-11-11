#pragma once
#ifndef COMPRESSOR_MODULE_H
#define COMPRESSOR_MODULE_H

#include <stdint.h>

#include "base_effect_module.h"
#include "daisysp-lgpl.h"

#ifdef __cplusplus

/** @file compressor_module.h */

using namespace daisysp;

namespace bkshepherd {

class CompressorModule : public BaseEffectModule {
  public:
    CompressorModule();
    ~CompressorModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) override;

  private:
    float m_levelMin = 0.0f;
    float m_levelMax = 1.0f;

    Compressor m_compressor;
};
} // namespace bkshepherd
#endif
#endif
