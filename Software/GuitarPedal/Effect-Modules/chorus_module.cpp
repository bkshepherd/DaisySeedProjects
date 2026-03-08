#include "chorus_module.h"
#include <array>

using namespace bkshepherd;

static const auto s_metaData = [] {
    std::array<ParameterMetaData, ChorusModule::PARAM_COUNT> params{};

    params[ChorusModule::WET] = {
        name : "Wet",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.65f},
        knobMapping : 0,
        midiCCMapping : 20
    };

    params[ChorusModule::DELAY] = {
        name : "Delay",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : 21
    };

    params[ChorusModule::LFO_FREQ] = {
        name : "LFO Freq",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.25f},
        knobMapping : 2,
        midiCCMapping : 22
    };

    params[ChorusModule::LFO_DEPTH] = {
        name : "LFO Depth",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : 3,
        midiCCMapping : 23
    };

    params[ChorusModule::FEEDBACK] = {
        name : "Feedback",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.25f},
        knobMapping : 4,
        midiCCMapping : 24
    };

    return params;
}();

// Default Constructor
ChorusModule::ChorusModule()
    : BaseEffectModule(), m_lfoFreqMin(1.0f), m_lfoFreqMax(20.0f)

{
    // Set the name of the effect
    m_name = "Chorus";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
ChorusModule::~ChorusModule() {
    // No Code Needed
}

void ChorusModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    m_chorus.Init(sample_rate);
}

void ChorusModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    // Calculate the effect
    m_chorus.SetDelay(GetParameterAsFloat(DELAY));
    m_chorus.SetLfoFreq(m_lfoFreqMin + (GetParameterAsFloat(LFO_FREQ) * GetParameterAsFloat(LFO_FREQ) * (m_lfoFreqMax - m_lfoFreqMin)));
    m_chorus.SetLfoDepth(GetParameterAsFloat(LFO_DEPTH));
    m_chorus.SetFeedback(GetParameterAsFloat(FEEDBACK));

    m_chorus.Process(m_audioLeft);

    m_audioLeft = m_chorus.GetLeft() * GetParameterAsFloat(WET) + m_audioLeft * (1.0f - GetParameterAsFloat(WET));
    m_audioRight = m_chorus.GetRight() * GetParameterAsFloat(WET) + m_audioRight * (1.0f - GetParameterAsFloat(WET));
}

void ChorusModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(m_audioLeft, inR);

    // Calculate the effect
    m_audioRight = m_chorus.GetRight() * GetParameterAsFloat(WET) + m_audioRight * (1.0f - GetParameterAsFloat(WET));
}

float ChorusModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * GetParameterAsFloat(WET);
    }

    return value;
}