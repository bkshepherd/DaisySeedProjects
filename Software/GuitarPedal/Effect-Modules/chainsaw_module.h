#pragma once
#ifndef CHAINSAW_MODULE_H
#define CHAINSAW_MODULE_H

#include "../Util/dist_dsp.h"
#include "mono_effect_module.h"
#ifdef __cplusplus

/** @file chainsaw_module.h */

namespace bkshepherd {

// Chainsaw: the "Swedish chainsaw" sound of a Boss HM-2-style heavy metal distortion with every knob up. A transistor pre-gain stage, an op-amp
// stage with asymmetric clipping, a pair of diodes clipping to ground and back-to-back diodes in the signal path (a small dead band: crossover
// distortion, and the tail of a note is gated), then the part that makes the sound: an active EQ after the clipping with resonant peaks, Low at
// 87 Hz and High as two stagger-tuned peaks at 960 Hz and 1.28 kHz (the HM-2's gyrator frequencies), boosting the dense harmonics of a nearly
// square wave into a buzzing rasp.
class ChainsawModule : public MonoEffectModule {
  public:
    enum Param { DIST = 0, LOW, HIGH, LEVEL, PARAM_COUNT };

    ChainsawModule();
    ~ChainsawModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;

  protected:
    void Process(const float *in, float *out, size_t n) override;

  private:
    void Recompute();

    float m_sampleRate;
    bool m_dirty;
    float m_preTarget, m_pre;
    float m_gainTarget, m_gain;
    float m_outTarget, m_out;

    distdsp::OnePole m_hpIn, m_lpPre, m_hpCouple, m_lpOp, m_dc;
    distdsp::SoftClipAdaa m_preClip, m_opClip;
    distdsp::HardClipAdaa m_diodes;
    distdsp::DeadZoneAdaa m_gate;
    distdsp::Biquad m_low, m_high1, m_high2, m_outLp;
};
} // namespace bkshepherd
#endif
#endif
