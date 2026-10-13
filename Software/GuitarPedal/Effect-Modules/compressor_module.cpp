#include "compressor_module.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;

// Parameters are stored in real units, which is also how the menu shows them: Level in dB, Ratio as x:1, Threshold in dB (negative),
// Attack and Release in milliseconds. Knobs still sweep each parameter's whole range (Attack and Release logarithmically).
static const auto s_metaData = [] {
    std::array<ParameterMetaData, CompressorModule::PARAM_COUNT> params{};

    params[CompressorModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 0,
        midiCCMapping : -1,
        minValue : -12,
        maxValue : 12,
        fineStepSize : 0.01f,
        unit : " dB",
        decimals : 1,
        showSign : true
    };

    params[CompressorModule::RATIO] = {
        name : "Ratio",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 4.0f},
        knobMapping : 1,
        midiCCMapping : -1,
        minValue : 1,
        maxValue : 20,
        fineStepSize : 0.01f,
        unit : ":1",
        decimals : 1
    };

    params[CompressorModule::THRESHOLD] = {
        name : "Thresh",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = -20.0f},
        knobMapping : 2,
        midiCCMapping : -1,
        minValue : -60,
        maxValue : 0,
        fineStepSize : 0.01f,
        unit : " dB",
        decimals : 1
    };

    params[CompressorModule::ATTACK] = {
        name : "Attack",
        valueType : ParameterValueType::Float,
        valueCurve : ParameterValueCurve::Log,
        valueBinCount : 0,
        defaultValue : {.float_value = 10.0f},
        knobMapping : 3,
        midiCCMapping : -1,
        minValue : 1,
        maxValue : 100,
        fineStepSize : 0.01f,
        unit : " ms",
        decimals : 1
    };

    params[CompressorModule::RELEASE] = {
        name : "Release",
        valueType : ParameterValueType::Float,
        valueCurve : ParameterValueCurve::Log,
        valueBinCount : 0,
        defaultValue : {.float_value = 200.0f},
        knobMapping : 4,
        midiCCMapping : -1,
        minValue : 10,
        maxValue : 1000,
        fineStepSize : 0.01f,
        unit : " ms",
        decimals : 0
    };

    return params;
}();

// Default Constructor
CompressorModule::CompressorModule() : BaseEffectModule() {
    // Set the name of the effect
    m_name = "Comp";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
CompressorModule::~CompressorModule() {
    // No Code Needed
}

void CompressorModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_compressor.Init(sample_rate);

    // ParameterChanged() only fires when a value changes, and the stored defaults equal the metadata defaults, so without this the
    // DaisySP compressor would keep its own built-in settings until the user touched each control.
    for (int p = 0; p < PARAM_COUNT; p++) {
        ParameterChanged(p);
    }
}

void CompressorModule::ParameterChanged(int parameter_id) {
    // The values are clamped to the supported ranges in case settings saved by an older version (when these parameters were 0..1) are loaded.
    switch (parameter_id) {
    case LEVEL: {
        const float levelDb = std::max(-24.0f, std::min(24.0f, GetParameterAsFloat(LEVEL)));
        m_levelGain = std::pow(10.0f, levelDb / 20.0f);
        break;
    }
    case RATIO:
        m_compressor.SetRatio(std::max(1.0f, GetParameterAsFloat(RATIO)));
        break;
    case THRESHOLD:
        // In dB, 0 dB down to -60 dB
        m_compressor.SetThreshold(std::max(-60.0f, std::min(0.0f, GetParameterAsFloat(THRESHOLD))));
        break;
    case ATTACK:
        m_compressor.SetAttack(std::max(0.5f, GetParameterAsFloat(ATTACK)) * 0.001f); // ms -> seconds
        break;
    case RELEASE:
        m_compressor.SetRelease(std::max(5.0f, GetParameterAsFloat(RELEASE)) * 0.001f); // ms -> seconds
        break;
    }
}

void CompressorModule::ProcessMono(float in) {
    const float compressor_out = m_compressor.Process(in);
    m_audioLeft = compressor_out * m_levelGain;
    m_audioRight = m_audioLeft;
}

void CompressorModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);
}

float CompressorModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    // TODO: Use gain for the LED
    // if (led_id == 1)
    // {
    //     return value * m_compressor.GetGain();
    // }

    return value;
}