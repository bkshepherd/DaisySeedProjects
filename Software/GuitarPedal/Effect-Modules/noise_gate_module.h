#pragma once
#ifndef NOISEGATE_MODULE_H
#define NOISEGATE_MODULE_H

#include <stdint.h>

#include "../Util/1efilter.hpp"
#include "base_effect_module.h"

#ifdef __cplusplus

/** @file noisegate_module.h */

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

  private:
    // inputs:
    // estimated frequency: overwritten by timestamps at runtime and not used
    // cutoff freq
    // beta: 0.0f disables it entirely, but used for scaling cutoff frequency
    // derivative cutoff freq: used when beta is > 0
    one_euro_filter<float, float> m_smoothingFilter{48000, 0.5f, 0.05f, 1.0f};

    float m_holdTimer; // Timer tracking hold duration
    bool m_gateOpen;   // Is the gate currently open?
    float m_prevTimeSeconds;

    float m_currentGain;
};
} // namespace bkshepherd
#endif
#endif
