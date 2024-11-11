#pragma once
#ifndef CHOPPER_MODULE_H
#define CHOPPER_MODULE_H

#include "Chopper/chopper.h"
#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file chopper_module.h */

using namespace daisysp;

namespace bkshepherd {

class ChopperModule : public BaseEffectModule {
  public:
    ChopperModule();
    ~ChopperModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void SetTempo(uint32_t bpm) override;
    float GetBrightnessForLED(int led_id) override;
    void DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                bool isEditing) override;

  private:
    Chopper m_chopper;
    float m_tempoFreqMin;
    float m_tempoFreqMax;
    float m_pulseWidthMin;
    float m_pulseWidthMax;

    float m_cachedEffectMagnitudeValue;
};
} // namespace bkshepherd
#endif
#endif
