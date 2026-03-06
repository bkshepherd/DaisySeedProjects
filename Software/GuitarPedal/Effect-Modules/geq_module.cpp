#include "geq_module.h"
#include <q/fx/biquad.hpp>
#include <array>

constexpr uint8_t NUM_FILTERS = bkshepherd::GraphicEQModule::PARAM_COUNT;

using namespace bkshepherd;

static const auto s_metaData = [] {
    std::array<ParameterMetaData, GraphicEQModule::PARAM_COUNT> params{};

    params[GraphicEQModule::BAND_100] = {
        name : "100",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 0,
        midiCCMapping : -1,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    };

    params[GraphicEQModule::BAND_200] = {
        name : "200",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 1,
        midiCCMapping : -1,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    };

    params[GraphicEQModule::BAND_400] = {
        name : "400",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 2,
        midiCCMapping : -1,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    };

    params[GraphicEQModule::BAND_800] = {
        name : "800",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 3,
        midiCCMapping : -1,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    };

    params[GraphicEQModule::BAND_1600] = {
        name : "1600",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 4,
        midiCCMapping : -1,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    };

    params[GraphicEQModule::BAND_3200] = {
        name : "3200",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 5,
        midiCCMapping : -1,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    };

    return params;
}();

// Default Constructor
GraphicEQModule::GraphicEQModule() : BaseEffectModule() {
    // Set the name of the effect
    m_name = "GraphicEQ";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
GraphicEQModule::~GraphicEQModule() {
    // No Code Needed
}

void GraphicEQModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    for (uint8_t i = 0; i < NUM_FILTERS; i++) {
        filter[i].config(GetParameterAsFloat(BAND_100 + i), centerFrequency[i], sample_rate, q[i]);
    }
}

void GraphicEQModule::ProcessMono(float in) {
    float out = in;

    for (uint8_t i = 0; i < NUM_FILTERS; i++) {
        out = filter[i](out);
    }

    m_audioLeft = out;
    m_audioRight = m_audioLeft;
}

void GraphicEQModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);
}

void GraphicEQModule::ParameterChanged(int parameter_id) {
    filter[parameter_id].config(GetParameterAsFloat(parameter_id), centerFrequency[parameter_id], GetSampleRate(), q[parameter_id]);
}

void GraphicEQModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                             bool isEditing) {
    display.WriteStringAligned(m_name, Font_7x10, boundsToDrawIn, Alignment::topCentered, true);

    const int width = boundsToDrawIn.GetWidth();
    const int barWidth = 10;

    const int stepWidth = (width / NUM_FILTERS);

    // This is used to help the "max" gain take up the full vertical amount of the screen
    const float magnitudeMultiplier = 1.4f;

    int top = 30;

    // Draw centerline
    display.DrawLine(0, top, width, top, true);

    int x = 0;
    for (int i = 0; i < NUM_FILTERS; i++) {
        const int gainParamId = BAND_100 + i;
        const bool positive = GetParameterAsFloat(gainParamId) > 0.0f;
        const float magnitude = std::abs(GetParameterAsFloat(gainParamId)) * magnitudeMultiplier;
        const int y = positive ? top - magnitude : top;

        Rectangle r(x, y, barWidth, magnitude);
        display.DrawRect(r, true, true);
        x += stepWidth;
    }
}
