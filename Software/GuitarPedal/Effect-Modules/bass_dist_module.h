#pragma once
#ifndef BASS_DIST_MODULE_H
#define BASS_DIST_MODULE_H

#include "../Util/dist_dsp.h"
#include "mono_effect_module.h"
#ifdef __cplusplus

/** @file bass_dist_module.h */

namespace bkshepherd {

// Bass Dist: a distortion that keeps the low end clean. The signal is split at the Crossover frequency (a fourth-order Linkwitz-Riley split);
// only the upper band is distorted (Soft, Hard or Fuzz), then the bands are added again, so the lows stay tight and the highs get the grit. Lows sets the
// level of the clean band, Mix blends the distorted band with the unchanged upper band.
class BassDistModule : public MonoEffectModule {
  public:
    enum Param { DRIVE = 0, CROSSOVER, CLIP, LOWS, TONE, LEVEL, MIX, PARAM_COUNT };

    BassDistModule();
    ~BassDistModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;

  protected:
    void Process(const float *in, float *out, size_t n) override;

  private:
    void Recompute();

    float m_sampleRate;
    bool m_dirty;
    int m_clipType;
    float m_driveTarget, m_drive;
    float m_lowsTarget, m_lows;
    float m_mixTarget, m_mix;
    float m_outTarget, m_out;

    distdsp::OnePole m_hpIn, m_tone, m_dc;
    distdsp::Biquad m_low1, m_low2, m_high1, m_high2;
    distdsp::SoftClipAdaa m_soft;
    distdsp::HardClipAdaa m_hard;
};
} // namespace bkshepherd
#endif
#endif
