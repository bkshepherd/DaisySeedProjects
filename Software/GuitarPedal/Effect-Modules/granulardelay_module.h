#pragma once
#ifndef GRANULARDELAY_MODULE_H
#define GRANULARDELAY_MODULE_H

#include "../Util/granularplayermod.h"
#include "daisysp.h"
#include <stdint.h>

#include "base_effect_module.h"
#ifdef __cplusplus

/** @file granulardelay_module.h */

using namespace daisysp;

namespace bkshepherd {

class GranularDelayModule : public BaseEffectModule {
  public:
    GranularDelayModule();
    ~GranularDelayModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;
    bool AlternateFootswitchForTempo() const override { return false; }
    void AlternateFootswitchPressed() override;

  private:
    GranularPlayerMod granular;

    Looper m_looper;

    int m_sample_index;

    float m_speed;
    float m_pitch;
    float m_grain_size;
    float m_width;

    float m_current_grainsize;

    int first_count;
    bool m_loop_recorded;

    bool m_hold;
};
} // namespace bkshepherd
#endif
#endif
