#pragma once
#ifndef BOOSTER_MODULE_H
#define BOOSTER_MODULE_H

#include "../Util/dist_dsp.h"
#include "mono_effect_module.h"
#ifdef __cplusplus

/** @file booster_module.h */

namespace bkshepherd {

// Booster: a gain stage to put in front of an amp or a distortion, in four types. Gain (0 to 24 dB) is how much the type boosts the part of the
// sound it is built for: everything for Clean, the highs for Treble, the centre frequency for Mid, the mids for Warm. The types differ in what
// they leave out or add:
//   Clean   a flat boost with a tilt Tone (lows against highs) and almost no colour: it only soft-limits at full scale.
//   Treble  a Rangemaster-style treble booster: a high-pass (the corner is Tone) in front of the gain, so the bass is cut and the highs are
//           boosted, then an asymmetric, germanium-like soft clip that adds a little grit when pushed, and a high cut at 11 kHz.
//   Mid     a bell boost at the centre frequency (Tone), the rest untouched.
//   Warm    a thick, slightly tube-like boost in the spirit of the Echoplex preamp boosters: +3 dB of lows, asymmetric soft saturation and a
//           high cut (Tone).
// Level is the output level after the boost (-24 to +6 dB).
class BoosterModule : public MonoEffectModule {
  public:
    enum Param { GAIN = 0, TONE, LEVEL, TYPE, PARAM_COUNT };
    enum Type { CLEAN = 0, TREBLE, MID, WARM, TYPE_COUNT };

    BoosterModule();
    ~BoosterModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;

  protected:
    void Process(const float *in, float *out, size_t n) override;

  private:
    void Recompute();

    float m_sampleRate;
    bool m_dirty;
    int m_type;
    float m_gainTarget, m_gain;
    float m_outTarget, m_out;

    // The clip of the type: out = ceiling * (S(v * invScale + bias) - S(bias)) * comp, S the soft clip of dist_dsp.h; comp restores the gain of
    // small signals that the bias would reduce, so the gain of a small signal is exactly the boost.
    float m_ceiling, m_invScale, m_bias, m_sBias, m_comp;
    bool m_adaa;

    distdsp::OnePole m_hpIn, m_dc;
    distdsp::Biquad m_b1, m_b2, m_b3; // before the gain (the type's EQ), and after the clip
    distdsp::SoftClipAdaa m_soft;
};
} // namespace bkshepherd
#endif
#endif
