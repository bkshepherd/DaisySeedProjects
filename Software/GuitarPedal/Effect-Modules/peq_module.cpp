#include "peq_module.h"
#include <q/fx/biquad.hpp>

using namespace bkshepherd;

namespace {
const float minGain = -15.f;
const float maxGain = 15.f;

constexpr uint8_t NUM_FILTERS = 3;
const float q[NUM_FILTERS] = {0.7f, 1.4f, 1.8f};

cycfi::q::peaking filter[NUM_FILTERS] = {{0, 100, 48000, q[0]}, {0, 800, 48000, q[1]}, {0, 4000, 48000, q[2]}};
} // namespace

static constexpr uint8_t s_paramCount = 9;
static const ParameterMetaData s_metaData[s_paramCount] = {{
                                                               name : "Low Freq",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 100.0f},
                                                               knobMapping : 0,
                                                               midiCCMapping : -1,
                                                               minValue : 35,
                                                               maxValue : 500
                                                           },
                                                           {
                                                               name : "Mid Freq",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 800.0f},
                                                               knobMapping : 1,
                                                               midiCCMapping : -1,
                                                               minValue : 250,
                                                               maxValue : 5000
                                                           },
                                                           {
                                                               name : "High Freq",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 4000.0f},
                                                               knobMapping : 2,
                                                               midiCCMapping : -1,
                                                               minValue : 1000,
                                                               maxValue : 20000
                                                           },
                                                           {
                                                               name : "Low Gain",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.0f},
                                                               knobMapping : 3,
                                                               midiCCMapping : -1,
                                                               minValue : static_cast<int>(minGain),
                                                               maxValue : static_cast<int>(maxGain)
                                                           },
                                                           {
                                                               name : "Mid Gain",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.0f},
                                                               knobMapping : 4,
                                                               midiCCMapping : -1,
                                                               minValue : static_cast<int>(minGain),
                                                               maxValue : static_cast<int>(maxGain)
                                                           },
                                                           {
                                                               name : "High Gain",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.0f},
                                                               knobMapping : 5,
                                                               midiCCMapping : -1,
                                                               minValue : static_cast<int>(minGain),
                                                               maxValue : static_cast<int>(maxGain)
                                                           }};

// Default Constructor
ParametricEQModule::ParametricEQModule() : BaseEffectModule() {
    // Set the name of the effect
    m_name = "ParaEQ";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
ParametricEQModule::~ParametricEQModule() {
    // No Code Needed
}

void ParametricEQModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    filter[0].config(GetParameterAsFloat(3), GetParameterAsFloat(0), sample_rate, q[0]);
    filter[1].config(GetParameterAsFloat(4), GetParameterAsFloat(1), sample_rate, q[1]);
    filter[2].config(GetParameterAsFloat(5), GetParameterAsFloat(2), sample_rate, q[2]);
}

void ParametricEQModule::ProcessMono(float in) {
    float out = in;

    for (uint8_t i = 0; i < NUM_FILTERS; i++) {
        out = filter[i](out);
    }

    m_audioLeft = out;
    m_audioRight = m_audioLeft;
}

void ParametricEQModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);
}

void ParametricEQModule::ParameterChanged(int parameter_id) {
    switch (parameter_id) {
    case 0:
    case 3:
        filter[0].config(GetParameterAsFloat(3), GetParameterAsFloat(0), GetSampleRate(), q[0]);
        break;
    case 1:
    case 4:
        filter[1].config(GetParameterAsFloat(4), GetParameterAsFloat(1), GetSampleRate(), q[1]);
        break;
    case 2:
    case 5:
        filter[2].config(GetParameterAsFloat(5), GetParameterAsFloat(2), GetSampleRate(), q[2]);
        break;
    }
}