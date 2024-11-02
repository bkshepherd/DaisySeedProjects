#include "frequency_detector_yin.h"

#include <cmath>

#include "daisy.h"
#include "yin.hpp"

// Length of audio sample to use for pitch detection/sending to the yin
// algorithm
constexpr int bufferLength = 2048;
// Buffer used to store the audio samples that get sent to the yin algorithm
static float buffer[bufferLength];

// Allowed uncertainty in the result (0.0f to 1.0f)
const float yinThreshold = 0.15f;

// Probability cutoff for valid detections (0.0f to 1.0f)
const float yinProbabilityAllowed = 0.90f;

// Buffer used internally to the yin algorithm to avoid dynamic allocations
// during runtime
static float internalYinBuffer[bufferLength / 2];
// Structure used for sending data to the yin algorithm, processing, and getting
// results
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
}

float FrequencyDetectorYin::Process(float in) {
  buffer[m_bufferIndex++] = in;

  if (m_bufferIndex > bufferLength) {
    m_bufferIndex = 0;

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