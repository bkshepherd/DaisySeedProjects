#pragma once
#ifndef LOOPER_MODULE_H
#define LOOPER_MODULE_H

#include <stdint.h>

#include "../Util/looper.h"
#include "base_effect_module.h"
#include "daisysp.h"
#ifdef __cplusplus

/** @file looper_module.h */

using namespace daisysp;

namespace bkshepherd
{

class LooperModule : public BaseEffectModule
{
  public:
    LooperModule();
    ~LooperModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) override;
    bool AlternateFootswitchForTempo() const override
    {
        return false;
    }
    void AlternateFootswitchPressed() override;
    void AlternateFootswitchHeldFor1Second() override;
    void DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                bool isEditing) override;

  private:
    void SetLooperMode();

    // Loopers and the buffers they'll use
    daisysp_modified::Looper m_looperL;
    daisysp_modified::Looper m_looperR;

    float m_inputLevelMin;
    float m_inputLevelMax;
    float m_loopLevelMin;
    float m_loopLevelMax;
};
} // namespace bkshepherd
#endif
#endif
