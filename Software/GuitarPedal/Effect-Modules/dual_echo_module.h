#pragma once
#ifndef DUAL_ECHO_MODULE_H
#define DUAL_ECHO_MODULE_H

#include "../Util/tape_modulator.h"
#include "base_effect_module.h"
#include "daisy_seed.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file dual_echo_module.h */

using namespace daisysp;

// 1.5 s quarter note at 48 kHz plus headroom for modulation excursion.
constexpr size_t DUAL_ECHO_MAX_DELAY_SAMPLES = static_cast<size_t>(48000.0f * 1.5f) + 2048;

namespace bkshepherd {

// Two-voice modulated echo inspired by the Andy Timmons "Halo" dual echo:
// dotted-8th + quarter voices, modulation only on the repeats, tape-style
// saturation/compression in the loop, and a momentary infinite hold.
class DualEchoModule : public BaseEffectModule {
  public:
    enum Param {
        TIME = 0,
        FEEDBACK,
        LEVEL,
        DEPTH,
        RATE,
        TONE,
        RHYTHM,
        SATURATE,
        LOW_CUT,
        SPREAD,
        CASCADE,
        PARAM_COUNT
    };

    // Same order as the Halo's Rhythm knob.
    enum Rhythm { RHYTHM_QUARTER = 0, RHYTHM_DOTTED8, RHYTHM_HALO, RHYTHM_BBD, RHYTHM_TAPE, RHYTHM_COUNT };

    struct ModeConfig {
        float ratioA;
        float ratioB;
        bool tapeHeads;   // Read heads at 0.5x and 1.0x of the voice time
        bool monoFeed;    // Both voices fed from the L+R sum; each voice gets separate L/R modulated taps
        bool triangleLfo;
        bool antiPhase;   // Voice B LFO 180 degrees from voice A at the same rate
        bool twoPoleLp;
        bool tapeWow;     // Use Perlin wow/flutter instead of the LFO
        float toneMaxHz;
        float satBase;
        float wetScale;
    };

    DualEchoModule();
    ~DualEchoModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void SetTempo(uint32_t bpm) override;
    float GetBrightnessForLED(int led_id) const override;
    void SetEnabled(bool isEnabled) override;
    void AlternateFootswitchHeldFor1Second() override;
    void AlternateFootswitchReleased() override;
    void DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                bool isEditing) override;

  private:
    using Line = DelayLine<float, DUAL_ECHO_MAX_DELAY_SAMPLES>;

    struct Voice {
        Line *line;
        OnePole lp1;
        OnePole lp2;
        OnePole hp;
        TapeModulator tapeMod;
        float lfoPhase;
        float env;
    };

    void UpdateMode();
    void UpdateFilters();
    void UpdateSaturation();
    void UpdateModulation();
    void UpdateTimeTarget();
    void ResetState();
    float ReadVoice(Voice &v, float ratio, float modSamples) const;
    float ProcessLoop(Voice &v, float loopIn, float hold);
    float VoiceModulation(Voice &v, float rateScale, float phaseOffset, float ampSamples, float width, float &modRight);
    void ProcessSample(float inL, float inR, float &outL, float &outR);

    float m_sr;
    ModeConfig m_mode;
    Voice m_voiceA;
    Voice m_voiceB;

    float m_timeTargetSamples;
    float m_timeSamples;
    float m_ratioA;
    float m_ratioB;
    float m_feedback;
    float m_cascade;
    float m_level;
    float m_spread;
    float m_lfoRateHz;
    float m_modAmpTarget;
    float m_modAmp;
    float m_flutterAmp;
    float m_drive;
    float m_biasOffset;
    float m_bias;
    float m_satNorm;
    float m_compK;
    float m_hold;
    volatile bool m_holdActive; // Set from the footswitch handler, read in the audio callback

    float m_envAttack;
    float m_envRelease;
    float m_holdCoef;

    float m_ledPhase;
};
} // namespace bkshepherd
#endif
#endif
