#include "reverb_module.h"
#include <array>

using namespace bkshepherd;

static const auto s_metaData = [] {
    std::array<ParameterMetaData, ReverbModule::PARAM_COUNT> params{};

    params[ReverbModule::TIME] = {
        name : "Time",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.45f},
        knobMapping : 0,
        midiCCMapping : 1
    };

    params[ReverbModule::DAMP] = {
        name : "Damp",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : 1,
        midiCCMapping : 21
    };

    params[ReverbModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.45f},
        knobMapping : 2,
        midiCCMapping : 22
    };

    return params;
}();

ReverbSc DSY_SDRAM_BSS reverbStereo;
// Default Constructor
ReverbModule::ReverbModule()
    : BaseEffectModule(), m_timeMin(0.6f), m_timeMax(1.0f), m_lpFreqMin(600.0f), m_lpFreqMax(16000.0f)

{
    // Set the name of the effect
    m_name = "Reverb";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
ReverbModule::~ReverbModule() {
    // No Code Needed
}

void ReverbModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_reverbStereo = &reverbStereo;
    m_reverbStereo->Init(sample_rate);
}

void ReverbModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    float sendl, sendr, wetl, wetr; // Reverb Inputs/Outputs
    sendl = m_audioLeft;
    sendr = m_audioRight;

    // Calculate the effect
    m_reverbStereo->SetFeedback(m_timeMin + GetParameterAsFloat(TIME) * (m_timeMax - m_timeMin));
    float invertedFreq =
        1.0 - GetParameterAsFloat(DAMP); // Invert the damping param so that knob left is less dampening, knob right is more dampening
    invertedFreq = invertedFreq * invertedFreq; // also square it for exponential taper (more control over lower frequencies)
    m_reverbStereo->SetLpFreq(m_lpFreqMin + invertedFreq * (m_lpFreqMax - m_lpFreqMin));

    m_reverbStereo->Process(sendl, sendr, &wetl, &wetr);
    m_audioLeft = wetl * GetParameterAsFloat(MIX) + sendl * (1.0 - GetParameterAsFloat(MIX));
    m_audioRight = wetr * GetParameterAsFloat(MIX) + sendr * (1.0 - GetParameterAsFloat(MIX));
}

void ReverbModule::ProcessStereo(float inL, float inR) {
    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(inL, inR);

    float sendl, sendr, wetl, wetr; // Reverb Inputs/Outputs
    sendl = m_audioLeft;
    sendr = m_audioRight;

    // Calculate the effect
    m_reverbStereo->SetFeedback(m_timeMin + GetParameterAsFloat(TIME) * (m_timeMax - m_timeMin));
    float invertedFreq =
        1.0 - GetParameterAsFloat(DAMP); // Invert the damping param so that knob left is less dampening, knob right is more dampening
    invertedFreq = invertedFreq * invertedFreq; // also square it for exponential taper (more control over lower frequencies)
    m_reverbStereo->SetLpFreq(m_lpFreqMin + invertedFreq * (m_lpFreqMax - m_lpFreqMin));

    m_reverbStereo->Process(sendl, sendr, &wetl, &wetr);
    m_audioLeft = wetl * GetParameterAsFloat(MIX) + inL * (1.0 - GetParameterAsFloat(MIX));
    m_audioRight = wetr * GetParameterAsFloat(MIX) + inR * (1.0 - GetParameterAsFloat(MIX));
}

float ReverbModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value; // * GetParameterAsFloat(TIME);
    }

    return value;
}