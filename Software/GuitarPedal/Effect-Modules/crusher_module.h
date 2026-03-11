#pragma once
#ifndef CRUSHER_MODULE_H
#define CRUSHER_MODULE_H

#include "base_effect_module.h"
#include "daisysp.h"
#include "../Util/frequency_detector_q.h"
#include "../Util/XorShift32.h"
#include <q/fx/biquad.hpp>
#include <stdint.h>
#ifdef __cplusplus

/** @file crusher_module.h */

using namespace daisysp;

namespace bkshepherd {

class Bitcrusher {
  public:
    void Init(float sample_rate) {
        quant        = 65536.0f;
        sampleRate   = sample_rate;
        targetRate   = sample_rate;
        phaseAccum   = 0.0f;
        heldSample   = 0.0f;
        jitterAmount = 0.0f;
        nextInterval = 1.0f;
    }

    float Process(float in) {
        float base = sampleRate / targetRate;
        if(base < 1.0f) base = 1.0f;

        if(nextInterval < 1.0f) nextInterval = base;

        phaseAccum += 1.0f;
        if(phaseAccum >= nextInterval) {
            phaseAccum -= nextInterval;

            heldSample = nearbyintf(in * quant) / quant;

            float jitter = rng.randSigned(-1.0f, 1.0f);
            float interval = base * (1.0f + 0.25f * jitterAmount * jitter);
            if(interval < 1.0f) interval = 1.0f;

            const float maxInterval = base * 4.0f;
            if(interval > maxInterval) interval = maxInterval;

            nextInterval = interval;
        }

        return heldSample;
    }

    void setTargetSampleRate(float rate) {
        if(rate < 100.0f) rate = 100.0f;
        if(rate > sampleRate) rate = sampleRate;
        targetRate = rate;

        // In per-sample processing this can be called every sample, so do not
        // reset timing state here or jitter gets canceled. Keep the pending
        // interval in a valid range for the new base rate instead.
        float base = sampleRate / targetRate;
        if(base < 1.0f) base = 1.0f;

        if(nextInterval < 1.0f) nextInterval = base;

        const float maxInterval = base * 4.0f;
        if(nextInterval > maxInterval) nextInterval = maxInterval;
    }

    void setJitter(float amount) { jitterAmount = (amount < 0.0f) ? 0.0f : amount; }

    void setNumberOfBits(float nBits) {
        if (nBits < 1.0f) {
            nBits = 1.0;
        } else if (nBits > 32.0f) {
            nBits = 32.0f;
        }
        quant = powf(2.0f, nBits);
    }

  private:
    float quant;
    float sampleRate;
    float targetRate;
    float phaseAccum;
    float heldSample;
    float jitterAmount;
    float nextInterval;
    XorShift32 rng;
};

class CrusherModule : public BaseEffectModule {
  public:
    enum Param {
        LEVEL = 0,
        BITS,
        RATE,
        PITCH_TRACKING,
        JITTER,
        CUTOFF,
        MIX,
        PARAM_COUNT
    };

    CrusherModule();
    ~CrusherModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;
    bool AlternateFootswitchForTempo() const override { return false; }
    void AlternateFootswitchPressed() override;

  private:
    Bitcrusher m_bitcrusherL;
    Bitcrusher m_bitcrusherR;

    float m_rateMin;
    float m_rateMax;
    float m_cutoffMin;
    float m_cutoffMax;
    cycfi::q::lowpass m_lpFilter[2];
    FrequencyDetectorQ m_pitchDetector;

    float GetSrrRate(float detectorInput);
};
} // namespace bkshepherd
#endif
#endif
