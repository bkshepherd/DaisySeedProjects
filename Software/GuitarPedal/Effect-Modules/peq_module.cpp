#include "peq_module.h"
#include <q/fx/biquad.hpp>

using namespace bkshepherd;

namespace {
const float minGain = -15.f;
const float maxGain = 15.f;

const float qLows = 0.7f;
const float qMids = 1.4f;
const float qHighs = 1.8f;

const float defaultLowFreq = 100.f;
const float defaultMidFreq = 800.f;
const float defaultHighFreq = 4000.f;

cycfi::q::peaking filterLows = {0, defaultLowFreq, 48000, qLows};
cycfi::q::peaking filterMids = {0, defaultMidFreq, 48000, qMids};
cycfi::q::peaking filterHighs = {0, defaultHighFreq, 48000, qHighs};
} // namespace

static constexpr uint8_t s_paramCount = 9;
static const ParameterMetaData s_metaData[s_paramCount] = {{
                                                               name : "Low Freq",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = defaultLowFreq},
                                                               knobMapping : 0,
                                                               midiCCMapping : -1,
                                                               minValue : static_cast<int>(defaultLowFreq - 80.f),
                                                               maxValue : static_cast<int>(defaultLowFreq + 80.f)
                                                           },
                                                           {
                                                               name : "Mid Freq",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = defaultMidFreq},
                                                               knobMapping : 1,
                                                               midiCCMapping : -1,
                                                               minValue : static_cast<int>(defaultMidFreq - 550.f),
                                                               maxValue : static_cast<int>(defaultMidFreq + 550.f)
                                                           },
                                                           {
                                                               name : "High Freq",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = defaultHighFreq},
                                                               knobMapping : 2,
                                                               midiCCMapping : -1,
                                                               minValue : static_cast<int>(defaultHighFreq - 3000.f),
                                                               maxValue : static_cast<int>(defaultHighFreq + 3000.f)
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

    filterLows.config(GetParameterAsFloat(3), GetParameterAsFloat(0), sample_rate, qLows);
    filterMids.config(GetParameterAsFloat(4), GetParameterAsFloat(1), sample_rate, qMids);
    filterHighs.config(GetParameterAsFloat(5), GetParameterAsFloat(2), sample_rate, qHighs);
}

void ParametricEQModule::ProcessMono(float in) {
    float out = in;

    out = filterLows(out);
    out = filterMids(out);
    out = filterHighs(out);

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
        filterLows.config(GetParameterAsFloat(3), GetParameterAsFloat(0), GetSampleRate(), qLows);
        break;
    case 1:
    case 4:
        filterMids.config(GetParameterAsFloat(4), GetParameterAsFloat(1), GetSampleRate(), qMids);
        break;
    case 2:
    case 5:
        filterHighs.config(GetParameterAsFloat(5), GetParameterAsFloat(2), GetSampleRate(), qHighs);
        break;
    }
}

void ParametricEQModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                                bool isEditing) {
    display.WriteStringAligned(m_name, Font_7x10, boundsToDrawIn, Alignment::topCentered, true);

    const int width = boundsToDrawIn.GetWidth();
    const int barWidth = 10;

    const int stepWidth = (width / 3);

    // This is used to help the "max" gain take up the full vertical amount of the screen
    const float magnitudeMultiplier = 1.4f;

    int top = 30;

    // Draw centerline
    display.DrawLine(0, top, width, top, true);

    // Don't default this to 0 so that the "3" bars stay centered
    int x = stepWidth / 2;
    for (int i = 0; i < 3; i++) {
        const int gainParamId = i + 3;

        const bool positive = GetParameterAsFloat(gainParamId) > 0.0f;
        const float magnitude = std::abs(GetParameterAsFloat(gainParamId)) * magnitudeMultiplier;
        const int y = positive ? top - magnitude : top;

        Rectangle r(x, y, barWidth, magnitude);
        display.DrawRect(r, true, true);
        x += stepWidth;
    }
}