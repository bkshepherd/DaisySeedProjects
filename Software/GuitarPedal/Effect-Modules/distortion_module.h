#pragma once
#ifndef DISTORTION_MODULE_H
#define DISTORTION_MODULE_H

#include <stdint.h>

#include "base_effect_module.h"
#include "daisysp.h"
#include <q/fx/biquad.hpp>

#ifdef __cplusplus

/** @file distortion_module.h */

using namespace daisysp;

namespace bkshepherd {

class DistortionModule : public BaseEffectModule {
  public:
    enum Param {
        LEVEL = 0,
        GAIN,
        TONE,
        DIST_TYPE,
        INTENSITY,
        OVERSAMP,
        PARAM_COUNT
    };

    DistortionModule();
    ~DistortionModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    void ProcessBlockInternal(const float *in, float *out, size_t n);
    float ProcessTiltToneControl(float in);
    void InitializeFilters();

    float m_levelMin = 0.0f;
    float m_levelMax = 1.0f;

    float m_gainMin = 1.0f;
    float m_gainMax = 20.0f;

    Tone m_tone;

    // The filters belong to the instance, so several Distortions (on its own and in the Dist Bench's stages) do not share their state
    cycfi::q::highpass m_preFilter{140.0f, 48000};            // the cutoffs and rates are set in Init
    cycfi::q::lowpass m_postFilter{8000.0f, 48000};
    cycfi::q::lowpass m_upsamplingLowpassFilter{0.0f, 48000};

    bool m_oversampling;

    // The pre-filter's cutoff follows the input level; its coefficients are recalculated every few samples, not for every sample (see ProcessMono)
    uint8_t m_preFilterCountdown = 0;
};
} // namespace bkshepherd
#endif
#endif
