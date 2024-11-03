#pragma once
#ifndef FREQUENCY_DETECTOR_Q_H
#define FREQUENCY_DETECTOR_Q_H
#include <cstdint>

#include "1efilter.hpp"
#include "frequency_detector_interface.h"

namespace cycfi {
namespace q {
class pitch_detector;
class signal_conditioner;
}  // namespace q
}  // namespace cycfi

class FrequencyDetectorQ : public FrequencyDetectorInterface {
 public:
  FrequencyDetectorQ();
  virtual ~FrequencyDetectorQ();
  void Init(float sampleRate) override;
  float Process(float in) override;

 private:
  float m_cachedFrequency;
  int m_bufferIndex;

  cycfi::q::pitch_detector *m_pitchDetector = nullptr;
  cycfi::q::signal_conditioner *m_preProcessor = nullptr;

  // inputs:
  // estimated frequency: overwritten by timestamps at runtime and not used
  // cutoff freq
  // beta: 0.0f disables it entirely, but used for scaling cutoff frequency
  // derivative cutoff freq: used when beta is > 0
  one_euro_filter<float, float> m_smoothingFilter{48000, 0.5f, 0.05f, 1.0f};
};
#endif