#include "multi_delay_module.h"
#include "Delays/delayline_revoct.h"
#include "Delays/delayline_reverse.h"
#include "../Util/audio_utilities.h"
#include "daisysp.h"

using namespace bkshepherd;

PitchShifter DSY_SDRAM_BSS ps_taps[4];
// Delay Max Definitions (Assumes 48kHz samplerate)
#define MAX_DELAY_TAP static_cast<size_t>(48000.0f * 8.f)
DelayLine<float, MAX_DELAY_TAP> DSY_SDRAM_BSS delayLineLeft0;
DelayLine<float, MAX_DELAY_TAP> DSY_SDRAM_BSS delayLineRight0;

float tap_delays[4] = {0.0f, 0.0f, 0.0f, 0.0f};
struct delay
{
    DelayLine<float, MAX_DELAY_TAP> *del;
    float                        currentDelay;
    float                        delayTarget;

    float Process(float feedback, float in)
    {
        //set delay times
        fonepole(currentDelay, delayTarget, .0002f);
        del->SetDelay(currentDelay);

        float read = del->Read();
        del->Write((feedback * read) + in);

        return read;
    }
};
delay     delays[2];

static const char* s_typeBinNames[] = {"Follower","Time"}; 
static const int s_paramCount = 13;
static const ParameterMetaData s_metaData[s_paramCount] = {
                                                           {name: "Wet %", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 0, knobMapping: 0, midiCCMapping: 20, minValue:0, maxValue:100 },
                                                           {name: "Delay L ms", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 0, knobMapping: 1, midiCCMapping: 21, minValue:0, maxValue:4000, fineStepSize:0.000025f },
                                                           {name: "Delay R ms", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 0, knobMapping: 2, midiCCMapping: 22, minValue:0, maxValue:4000, fineStepSize:0.000025f },
                                                           {name: "Feedback", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 0, knobMapping: 3, midiCCMapping: 31, minValue:0, maxValue:100,},
                                                           {name: "Tap Mode", valueType: ParameterValueType::Binned, valueBinCount: 2, valueBinNames: s_typeBinNames, defaultValue: 0, knobMapping: 1, midiCCMapping: 20},
                                                           {name: "Shift Tap 1", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 0, knobMapping: -1, midiCCMapping: 27, minValue:-12, maxValue:12, fineStepSize:0.004166f },
                                                           {name: "Shift Tap 2", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 0, knobMapping: -1, midiCCMapping: 28, minValue:-12, maxValue:12, fineStepSize:0.004166f },
                                                           {name: "Shift Tap 3", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 0, knobMapping: -1, midiCCMapping: 29, minValue:-12, maxValue:12, fineStepSize:0.004166f },
                                                           {name: "Shift Tap 4", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 0, knobMapping: -1, midiCCMapping: 30, minValue:-12, maxValue:12, fineStepSize:0.004166f },
                                                           {name: "Delay Tap 1", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 0, knobMapping: -1, midiCCMapping: 23, minValue:0, maxValue:4000, fineStepSize:0.00025f},
                                                           {name: "Delay Tap 2", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 0, knobMapping: -1, midiCCMapping: 24, minValue:0, maxValue:4000, fineStepSize:0.00025f},
                                                           {name: "Delay Tap 3", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 0, knobMapping: -1, midiCCMapping: 25, minValue:0, maxValue:4000, fineStepSize:0.00025f},
                                                           {name: "Delay Tap 4", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 0, knobMapping: -1, midiCCMapping: 26, minValue:0, maxValue:4000, fineStepSize:0.00025f}};

// Default Constructor
MultiDelayModule::MultiDelayModule() : BaseEffectModule(),
                                        m_isInitialized(false),
                                        m_cachedEffectMagnitudeValue(1.0f),
                                        m_delaySamplesMin(0.0f),
                                        m_delaySamplesMax(192000.0f),
                                        m_pitchShiftMin(-12.0f),
                                        m_pitchShiftMax(12.0f)
{
    // Set the name of the effect
    m_name = "Multi Delay";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;
    
    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
    
    m_isInitialized = true;
}

// Destructor
MultiDelayModule::~MultiDelayModule()
{
    // No Code Needed
}

