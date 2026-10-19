#pragma once
#ifndef SCREAMER_MODULE_H
#define SCREAMER_MODULE_H

#include "../Util/dist_dsp.h"
#include "mono_effect_module.h"
#ifdef __cplusplus

/** @file screamer_module.h */

namespace bkshepherd {

// Screamer: a Tube Screamer-style overdrive. As in the original circuit the clipping acts on a high-passed copy (corner about 720 Hz) that is added
// to the clean signal, which gives the mid hump and keeps the lows tight; a tone low-pass follows. Modes: Classic (symmetric soft clip),
// Asymmetric (a biased clip, with even harmonics) and Hard (a harder clip).
class ScreamerModule : public MonoEffectModule {
  public:
    enum Param { DRIVE = 0, TONE, LEVEL, MODE, PARAM_COUNT };

    ScreamerModule();
    ~ScreamerModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;

  protected:
    void Process(const float *in, float *out, size_t n) override;

  private:
    void Recompute();

    float m_sampleRate;
    bool m_dirty;
    int m_mode;
    float m_driveTarget, m_drive;
    float m_outTarget, m_out;

    distdsp::OnePole m_hpIn, m_emph, m_lpOp, m_tone, m_dc;
    distdsp::SoftClipAdaa m_soft;
    distdsp::HardClipAdaa m_hard;
};
} // namespace bkshepherd
#endif
#endif
