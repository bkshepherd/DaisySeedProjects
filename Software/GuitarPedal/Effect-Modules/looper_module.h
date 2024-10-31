#pragma once
#ifndef LOOPER_MODULE_H
#define LOOPER_MODULE_H

#include <stdint.h>

#include "../Util/looper.h"
#include "base_effect_module.h"
#ifdef __cplusplus

/** @file looper_module.h */

namespace bkshepherd {

class LooperModule : public BaseEffectModule {
 public:
  LooperModule();
  ~LooperModule();

  void Init(float sample_rate) override;
  void ParameterChanged(int parameter_id) override;
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
  void SetLooperMode();

  daisysp_modified::Looper m_looper;

  float m_inputLevelMin;
  float m_inputLevelMax;
  float m_loopLevelMin;
  float m_loopLevelMax;
};
}  // namespace bkshepherd
#endif
#endif
