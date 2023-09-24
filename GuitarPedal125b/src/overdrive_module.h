#pragma once
#ifndef OVERDRIVE_MODULE_H
#define OVERDRIVE_MODULE_H

#include <stdint.h>
#include "daisysp.h"
#include "base_effect_module.h"
#ifdef __cplusplus

/** @file overdrive_module.h */

using namespace daisysp;

namespace bkshepherd
{

class OverdriveModule : public BaseEffectModule
{
  public:
    OverdriveModule();
    ~OverdriveModule();

    void Init(float sample_rate) override;
    float ProcessMono(float in) override;
    float ProcessStereoLeft(float in) override;
    float ProcessStereoRight(float in) override;

  private:
    Overdrive m_overdriveLeft;
    Overdrive m_overdriveRight;
    float m_driveMin;
    float m_driveMax;
    float m_levelMin;
    float m_levelMax;
};
} // namespace bkshepherd
#endif
#endif
