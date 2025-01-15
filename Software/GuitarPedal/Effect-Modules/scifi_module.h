#pragma once
#ifndef SCIFI_MODULE_H
#define SCIFI_MODULE_H

#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file scifi_module.h */

// The scifi module is 3 effects in one, a polyoctave into reverb into overdrive.

//
// NOTE: This the octave effect code was adapted from https://github.com/schult/terrarium-poly-octave
//       (Under the MIT License)

using namespace daisysp;

namespace bkshepherd {

class SciFiModule : public BaseEffectModule {
  public:
    SciFiModule();
    ~SciFiModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    ReverbSc *m_reverbStereo;
    int bin_counter = 0;
    float buff[6];
    float buff_out[6];

    float m_driveMin;
    float m_driveMax;
    float m_levelMin;
    float m_levelMax;

    float m_timeMin;
    float m_timeMax;
    float m_lpFreqMin;
    float m_lpFreqMax;

    Overdrive m_overdriveLeft;
    Overdrive m_overdriveRight;

    float m_cachedEffectMagnitudeValue;
};
} // namespace bkshepherd
#endif
#endif
