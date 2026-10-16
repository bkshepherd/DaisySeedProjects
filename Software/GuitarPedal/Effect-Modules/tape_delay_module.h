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
    float GetMaxTrailSeconds() const override { return 10.0f; }

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
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;
    void SetTempo(uint32_t bpm) override;
    float GetBrightnessForLED(int led_id) const override;
    void SetEnabled(bool isEnabled) override;
    int GetMappedParameterIDForKnob(int knob_id) const override;
    void AlternateFootswitchHeldFor1Second() override;
    void DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                bool isEditing) override;

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

    float m_smoothedTime;
    float m_smoothedMix;
    float m_smoothedRepeats;
    float m_ledValue;
    bool m_shiftModeActive = false;

    float m_sampleRate;

    DelayLine<float, TAPE_MAX_DELAY_SAMPLES> *m_delayL;
    DelayLine<float, TAPE_MAX_DELAY_SAMPLES> *m_delayR;

    Tone m_toneL;
    Tone m_toneR;
    Svf m_hpL;
    Svf m_hpR;

    ReverbSc *m_reverb;
    TapeModulator m_tapeModL;
    TapeModulator m_tapeModR;
    Oscillator m_ledOsc;

    // Values that depend only on the parameters. They are computed once per ProcessSamples() call instead of once per sample.
    struct WowParams {
        float wowRate, flutterRate, wowDepth, flutterDepth, flutterMix;
    };
    struct ImperfectionParams {
        float amt, dropoutChance, crinkleChance, spliceChance;
    };
    struct ChannelParams {
        bool isLoFi;
        float division;
        float minDelay;
        int mode;
        int headCfg;
        float drive;
    };

    void UpdateMix();
    bool IsLoFiMode() const;
    void UpdateDelayTimeAndLed(bool isLoFi, float wobble);
    float GetDivisionMultiplier() const;
    WowParams ComputeWowParams(float rateScale, bool isLoFi) const;
    float Random01();
    ImperfectionParams ComputeImperfectionParams(float amount) const;
    void UpdateImperfections(const ImperfectionParams &p, float &dropoutGain, float &crinkleOffset);
    void GetHeadMix(float baseSamples, DelayLine<float, TAPE_MAX_DELAY_SAMPLES> &delay, float &out, int mode, int headCfg) const;
    void ResetInternalState();
    float ApplySafetyLimiter(float sample) const;
    float ProcessChannel(float input, float speedMod, float dropoutGain, float crinkleOffset, const ChannelParams &cp,
                         DelayLine<float, TAPE_MAX_DELAY_SAMPLES> &delay, Tone &tone, Svf &hp);
    // Shared implementation for ProcessMono/ProcessStereo (n = 1) and the block versions.
    // noclone: ProcessMono/ProcessStereo call this with n = 1, which makes GCC emit a second, specialised copy (see SciFiModule).
    __attribute__((noclone)) void ProcessSamples(const float *inL, const float *inR, float *outL, float *outR, size_t n);
};
} // namespace bkshepherd
#endif
#endif
