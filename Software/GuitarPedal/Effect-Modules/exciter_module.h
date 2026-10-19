#pragma once
#ifndef EXCITER_MODULE_H
#define EXCITER_MODULE_H

#include "../Util/dist_dsp.h"
#include "mono_effect_module.h"
#ifdef __cplusplus

/** @file exciter_module.h */

namespace bkshepherd {

// Exciter: adds controlled harmonics with a Chebyshev waveshaper. The part of the signal above Focus is driven into the shaper, which outputs the 2nd to
// 5th harmonic of what comes in (T2..T5, each with its own level), and the result is mixed with the unchanged input. The harmonics grow with the playing
// level (a harmonic of order n grows as the input to the power n), so it adds sparkle and warmth on a clean tone and more on hard picking.
class ExciterModule : public MonoEffectModule {
  public:
    enum Param { DRIVE = 0, H2, H3, H4, H5, FOCUS, MIX, LEVEL, PARAM_COUNT };

    ExciterModule();
    ~ExciterModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;

  protected:
    void Process(const float *in, float *out, size_t n) override;

  private:
    void Recompute();

    float m_sampleRate;
    bool m_dirty;
    float m_driveTarget, m_drive;
    float m_h2Target, m_h2, m_h3Target, m_h3, m_h4Target, m_h4, m_h5Target, m_h5;
    float m_mixTarget, m_mix;
    float m_outTarget, m_out;

    distdsp::OnePole m_hpIn, m_focus, m_dc;
};
} // namespace bkshepherd
#endif
#endif
