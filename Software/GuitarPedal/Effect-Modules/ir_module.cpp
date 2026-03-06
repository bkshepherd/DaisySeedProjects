#include "ir_module.h"
#include "../Util/audio_utilities.h"
#include "ImpulseResponse/ir_data_large.h"
#include <array>

using namespace bkshepherd;

static const char *s_irNames_large[2] = {"Rhythm", "Lead"};

static const auto s_metaData = [] {
    std::array<ParameterMetaData, IrModule::PARAM_COUNT> params{};

    params[IrModule::IR] = {
        name : "IR",
        valueType : ParameterValueType::Binned,
        valueBinCount : 2,
        valueBinNames : s_irNames_large,
        defaultValue : {.uint_value = 0},
        knobMapping : 0,
        midiCCMapping : 14
    };

    params[IrModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : 15
    };

    return params;
}();

// Default Constructor
IrModule::IrModule() : BaseEffectModule(), m_levelMin(0.0f), m_levelMax(2.0f), m_cachedEffectMagnitudeValue(1.0f) {
    // Set the name of the effect
    m_name = "IR";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
IrModule::~IrModule() {
    // No Code Needed
}

void IrModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    SelectIR();
}

void IrModule::ParameterChanged(int parameter_id) {
    if (parameter_id == IR) { // Change IR
        SelectIR();
    } else if (parameter_id == LEVEL) { // Level
    }
}

// void IrModule::AlternateFootswitchPressed() {
// Increment the IR selection by pressing alternate footswitch
// unsigned int irIndex = GetParameterAsBinnedValue(IR); // not doing -1 here to increment index by 1
// if (irIndex == ir_collection_large.size()) {
//    irIndex = 0; // reset back to 0
//}
// SetParameterAsBinnedValue(IR, irIndex + 1);
// if (irIndex != m_currentIRindex) {
//    mIR.Init(ir_collection_large[irIndex]); // ir_data is from ir_data_large.h
//}
// m_currentIRindex = irIndex;

//}

void IrModule::SelectIR() {
    unsigned int irIndex = GetParameterAsBinnedValue(IR) - 1;
    if (irIndex != m_currentIRindex) {
        mIR.Init(ir_collection_large[irIndex]); // ir_data is from ir_data_large.h
    }
    m_currentIRindex = irIndex;
}

void IrModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    float input = m_audioLeft;
    const float level = m_levelMin + (GetParameterAsFloat(LEVEL) * (m_levelMax - m_levelMin));

    // IMPULSE RESPONSE //
    m_audioLeft = mIR.Process(input) * level * 0.5; // 0.5 is level adjust for loud output
    m_audioRight = m_audioLeft;
}

void IrModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);
}

float IrModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
