#include "chorus_module.h"

using namespace bkshepherd;

static const int s_paramCount = 5;
static const ParameterMetaData s_metaData[s_paramCount] = {{name: "Wet", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 83, knobMapping: 0, midiCCMapping: 20},
                                                           {name: "Delay", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 64, knobMapping: 1, midiCCMapping: 21},
                                                           {name: "LFO Freq", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 35, knobMapping: 2, midiCCMapping: 22},
                                                           {name: "LFO Depth", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 40, knobMapping: 3, midiCCMapping: 23},
                                                           {name: "Feedback", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 30, knobMapping: 4, midiCCMapping: 24}};

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

float ChorusModule::GetBrightnessForLED(int led_id)
{    
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1)
    {
        return value * GetParameterAsMagnitude(0);
    }

    return value;
}