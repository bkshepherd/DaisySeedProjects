#pragma once
#ifndef OCTAVE_FUZZ_MODULE_H
#define OCTAVE_FUZZ_MODULE_H

#include "../Util/dist_dsp.h"
#include "mono_effect_module.h"
#ifdef __cplusplus

/** @file octave_fuzz_module.h */

namespace bkshepherd {

// Octa Fuzz: an octave-up fuzz in the spirit of the Octavia. The amplified signal is full-wave rectified (which doubles the frequency), blended with the
// original by Octave, and driven into a hard clip; Tone is a low-pass at the end. Played on the higher strings it gives the typical ring-modulator-like
// octave; at Octave = 0 it is a plain fuzz.
class OctaveFuzzModule : public MonoEffectModule {
  public:
    enum Param { FUZZ = 0, OCTAVE, TONE, LEVEL, PARAM_COUNT };

    OctaveFuzzModule();
    ~OctaveFuzzModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;

  protected:
    void Process(const float *in, float *out, size_t n) override;

  private:
    void Recompute();

    float m_sampleRate;
    bool m_dirty;
    float m_fuzzTarget, m_fuzz;
    float m_octaveTarget, m_octave;
    float m_outTarget, m_out;

    distdsp::OnePole m_hpIn, m_rectDc, m_tone, m_dc;
    distdsp::HardClipAdaa m_clip;
};
} // namespace bkshepherd
#endif
#endif
