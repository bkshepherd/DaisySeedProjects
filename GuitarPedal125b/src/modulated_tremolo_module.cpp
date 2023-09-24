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
    // Set the name of the effect
    m_name = "Tremolo";

    // Initialize Parameters for this Effect
    this->InitParams(5);
    static const char* paramNames[] = {"Wave", "Depth", "Freq", "Osc Wave", "Osc Freq"};
    m_paramNames = paramNames;
    
    // Initialize the desired list of knobs mapped to parameters
    static const int knobMappings[] = {-1, 1, 0, -1, 2};  // -1 no knob mapped
    m_knobMappings = knobMappings;

    // m_params[0] - Tremolo Waveform Parameter
    // values: 0 is Sine, 1 is Triangle, 2 is Saw, 3 is Ramp, 4 is Square

    // m_params[1] - Tremolo Depth Parameter
    // values: 0 .. 127, Min to Max Depth

    // m_params[2] - Tremolo Frequency Parameter
    // values: 0 .. 127, Min to Max Frequency

    // m_params[3] - Tremolo Oscilator Waveform Parameter
    // values: 0 is Sine, 1 is Triangle, 2 is Saw, 3 is Ramp, 4 is Square

    // m_params[4] - Tremolo Oscilator Frequency Parameter
    // values 0 .. 127, Min to Max Frequency
}

// Destructor
ModulatedTremoloModule::~ModulatedTremoloModule()
{
    // No Code Needed
}

void ModulatedTremoloModule::Init(float sample_rate)
{
    BaseEffectModule::Init(sample_rate);

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
