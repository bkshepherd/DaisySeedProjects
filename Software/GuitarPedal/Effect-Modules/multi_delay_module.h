#pragma once
#ifndef MULTI_DELAY_MODULE_H
#define MULTI_DELAY_MODULE_H

#include <stdint.h>
#include "daisysp.h"
#include "base_effect_module.h"
#ifdef __cplusplus

/** @file multi_delay_module.h */

using namespace daisysp;

namespace bkshepherd
{

class MultiDelayModule : public BaseEffectModule
{
  public:
    MultiDelayModule();
    ~MultiDelayModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void SetTempo(uint32_t bpm) override;
    float GetBrightnessForLED(int led_id) override;
	void SetDelayTime(uint8_t index, float delay);
	void ParameterChanged(int parameter_id);
	void SetTargetTapDelayTime(uint8_t index, float value, float multiplier);

  private:
	bool m_isInitialized;
	float m_cachedEffectMagnitudeValue;
    float m_delaySamplesMin;
    float m_delaySamplesMax;
	float m_pitchShiftMin;
	float m_pitchShiftMax;
	float m_tapTargetDelay[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};
} // namespace bkshepherd
#endif
#endif
