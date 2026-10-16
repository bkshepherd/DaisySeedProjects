#pragma once
#ifndef BALNEA_MODULE_H
#define BALNEA_MODULE_H

#include "Balnea/balnea_dsp.h"
#include "../Util/RuntimeDelayLine.h"
#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file balnea_module.h */

namespace bkshepherd {

// Balnea: a digital delay and pitch shifter inspired by the CBA Thermae pedal, adapted from the Balnea pedal of the FunBox project by GuitarML (MIT licence, see Balnea/README.md).
// The delay lines are read at a different speed than they are written, which transposes the repeats; a sequencer steps through up to three speeds (the tapped tempo, and the
// two intervals) once per delay time, so the repeats climb or fall in pitch as they recirculate. Tap tempo, reverse, wow and flutter, and a MIDI synth to play along with.
class BalneaModule : public BaseEffectModule {
  public:
    float GetMaxTrailSeconds() const override { return 10.0f; }

    enum Param {
        MIX = 0,
        TONE,
        REGEN,
        GLIDE,
        INT1,
        INT2,
        TIME,
        MOD_RATE,
        MOD_DEPTH,
        BASE_STAGE,
        INT1_STAGE,
        INT2_STAGE,
        SCALE,
        REVERSE,
        TAP_SEQ,
        PARAM_COUNT
    };

    BalneaModule();
    ~BalneaModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;
    float GetBrightnessForLED(int led_id) const override;
    bool AlternateFootswitchForTempo() const override { return false; } // the footswitch is Balnea's own tap tempo
    void AlternateFootswitchPressed() override;
    void AlternateFootswitchReleased() override;

    // MIDI keyboard: a synth that is mixed with the input
    void OnNoteOn(float notenumber, float velocity) override;
    void OnNoteOff(float notenumber, float velocity) override;

  private:
    struct Tap {
        daisysp_modified::DelayLine<float> line; // the delay line, which is read at its own speed
        balnea::DelayLineReverse rev;            // the same signal for the reverse mode
        daisysp::Svf lpf;                        // the filter in the feedback path
        float currentDelay = 4096.0f;
        float currentSpeed = 1.0f;
        float speed = 1.0f;
        float incCounter = 0.0f;
        float currentSample = 0.0f;
    };

    float ProcessTap(Tap &t, float in, float modSpeed, float glide, float feedback, bool reverse);
    void ApplyTime(float ms);
    float IntervalMultiplier(int knobIndex, int scale) const;
    void ProcessSamples(const float *inL, const float *inR, float *outL, float *outR, size_t n);

    Tap m_tap[2];
    daisysp::Svf m_tone[2]; // the resonant low pass filter on the wet signal
    balnea::TapeModulator m_tape;
    balnea::VoiceManager<12> m_voices;

    float m_sampleRate;
    float m_delayTarget; // the length in samples that is read at the base speed
    float m_baseSpeed;
    int m_baseDelaySamples; // the tapped (or set) delay time in samples
    float m_toneFreq;       // the last cutoff set, so the filters are only retuned when it changes
    float m_modLeft, m_modRight;
    int m_modCounter;

    // The sequencer: stage 0 plays at the base speed, 1 at interval 1 and 2 at both intervals together
    int m_stage;
    int m_stageTimer;
    bool m_advanceStage; // requested by a tap in tap-sequencer mode

    // Tap tempo
    bool m_tapOn;          // waiting for the second tap
    int m_tapTimer;        // samples since the first tap
    bool m_footDown;       // the footswitch is down
    int m_footHeldSamples;
    bool m_selfOsc;        // held for 300 ms: the feedback goes above 1
    bool m_resetSequence;  // the time was changed

    // LED 2 blinks once per delay time
    int m_ledTimer;
    bool m_ledOn;
};
} // namespace bkshepherd
#endif
#endif
