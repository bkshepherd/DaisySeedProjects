#include "crusher_module.h"

using namespace bkshepherd;

static const int s_paramCount = 3;
static const ParameterMetaData s_metaData[s_paramCount] = {{
                                                               name : "Level",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.3f},
                                                               knobMapping : 0,
                                                               midiCCMapping : -1
                                                           },
                                                           {
                                                               name : "Bits",
                                                               valueType : ParameterValueType::Binned,
                                                               valueBinCount : 32,
                                                               defaultValue : {.uint_value = 32},
                                                               knobMapping : 1,
                                                               midiCCMapping : -1
                                                           },
                                                           {
                                                               name : "Cutoff",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.5f},
                                                               knobMapping : 2,
                                                               midiCCMapping : -1
                                                           }};

// Default Constructor
CrusherModule::CrusherModule() : BaseEffectModule(), m_levelMin(0.01), m_levelMax(20), m_cutoffMin(500), m_cutoffMax(20000) {
    // Set the name of the effect
    m_name = "Crusher";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
CrusherModule::~CrusherModule() {
    // No Code Needed
}

void CrusherModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_tone.Init(sample_rate);
    m_bitcrusher.Init();
    m_bitcrusher.setNumberOfBits(32.0);
}

void CrusherModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    float level = m_levelMin + (GetParameterAsFloat(0) * (m_levelMax - m_levelMin));
    float cutoff = m_cutoffMin + GetParameterAsFloat(2) * (m_cutoffMax - m_cutoffMin);
    float bits = (float)GetParameterAsBinnedValue(1);

    m_tone.SetFreq(cutoff);
    m_bitcrusher.setNumberOfBits(bits);
    float out = m_bitcrusher.Process(in);

    m_audioRight = m_audioLeft = out * level;
}

void CrusherModule::ProcessStereo(float inL, float inR) {
    BaseEffectModule::ProcessStereo(inL, inR);

    float level = m_levelMin + (GetParameterAsFloat(0) * (m_levelMax - m_levelMin));
    float cutoff = m_cutoffMin + GetParameterAsFloat(2) * (m_cutoffMax - m_cutoffMin);
    float bits = (float)GetParameterAsBinnedValue(1);

    m_tone.SetFreq(cutoff);
    m_bitcrusher.setNumberOfBits(bits);

    float outL = m_bitcrusher.Process(inL);
    float outR = m_bitcrusher.Process(inR);

    m_audioLeft = outL * level;
    m_audioRight = outR * level;
}
