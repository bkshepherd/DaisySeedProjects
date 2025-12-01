#pragma once
#ifndef PHASER_MODULE_H
#define PHASER_MODULE_H

#include <stdint.h>

#include "base_effect_module.h"
#include "daisysp.h"

#ifdef __cplusplus

/** @file phaser_module.h */

using namespace daisysp;

namespace bkshepherd {

class PhaserModule : public BaseEffectModule {
  public:
    PhaserModule();
    ~PhaserModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    float m_levelMin = 0.0f;
    float m_levelMax = 1.0f;
    Phaser m_phaserL;
    Phaser m_phaserR;
    float m_targetRate = 1.0f;
    float m_targetDepth = 1.0f;
    float m_smoothedRate = 1.0f;
    float m_smoothedDepth = 1.0f;
};
} // namespace bkshepherd
#endif
#endif
