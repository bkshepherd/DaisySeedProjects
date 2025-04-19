#pragma once
#ifndef IR_MODULE_H
#define IR_MODULE_H

#include "ImpulseResponse/ImpulseResponse.h"
#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>

#ifdef __cplusplus

/** @file amp_module.h */

using namespace daisysp;

namespace bkshepherd {

class IrModule : public BaseEffectModule {
  public:
    IrModule();
    ~IrModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;

    void SelectIR();
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;
    // void AlternateFootswitchPressed() override;

  private:
    float m_levelMin;
    float m_levelMax;

    float m_cachedEffectMagnitudeValue;

    ImpulseResponse mIR;
    int m_currentIRindex = -1;
};
} // namespace bkshepherd
#endif
#endif
