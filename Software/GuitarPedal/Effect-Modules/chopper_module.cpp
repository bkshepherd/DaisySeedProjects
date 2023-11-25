#include "chopper_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;

static const int s_paramCount = 4;
static const ParameterMetaData s_metaData[s_paramCount] = {{"Wet", 1, 0, 63, 0, 20},
                                                           {"Tempo", 1, 0, 63, 1, 23},
                                                           {"PulseW", 1, 0, 38, 2, 24},
                                                           {"Pattern", 3, 14, 0, 3, 25}};

// Default Constructor
ChopperModule::ChopperModule() : BaseEffectModule(),
                                    m_tempoFreqMin(0.5f),
                                    m_tempoFreqMax(4.0f),
                                    m_pulseWidthMin(0.1f),
                                    m_pulseWidthMax(0.9f),
                                    m_cachedEffectMagnitudeValue(1.0f)

{
    // Set the name of the effect
    m_name = "Chopper";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
ChopperModule::~ChopperModule()
{
    // No Code Needed
}

void ChopperModule::Init(float sample_rate)
{
    BaseEffectModule::Init(sample_rate);

    m_chopper.Init(sample_rate);
}

void ChopperModule::ProcessMono(float in)
{
    BaseEffectModule::ProcessMono(in);

    // Setup the Effect 
    m_chopper.SetFreq(m_tempoFreqMin + (GetParameterAsMagnitude(1) * (m_tempoFreqMax - m_tempoFreqMin)));
    m_chopper.SetAmp(1.0f);
    m_chopper.SetPw(m_pulseWidthMin + (GetParameterAsMagnitude(2) * (m_pulseWidthMax - m_pulseWidthMin)));
    m_chopper.SetPattern(GetParameterAsBinnedValue(3) - 1);

    // Calculate the Effect
    // Ease the effect value into it's target to avoid clipping
    fonepole(m_cachedEffectMagnitudeValue, m_chopper.Process(), .01f);

    float audioLeftWet =  m_cachedEffectMagnitudeValue * m_audioLeft;

    // Handle the wet / dry mix
    m_audioLeft = audioLeftWet * GetParameterAsMagnitude(0) + m_audioLeft * (1.0f - GetParameterAsMagnitude(0));
    m_audioRight = m_audioLeft;
}

void ChopperModule::ProcessStereo(float inL, float inR)
{    
    // Calculate the mono effect
    ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(m_audioLeft, inR);

    // Calculate the Effect
    float audioRightWet = m_cachedEffectMagnitudeValue * m_audioRight;

    // Handle the wet / dry mix
    m_audioRight = audioRightWet * GetParameterAsMagnitude(0) + m_audioRight * (1.0f - GetParameterAsMagnitude(0));
}

void ChopperModule::SetTempo(uint32_t bpm)
{
    float freq = tempo_to_freq(bpm);

    // Adjust the frequency into a range that makes sense for the effect
    freq = freq / 4.0f;

    if (freq <= m_tempoFreqMin)
    {
        SetParameterRaw(1, 0);
    }
    else if (freq >= m_tempoFreqMax)
    {
        SetParameterRaw(1, 127);
    }
    else 
    {
        // Get the parameter as close as we can to target tempo
        SetParameterRaw(1, ((freq - m_tempoFreqMin) / (m_tempoFreqMax - m_tempoFreqMin)) * 128);
    }
}

float ChopperModule::GetBrightnessForLED(int led_id)
{    
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1)
    {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}