#pragma once
#ifndef VENUS_MODULE_H
#define VENUS_MODULE_H

#include "Venus/cmsis_fft.h"
#include "Venus/sliced_fourier.h"
#include "Venus/venus_spectral.h"
#include "base_effect_module.h"
#include "daisysp-lgpl.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file venus_module.h */

using namespace daisysp;

namespace bkshepherd {

// Venus: a spectral reverb that works on the short-time Fourier transform of the signal, with octave and fifth shimmer, detune, spectral freeze,
// two lo-fi modes and two "drift" modes that slowly modulate the controls. Adapted from the Venus pedal of the FunBox project by GuitarML (MIT
// licence, see Venus/README.md). Mono in, the same signal on both outputs.
class VenusModule : public BaseEffectModule {
  public:
    enum Param {
        DECAY = 0,
        MIX,
        DAMP,
        SHIMMER,
        SHIMMER_TONE,
        DETUNE,
        SHIMMER_MODE,
        LOFI,
        DRIFT,
        FREEZE,
        PARAM_COUNT
    };

    VenusModule();
    ~VenusModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;
    float GetBrightnessForLED(int led_id) const override;

    // Holding the secondary footswitch freezes the reverb (the Freeze parameter latches it)
    void AlternateFootswitchPressed() override { m_footswitchFreeze = true; }
    void AlternateFootswitchReleased() override { m_footswitchFreeze = false; }
    bool AlternateFootswitchForTempo() const override { return false; }

  private:
    // noclone: ProcessMono calls this with n = 1, which would otherwise make GCC emit a second, specialised copy
    __attribute__((noclone)) void ProcessSamples(const float *in, float *outL, float *outR, size_t n);
    void UpdateFrameParams();
    void ApplyLoFiMode();
    void ApplyDriftMode();

    venus::SpectralReverb m_reverb;
    venus::CmsisRealFft<venus::kN> *m_fft;
    soundmath::SlicedFourier<float, venus::kN, venus::CmsisRealFft<venus::kN>> *m_stft;

    SampleRateReducer m_reducer;
    Tone m_lowpass;
    Oscillator m_drift[4];

    float m_sampleRate;
    float m_srRatio;           // sample rate relative to the 32 kHz the original was written for
    int m_stepAccum;           // schedule of the pipeline stages (see ProcessSamples)
    int m_driftAccum;          // samples since the drift oscillators were last advanced
    int m_paramAccum;          // samples since the frame parameters were last updated
    float m_driftValue[4];     // current drift multipliers (absolute values of the drift oscillators)
    bool m_footswitchFreeze;
};
} // namespace bkshepherd
#endif
#endif
