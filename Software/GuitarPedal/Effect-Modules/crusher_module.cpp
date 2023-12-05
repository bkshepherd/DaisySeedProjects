#include "crusher_module.h"

using namespace bkshepherd;

static const int s_paramCount = 3;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {name : "Rate", valueType : ParameterValueType::FloatMagnitude, valueBinCount : 0, defaultValue : 57, knobMapping : 0, midiCCMapping : 1},
    {name : "Level", valueType : ParameterValueType::FloatMagnitude, valueBinCount : 0, defaultValue : 40, knobMapping : 1, midiCCMapping : 21},
    {name : "Cutoff", valueType : ParameterValueType::FloatMagnitude, valueBinCount : 0, defaultValue : 40, knobMapping : 2, midiCCMapping : 21},
};

// Default Constructor
CrusherModule::CrusherModule() : BaseEffectModule()
//, m_driveMin(0.4f), m_driveMax(0.8f), m_levelMin(0.01f), m_levelMax(0.20f)

{
  // Set the name of the effect
  m_name = "Crusher";

  // Setup the meta data reference for this Effect
  m_paramMetaData = s_metaData;

  // Initialize Parameters for this Effect
  this->InitParams(s_paramCount);
}

// Destructor
CrusherModule::~CrusherModule()
{
  // No Code Needed
}

void CrusherModule::Init(float sample_rate)
{
  BaseEffectModule::Init(sample_rate);
  tone.Init(sample_rate);
  crushmod = 1; // 1- 50
  cutoff = 500; // 500 - 20000
}

void CrusherModule::Process(float &outl, float &outr, float inl, float inr)
{
  crushcount++;
  crushcount %= crushmod;
  if (crushcount == 0) {
    crushsr = inr;
    crushsl = inl;
  }
  outl = tone.Process(crushsl);
  outr = tone.Process(crushsr);
}

void CrusherModule::ProcessMono(float in)
{
  BaseEffectModule::ProcessMono(in);

  cutoff = GetParameterAsMagnitude(2) * 19500 + 500;
  tone.SetFreq(cutoff);
  crushmod = (int)GetParameterAsMagnitude(0) * 49 - 1;

  float left, right;
  Process(left, right, in, in);

  /*
    // Calculate the effect
    m_overdriveLeft.SetDrive(m_driveMin + (GetParameterAsMagnitude(0) * (m_driveMax - m_driveMin)));
    m_audioLeft = m_overdriveLeft.Process(m_audioLeft);

    // Adjust the level
    m_audioLeft = m_audioLeft * (m_levelMin + (GetParameterAsMagnitude(1) * (m_levelMax - m_levelMin)));
  */
  m_audioRight = m_audioLeft = left;
}

void CrusherModule::ProcessStereo(float inL, float inR)
{
  cutoff = GetParameterAsMagnitude(2) * 19500 + 500;
  tone.SetFreq(cutoff);
  crushmod = (int)GetParameterAsMagnitude(0) * 49 - 1;

  float left, right;
  Process(left, right, inL, inR);

  /*
    // Calculate the effect
    m_overdriveLeft.SetDrive(m_driveMin + (GetParameterAsMagnitude(0) * (m_driveMax - m_driveMin)));
    m_audioLeft = m_overdriveLeft.Process(m_audioLeft);

    // Adjust the level
    m_audioLeft = m_audioLeft * (m_levelMin + (GetParameterAsMagnitude(1) * (m_levelMax - m_levelMin)));
  */
  m_audioRight = m_audioLeft = left;
  m_audioRight = inR;
  m_audioLeft = inL;
}

float CrusherModule::GetBrightnessForLED(int led_id)
{
  float value = BaseEffectModule::GetBrightnessForLED(led_id);

  if (led_id == 1) {
    // return value * GetParameterAsMagnitude(0);
  }

  return value;
}