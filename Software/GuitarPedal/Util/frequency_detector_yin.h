#pragma once
#ifndef FREQUENCY_DETECTOR_YIN_H
#define FREQUENCY_DETECTOR_YIN_H
#include <cstdint>

#include "1efilter.hpp"
#include "frequency_detector_interface.h"
class FrequencyDetectorYin : public FrequencyDetectorInterface {
  public:
    FrequencyDetectorYin();
    virtual ~FrequencyDetectorYin();
    void Init(float sampleRate) override;
    float Process(float in) override;

  private:
    float m_cachedFrequency = 0.0f;
    uint32_t m_bufferIndex = 0;
    uint32_t m_sampleRate = 0;

    // inputs:
    // estimated frequency: overwritten by timestamps at runtime and not used
    // cutoff freq
    // beta: 0.0f disables it entirely, but used for scaling cutoff frequency
    // derivative cutoff freq: used when beta is > 0
    one_euro_filter<float, float> m_smoothingFilter{48000, 0.5f, 0.05f, 1.0f};
};
#endif