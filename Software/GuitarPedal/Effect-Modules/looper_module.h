#pragma once
#ifndef LOOPER_MODULE_H
#define LOOPER_MODULE_H

#include "../Util/varSpeedLooper.h"
#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>
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
    float GetBrightnessForLED(int led_id) const override;
    bool AlternateFootswitchForTempo() const override { return false; }
    void AlternateFootswitchPressed() override;
    void AlternateFootswitchHeldFor1Second() override;
    void DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                bool isEditing) override;

  private:
    void SetLooperMode();
    daisysp::Tone tone;  // Low Pass
    daisysp::Tone toneR; // Low Pass
    daisysp_modified::varSpeedLooper m_looper;
    daisysp_modified::varSpeedLooper m_looperR; // Added another looper for stereo loops

    float m_inputLevelMin;
    float m_inputLevelMax;
    float m_loopLevelMin;
    float m_loopLevelMax;
    float currentSpeed;
    float m_toneFreqMin;
    float m_toneFreqMax;
};
} // namespace bkshepherd
#endif
#endif
