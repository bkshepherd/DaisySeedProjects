#include "frequency_detector_q.h"

#include <q/fx/signal_conditioner.hpp>
#include <q/pitch/pitch_detector.hpp>
#include <q/support/pitch_names.hpp>

#include "daisy.h"

using namespace cycfi::q;

FrequencyDetectorQ::FrequencyDetectorQ() {}

FrequencyDetectorQ::~FrequencyDetectorQ() {
    delete m_pitchDetector;
    m_pitchDetector = nullptr;

    delete m_preProcessor;
    m_preProcessor = nullptr;
}

void FrequencyDetectorQ::Init(float sampleRate) {
    // The frequency detection bounds;
    frequency lowest_frequency = cycfi::q::pitch_names::C[2];
    frequency highest_frequency = cycfi::q::pitch_names::C[5];

    m_pitchDetector = new pitch_detector{lowest_frequency, highest_frequency, sampleRate, lin_to_db(0)};

    cycfi::q::signal_conditioner::config preprocessor_config;
    m_preProcessor = new signal_conditioner{preprocessor_config, lowest_frequency, highest_frequency, sampleRate};
}

float FrequencyDetectorQ::Process(float in) {
    // Pre-process the signal for pitch detection
    float preProcessedSignal = m_preProcessor->operator()(in);

    // Send the processed sample through the pitch detector
    const bool ready = m_pitchDetector->operator()(preProcessedSignal);

    // If result is ready, get the detected frequency
    if (ready) {
        const float freq = m_pitchDetector->get_frequency();

        // Run a smoothing filter on the detected frequency
        const float currentTimeInSeconds = static_cast<float>(daisy::System::GetNow()) / 1000.f;
        m_cachedFrequency = m_smoothingFilter(freq, currentTimeInSeconds);
    }

    return m_cachedFrequency;
}