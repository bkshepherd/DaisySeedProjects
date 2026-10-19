#pragma once
#ifndef TAPE_SAT_MODULE_H
#define TAPE_SAT_MODULE_H

#include "../Util/dist_dsp.h"
#include "mono_effect_module.h"
#ifdef __cplusplus

/** @file tape_sat_module.h */

namespace bkshepherd {

// Tape Sat: a soft saturator with unity gain for small signals (it does not get louder with Drive, it compresses and adds harmonics). Warmth boosts
// the lows before the saturation and takes the boost out again after it, so the lows saturate more (the "head bump"); Tone is the high roll-off.
// Modes: Tape (a slight bias, even harmonics) and Transformer (no bias, more low-end saturation).
class TapeSatModule : public MonoEffectModule {
  public:
    enum Param { DRIVE = 0, WARMTH, TONE, MIX, LEVEL, MODE, PARAM_COUNT };

    TapeSatModule();
    ~TapeSatModule();

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
    float m_mixTarget, m_mix;
    float m_outTarget, m_out;

    distdsp::OnePole m_hpIn, m_tone, m_dc;
    distdsp::Biquad m_pre, m_post;
    distdsp::SoftClipAdaa m_clip;
};
} // namespace bkshepherd
#endif
#endif
