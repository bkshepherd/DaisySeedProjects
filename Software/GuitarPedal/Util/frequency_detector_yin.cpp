#include "frequency_detector_yin.h"

#include <cmath>

#include "daisy.h"
#include "yin.h"

static uint32_t bufferIndex = 0;
const float yinThreshold = 0.15;
constexpr int yinBufferLength = 2048;
static float buffer[yinBufferLength];
static float internalYinBuffer[yinBufferLength / 2];
static Yin yin{yinBufferLength, yinBufferLength / 2, &internalYinBuffer[0], 0.f,
               yinThreshold};

FrequencyDetectorYin::FrequencyDetectorYin() : FrequencyDetectorInterface() {
  //
}

FrequencyDetectorYin::~FrequencyDetectorYin() {
  //
}

void FrequencyDetectorYin::Init(float sampleRate) {
  m_sampleRate = static_cast<uint32_t>(sampleRate);

  for (int i = 0; i < yinBufferLength; i++) {
    buffer[i] = 0.0f;
  }

  m_smoothingFilter.setSampleRate(sampleRate);
}

float FrequencyDetectorYin::Process(float in) {
  buffer[bufferIndex++] = in;

  if (bufferIndex > yinBufferLength) {
    bufferIndex = 0;

    // Reinitialize yin struct
    yin.probability = 0.0f;
    yin.threshold = yinThreshold;
    yin.bufferSize = yinBufferLength;
    yin.halfBufferSize = yinBufferLength / 2;
    for (int i = 0; i < yin.halfBufferSize; i++) {
      yin.yinBuffer[i] = 0.0f;
    }

    // Get pitch
    const float freq = Yin_getPitch(&yin, &buffer[0], m_sampleRate);

    if (yin.probability > 0.90) {
      // Run a smoothing filter on the detected frequency
      const float currentTimeInSeconds =
          static_cast<float>(daisy::System::GetNow()) / 1000.f;
      m_cachedFrequency = m_smoothingFilter(freq, currentTimeInSeconds);
    }
  }

  return m_cachedFrequency;
}