#include "autopan_module.h"

using namespace bkshepherd;

static const int s_paramCount = 4;
static const ParameterMetaData s_metaData[s_paramCount] = {{"Wet", 1, 0, 127, 0, 20},
                                                           {"Osc Wave", 3, 8, 0, 2, 21},
                                                           {"Osc Freq", 1, 0, 12, 1, 1},
                                                           {"Stereo", 2, 0, 0, -1, 23}};       // 0 is Mono (even if fed stereo) 1 is Stereo

// Default Constructor
AutoPanModule::AutoPanModule() : BaseEffectModule(),
                                                m_freqOscFreqMin(0.01f),
                                                m_freqOscFreqMax(4.0f)

{
    // Set the name of the effect
    m_name = "AutoPan";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
AutoPanModule::~AutoPanModule()
{
    // No Code Needed
}

void AutoPanModule::Init(float sample_rate)
{
    BaseEffectModule::Init(sample_rate);

    m_freqOsc.Init(sample_rate);
}

void AutoPanModule::ProcessMono(float in)
{
    BaseEffectModule::ProcessMono(in);

    // Calculate Pan Oscillation 
    m_freqOsc.SetWaveform(GetParameterAsBinnedValue(1) - 1);
    m_freqOsc.SetAmp(0.5f);
    m_freqOsc.SetFreq(m_freqOscFreqMin + (GetParameterAsMagnitude(2) * m_freqOscFreqMax));
    float mod = 0.5f + m_freqOsc.Process();

    if (GetParameterRaw(2) == 0) {
        mod = 0.5f;
        m_pan = mod;
    }

    // Ease the m_pan value into it's target to avoid clipping
    fonepole(m_pan, mod, .001f);

    float r = sqrtf(2.0f) / 2.0f;
    float scaled = m_pan * HALFPI_F;
    float angle = scaled / 2.0f;

    float audioLeftWet = m_audioLeft * (r * (cosf(angle) - sinf(angle)));
    float audioRightWet = m_audioRight * (r * (cosf(angle) + sinf(angle)));

    // Handle the wet / dry mix
    m_audioLeft = audioLeftWet * GetParameterAsMagnitude(0) + m_audioLeft * (1.0f - GetParameterAsMagnitude(0));
    m_audioRight = audioRightWet * GetParameterAsMagnitude(0) + m_audioRight * (1.0f - GetParameterAsMagnitude(0));
}

void AutoPanModule::ProcessStereo(float inL, float inR)
{    
    // Calculate the mono effect
    ProcessMono(inL);

    // If we are processing in mono only no need to do anything
    if (!GetParameterAsBool(3))
    {
        return;
    }

    // Do the base stereo calculation (which resets both signals even though we calculated it in mono)
    BaseEffectModule::ProcessStereo(inL, inR);

    // Calculate Stereo Effect
    float adjustedPan = (m_pan - 0.5f) * 2.0f;
    float mSignal = 0.5f * (m_audioLeft + m_audioRight);
    float sSignal = m_audioLeft - m_audioRight;

    float audioLeftWet = 0.5f * (1.0f + adjustedPan) * mSignal + sSignal;
    float audioRightWet = 0.5f * (1.0f - adjustedPan) * mSignal - sSignal;

    // Handle the wet / dry mix
    m_audioLeft = audioLeftWet * GetParameterAsMagnitude(0) + m_audioLeft * (1.0f - GetParameterAsMagnitude(0));
    m_audioRight = audioRightWet * GetParameterAsMagnitude(0) + m_audioRight * (1.0f - GetParameterAsMagnitude(0));
}

float AutoPanModule::GetOutputLEDBrightness()
{    
    return BaseEffectModule::GetOutputLEDBrightness() * m_pan;
}