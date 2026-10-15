#pragma once
#ifndef MERCURY_MODULE_H
#define MERCURY_MODULE_H

#include "base_effect_module.h"
#include <q/fx/biquad.hpp>
#include <stdint.h>
#ifdef __cplusplus

/** @file mercury_module.h */

namespace bkshepherd {

// Mercury: an amp sim built on Neural Amp Modeler (NAM) models, adapted from the Mercury pedal of the FunBox project by GuitarML (MIT licence; the network code is from RTNeural-NAM,
// BSD-3-Clause, see Mercury/README.md). Nine "pico" NAM models (small, so that they run in real time on the Daisy), a four band EQ (bass, mid, treble, presence), no cabinet. Mono: only
// the left input is used. (The NAM A2 module is the other amp sim here; it runs official NAM A2 Nano models.)
class MercuryModule : public BaseEffectModule {
  public:
    enum Param {
        GAIN = 0,
        LEVEL,
        PRESENCE,
        BASS,
        MID,
        TREBLE,
        MODEL,
        EQ,
        PARAM_COUNT
    };

    MercuryModule();
    ~MercuryModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;

  private:
    void SelectModel();
    void ConfigureBand(int band);
    float ProcessSample(float in);

    cycfi::q::peaking m_eq[4]; // bass, mid, treble, presence
    float m_sampleRate;
    float m_gain;
    float m_level;
    bool m_eqOn;
    int m_currentModel;
    float m_modelGain;
    // Written on the main loop in SelectModel(), read in the audio callback: volatile so the callback sees the mute while the weights are loaded and the network settles
    volatile bool m_muteOutput;
};
} // namespace bkshepherd
#endif
#endif
