#include "crusher_module.h"

using namespace bkshepherd;

static const int s_paramCount = 3;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {name : "Rate", valueType : ParameterValueType::FloatMagnitude, valueBinCount : 0, defaultValue : 1, knobMapping : 0, midiCCMapping : -1},
    {name : "Level", valueType : ParameterValueType::FloatMagnitude, valueBinCount : 0, defaultValue : 40, knobMapping : 1, midiCCMapping : -1},
    {name : "Cutoff", valueType : ParameterValueType::FloatMagnitude, valueBinCount : 0, defaultValue : 40, knobMapping : 2, midiCCMapping : -1},
};

// Default Constructor
CrusherModule::CrusherModule() : BaseEffectModule(), m_rateMin(1), m_rateMax(50), m_levelMin(0.01), m_levelMax(20), m_cutoffMin(500), m_cutoffMax(20000)
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

  float level = m_levelMin + (GetParameterAsMagnitude(1) * (m_levelMax - m_levelMin));
  float cutoff = m_cutoffMin + GetParameterAsMagnitude(2) * (m_cutoffMax - m_cutoffMin);

  tone.SetFreq(cutoff);

  crushmod = (int)m_rateMin + GetParameterAsMagnitude(0) * (m_rateMax - m_rateMin);

  float left, right;
  Process(left, right, in, in);

  m_audioRight = m_audioLeft = left * level;
}

void CrusherModule::ProcessStereo(float inL, float inR)
{
  float level = m_levelMin + (GetParameterAsMagnitude(1) * (m_levelMax - m_levelMin));
  float cutoff = m_cutoffMin + GetParameterAsMagnitude(2) * (m_cutoffMax - m_cutoffMin);

  tone.SetFreq(cutoff);

  crushmod = (int)m_rateMin + GetParameterAsMagnitude(0) * (m_rateMax - m_rateMin);

  float left, right;
  Process(left, right, inL, inR);

  m_audioRight = right * level;
  m_audioLeft = left * level;
}
/*
float CrusherModule::GetBrightnessForLED(int led_id)
{
  float value = BaseEffectModule::GetBrightnessForLED(led_id);

  if (led_id == 1) {
    // return value * GetParameterAsMagnitude(0);
  }

  return value;
}
*/