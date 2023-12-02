#pragma once
#ifndef METRO_MODULE_H
#define METRO_MODULE_H

#include "daisysp.h"
#include "base_effect_module.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file chopper_module.h */

using namespace daisysp;

namespace bkshepherd
{

class MetroModule : public BaseEffectModule
{
public:
  MetroModule();
  ~MetroModule();

  void Init(float sample_rate) override;
  void ProcessMono(float in) override;
  void ProcessStereo(float inL, float inR) override;
  void SetTempo(uint32_t bpm) override;
  float GetBrightnessForLED(int led_id) override;
  void DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn, bool isEditing) override;

private:
  float m_tempoFreqMin;
  float m_tempoFreqMax;

  float m_cachedEffectMagnitudeValue;
};
} // namespace bkshepherd
#endif
#endif
