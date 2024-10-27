#pragma once
#ifndef LOOPER_MODULE_H
#define LOOPER_MODULE_H

#include <stdint.h>

#include "base_effect_module.h"
#include "daisysp.h"
#ifdef __cplusplus

/** @file looper_module.h */

namespace bkshepherd {

class LooperModule : public BaseEffectModule {
 public:
  LooperModule();
  ~LooperModule();

  void Init(float sample_rate) override;
  void ProcessMono(float in) override;
  void ProcessStereo(float inL, float inR) override;
  float GetBrightnessForLED(int led_id) override;
  bool AlternateFootswitchForTempo() const override { return false; }
  void AlternateFootswitchPressed() override;
  void AlternateFootswitchHeldFor1Second() override;
  void DrawUI(OneBitGraphicsDisplay &display, int currentIndex,
              int numItemsTotal, Rectangle boundsToDrawIn,
              bool isEditing) override;

 private:
  daisysp::Looper m_looperL;
  daisysp::Looper m_looperR;

  float m_inputLevelMin;
  float m_inputLevelMax;
  float m_loopLevelMin;
  float m_loopLevelMax;
};
}  // namespace bkshepherd
#endif
#endif
