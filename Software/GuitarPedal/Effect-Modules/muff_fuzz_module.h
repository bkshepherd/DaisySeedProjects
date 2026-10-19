#pragma once
#ifndef MUFF_FUZZ_MODULE_H
#define MUFF_FUZZ_MODULE_H

#include "../Util/dist_dsp.h"
#include "mono_effect_module.h"
#ifdef __cplusplus

/** @file muff_fuzz_module.h */

namespace bkshepherd {

// Muff Fuzz: a Big Muff-style fuzz. An input booster and two clipping stages in cascade (each band-limited) give the thick, sustaining fuzz, and the
// tone control crossfades between a low-pass and a high-pass of the same corner (which scoops the middle at the centre). Modes move the tone
// corner: Classic (about 1.5 kHz), Fat (lower, darker) and Bright (higher).
class MuffFuzzModule : public MonoEffectModule {
  public:
    enum Param { SUSTAIN = 0, TONE, VOLUME, MODE, PARAM_COUNT };

    MuffFuzzModule();
    ~MuffFuzzModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;

  protected:
    void Process(const float *in, float *out, size_t n) override;

  private:
    void Recompute();

    float m_sampleRate;
    bool m_dirty;
    float m_sustainTarget, m_sustain;
    float m_toneTarget, m_tone;
    float m_outTarget, m_out;

    distdsp::OnePole m_hpIn, m_c1, m_c2, m_c3, m_lpA, m_lpB, m_lpC, m_toneSplit, m_dc;
    distdsp::SoftClipAdaa m_clipA, m_clipB, m_clipC;
};
} // namespace bkshepherd
#endif
#endif
