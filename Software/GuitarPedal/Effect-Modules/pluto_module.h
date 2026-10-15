#pragma once
#ifndef PLUTO_MODULE_H
#define PLUTO_MODULE_H

#include "../Util/var_speed_looper.h"
#include "base_effect_module.h"
#include "daisysp.h"
#include "daisysp-lgpl.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file pluto_module.h */

namespace bkshepherd {

// Pluto: a wacky dual (or stereo) looper for endless experimentation, adapted from the Pluto pedal of the FunBox project by GuitarML (MIT licence, see Pluto/README.md). Two loopers of up
// to a minute each, with a variable speed (smooth or stepped, backwards to 3x), a loop mode (normal, one-time dub, frippertronics) and one live effect that the two Mod controls
// shape: random speed variations (Stability), a low/high pass filter, or a stereo reverb.
class PlutoModule : public BaseEffectModule {
  public:
    enum Param {
        LEVEL_A = 0,
        MOD_A,
        LEVEL_B,
        SPEED_A,
        MOD_B,
        SPEED_B,
        SPEED_MODE,
        EFFECT,
        LOOP_MODE,
        FOOT_TARGET,
        PARAM_COUNT
    };

    PlutoModule();
    ~PlutoModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;
    float GetBrightnessForLED(int led_id) const override;
    bool AlternateFootswitchForTempo() const override { return false; }
    void AlternateFootswitchPressed() override;      // record / stop recording; a second press within a second pauses
    void AlternateFootswitchHeldFor1Second() override; // clears the loop

  private:
    void ApplyLoopMode();
    void ProcessSamples(const float *inL, const float *inR, float *outL, float *outR, size_t n, bool stereo);
    bool Selected(int looper) const;

    varspeed::Looper m_looper[2];
    daisysp::ReverbSc *m_verb;
    daisysp::Tone m_lp[2];   // low pass
    daisysp::ATone m_hp[2];  // high pass
    daisysp::Balance m_bal[2]; // keeps the level when filtering
    daisysp::SmoothRandomGenerator m_rand[2];

    float m_sampleRate;
    float m_currentSpeed[2];
    bool m_paused[2];
    bool m_checkDoubleTap[2];
    int m_doubleTapSamples[2];
    float m_ledPhase;
    float m_ledBrightness;
};
} // namespace bkshepherd
#endif
#endif
