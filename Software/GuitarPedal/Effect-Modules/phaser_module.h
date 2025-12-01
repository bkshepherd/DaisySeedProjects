#pragma once
#ifndef PHASER_MODULE_H
#define PHASER_MODULE_H

#include "../Util/simple_phaser.h"
#include "base_effect_module.h"
#include <stdint.h>

#ifdef __cplusplus

/** @file phaser_module.h */

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

    SimplePhaser m_phaser; // mono

    float m_targetRate = 1.0f;
    float m_targetDepth = 0.95f;
    float m_smoothedRate = 1.0f;
    float m_smoothedDepth = 0.95f;

    float m_ledEnv = 0.0f; // 0..1 envelope for LED
};

} // namespace bkshepherd
#endif
#endif
