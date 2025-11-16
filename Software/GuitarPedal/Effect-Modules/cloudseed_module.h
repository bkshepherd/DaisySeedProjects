#pragma once
#ifndef CLOUDSEED_MODULE_H
#define CLOUDSEED_MODULE_H

#include "base_effect_module.h"
#include "linear_change.h"
#include "daisysp.h"
#include <stdint.h>

#include "../dependencies/CloudSeed/AudioLib/MathDefs.h"
#include "../dependencies/CloudSeed/AudioLib/ValueTables.h"
#include "../dependencies/CloudSeed/Default.h"
#include "../dependencies/CloudSeed/FastSin.h"
#include "../dependencies/CloudSeed/ReverbController.h"

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
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;
    bool AlternateFootswitchForTempo() const override { return false; }
    void AlternateFootswitchPressed() override;

  private:
    struct Mix {
        float wet;
        float dry;
    };

    void CalculateMix();
    Mix CalculateMix(float mixValue);

    CloudSeed::ReverbController *reverb = 0;

    float m_gainMin;
    float m_gainMax;
    int throttle_counter = 0;

    Mix currentMix;

    float m_cachedEffectMagnitudeValue;

    bool inputMuteForWet = false;

    // Gradual transition of dry mix
    static constexpr int linearChangeDryLevelSteps = 25000; // Number of steps for gradual change
    LinearChange linearChangeDryLevel;
};
} // namespace bkshepherd
#endif
#endif
