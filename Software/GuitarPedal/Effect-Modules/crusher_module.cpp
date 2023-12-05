#include "crusher_module.h"

using namespace bkshepherd;

static const int s_paramCount = 3;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {name : "Level", valueType : ParameterValueType::FloatMagnitude, valueBinCount : 0, defaultValue : 40, knobMapping : 0, midiCCMapping : -1},
    {name : "Rate", valueType : ParameterValueType::FloatMagnitude, valueBinCount : 0, defaultValue : 1, knobMapping : 1, midiCCMapping : -1},
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
  crushcount = 0;
}

void CrusherModule::Process(float &outl, float &outr, float inl, float inr, int crushmod)
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

  float left, right;
  float level = m_levelMin + (GetParameterAsMagnitude(0) * (m_levelMax - m_levelMin));
  float cutoff = m_cutoffMin + GetParameterAsMagnitude(2) * (m_cutoffMax - m_cutoffMin);
  int crushmod = (int)m_rateMin + GetParameterAsMagnitude(1) * (m_rateMax - m_rateMin);

  tone.SetFreq(cutoff);

  Process(left, right, in, in, crushmod);

  m_audioRight = m_audioLeft = left * level;
}

void CrusherModule::ProcessStereo(float inL, float inR)
{
  BaseEffectModule::ProcessStereo(inL, inR);

  float left, right;
  float level = m_levelMin + (GetParameterAsMagnitude(0) * (m_levelMax - m_levelMin));
  float cutoff = m_cutoffMin + GetParameterAsMagnitude(2) * (m_cutoffMax - m_cutoffMin);
  int crushmod = (int)m_rateMin + GetParameterAsMagnitude(1) * (m_rateMax - m_rateMin);

  tone.SetFreq(cutoff);

  Process(left, right, inL, inR, crushmod);

  m_audioRight = right * level;
  m_audioLeft = left * level;
}
