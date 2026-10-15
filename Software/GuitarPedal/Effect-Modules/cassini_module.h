#pragma once
#ifndef CASSINI_MODULE_H
#define CASSINI_MODULE_H

#include "../Util/var_speed_looper.h"
#include "base_effect_module.h"
#include "daisysp-lgpl.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file cassini_module.h */

namespace bkshepherd {

// Cassini: a chromatic sampler for a MIDI keyboard, adapted from the Cassini pedal of the FunBox project by GuitarML (MIT licence, see Cassini/README.md). Record up to 20 seconds of the
// input into one of three banks, and play it from a keyboard: every key plays the sample at its own pitch (from middle C), through an envelope, a low/high pass filter and a reverb, and a
// looper can loop what you play.
class CassiniModule : public BaseEffectModule {
  public:
    enum Param {
        TRIM_START = 0,
        TRIM_END,
        LEVEL,
        LOOP_SPEED,
        FILTER,
        REVERB,
        SPEED_MODE,
        SAMPLE_MODE,
        BANK,
        FOOT_MODE,
        PARAM_COUNT
    };

    static constexpr int kBanks = 3;
    static constexpr int kVoiceCount = 12;

    CassiniModule();
    ~CassiniModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;
    float GetBrightnessForLED(int led_id) const override;
    bool AlternateFootswitchForTempo() const override { return false; }
    void AlternateFootswitchPressed() override;
    void AlternateFootswitchReleased() override;
    void AlternateFootswitchHeldFor1Second() override;

    void OnNoteOn(float notenumber, float velocity) override;
    void OnNoteOff(float notenumber, float velocity) override;

  private:
    // One key's playback of the sample
    class Voice {
      public:
        void Init(float sampleRate);
        float Process(const float *sample, int size, int trimStart, int trimEnd, bool oneShot);
        void OnNoteOn(float note, float velocity);
        void OnNoteOff() { m_gate = false; }
        bool IsActive() const { return m_active; }
        float GetNote() const { return m_note; }

      private:
        float NextSample(const float *sample, int size, int trimStart, int trimEnd, bool oneShot);
        daisysp::Adsr m_env;
        float m_note = 0.0f, m_velocity = 0.0f;
        bool m_active = false, m_gate = false, m_firstSample = true;
        float m_speed = 1.0f;
        float m_playhead = 0.0f;
    };

    void ProcessSamples(const float *in, float *outL, float *outR, size_t n);
    int SampleBank() const;

    Voice m_voices[kVoiceCount];
    varspeed::Looper m_looper;
    daisysp::ReverbSc *m_verb;
    daisysp::Svf m_filter;
    daisysp::SmoothRandomGenerator m_rand;

    float m_sampleRate;
    float m_currentSpeed;
    bool m_paused;
    bool m_checkDoubleTap;
    int m_doubleTapSamples;
    bool m_recording; // the footswitch is down and the sample records
    int m_recordIndex;
    int m_sampleSize[kBanks];
    float m_ledPhase;
    float m_ledBrightness;
};
} // namespace bkshepherd
#endif
#endif
