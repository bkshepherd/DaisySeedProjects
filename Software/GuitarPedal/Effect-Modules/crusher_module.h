#pragma once
#ifndef CRUSHER_MODULE_H
#define CRUSHER_MODULE_H

#include <stdint.h>
#include "daisysp.h"
#include "base_effect_module.h"
#ifdef __cplusplus

/** @file crusher_module.h */

using namespace daisysp;

namespace bkshepherd
{

class CrusherModule : public BaseEffectModule
{
public:
  CrusherModule();
  ~CrusherModule();

  void Init(float sample_rate) override;
  void ProcessMono(float in) override;
  void ProcessStereo(float inL, float inR) override;
  float GetBrightnessForLED(int led_id) override;

private:
  void Process(float &outl, float &outr, float inl, float inr);

  Tone tone;
  int crushmod, crushcount;
  float crushsl, crushsr, drywet;

  // Parameter limits
  float m_rateMin;
  float m_rateMax;
  float m_levelMin;
  float m_levelMax;
  float m_cutoffMin;
  float m_cutoffMax;
};
} // namespace bkshepherd
#endif
#endif