void MultiDelayModule::Init(float sample_rate)
{
    BaseEffectModule::Init(sample_rate);
    delayLineLeft0.Init();
    delayLineRight0.Init();
    delays[0].del = &delayLineLeft0;
    delays[0].currentDelay = GetParameterAsMagnitude(1);
    delays[1].del = &delayLineRight0;
    delays[1].currentDelay = GetParameterAsMagnitude(2);

    for(int i = 0; i < 4; ++i)
    {
        ps_taps[i].Init(sample_rate);
    }
}

void MultiDelayModule::ParameterChanged(int parameter_id)
{
    if(parameter_id == 1)
    {
        delays[0].delayTarget = 48.0f * GetParameterAsFloat(1);
        if(GetParameterAsBinnedValue(4) == 1)
        {
            SetTargetTapDelayTime(0, delays[0].delayTarget, 2.0f);
            SetTargetTapDelayTime(1, delays[0].delayTarget, 4.0f);
        }
    }
    else if(parameter_id == 2)
    {
        delays[1].delayTarget = 48.0f * GetParameterAsFloat(2);
        if(GetParameterAsBinnedValue(4) == 1)
        {
            SetTargetTapDelayTime(2, delays[1].delayTarget, 2.0f);
            SetTargetTapDelayTime(3, delays[1].delayTarget, 4.0f);
        }
    }
    else if(parameter_id == 4)
    {
        if(GetParameterAsBinnedValue(parameter_id) == 1)
        {
            SetTargetTapDelayTime(0, delays[0].delayTarget, 2.0f);
            SetTargetTapDelayTime(1, delays[0].delayTarget, 4.0f);
            SetTargetTapDelayTime(2, delays[1].delayTarget, 2.0f);
            SetTargetTapDelayTime(3, delays[1].delayTarget, 4.0f);
        }
    }
     else if(parameter_id > 4 && parameter_id < 9 && m_isInitialized == true)
    {
        ps_taps[parameter_id - 5].SetTransposition(m_pitchShiftMin + (m_pitchShiftMax - m_pitchShiftMin) * GetParameterAsMagnitude(parameter_id));
    }
    else if(parameter_id > 8 && parameter_id < 11)
    {
        m_tapTargetDelay[parameter_id -9] = 48.0f * GetParameterAsFloat(parameter_id);
    }
}

void MultiDelayModule::SetTargetTapDelayTime(uint8_t index, float value, float multiplier)
{
    m_tapTargetDelay[index] = value * multiplier;
}

void PreProcessTaps(float *current, float target)
{
    fonepole(*current, target, .0002f);
}

void MultiDelayModule::ProcessMono(float in)
{
    BaseEffectModule::ProcessMono(in);

    float taps[2];
    float sig = delays[0].Process(GetParameterAsMagnitude(3), m_audioLeft) / 3.f;
    for(int i = 0; i < 2; ++ i)
    {
        PreProcessTaps(&tap_delays[i],m_tapTargetDelay[i]);
        taps[i] = delays[0].del->Read(tap_delays[i]);
        // temporary workaround, only process one pitch shifter for now, using all 4 causes right channel to be silent.
        if(i == 0)
            taps[i] = ps_taps[i].Process(taps[i]);
    }
    
    sig += taps[0]/ 3.f + taps[1] /3.f;
    
    m_audioLeft = sig*GetParameterAsMagnitude(0) + m_audioLeft * (1.0f - GetParameterAsMagnitude(0));
    m_audioRight = m_audioLeft;
}

void MultiDelayModule::ProcessStereo(float inL, float inR)
{
    // Calculate the mono effect
    ProcessMono(inL);
    inR = inL;
    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(m_audioLeft, inR);
    float taps[2];
    float sig = delays[1].Process(GetParameterAsMagnitude(3), m_audioRight) / 3.f;
    for(int i = 0; i < 2; ++ i)
    {
        PreProcessTaps(&tap_delays[i+2],m_tapTargetDelay[i+2]);
        taps[i] = delays[1].del->Read(tap_delays[i+2]);
        // temporary workaround, only process one pitch shifter for now, using all 4 causes right channel to be silent.
        if(i == 0)
            taps[i] = ps_taps[i+2].Process(taps[i]);
    }
    sig += taps[0]/ 3.f + taps[1] /3.f;
    

    m_audioRight = sig*GetParameterAsMagnitude(0) + m_audioRight * (1.0f - GetParameterAsMagnitude(0));
}

void MultiDelayModule::SetTempo(uint32_t bpm)
{
    // TODO: Add Tempo handling
}

float MultiDelayModule::GetBrightnessForLED(int led_id)
{    
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1)
    {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
