#pragma once
#ifndef REVERB_MODULE_H
#define REVERB_MODULE_H

#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file reverb_module.h */

using namespace daisysp;

namespace bkshepherd {

class ReverbModule : public BaseEffectModule {
  public:
    float GetMaxTrailSeconds() const override { return 8.0f; }

    enum Param {
        TIME = 0,
        DAMP,
        MIX,
        PARAM_COUNT
    };

    ReverbModule();
    ~ReverbModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    void ApplyParameters();
    void ProcessBlockInternal(const float *inL, const float *inR, float *outL, float *outR, size_t n);

    ReverbSc *m_reverbStereo;
    float m_timeMin;
    float m_timeMax;
    float m_lpFreqMin;
    float m_lpFreqMax;
};
} // namespace bkshepherd
#endif
#endif
