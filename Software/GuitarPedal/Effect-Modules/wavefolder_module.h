#pragma once
#ifndef WAVEFOLDER_MODULE_H
#define WAVEFOLDER_MODULE_H

#include "../Util/dist_dsp.h"
#include "mono_effect_module.h"
#ifdef __cplusplus

/** @file wavefolder_module.h */

namespace bkshepherd {

// Wavefolder: the signal is folded back on itself by a sine, y = sin(a * (x + bias)), which makes metallic, synth-like harmonics (unlike clipping, more drive
// adds new folds instead of flattening the peaks). The fold is anti-aliased with first-order antiderivative anti-aliasing. Symmetry moves the bias (even
// harmonics), Tone is a low-pass on the result and Mix blends the dry signal.
class WavefolderModule : public MonoEffectModule {
  public:
    enum Param { FOLD = 0, SYMMETRY, TONE, MIX, LEVEL, PARAM_COUNT };

    WavefolderModule();
    ~WavefolderModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;

  protected:
    void Process(const float *in, float *out, size_t n) override;

  private:
    void Recompute();

    float m_sampleRate;
    bool m_dirty;
    float m_foldTarget, m_fold;
    float m_biasTarget, m_bias;
    float m_mixTarget, m_mix;
    float m_outTarget, m_out;
    float m_x1; // the previous (biased) input of the anti-aliased fold

    distdsp::OnePole m_hpIn, m_dc, m_tone;
};
} // namespace bkshepherd
#endif
#endif
