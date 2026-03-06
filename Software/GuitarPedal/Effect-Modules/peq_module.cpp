#include "peq_module.h"
#include <q/fx/biquad.hpp>
#include <array>

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

static const auto s_metaData = [] {
    std::array<ParameterMetaData, ParametricEQModule::PARAM_COUNT> params{};

    params[ParametricEQModule::LOW_FREQ] = {
        name : "Low Freq",
        valueType : ParameterValueType::Float,
        valueCurve : ParameterValueCurve::Log,
        valueBinCount : 0,
        defaultValue : {.float_value = defaultLowFreq},
        knobMapping : 0,
        midiCCMapping : -1,
        minValue : 35,
        maxValue : 500
    };

    params[ParametricEQModule::MID_FREQ] = {
        name : "Mid Freq",
        valueType : ParameterValueType::Float,
        valueCurve : ParameterValueCurve::Log,
        valueBinCount : 0,
        defaultValue : {.float_value = defaultMidFreq},
        knobMapping : 1,
        midiCCMapping : -1,
        minValue : 250,
        maxValue : 5000
    };

    params[ParametricEQModule::HIGH_FREQ] = {
        name : "High Freq",
        valueType : ParameterValueType::Float,
        valueCurve : ParameterValueCurve::Log,
        valueBinCount : 0,
        defaultValue : {.float_value = defaultHighFreq},
        knobMapping : 2,
        midiCCMapping : -1,
        minValue : 1000,
        maxValue : 20000
    };

    params[ParametricEQModule::LOW_GAIN] = {
        name : "Low Gain",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 3,
        midiCCMapping : -1,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    };

    params[ParametricEQModule::MID_GAIN] = {
        name : "Mid Gain",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 4,
        midiCCMapping : -1,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    };

    params[ParametricEQModule::HIGH_GAIN] = {
        name : "High Gain",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 5,
        midiCCMapping : -1,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    };

    params[ParametricEQModule::LOW_Q] = {
        name : "Low Q",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_qBinNames,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : -1,
    };

    params[ParametricEQModule::MID_Q] = {
        name : "Mid Q",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_qBinNames,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : -1,
    };

    params[ParametricEQModule::HIGH_Q] = {
        name : "High Q",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_qBinNames,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : -1,
    };

    return params;
}();

// Default Constructor
ParametricEQModule::ParametricEQModule() : BaseEffectModule() {
    // Set the name of the effect
    m_name = "ParaEQ";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
ParametricEQModule::~ParametricEQModule() {
    // No Code Needed
}

void ParametricEQModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    const int qLowIndex = GetParameterAsBinnedValue(LOW_Q) - 1;
    const int qMidIndex = GetParameterAsBinnedValue(MID_Q) - 1;
    const int qHighIndex = GetParameterAsBinnedValue(HIGH_Q) - 1;

    filterLows.config(GetParameterAsFloat(LOW_GAIN), GetParameterAsFloat(LOW_FREQ), sample_rate, qLow[qLowIndex]);
    filterMids.config(GetParameterAsFloat(MID_GAIN), GetParameterAsFloat(MID_FREQ), sample_rate, qMid[qMidIndex]);
    filterHighs.config(GetParameterAsFloat(HIGH_GAIN), GetParameterAsFloat(HIGH_FREQ), sample_rate, qHigh[qHighIndex]);
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
    case LOW_FREQ:
    case LOW_GAIN:
    case LOW_Q: {
        const int qLowIndex = GetParameterAsBinnedValue(LOW_Q) - 1;
        filterLows.config(GetParameterAsFloat(LOW_GAIN), GetParameterAsFloat(LOW_FREQ), GetSampleRate(), qLow[qLowIndex]);
        break;
    }
    case MID_FREQ:
    case MID_GAIN:
    case MID_Q: {
        const int qMidIndex = GetParameterAsBinnedValue(MID_Q) - 1;
        filterMids.config(GetParameterAsFloat(MID_GAIN), GetParameterAsFloat(MID_FREQ), GetSampleRate(), qMid[qMidIndex]);
        break;
    }
    case HIGH_FREQ:
    case HIGH_GAIN:
    case HIGH_Q: {
        const int qHighIndex = GetParameterAsBinnedValue(HIGH_Q) - 1;
        filterHighs.config(GetParameterAsFloat(HIGH_GAIN), GetParameterAsFloat(HIGH_FREQ), GetSampleRate(), qHigh[qHighIndex]);
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
        const int gainParamId = LOW_GAIN + i;
        const int qParamId = LOW_Q + i;

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
