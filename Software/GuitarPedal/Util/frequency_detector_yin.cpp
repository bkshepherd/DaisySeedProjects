#include "frequency_detector_yin.h"

#include <cmath>

#include "daisy.h"
#include "yin.hpp"

// Buffer used to store the audio samples that get sent to the yin algorithm
static float buffer[yin::audioBufferLength];

// Allowed uncertainty in the result (0.0f to 1.0f)
const float yinThreshold = 0.15f;

// Probability cutoff for valid detections (0.0f to 1.0f)
const float yinProbabilityAllowed = 0.90f;

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
    for (int i = 0; i < yin::audioBufferLength; i++) {
        buffer[i] = 0.0f;
    }
}

float FrequencyDetectorYin::Process(float in) {
    buffer[m_bufferIndex++] = in;

    if (m_bufferIndex > yin::audioBufferLength) {
        m_bufferIndex = 0;

        // Reinitialize yin
        yin::init(&yinData, yinThreshold);

        // Get pitch
        const float freq = yin::getPitch(&yinData, &buffer[0], m_sampleRate);

        if (yinData.probability > yinProbabilityAllowed) {
            // Run a smoothing filter on the detected frequency
            const float currentTimeInSeconds = static_cast<float>(daisy::System::GetNow()) / 1000.f;
            m_cachedFrequency = m_smoothingFilter(freq, currentTimeInSeconds);
        }
    }

    return m_cachedFrequency;
}