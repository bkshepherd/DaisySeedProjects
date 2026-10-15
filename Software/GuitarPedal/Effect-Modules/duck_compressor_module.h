#pragma once
#ifndef DUCKCOMPRESSOR_MODULE_H
#define DUCKCOMPRESSOR_MODULE_H

#include <stdint.h>

#include "SimpleCompressor/SimpleComp.h"
#include "base_effect_module.h"

#ifdef __cplusplus

/** @file duck_compressor_module.h */

namespace bkshepherd {

// A side-chain compressor ("ducker"): the left input is the audio that is heard and compressed, the right input is the key that is only listened to: when it rises above the threshold,
// the audio is turned down (by the ratio, with the attack and release times). The key is not in the output. Built on the Simple Compressor of ChunkWare Music Software (see SimpleCompressor/).
// With "Sidechain" off, or on a mono input, the audio is its own key: an ordinary compressor.
class DuckCompressorModule : public BaseEffectModule {
  public:
    enum Param {
        LEVEL = 0,
        RATIO,
        THRESHOLD,
        ATTACK,
        RELEASE,
        SIDECHAIN,
        PARAM_COUNT
    };

    DuckCompressorModule();
    ~DuckCompressorModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    void ProcessSamples(const float *audio, const float *key, float *outL, float *outR, size_t n);

    float m_levelGain = 1.0f;
    float m_gainReductiondB = 0.0f; // of the last sample, for the LED
    chunkware_simple::SimpleComp m_simpleComp;
};
} // namespace bkshepherd
#endif
#endif
