#include "peq_module.h"
#include <q/fx/biquad.hpp>

using namespace bkshepherd;

namespace {
const float minGain = -15.f;
const float maxGain = 15.f;

// Q values for narrow, medium, wide
const float qLow[3] = {4.0f, 2.5f, 1.0f};
const float qMid[3] = {4.0f, 2.5f, 1.0f};
const float qHigh[3] = {4.0f, 2.5f, 1.0f};

const float defaultLowFreq = 130.f;
const float defaultMidFreq = 1100.f;
const float defaultHighFreq = 4400.f;

cycfi::q::peaking filterLows = {0, defaultLowFreq, 48000, qLow[1]};
cycfi::q::peaking filterMids = {0, defaultMidFreq, 48000, qMid[1]};
cycfi::q::peaking filterHighs = {0, defaultHighFreq, 48000, qHigh[1]};
} // namespace

static const char *s_qBinNames[3] = {"Narrow", "Medium", "Wide"};

static constexpr uint8_t s_paramCount = 9;
static const ParameterMetaData s_metaData[s_paramCount] = {{
                                                               name : "Low Freq",
                                                               valueType : ParameterValueType::Float,
                                                               valueCurve : ParameterValueCurve::Log,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = defaultLowFreq},
                                                               knobMapping : 0,
                                                               midiCCMapping : -1,
                                                               minValue : 35,
                                                               maxValue : 500
                                                           },
                                                           {
                                                               name : "Mid Freq",
                                                               valueType : ParameterValueType::Float,
                                                               valueCurve : ParameterValueCurve::Log,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = defaultMidFreq},
                                                               knobMapping : 1,
                                                               midiCCMapping : -1,
                                                               minValue : 250,
                                                               maxValue : 5000
                                                           },
                                                           {
                                                               name : "High Freq",
                                                               valueType : ParameterValueType::Float,
                                                               valueCurve : ParameterValueCurve::Log,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = defaultHighFreq},
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
                                                           },
                                                           {
                                                               name : "Low Q",
                                                               valueType : ParameterValueType::Binned,
                                                               valueBinCount : 3,
                                                               valueBinNames : s_qBinNames,
                                                               defaultValue : {.uint_value = 1},
                                                               knobMapping : -1,
                                                               midiCCMapping : -1,
                                                           },
                                                           {
                                                               name : "Mid Q",
                                                               valueType : ParameterValueType::Binned,
                                                               valueBinCount : 3,
                                                               valueBinNames : s_qBinNames,
                                                               defaultValue : {.uint_value = 1},
                                                               knobMapping : -1,
                                                               midiCCMapping : -1,
                                                           },
                                                           {
                                                               name : "High Q",
                                                               valueType : ParameterValueType::Binned,
                                                               valueBinCount : 3,
                                                               valueBinNames : s_qBinNames,
                                                               defaultValue : {.uint_value = 1},
                                                               knobMapping : -1,
                                                               midiCCMapping : -1,
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

    const int qLowIndex = GetParameterAsBinnedValue(6) - 1;
    const int qMidIndex = GetParameterAsBinnedValue(7) - 1;
    const int qHighIndex = GetParameterAsBinnedValue(8) - 1;

    filterLows.config(GetParameterAsFloat(3), GetParameterAsFloat(0), sample_rate, qLow[qLowIndex]);
    filterMids.config(GetParameterAsFloat(4), GetParameterAsFloat(1), sample_rate, qMid[qMidIndex]);
    filterHighs.config(GetParameterAsFloat(5), GetParameterAsFloat(2), sample_rate, qHigh[qHighIndex]);
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
    case 6: {
        const int qLowIndex = GetParameterAsBinnedValue(6) - 1;
        filterLows.config(GetParameterAsFloat(3), GetParameterAsFloat(0), GetSampleRate(), qLow[qLowIndex]);
        break;
    }
    case 1:
    case 4:
    case 7: {
        const int qMidIndex = GetParameterAsBinnedValue(7) - 1;
        filterMids.config(GetParameterAsFloat(4), GetParameterAsFloat(1), GetSampleRate(), qMid[qMidIndex]);
        break;
    }
    case 2:
    case 5:
    case 8: {
        const int qHighIndex = GetParameterAsBinnedValue(8) - 1;
        filterHighs.config(GetParameterAsFloat(5), GetParameterAsFloat(2), GetSampleRate(), qHigh[qHighIndex]);
        break;
    }
    }
}

void ParametricEQModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                                bool isEditing) {
    display.WriteStringAligned(m_name, Font_7x10, boundsToDrawIn, Alignment::topCentered, true);

    const int width = boundsToDrawIn.GetWidth();

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
        const int qParamId = i + 6;

        const int q = GetParameterAsBinnedValue(qParamId) - 1;

        // Make narrow and wide Q values narrower and wider bars
        int barWidth = 1;
        switch (q) {
        case 0:
            barWidth = 5;
            break;
        case 1:
            barWidth = 10;
            break;
        case 2:
            barWidth = 15;
            break;
        }

        const bool positive = GetParameterAsFloat(gainParamId) > 0.0f;
        const float magnitude = std::abs(GetParameterAsFloat(gainParamId)) * magnitudeMultiplier;
        const int y = positive ? top - magnitude : top;

        Rectangle r(x, y, barWidth, magnitude);
        display.DrawRect(r, true, true);
        x += stepWidth;
    }
}