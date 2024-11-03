#include "compressor_module.h"

using namespace bkshepherd;

static const int s_paramCount = 5;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {
        name : "Level",
        valueType : ParameterValueType::FloatMagnitude,
        valueBinCount : 0,
        defaultValue : 64,
        knobMapping : 0,
        midiCCMapping : -1
    },
    {
        name : "Ratio",
        valueType : ParameterValueType::FloatMagnitude,
        valueBinCount : 0,
        defaultValue : 64,
        knobMapping : 1,
        midiCCMapping : -1
    },
    {
        name : "Thresh",
        valueType : ParameterValueType::FloatMagnitude,
        valueBinCount : 0,
        defaultValue : 64,
        knobMapping : 2,
        midiCCMapping : -1
    },
    {
        name : "Attack",
        valueType : ParameterValueType::FloatMagnitude,
        valueBinCount : 0,
        defaultValue : 64,
        knobMapping : 3,
        midiCCMapping : -1
    },
    {
        name : "Release",
        valueType : ParameterValueType::FloatMagnitude,
        valueBinCount : 0,
        defaultValue : 64,
        knobMapping : 4,
        midiCCMapping : -1
    },
};

// Default Constructor
CompressorModule::CompressorModule() : BaseEffectModule()
{
    // Set the name of the effect
    m_name = "Comp";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
CompressorModule::~CompressorModule()
{
    // No Code Needed
}

void CompressorModule::Init(float sample_rate)
{
    BaseEffectModule::Init(sample_rate);

    m_compressor.Init(sample_rate);
}

void CompressorModule::ParameterChanged(int parameter_id)
{
    switch (parameter_id)
    {
    case 1: {
        const float ratioMin = 1.0f;
        const float ratioMax = 40.0f;
        float ratio = ratioMin + (GetParameterAsMagnitude(1) * (ratioMax - ratioMin));
        m_compressor.SetRatio(ratio);
        break;
    }
    case 2: {
        const float thresholdMin = 0.0f;
        const float thresholdMax = 80.0f;
        float threshold = thresholdMin + (GetParameterAsMagnitude(2) * (thresholdMax - thresholdMin));
        // This is in dB so it is supposed to be 0dB to -80dB
        m_compressor.SetThreshold(-threshold);
        break;
    }
    case 3: {
        const float attackMin = 0.001f;
        const float attackMax = 10.0f;
        float attack = attackMin + (GetParameterAsMagnitude(3) * (attackMax - attackMin));
        m_compressor.SetAttack(attack);
        break;
    }
    case 4: {
        const float releaseMin = 0.001f;
        const float releaseMax = 10.0f;
        float release = releaseMin + (GetParameterAsMagnitude(4) * (releaseMax - releaseMin));
        m_compressor.SetRelease(release);
        break;
    }
    }
}

void CompressorModule::ProcessMono(float in)
{
    const float compressor_out = m_compressor.Process(in);

    const float level = m_levelMin + (GetParameterAsMagnitude(0) * (m_levelMax - m_levelMin));

    m_audioLeft = compressor_out * level;
    m_audioRight = m_audioLeft;
}

void CompressorModule::ProcessStereo(float inL, float inR)
{
    // Calculate the mono effect
    ProcessMono(inL);
}

float CompressorModule::GetBrightnessForLED(int led_id)
{
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    // TODO: Use gain for the LED
    // if (led_id == 1)
    // {
    //     return value * m_compressor.GetGain();
    // }

    return value;
}