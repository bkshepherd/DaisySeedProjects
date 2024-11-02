#pragma once
#ifndef FREQUENCY_DETECTOR_YIN_H
#define FREQUENCY_DETECTOR_YIN_H
#include <cstdint>

#include "frequency_detector_interface.h"
class FrequencyDetectorYin : public FrequencyDetectorInterface {
 public:
  FrequencyDetectorYin();
  virtual ~FrequencyDetectorYin();
  void Init(float sampleRate) override;
  void Process(float in) override;
  float GetFrequency() const override { return m_cachedFrequency; }

 private:
  float m_cachedFrequency;
  int m_bufferIndex;
  uint32_t m_sampleRate;
};
#endif