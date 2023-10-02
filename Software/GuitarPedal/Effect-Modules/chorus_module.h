#pragma once
#ifndef CHORUS_MODULE_H
#define CHORUS_MODULE_H

#include <stdint.h>
#include "daisysp.h"
#include "base_effect_module.h"
#ifdef __cplusplus

/** @file chorus_module.h */

using namespace daisysp;

namespace bkshepherd
{

class ChorusModule : public BaseEffectModule
{
  public:
    ChorusModule();
    ~ChorusModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;

  private:
    Chorus m_chorus;
    float m_lfoFreqMin;
    float m_lfoFreqMax;
};
} // namespace bkshepherd
#endif
#endif
