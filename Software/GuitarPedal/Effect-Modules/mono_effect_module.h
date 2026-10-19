#pragma once
#ifndef MONO_EFFECT_MODULE_H
#define MONO_EFFECT_MODULE_H

#include "base_effect_module.h"
#ifdef __cplusplus

namespace bkshepherd {

// Base for the mono effects of the distortion family: an effect implements Process() for a block (n >= 1) and gets the per-sample, stereo and
// block entry points from here. The left input is processed and the result goes to both outputs, as in the Distortion module. Process() may be
// called with n = 1 (ProcessMono) or with the whole audio block; the result does not depend on how the samples are divided into calls.
class MonoEffectModule : public BaseEffectModule {
  public:
    void ProcessMono(float in) override {
        float out;
        Process(&in, &out, 1);
        m_audioLeft = out;
        m_audioRight = out;
    }
    void ProcessStereo(float inL, float /*inR*/) override { ProcessMono(inL); }
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override {
        Process(in, outL, n);
        for (size_t i = 0; i < n; i++) outR[i] = outL[i];
        m_audioLeft = outL[n - 1];
        m_audioRight = outR[n - 1];
    }
    void ProcessStereoBlock(const float *inL, const float * /*inR*/, float *outL, float *outR, size_t n) override { ProcessMonoBlock(inL, outL, outR, n); }

  protected:
    /** Processes n samples. in and out may be the same buffer. */
    virtual void Process(const float *in, float *out, size_t n) = 0;

    /** Runs 0.15 s of silence through the effect so that the filters and the DC blockers reach their resting state (a bias or a harmonic offset
     *  would otherwise start as a small thump). Call it at the end of Init(). */
    void Settle() {
        const float zeros[64] = {0.0f};
        float out[64];
        for (int i = 0; i < 110; i++) Process(zeros, out, 64);
    }
};
} // namespace bkshepherd
#endif
#endif
