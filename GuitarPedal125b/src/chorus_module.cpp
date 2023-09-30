#include "chorus_module.h"

using namespace bkshepherd;

static const int s_paramCount = 5;
static const ParameterMetaData s_metaData[s_paramCount] = {{"Wet", 83, 0, 20},
                                                           {"Delay", 64, 1, 21},
                                                           {"LFO Freq", 35, 2, 22},
                                                           {"LFO Depth", 40, 3, 23},
                                                           {"Feedback", 30, 4, 24}};

// Default Constructor
ChorusModule::ChorusModule() : BaseEffectModule(),
                                        m_lfoFreqMin(1.0f),
                                        m_lfoFreqMax(20.0f)

{
    // Set the name of the effect
    m_name = "Chorus";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
ChorusModule::~ChorusModule()
{
    // No Code Needed
}

void ChorusModule::Init(float sample_rate)
{
    BaseEffectModule::Init(sample_rate);

    m_chorus.Init(sample_rate);
}

void ChorusModule::ProcessMono(float in)
{
    BaseEffectModule::ProcessMono(in);

    // Calculate the effect
    m_chorus.SetDelay(GetParameterAsMagnitude(1));
    m_chorus.SetLfoFreq(m_lfoFreqMin + (GetParameterAsMagnitude(2) * GetParameterAsMagnitude(2) * (m_lfoFreqMax - m_lfoFreqMin)));
    m_chorus.SetLfoDepth(GetParameterAsMagnitude(3));
    m_chorus.SetFeedback(GetParameterAsMagnitude(4));

    m_chorus.Process(m_audioLeft);

    m_audioLeft = m_chorus.GetLeft() * GetParameterAsMagnitude(0) + m_audioLeft * (1.0f - GetParameterAsMagnitude(0));
    m_audioRight = m_chorus.GetRight() * GetParameterAsMagnitude(0) + m_audioRight * (1.0f - GetParameterAsMagnitude(0));
}

void ChorusModule::ProcessStereo(float inL, float inR)
{
    // Calculate the mono effect
    ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(m_audioLeft, inR);

    // Calculate the effect
    m_audioRight = m_chorus.GetRight() * GetParameterAsMagnitude(0) + m_audioRight * (1.0f - GetParameterAsMagnitude(0));
}
