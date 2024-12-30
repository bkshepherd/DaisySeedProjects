#include "geq_module.h"
#include <q/fx/biquad.hpp>

constexpr uint8_t NUM_FILTERS = 6;

using namespace bkshepherd;

namespace {
const float minGain = -15.f;
const float maxGain = 15.f;

const float centerFrequency[NUM_FILTERS] = {100.f, 200.f, 400.f, 800.f, 1600.f, 3200.f};
const float q[NUM_FILTERS] = {0.7f, 0.8f, 1.1f, 1.4f, 1.6f, 1.8f};

cycfi::q::peaking filter[NUM_FILTERS] = {{0, centerFrequency[0], 48000, q[0]}, {0, centerFrequency[1], 48000, q[1]},
                                         {0, centerFrequency[2], 48000, q[2]}, {0, centerFrequency[3], 48000, q[3]},
                                         {0, centerFrequency[4], 48000, q[4]}, {0, centerFrequency[5], 48000, q[5]}};
} // namespace

static constexpr uint8_t s_paramCount = NUM_FILTERS;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {
        name : "100",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 0,
        midiCCMapping : -1,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    },
    {
        name : "200",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 1,
        midiCCMapping : -1,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    },
    {
        name : "400",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 2,
        midiCCMapping : -1,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    },
    {
        name : "800",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 3,
        midiCCMapping : -1,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    },
    {
        name : "1600",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 4,
        midiCCMapping : -1,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    },
    {
        name : "3200",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 5,
        midiCCMapping : -1,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    },
};

// Default Constructor
GraphicEQModule::GraphicEQModule() : BaseEffectModule() {
    // Set the name of the effect
    m_name = "GraphicEQ";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
GraphicEQModule::~GraphicEQModule() {
    // No Code Needed
}

void GraphicEQModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    for (uint8_t i = 0; i < NUM_FILTERS; i++) {
        filter[i].config(GetParameterAsFloat(i), centerFrequency[i], sample_rate, q[i]);
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

    const int width = boundsToDrawIn.GetWidth();
    const int barWidth = 10;

    const int stepWidth = (width / NUM_FILTERS);

    int top = 30;

    int x = 0;
    for (int i = 0; i < NUM_FILTERS; i++) {
        bool positive = GetParameterAsFloat(i) > 0.0f;
        float magnitude = std::abs(GetParameterAsFloat(i));
        if (positive) {
            Rectangle r(x, top - magnitude, barWidth, magnitude);
            display.DrawRect(r, true, true);
        } else {
            Rectangle r(x, top, barWidth, magnitude);
            display.DrawRect(r, true, true);
        }
        x += stepWidth;
    }
}