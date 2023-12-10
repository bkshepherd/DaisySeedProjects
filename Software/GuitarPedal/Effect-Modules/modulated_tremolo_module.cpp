#include "modulated_tremolo_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;

static const char* s_waveBinNames[8] = {"Sine", "Triangle", "Saw", "Ramp", "Square", "Poly Tri", "Poly Saw", "Poly Sqr"};

static const int s_paramCount = 5;
static const ParameterMetaData s_metaData[s_paramCount] = {{name: "Wave", valueType: ParameterValueType::Binned, valueBinCount: 8, valueBinNames: s_waveBinNames, defaultValue: 0, knobMapping: 3, midiCCMapping: 20},
                                                           {name: "Depth", valueType: ParameterValueType::FloatMagnitude, defaultValue: 74, knobMapping: 1, midiCCMapping: 21},
                                                           {name: "Freq", valueType: ParameterValueType::FloatMagnitude, defaultValue: 67, knobMapping: 0, midiCCMapping: 1},
                                                           {name: "Osc Wave", valueType: ParameterValueType::Binned, valueBinCount: 8, valueBinNames: s_waveBinNames, defaultValue: 0, knobMapping: 4, midiCCMapping: 23},
                                                           {name: "Osc Freq", valueType: ParameterValueType::FloatMagnitude, defaultValue: 12, knobMapping: 2, midiCCMapping: 24}};

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

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;
    
    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
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

void ModulatedTremoloModule::ProcessMono(float in)
{
    BaseEffectModule::ProcessMono(in);

    // Calculate Tremolo Frequency Oscillation
    m_freqOsc.SetWaveform(GetParameterAsBinnedValue(3) - 1);
    m_freqOsc.SetAmp(0.5f);
    m_freqOsc.SetFreq(m_freqOscFreqMin + (GetParameterAsMagnitude(4) * m_freqOscFreqMax));
    float mod = 0.5f + m_freqOsc.Process();

    if (GetParameterRaw(4) == 0) {
        mod = 1.0f;
    }

    // Calculate the effect
    m_tremolo.SetWaveform(GetParameterAsBinnedValue(0) - 1);
    m_tremolo.SetDepth(GetParameterAsMagnitude(1));
    m_tremolo.SetFreq(m_tremoloFreqMin + ((GetParameterAsMagnitude(2) * m_tremoloFreqMax) * mod));

    // Ease the effect value into it's target to avoid clipping with square or sawtooth waves
    fonepole(m_cachedEffectMagnitudeValue, m_tremolo.Process(1.0f), .01f);

    m_audioLeft = m_audioLeft * m_cachedEffectMagnitudeValue;
    m_audioRight = m_audioLeft;
}

void ModulatedTremoloModule::ProcessStereo(float inL, float inR)
{    
    // Calculate the mono effect
    ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(m_audioLeft, inR);

    // Use the same magnitude as already calculated for the Left Audio
    m_audioRight = m_audioRight * m_cachedEffectMagnitudeValue;
}

void ModulatedTremoloModule::SetTempo(uint32_t bpm)
{
    float freq = tempo_to_freq(bpm);

    if (freq <= m_tremoloFreqMin)
    {
        SetParameterRaw(2, 0);
    }
    else if (freq >= m_tremoloFreqMax)
    {
        SetParameterRaw(2, 127);
    }
    else 
    {
        // Get the parameter as close as we can to target tempo
        SetParameterRaw(2, ((freq - m_tremoloFreqMin) / (m_tremoloFreqMax - m_tremoloFreqMin)) * 128);
    }
}

float ModulatedTremoloModule::GetBrightnessForLED(int led_id)
{    
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1)
    {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
