#include "midi_keys_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;

static VoiceManager<8> voice_handler; // 24 or 16 was too much for 125B pedal, froze when effect engaged, 12 works

static const int s_paramCount = 2;
static const ParameterMetaData s_metaData[s_paramCount] = {{name: "CutoffFreq", valueType: ParameterValueType::Float, defaultValue: {.float_value = 0.5f}, knobMapping: 0, midiCCMapping: 14},
                                                           {name: "Level", valueType: ParameterValueType::Float, defaultValue: {.float_value = 0.5f}, knobMapping: 1, midiCCMapping: 15}
                                                           //{name: "Osc Wave", valueType: ParameterValueType::Binned, valueBinCount: 8, valueBinNames: s_waveBinNames, defaultValue: {.uint_value = 0}, knobMapping: 4, midiCCMapping: 23}
                                                           };

// Default Constructor
MidiKeysModule::MidiKeysModule() : BaseEffectModule(),
                                                        m_freqMin(250.0f),
                                                        m_freqMax(8500.0f),
                                                        m_cachedEffectMagnitudeValue(1.0f)
{
    // Set the name of the effect
    m_name = "MidiKeys";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;
    
    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
MidiKeysModule::~MidiKeysModule()
{
    // No Code Needed
}

void MidiKeysModule::Init(float sample_rate)
{
    BaseEffectModule::Init(sample_rate);

    voice_handler.Init(sample_rate);

}

void MidiKeysModule::OnNoteOn(float notenumber, float velocity)
{
    // Note Off can come in as Note On w/ 0 Velocity
    if(velocity == 0.f)
    {
        voice_handler.OnNoteOff(notenumber, velocity);
    }
    else
    {
        voice_handler.OnNoteOn(notenumber, velocity);
    }
}

void MidiKeysModule::OnNoteOff(float notenumber, float velocity)
{
    voice_handler.OnNoteOff(notenumber, velocity);
}

void MidiKeysModule::ProcessMono(float in)
{
    BaseEffectModule::ProcessMono(in);

    float through_audioL = m_audioLeft;
    //float through_audioR = m_audioRight;

    voice_handler.SetCutoff(m_freqMin + GetParameterAsFloat(0) * (m_freqMax -  m_freqMin));

    float sum        = 0.f;
    sum        = voice_handler.Process() * 0.4f * GetParameterAsFloat(1); // was 0.5f, needs more volume reduction
    m_audioLeft     = sum + through_audioL;
    m_audioRight = m_audioLeft;

}

void MidiKeysModule::ProcessStereo(float inL, float inR)
{    
    // Calculate the mono effect
    //ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(inL, inR);

    float through_audioL = m_audioLeft;
    float through_audioR = m_audioRight;

    voice_handler.SetCutoff(m_freqMin + GetParameterAsFloat(0) * (m_freqMax -  m_freqMin));

    float sum        = 0.f;
    sum        = voice_handler.Process() * 0.4f * GetParameterAsFloat(1); // was 0.5f, needs more volume reduction
    m_audioLeft     = sum + through_audioL;
    m_audioRight = m_audioLeft+ through_audioR;

}



float MidiKeysModule::GetBrightnessForLED(int led_id) const
{    
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1)
    {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
