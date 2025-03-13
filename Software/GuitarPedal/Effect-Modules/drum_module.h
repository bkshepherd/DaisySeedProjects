#pragma once
#ifndef DRUM_MODULE_H
#define DRUM_MODULE_H

#include "../Util/operator.h"
#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file drum_module.h */


using namespace daisysp;

namespace bkshepherd {



class DrumModule : public BaseEffectModule {
  public:
    DrumModule();
    ~DrumModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void OnNoteOn(float notenumber, float velocity) override;
    void OnNoteOff(float notenumber, float velocity) override;
    void AlternateFootswitchPressed() override;
    float GetBrightnessForLED(int led_id) const override;

  private:

    int instrument_;
    int voice_;      // Used to determine which voice to play in Kit mode

    AnalogSnareDrum snare;
    AnalogBassDrum  bass;
    HiHat<>           hihat;
    SyntheticSnareDrum synthsnare;
    SyntheticBassDrum  synthbass;

    bool m_sustain;
    float m_cachedEffectMagnitudeValue;
};
} // namespace bkshepherd
#endif
#endif
