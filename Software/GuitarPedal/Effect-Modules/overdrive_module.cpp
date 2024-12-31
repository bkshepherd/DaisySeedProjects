#include "overdrive_module.h"

using namespace bkshepherd;

static const int s_paramCount = 2;
static const ParameterMetaData s_metaData[s_paramCount] = {{
                                                               name : "Drive",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.45f},
                                                               knobMapping : 1,
                                                               midiCCMapping : 1
                                                           },
                                                           {
                                                               name : "Level",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.3f},
                                                               knobMapping : 0,
                                                               midiCCMapping : 21
                                                           }};

// Default Constructor
OverdriveModule::OverdriveModule()
    : BaseEffectModule(), m_driveMin(0.4f), m_driveMax(0.8f), m_levelMin(0.01f), m_levelMax(0.20f)

{
    // Set the name of the effect
    m_name = "Overdrive";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
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
    m_overdriveLeft.SetDrive(m_driveMin + (GetParameterAsFloat(0) * (m_driveMax - m_driveMin)));
    m_audioLeft = m_overdriveLeft.Process(m_audioLeft);

    // Adjust the level
    m_audioLeft = m_audioLeft * (m_levelMin + (GetParameterAsFloat(1) * (m_levelMax - m_levelMin)));
    m_audioRight = m_audioLeft;
}

void OverdriveModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(m_audioLeft, inR);

    // Calculate the effect
    m_overdriveRight.SetDrive(m_driveMin + (GetParameterAsFloat(0) * (m_driveMax - m_driveMin)));
    m_audioRight = m_overdriveRight.Process(m_audioRight);

    // Adjust the level
    m_audioRight = m_audioRight * (m_levelMin + (GetParameterAsFloat(1) * (m_levelMax - m_levelMin)));
}

float OverdriveModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * GetParameterAsFloat(0);
    }

    return value;
}