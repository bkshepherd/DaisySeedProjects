#include "frequency_detector_q.h"

#include <new>

#include <q/fx/signal_conditioner.hpp>
#include <q/pitch/pitch_detector.hpp>
#include <q/support/pitch_names.hpp>

#include "daisy.h"

using namespace cycfi::q;

cycfi::q::pitch_detector *FrequencyDetectorQ::m_pitchDetector = nullptr;
cycfi::q::signal_conditioner *FrequencyDetectorQ::m_preProcessor = nullptr;

FrequencyDetectorQ::FrequencyDetectorQ() {}

FrequencyDetectorQ::~FrequencyDetectorQ() {}

bool FrequencyDetectorQ::IsInitialized() const { return m_pitchDetector != nullptr && m_preProcessor != nullptr; }

void FrequencyDetectorQ::Init(float sampleRate) {
    if (IsInitialized()) {
        return;
    }

    if ((m_pitchDetector == nullptr) != (m_preProcessor == nullptr)) {
        delete m_pitchDetector;
        m_pitchDetector = nullptr;
        delete m_preProcessor;
        m_preProcessor = nullptr;
    }

    if (m_pitchDetector == nullptr && m_preProcessor == nullptr) {
        // Use C1 as the detector floor and share a single heavy detector state
        // across all modules that use FrequencyDetectorQ.
        const frequency lowest_frequency = cycfi::q::pitch_names::C[1];
        const frequency highest_frequency = cycfi::q::pitch_names::C[5];

        auto *pitchDetector = new (std::nothrow) pitch_detector{lowest_frequency, highest_frequency, sampleRate, lin_to_db(0)};
        if (pitchDetector == nullptr) {
            return;
        }

        cycfi::q::signal_conditioner::config preprocessor_config;
        auto *preProcessor = new (std::nothrow) signal_conditioner{preprocessor_config, lowest_frequency, highest_frequency, sampleRate};
        if (preProcessor == nullptr) {
            delete pitchDetector;
            return;
        }

        m_pitchDetector = pitchDetector;
        m_preProcessor = preProcessor;
    }
}

float FrequencyDetectorQ::Process(float in) {
    if (!IsInitialized()) {
        return m_cachedFrequency;
    }

    // Pre-process the signal for pitch detection
    const float preProcessedSignal = m_preProcessor->operator()(in);

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
