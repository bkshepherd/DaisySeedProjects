#include "modulated_tremolo_module.h"

using namespace bkshepherd;

// Default Constructor
ModulatedTremoloModule::ModulatedTremoloModule() : BaseEffectModule(),
                                                        m_tremoloFreqMin(1.0f),
                                                        m_tremoloFreqMax(20.0f),
                                                        m_freqOscFreqMin(0.01f),
                                                        m_freqOscFreqMax(1.0f),
                                                        m_cachedEffectMagnitudeValue(1.0f)
{
    // No Code Needed
}

// Destructor
ModulatedTremoloModule::~ModulatedTremoloModule()
{
    // No Code Needed
}

void ModulatedTremoloModule::Init(float sample_rate)
{
    BaseEffectModule::Init(sample_rate);

    // Initialize Default Parameters for this Effect
    m_paramCount = 6;
    m_params = new uint8_t[m_paramCount];

    // Tremolo Waveform Parameter
    m_params[0] = 0; // 0 is Sine, 1 is Triangle, 2 is Saw, 3 is Ramp, 4 is Square

    // Tremolo Depth Parameter
    m_params[1] = 127; // 0 .. 255, Min to Max Depth

    // Tremolo Frequency Parameter
    m_params[2] = 127; // 0 .. 255, Min to Max Frequency

    // Tremolo Oscilator Waveform Parameter
    m_params[3] = 0; // 0 is Sine, 1 is Triangle, 2 is Saw, 3 is Ramp, 4 is Square

    // Tremolo Oscilator Frequency Parameter
    m_params[4] = 0; // 0 .. 255, Min to Max Frequency

    m_tremolo.Init(sample_rate);

    m_freqOsc.Init(sample_rate);
}

float ModulatedTremoloModule::ProcessMono(float in)
{
    float value = BaseEffectModule::ProcessMono(in);

    // Calculate Tremolo Frequency Oscillation
    m_freqOsc.SetWaveform(GetParameter(3));
    m_freqOsc.SetAmp(1.0f);
    m_freqOsc.SetFreq(m_freqOscFreqMin + (GetParameterAsMagnitude(4) * m_freqOscFreqMax));
    float mod = m_freqOsc.Process();

    if (GetParameter(4) == 0) {
        mod = 1.0f;
    }

    // Calculate the effect
    m_tremolo.SetWaveform(GetParameter(0));
    m_tremolo.SetDepth(GetParameterAsMagnitude(1));
    m_tremolo.SetFreq(m_tremoloFreqMin + ((GetParameterAsMagnitude(2) * m_tremoloFreqMax) * mod));
    m_cachedEffectMagnitudeValue = m_tremolo.Process(1.0f);

    return value * m_cachedEffectMagnitudeValue;
}

float ModulatedTremoloModule::ProcessStereoLeft(float in)
{    
    return ProcessMono(in);
}

float ModulatedTremoloModule::ProcessStereoRight(float in)
{
    float value = BaseEffectModule::ProcessStereoRight(in);

    // Use the same magnitude as already calculated for the Left channel
    return value * m_cachedEffectMagnitudeValue;
}

float ModulatedTremoloModule::GetOutputLEDBrightness()
{    
    return BaseEffectModule::GetOutputLEDBrightness() * m_cachedEffectMagnitudeValue;
}
