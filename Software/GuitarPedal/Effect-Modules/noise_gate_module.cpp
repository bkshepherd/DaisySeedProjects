#include "noise_gate_module.h"
#include <q/fx/envelope.hpp>

using namespace bkshepherd;

// The threshold knob/parameter will generate a float between 0 and 1 which gets
// multiplied by this value to be used as the threshold against the audio input
// signal
const float maxThreshold = 0.15;

// These are placeholder values that will get overwritten with the attack and release parameters at initialization
cycfi::q::ar_envelope_follower env_follower(48000.0, 0.002f, 0.020f);

static const int s_paramCount = 3;
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
                                                               defaultValue : {.float_value = 20.0f},
                                                               knobMapping : 2,
                                                               midiCCMapping : -1,
                                                               minValue : static_cast<int>(1),
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
    case 2:
        env_follower.config(GetParameterAsFloat(1) / 1000.f, GetParameterAsFloat(2) / 1000.f, GetSampleRate());
        break;
    }
}

void NoiseGateModule::ProcessMono(float in) {
    // Update envelope follower with input signal
    float env_level = env_follower(in);

    // Apply noise gate
    const float out = (env_level > GetParameterAsFloat(0) * maxThreshold) ? in : 0.0f;

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
        return m_audioLeft > 0 ? 1.0f : 0.0f;
    }

    return value;
}