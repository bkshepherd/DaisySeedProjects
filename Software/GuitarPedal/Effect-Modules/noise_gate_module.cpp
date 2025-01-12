#include "noise_gate_module.h"
#include <q/fx/envelope.hpp>

using namespace bkshepherd;

// The threshold knob/parameter will generate a float between 0 and 1 which gets
// multiplied by this value to be used as the threshold against the audio input
// signal
const float maxThreshold = 0.2;

// These are placeholder values that will get overwritten with the attack and release parameters at initialization
cycfi::q::ar_envelope_follower env_follower(48000.0, 0.002f, 0.020f);

static const int s_paramCount = 5;
static const ParameterMetaData s_metaData[s_paramCount] = {{
                                                               name : "Threshold",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.5f},
                                                               knobMapping : 0,
                                                               midiCCMapping : -1
                                                           },
                                                           {
                                                               name : "Atk [ms]",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 1.0f},
                                                               knobMapping : 1,
                                                               midiCCMapping : -1,
                                                               minValue : static_cast<int>(1),
                                                               maxValue : static_cast<int>(20)
                                                           },
                                                           {
                                                               name : "Rel [ms]",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 50.0f},
                                                               knobMapping : 2,
                                                               midiCCMapping : -1,
                                                               minValue : static_cast<int>(1),
                                                               maxValue : static_cast<int>(500)
                                                           },
                                                           {
                                                               name : "Hold [ms]",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 50.0f},
                                                               knobMapping : 3,
                                                               midiCCMapping : -1,
                                                               minValue : static_cast<int>(0),
                                                               maxValue : static_cast<int>(1000)
                                                           },
                                                           {
                                                               name : "Fade [ms]",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 200.0f},
                                                               knobMapping : 4,
                                                               midiCCMapping : -1,
                                                               minValue : static_cast<int>(0),
                                                               maxValue : static_cast<int>(500)
                                                           }};

// Default Constructor
NoiseGateModule::NoiseGateModule() : BaseEffectModule() {
    // Set the name of the effect
    m_name = "Noise Gate";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
NoiseGateModule::~NoiseGateModule() {
    // No Code Needed
}

void NoiseGateModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    env_follower.config(GetParameterAsFloat(1) / 1000.f, GetParameterAsFloat(2) / 1000.f, sample_rate);
}

void NoiseGateModule::ParameterChanged(int parameter_id) {
    switch (parameter_id) {
    case 1:
        env_follower.attack(GetParameterAsFloat(1) / 1000.f, GetSampleRate());
        break;
    case 2:
        env_follower.release(GetParameterAsFloat(2) / 1000.f, GetSampleRate());
        break;
    }
}

void NoiseGateModule::ProcessMono(float in) {
    // Update envelope follower with input signal
    const float raw_env_level = env_follower(std::abs(in));

    // Run a smoothing filter on the envelope to prevent crackling due to rapid adjustments of the envelope state
    const float currentTimeInSeconds = static_cast<float>(daisy::System::GetNow()) / 1000.f;
    const float smoothed_env_level = m_smoothingFilter(raw_env_level, currentTimeInSeconds);

    if (smoothed_env_level > GetParameterAsFloat(0) * maxThreshold) {
        // Signal is above the threshold, open the gate and reset the timer
        m_gateOpen = true;
        m_holdTimer = 0.0f;
        m_currentGain = 1.0f; // Fully open
    } else if (m_gateOpen) {
        // Signal is below the threshold but within hold time

        const float dt = currentTimeInSeconds - m_prevTimeSeconds;
        m_holdTimer += dt;

        if (m_holdTimer >= (GetParameterAsFloat(3) / 1000.0f)) {
            // Gate is closing: start fading out
            const float fadeOutStep = dt / (GetParameterAsFloat(4) / 1000.0f);
            m_currentGain -= fadeOutStep;
            if (m_currentGain <= 0.0f) {
                m_currentGain = 0.0f;
                m_gateOpen = false;
            }
        }
    }

    m_prevTimeSeconds = currentTimeInSeconds;

    // Apply noise gate using the smoothed envelope value and hold time, and
    // also the current gain value (used for fade)
    const float out = m_gateOpen ? in * m_currentGain : 0.0f;

    m_audioLeft = out;
    m_audioRight = m_audioLeft;
}

void NoiseGateModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);
}

float NoiseGateModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return m_gateOpen ? 1.0f : 0.0f;
    }

    return value;
}