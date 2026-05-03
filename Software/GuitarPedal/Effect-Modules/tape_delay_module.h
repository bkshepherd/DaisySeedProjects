#pragma once
#ifndef TAPE_DELAY_MODULE_H
#define TAPE_DELAY_MODULE_H

#include "../Util/tape_modulator.h"
#include "base_effect_module.h"
#include "daisy_seed.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file tape_delay_module.h */

using namespace daisysp;

// Delay Max Definitions (Assumes 48kHz samplerate)
constexpr size_t TAPE_MAX_DELAY_SAMPLES = static_cast<size_t>(48000.0f * 8.f);

namespace bkshepherd {

class TapeDelayModule : public BaseEffectModule {
  public:
    enum Param {
        TIME = 0,
        MIX,
        REPEATS,
        MODE,
        WOW_FLUTTER,
        TAPE_AGE,
        TAP_DIV,
        TAPE_BIAS,
        IMPERFECTIONS,
        LOW_END_CONTOUR,
        REVERB_MIX,
        HEAD_CONFIG,
        PARAM_COUNT
    };

    TapeDelayModule();
    ~TapeDelayModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void SetTempo(uint32_t bpm) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    float m_timeMinSamples;
    float m_timeMaxSamples;
    float m_mixWet;
    float m_mixDry;
    float m_currentDelaySamples;

    float m_ageLpMin;
    float m_ageLpMax;
    float m_contourHpMin;
    float m_contourHpMax;

    float m_wowRateMin;
    float m_wowRateMax;
    float m_flutterRateMin;
    float m_flutterRateMax;
    float m_wowDepthMaxSamples;
    float m_flutterDepthMaxSamples;

    float m_dropoutGain;
    float m_dropoutGainTarget;
    float m_crinkleOffset;
    float m_crinkleOffsetTarget;
    float m_dropoutSamplesRemaining;
    float m_crinkleSamplesRemaining;
    float m_imperfectionCooldownSamples;
    uint32_t m_randState;

    float m_ledValue;

    float m_sampleRate;

    DelayLine<float, TAPE_MAX_DELAY_SAMPLES> *m_delayL;
    DelayLine<float, TAPE_MAX_DELAY_SAMPLES> *m_delayR;

    Tone m_toneL;
    Tone m_toneR;
    Svf m_hpL;
    Svf m_hpR;

    ReverbSc m_reverb;
    TapeModulator m_tapeMod;
    Oscillator m_ledOsc;

    void UpdateMix();
    bool IsLoFiMode() const;
    void UpdateDelayTimeAndLed();
    float GetDivisionMultiplier() const;
    float GetWowFlutterOffset();
    float Random01();
    void UpdateImperfections(float amount, float &dropoutGain, float &crinkleOffset);
    void GetHeadMix(float baseSamples, DelayLine<float, TAPE_MAX_DELAY_SAMPLES> &delay, float &out) const;
    float ProcessChannel(float input, float speedMod, float dropoutGain, float crinkleOffset, float age, bool isLoFi,
               DelayLine<float, TAPE_MAX_DELAY_SAMPLES> &delay, Tone &tone, Svf &hp);
};
} // namespace bkshepherd
#endif
#endif
