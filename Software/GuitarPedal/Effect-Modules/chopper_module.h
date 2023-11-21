#pragma once
#ifndef CHOPPER_MODULE_H
#define CHOPPER_MODULE_H

#include <stdint.h>
#include "daisysp.h"
#include "base_effect_module.h"
#include "Chopper/chopper.h"
#ifdef __cplusplus

/** @file chopper_module.h */

using namespace daisysp;
using namespace bytebeat;

namespace bkshepherd
{

class ChopperModule : public BaseEffectModule
{
  public:
    ChopperModule();
    ~ChopperModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetOutputLEDBrightness() override;

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
