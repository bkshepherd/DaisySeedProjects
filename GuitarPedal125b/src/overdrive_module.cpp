#include "overdrive_module.h"

using namespace bkshepherd;

static const int s_paramCount = 2;
static const ParameterMetaData s_metaData[s_paramCount] = {{"Drive", 57, 1, 1},
                                                           {"Level", 40, 0, 21}};

// Default Constructor
OverdriveModule::OverdriveModule() : BaseEffectModule(),
                                        m_driveMin(0.4f),
                                        m_driveMax(0.8f),
                                        m_levelMin(0.01f),
                                        m_levelMax(0.20f)

{
    // Set the name of the effect
    m_name = "Overdrive";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
OverdriveModule::~OverdriveModule()
{
    // No Code Needed
}

void OverdriveModule::Init(float sample_rate)
{
    BaseEffectModule::Init(sample_rate);

    m_overdriveLeft.Init();
    m_overdriveRight.Init();
}

float OverdriveModule::ProcessMono(float in)
{
    float value = BaseEffectModule::ProcessMono(in);

    // Calculate the effect
    m_overdriveLeft.SetDrive(m_driveMin + (GetParameterAsMagnitude(0) * (m_driveMax - m_driveMin)));
    value = m_overdriveLeft.Process(value);

    // Adjust the level
    value = value * (m_levelMin + (GetParameterAsMagnitude(1) * (m_levelMax - m_levelMin)));

    return value;
}

float OverdriveModule::ProcessStereoLeft(float in)
{    
    return ProcessMono(in);
}

float OverdriveModule::ProcessStereoRight(float in)
{
    float value = BaseEffectModule::ProcessStereoRight(in);

    // Calculate the effect
    m_overdriveRight.SetDrive(m_driveMin + (GetParameterAsMagnitude(0) * (m_driveMax - m_driveMin)));
    value = m_overdriveRight.Process(value);

    // Adjust the level
    value = value * (m_levelMin + (GetParameterAsMagnitude(1) * (m_levelMax - m_levelMin)));

    return value;
}
