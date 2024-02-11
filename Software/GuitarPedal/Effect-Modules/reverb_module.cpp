#include "reverb_module.h"

using namespace bkshepherd;

static const int s_paramCount = 3;
static const ParameterMetaData s_metaData[s_paramCount] = {{name: "Time", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 57, knobMapping: 0, midiCCMapping: 1},
                                                           {name: "Damp", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 40, knobMapping: 1, midiCCMapping: 21},
                                                           {name: "Mix", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 57, knobMapping: 2, midiCCMapping: 22}};

ReverbSc DSY_SDRAM_BSS reverbStereo;
// Default Constructor
ReverbModule::ReverbModule() : BaseEffectModule(),
                                        m_timeMin(0.6f),
                                        m_timeMax(1.0f),
                                        m_lpFreqMin(600.0f),
                                        m_lpFreqMax(16000.0f)

{
    // Set the name of the effect
    m_name = "Reverb";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
ReverbModule::~ReverbModule()
{
    // No Code Needed
}

void ReverbModule::Init(float sample_rate)
{
    BaseEffectModule::Init(sample_rate);
    m_reverbStereo = &reverbStereo;
    m_reverbStereo->Init(sample_rate);

}

void ReverbModule::ProcessMono(float in)
{
    BaseEffectModule::ProcessMono(in);

    float sendl, sendr, wetl, wetr;  // Reverb Inputs/Outputs
    sendl = m_audioLeft;
    sendr = m_audioRight;

    // Calculate the effect
    m_reverbStereo->SetFeedback(m_timeMin + GetParameterAsMagnitude(0) * (m_timeMax - m_timeMin));
    float invertedFreq = 1.0 - GetParameterAsMagnitude(1); // Invert the damping param so that knob left is less dampening, knob right is more dampening
    invertedFreq = invertedFreq * invertedFreq; // also square it for exponential taper (more control over lower frequencies)
    m_reverbStereo->SetLpFreq(m_lpFreqMin + invertedFreq * (m_lpFreqMax - m_lpFreqMin));

    m_reverbStereo->Process(sendl, sendr, &wetl, &wetr);
    m_audioLeft = wetl * GetParameterAsMagnitude(2) + sendl * (1.0 - GetParameterAsMagnitude(2));
    m_audioRight = wetr * GetParameterAsMagnitude(2) + sendr * (1.0 - GetParameterAsMagnitude(2));

}

void ReverbModule::ProcessStereo(float inL, float inR)
{    
    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(inL, inR);

    float sendl, sendr, wetl, wetr;  // Reverb Inputs/Outputs
    sendl = m_audioLeft;
    sendr = m_audioRight;

    // Calculate the effect
    m_reverbStereo->SetFeedback(m_timeMin + GetParameterAsMagnitude(0) * (m_timeMax - m_timeMin));
    float invertedFreq = 1.0 - GetParameterAsMagnitude(1); // Invert the damping param so that knob left is less dampening, knob right is more dampening
    invertedFreq = invertedFreq * invertedFreq; // also square it for exponential taper (more control over lower frequencies)
    m_reverbStereo->SetLpFreq(m_lpFreqMin + invertedFreq * (m_lpFreqMax - m_lpFreqMin));

    m_reverbStereo->Process(sendl, sendr, &wetl, &wetr);
    m_audioLeft = wetl * GetParameterAsMagnitude(2) + inL * (1.0 - GetParameterAsMagnitude(2));
    m_audioRight = wetr * GetParameterAsMagnitude(2) + inR * (1.0 - GetParameterAsMagnitude(2));
}

float ReverbModule::GetBrightnessForLED(int led_id)
{    
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1)
    {
        return value; // * GetParameterAsMagnitude(0);
    }

    return value;
}