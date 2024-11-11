#pragma once
#ifndef AUTOPAN_MODULE_H
#define AUTOPAN_MODULE_H

#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file autopan_module.h */

using namespace daisysp;

namespace bkshepherd {

class AutoPanModule : public BaseEffectModule {
  public:
    AutoPanModule();
    ~AutoPanModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void SetTempo(uint32_t bpm) override;
    float GetBrightnessForLED(int led_id) override;
    void UpdateUI(float elapsedTime) override;
    void DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                bool isEditing) override;

  private:
    float m_pan; // 0 to 1 value 0 is full left, 1 is full right.
    Oscillator m_freqOsc;
    float m_freqOscFreqMin;
    float m_freqOscFreqMax;
};
} // namespace bkshepherd
#endif
#endif
