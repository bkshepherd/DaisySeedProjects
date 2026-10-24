#pragma once
#ifndef MULTI_PHASER_MODULE_H
#define MULTI_PHASER_MODULE_H

#include "base_effect_module.h"
#ifdef __cplusplus

/** @file multi_phaser_module.h */

namespace bkshepherd {

// Multi Phaser: up to four phasers, each with its own settings, in series (each one phases the output of the one before: a deeper, more complex sweep)
// or in parallel (the phasers are averaged: the notches of all of them at once, which sweep independently of one another). A phaser is a chain of
// first-order allpass filters whose corner frequency is swept by a low-frequency oscillator, with a feedback from the end of the chain to its input.
// Added to the signal they put a notch in the spectrum for every two stages. The settings of every phaser:
//   On        whether the phaser is part of the effect (at least one is needed to hear anything)
//   Rate      the speed of its oscillator, 0.05 to 10 Hz
//   Depth     how far the notches sweep: at 100% two and a half octaves up and down from the Center
//   Feedback  from -85% to +85% (50% of the knob is 0): positive feedback makes the notches narrow and resonant, negative one hollow
//   Stages    2, 4, 6, 8, 10 or 12 allpass stages: a notch for every two
//   Center    the frequency the notches sweep around, 100 Hz to 4 kHz
//   Wave      Sine, Triangle or Random (a new random position for every cycle, reached smoothly)
//   Phase     where in its cycle the oscillator is, against the others: with the same Rate they keep this distance
// Mix is how much of the phased signal is heard against the dry one (the notches are deepest at 100%: every phaser adds the dry signal to the
// phase-shifted one in equal parts, as the classic units do). Routing chooses Series or Parallel, and Output Mono or Stereo: in Stereo the right
// channel is the same phasers a quarter of a cycle later (it costs twice the processing).
class MultiPhaserModule : public BaseEffectModule {
  public:
    static constexpr int kUnits = 4;
    static constexpr int kMaxStages = 12;
    static constexpr float kMakeup = 1.7f; // the make-up gain of every phaser (what the sum of the input and the allpass chain takes off)
    enum UnitParam { U_ON = 0, U_RATE, U_DEPTH, U_FEEDBACK, U_STAGES, U_CENTER, U_WAVE, U_PHASE, U_COUNT };
    enum Param { MIX = 0, ROUTING, OUTPUT, UNIT_BASE, PARAM_COUNT = UNIT_BASE + kUnits * U_COUNT };
    enum Routing { SERIES = 0, PARALLEL, ROUTING_COUNT };
    enum OutputMode { MONO_OUT = 0, STEREO_OUT, OUTPUT_COUNT };
    enum Wave { SINE = 0, TRIANGLE, RANDOM, WAVE_COUNT };

    static constexpr int UnitParamId(int unit, int p) { return UNIT_BASE + unit * U_COUNT + p; }

    MultiPhaserModule();
    ~MultiPhaserModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void SetEnabled(bool isEnabled) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    static constexpr int kControlPeriod = 16; // samples between the updates of the oscillators and of the filter coefficients (the coefficients are interpolated)

    struct Unit {
        // what the settings ask for
        bool on = false;
        int stages = 4, wave = SINE;   // the settings
        int stagesActive = 4;          // the number of stages in use: follows `stages` while the phaser is faded out
        int settle = 0;                // samples left of the wait after a change of the stages
        float gain = 1.0f;             // the level of the phaser: the make-up gain and the correction for the feedback
        float rate = 0.5f, depth = 0.8f, feedback = 0.0f, center = 800.0f, phase = 0.0f;
        // what it is doing (the rate, depth, feedback and center follow the settings smoothly)
        float rateS = 0.5f, depthS = 0.8f, feedbackS = 0.0f, centerS = 800.0f;
        float act = 0.0f;        // 0 when it is off, 1 when it is on: a crossfade, so that switching a phaser does not click
        float lfoPhase = 0.0f;   // cycles
        float rndValue = 0.0f, rndTarget = 0.0f; // the Random wave
        uint32_t rng = 1;
        bool fresh = false;      // its coefficients are set at once at the next update (it has just been switched on)
        float lfo = 0.0f;        // -1..1, the latest value of the left channel (the LED)
        float a[2] = {0.0f, 0.0f}, da[2] = {0.0f, 0.0f}; // allpass coefficient of the two channels and its step per sample
        float s[2][kMaxStages] = {};
        float fbState[2] = {0.0f, 0.0f};
        float Process(int ch, float x);
        void Clear();
    };

    void Process(const float *inL, const float *inR, float *outL, float *outR, size_t n);
    void Recompute();
    void ControlUpdate(bool stereo);
    void ClearState();

    float m_sampleRate;
    bool m_dirty;
    float m_mixTarget, m_mix;
    float m_stereoTarget, m_stereo; // 0: mono, 1: stereo (the right channel is crossfaded in)
    int m_routingTarget, m_routing; // the routing in use follows the setting when the effect is ducked (a switch is made at silence of the effect, not mid-sweep)
    float m_duck;                   // 1, or on the way to 0 and back when the routing changes
    int m_settle;                   // samples left of the wait after the routing has changed
    int m_tick;
    Unit m_unit[kUnits];
};
} // namespace bkshepherd
#endif
#endif
