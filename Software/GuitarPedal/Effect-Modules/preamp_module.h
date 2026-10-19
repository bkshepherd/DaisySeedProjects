#pragma once
#ifndef PREAMP_MODULE_H
#define PREAMP_MODULE_H

#include "../Util/dist_dsp.h"
#include "mono_effect_module.h"
#ifdef __cplusplus

/** @file preamp_module.h */

namespace bkshepherd {

// Preamp: a light amp-like tone (two cascaded tube-style gain stages, a three-band tone stack and a gentle high cut) for chains where a NAM model
// is too heavy. Three voicings: Clean (one stage), Crunch and Lead (two stages, more gain, a higher mid frequency, less top end).
class PreampModule : public MonoEffectModule {
  public:
    enum Param { GAIN = 0, BASS, MID, TREBLE, VOICING, LEVEL, PARAM_COUNT };

    PreampModule();
    ~PreampModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;

  protected:
    void Process(const float *in, float *out, size_t n) override;

  private:
    void Recompute();

    float m_sampleRate;
    bool m_dirty;
    int m_voicing;
    float m_gain1Target, m_gain1;
    float m_outTarget, m_out;

    distdsp::OnePole m_hpIn, m_hpC1, m_lp1, m_hp2, m_lp2;
    distdsp::Biquad m_bass, m_mid, m_treble, m_outLp;
    distdsp::SoftClipAdaa m_clip1, m_clip2;
};
} // namespace bkshepherd
#endif
#endif
