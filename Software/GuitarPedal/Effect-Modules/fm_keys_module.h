#pragma once
#ifndef FM_KEYS_MODULE_H
#define FM_KEYS_MODULE_H

#include "../Util/fm_voices.h"
#include "../Util/operator.h"
#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file midi_keys_module.h */
// Implements the DaisyExamples Midi effect for DaisyField, but on the 125B pedal
// Polyphonic MIDI Synth
// 12 (changed from 24, didn't work) voices of polyphony. Each voice is a band-limited saw waveform running through a state variable
// filter.

using namespace daisysp;

namespace bkshepherd {

class FmKeysModule : public BaseEffectModule {
  public:
    enum Param {
        LEVEL = 0,
        RATIO,
        MOD_LEVEL,
        MOD_RATIO,
        CAR_ATTACK,
        CAR_DECAY,
        CAR_SUSTAIN,
        CAR_RELEASE,
        MOD_ATTACK,
        MOD_DECAY,
        MOD_SUSTAIN,
        MOD_RELEASE,
        PARAM_COUNT
    };

    FmKeysModule();
    ~FmKeysModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void OnNoteOn(float notenumber, float velocity) override;
    void OnNoteOff(float notenumber, float velocity) override;

    float GetBrightnessForLED(int led_id) const override;

  private:
    float m_freqMin;
    float m_freqMax;

    VoiceManagerFm<16> m_voice_handler; // starting with 1 voice for testing, 16 WORKS, 18 freezes
                                        // With added carrier env control, 16 glitches, 12 works

    float m_cachedEffectMagnitudeValue;
};
} // namespace bkshepherd
#endif
#endif
