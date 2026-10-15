#pragma once
#ifndef URANUS_MODULE_H
#define URANUS_MODULE_H

#include "../Util/fm_voices.h"
#include "../Util/granularplayermod.h"
#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file uranus_module.h */

namespace bkshepherd {

// Uranus: a granular delay and synth, adapted from the Uranus pedal of the FunBox project by GuitarML (MIT licence, see Uranus/README.md). The input goes into half a second of
// audio (with feedback) that two grain players cut into small grains, transposed and played back at a chosen speed. With a MIDI keyboard the same grains become a monophonic
// granular synth (an ADSR over the grains, transposed from middle C), or the input is replaced by a polyphonic two-operator FM synth.
class UranusModule : public BaseEffectModule {
  public:
    enum Param {
        SIZE = 0,
        MIX,
        PITCH,
        FEEDBACK,
        WIDTH,
        SPEED,
        GRAIN_MODE,
        GRAIN_ENV,
        SYNTH_MODE,
        HOLD,
        SPREAD,
        LFO_DEPTH,
        LFO_RATE,
        FM_CAR_LEVEL,
        FM_CAR_RATIO,
        FM_MOD_LEVEL,
        FM_MOD_RATIO,
        MOD_ATTACK,
        MOD_DECAY,
        MOD_SUSTAIN,
        MOD_RELEASE,
        ENV_ATTACK,
        ENV_DECAY,
        ENV_SUSTAIN,
        ENV_RELEASE,
        PARAM_COUNT
    };

    UranusModule();
    ~UranusModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;
    float GetBrightnessForLED(int led_id) const override;
    bool AlternateFootswitchForTempo() const override { return false; }
    void AlternateFootswitchPressed() override; // toggles Hold

    // MIDI keyboard: the granular synth (one note at a time, transposed from middle C) or the FM synth (polyphonic), depending on Synth Mode
    void OnNoteOn(float notenumber, float velocity) override;
    void OnNoteOff(float notenumber, float velocity) override;

  private:
    void ApplyParameter(int parameter_id);
    void ProcessSamples(const float *in, float *outL, float *outR, size_t n);

    GranularPlayerMod m_swarm[2];
    daisysp::Adsr m_env;       // the envelope of the granular synth
    daisysp::Oscillator m_lfo; // pitch modulation
    VoiceManagerFm<12> *m_voices;

    float m_sampleRate;
    int m_sampleIndex;         // write position in the audio the grains are cut from
    float m_currentGrainSize;  // smoothed grain size in ms
    bool m_gate;               // a key is down (granular synth)
    float m_note;              // the last key, 0 .. 127
    float m_velocity;          // 0 .. 127
};
} // namespace bkshepherd
#endif
#endif
