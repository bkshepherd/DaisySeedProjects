#pragma once
#ifndef MARS_MODULE_H
#define MARS_MODULE_H

#include "Mars/mars_dsp.h"
#include "ImpulseResponse/ImpulseResponse.h"
#include "base_effect_module.h"
#include "daisysp-lgpl.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file mars_module.h */

namespace bkshepherd {

// Mars: an amp sim and delay, adapted from the Mars pedal of the FunBox project by GuitarML (MIT licence, see Mars/README.md). Three neural amp/pedal models (small GRU networks), three
// impulse responses (cabinets), a low/high pass filter and a delay with dotted eighth and triplet modes. Mono: only the left input is used.
class MarsModule : public BaseEffectModule {
  public:
    enum Param {
        GAIN = 0,
        MIX,
        LEVEL,
        FILTER,
        DELAY_TIME,
        DELAY_FEEDBACK,
        MODEL,
        IR,
        DELAY_MODE,
        AMP_ON,
        IR_ON,
        PARAM_COUNT
    };

    MarsModule();
    ~MarsModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;

  private:
    void ApplyModel();
    void ApplyIr();
    void ApplyDelayMode();
    void ProcessSamples(const float *in, float *outL, float *outR, size_t n);

    mars::Gru<> m_gru;
    float m_levelAdjust;
    float m_modelGain; // the level calibration of the selected model
    float m_cabGain;   // ... and cabinet
    ImpulseResponse m_ir;
    mars::DelayLine2Tap *m_delay;
    daisysp::Tone m_lp;
    daisysp::ATone m_hp;
    daisysp::Balance m_bal;
    float m_currentDelay;
    bool m_secondTap;
};
} // namespace bkshepherd
#endif
#endif
