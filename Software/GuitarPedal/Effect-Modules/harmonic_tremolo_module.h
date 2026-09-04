#pragma once
#ifndef HARMONIC_TREMOLO_MODULE_H
#define HARMONIC_TREMOLO_MODULE_H

#include "base_effect_module.h"
#include "daisysp.h"
#include <q/fx/biquad.hpp>
#include <stdint.h>
#ifdef __cplusplus

/** @file harmonic_tremolo_module.h */

using namespace daisysp;

namespace bkshepherd {

/** First order RC low pass, alpha = exp(-2 * pi * fc / fs).
 *
 * The band split this module is built around comes from a Fender 6G12-A, whose
 * filters are single RC pairs. A biquad would be twice as steep and would not
 * blend the two bands the same way, so the one pole form is kept deliberately.
 */
struct OnePoleLowPass {
    void Init(float cutoff, float sample_rate) {
        alpha = expf(-2.0f * PI_F * cutoff / sample_rate);
        prevY = 0.0f;
    }

    float Process(float in) {
        prevY = (1.0f - alpha) * in + alpha * prevY;
        return prevY;
    }

    float alpha = 0.0f;
    float prevY = 0.0f;
};

/** First order RC high pass, matching OnePoleLowPass. */
struct OnePoleHighPass {
    void Init(float cutoff, float sample_rate) {
        alpha = expf(-2.0f * PI_F * cutoff / sample_rate);
        prevX = 0.0f;
        prevY = 0.0f;
    }

    float Process(float in) {
        prevY = (1.0f + alpha) * 0.5f * (in - prevX) + alpha * prevY;
        prevX = in;
        return prevY;
    }

    float alpha = 0.0f;
    float prevX = 0.0f;
    float prevY = 0.0f;
};

/** Harmonic tremolo.
 *
 * Splits the signal into a low and a high band and modulates them with
 * opposite LFO phase, so the tone sweeps between dark and bright instead of
 * simply getting louder and quieter. A fixed EQ chain after the split supplies
 * the rest of the vintage voicing.
 */
class HarmonicTremoloModule : public BaseEffectModule {
  public:
    enum Param {
        SPEED = 0,
        DEPTH,
        MIX,
        LEVEL,
        PARAM_COUNT
    };

    HarmonicTremoloModule();
    ~HarmonicTremoloModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void SetTempo(uint32_t bpm) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    /** Runs one channel's band split, modulation and EQ chain.
        \param channel 0 for left, 1 for right.
        \param in Dry input sample for that channel.
        \param lfo Current LFO value, shared by both channels.
        \return The wet sample, before mix and level.
    */
    float ProcessChannel(int channel, float in, float lfo);

    Oscillator m_lfo;

    // Band splitting filters, [0] is left and [1] is right.
    OnePoleLowPass m_bandLow[2];
    OnePoleHighPass m_bandHigh[2];

    // Fixed EQ voicing applied after the two bands are recombined.
    OnePoleHighPass m_eqHighPass[2];
    OnePoleLowPass m_eqLowPass[2];
    cycfi::q::biquad m_eqLowShelf[2];
    cycfi::q::peaking m_eqPeakLowMid[2];
    cycfi::q::peaking m_eqPeakPresence[2];

    float m_speedSmoothed;
    float m_depthSmoothed;
    float m_lastLfoValue;
};
} // namespace bkshepherd
#endif
#endif
