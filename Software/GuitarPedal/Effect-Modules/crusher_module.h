#pragma once
#ifndef CRUSHER_MODULE_H
#define CRUSHER_MODULE_H

#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file crusher_module.h */

using namespace daisysp;

namespace bkshepherd {

class Bitcrusher {
  public:
    Bitcrusher() {}
    ~Bitcrusher() {}

    void Init() { quant = 65536.0; } // need some default value

    float Process(float in) { return truncf(in * quant) / quant; }

    void setNumberOfBits(float nBits) {
        if (nBits < 1.0) {
            nBits = 1.0;
        } else if (nBits > 32.0) {
            nBits = 32.0;
        }
        quant = powf(2.0, nBits);
    }

  private:
    float quant;
};

class CrusherModule : public BaseEffectModule {
  public:
    CrusherModule();
    ~CrusherModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;

  private:
    Tone m_tone;
    Bitcrusher m_bitcrusher;

    // Parameter limits
    // float m_bitsMin;
    // float m_bitsMax;
    float m_levelMin;
    float m_levelMax;
    float m_cutoffMin;
    float m_cutoffMax;
};
} // namespace bkshepherd
#endif
#endif
