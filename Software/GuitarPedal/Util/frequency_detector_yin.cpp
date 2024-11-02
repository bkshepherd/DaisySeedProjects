#include "frequency_detector_yin.h"

#include <cmath>

#include "daisy.h"
#include "yin.hpp"

static uint32_t bufferIndex = 0;
constexpr int bufferLength = 2048;
static float buffer[bufferLength];

const float yinThreshold = 0.15f;
const float yinProbabilityAllowed = 0.90f;
static float internalYinBuffer[bufferLength / 2];
static yin::Yin yinData;

FrequencyDetectorYin::FrequencyDetectorYin() : FrequencyDetectorInterface() {
  //
}

FrequencyDetectorYin::~FrequencyDetectorYin() {
  //
}

void FrequencyDetectorYin::Init(float sampleRate) {
  m_sampleRate = static_cast<uint32_t>(sampleRate);

  // Initialize buffer just in case
  for (int i = 0; i < bufferLength; i++) {
    buffer[i] = 0.0f;
  }

  m_smoothingFilter.setSampleRate(sampleRate);
}

float FrequencyDetectorYin::Process(float in) {
  buffer[bufferIndex++] = in;

  if (bufferIndex > bufferLength) {
    bufferIndex = 0;

    // Reinitialize yin
    yin::init(&yinData, internalYinBuffer, bufferLength, yinThreshold);

    // Get pitch
    const float freq = yin::getPitch(&yinData, &buffer[0], m_sampleRate);

    if (yinData.probability > yinProbabilityAllowed) {
      // Run a smoothing filter on the detected frequency
      const float currentTimeInSeconds =
          static_cast<float>(daisy::System::GetNow()) / 1000.f;
      m_cachedFrequency = m_smoothingFilter(freq, currentTimeInSeconds);
    }
  }

  return m_cachedFrequency;
}