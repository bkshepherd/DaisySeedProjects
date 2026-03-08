#include "overdrive_module.h"
#include <array>

using namespace bkshepherd;

static const auto s_metaData = [] {
    std::array<ParameterMetaData, OverdriveModule::PARAM_COUNT> params{};

    params[OverdriveModule::DRIVE] = {
        name : "Drive",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.45f},
        knobMapping : 1,
        midiCCMapping : 1
    };

    params[OverdriveModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : 0,
        midiCCMapping : 21
    };

    return params;
}();

// Default Constructor
OverdriveModule::OverdriveModule()
    : BaseEffectModule(), m_driveMin(0.4f), m_driveMax(0.8f), m_levelMin(0.01f), m_levelMax(0.20f)

{
    // Set the name of the effect
    m_name = "Overdrive";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
OverdriveModule::~OverdriveModule() {
    // No Code Needed
}

void OverdriveModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    m_overdriveLeft.Init();
    m_overdriveRight.Init();
}

void OverdriveModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    // Calculate the effect
    m_overdriveLeft.SetDrive(m_driveMin + (GetParameterAsFloat(DRIVE) * (m_driveMax - m_driveMin)));
    m_audioLeft = m_overdriveLeft.Process(m_audioLeft);

    // Adjust the level
    m_audioLeft = m_audioLeft * (m_levelMin + (GetParameterAsFloat(LEVEL) * (m_levelMax - m_levelMin)));
    m_audioRight = m_audioLeft;
}

void OverdriveModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(m_audioLeft, inR);

    // Calculate the effect
    m_overdriveRight.SetDrive(m_driveMin + (GetParameterAsFloat(DRIVE) * (m_driveMax - m_driveMin)));
    m_audioRight = m_overdriveRight.Process(m_audioRight);

    // Adjust the level
    m_audioRight = m_audioRight * (m_levelMin + (GetParameterAsFloat(LEVEL) * (m_levelMax - m_levelMin)));
}

float OverdriveModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * GetParameterAsFloat(DRIVE);
    }

    return value;
}