#pragma once
#ifndef TEST_TONE_MODULE_H
#define TEST_TONE_MODULE_H

#include "../Util/XorShift32.h"
#include "base_effect_module.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file test_tone_module.h */

namespace bkshepherd {

// Test Tone: a signal generator for calibrating levels and checking the signal path (together with the input and output gain settings and the level readout of Show Debug).
// It replaces the input with a sine, square or triangle wave of a chosen frequency, or white noise, at a chosen level. When the effect is off (bypassed) the input passes as usual.
class TestToneModule : public BaseEffectModule {
  public:
    enum Param {
        LEVEL = 0, // dB, peak level relative to full scale (for noise: the same RMS as a sine of that peak level)
        TYPE,      // Sine, Square, Triangle, Noise
        FREQUENCY, // Hz, not used by the noise
        PARAM_COUNT
    };

    enum Type { SINE = 0, SQUARE, TRIANGLE, NOISE };

    TestToneModule();
    ~TestToneModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;

  private:
    void ProcessSamples(float *outL, float *outR, size_t n);

    float m_phase;      // 0 .. 1
    float m_phaseInc;   // frequency / sample rate
    XorShift32 m_noise;
    float m_sampleRate;
    int m_type;          // the waveform that is playing
    float m_targetGain;  // linear peak gain from the Level parameter
    float m_gain;        // the smoothed gain
    float m_fade;        // 1 normally; goes to 0 and back when the type changes, so switching the type does not click
};
} // namespace bkshepherd
#endif
#endif
