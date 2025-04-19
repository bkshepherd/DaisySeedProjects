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
    void SetTempo(uint32_t bpm) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    int instrument_;
    int voice_; // Used to determine which voice to play in Kit mode

    Metro metro;
    uint32_t m_bpm;
    bool auto_mode = true;
    int beat_count; // 0 to 15 for now, indicates which voice to play and when
    float tap_mag = 0.0;
    int led_tempo_count;

    int time_sig = 0;
    int selected_beat = 0;

    AnalogSnareDrum snare;
    AnalogBassDrum bass;
    HiHat<> hihat;
    SyntheticSnareDrum synthsnare;
    SyntheticBassDrum synthbass;

    float m_cachedEffectMagnitudeValue;
};
} // namespace bkshepherd
#endif
#endif
