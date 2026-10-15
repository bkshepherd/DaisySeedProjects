#include "duck_compressor_module.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

using namespace bkshepherd;

static const char *s_keyNames[2] = {"Left", "Right"};

// Parameters are in real units, as in the Compressor module: Level (make-up gain) in dB, Ratio as n:1, Threshold in dB, Attack and Release in milliseconds.
static const auto s_metaData = [] {
    std::array<ParameterMetaData, DuckCompressorModule::PARAM_COUNT> params{};

    params[DuckCompressorModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 3.0f},
        knobMapping : 0,
        midiCCMapping : -1,
        minValue : -12,
        maxValue : 12,
        fineStepSize : 0.01f,
        unit : " dB",
        decimals : 1,
        showSign : true
    };
    params[DuckCompressorModule::RATIO] = {
        name : "Ratio",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 8.0f},
        knobMapping : 1,
        midiCCMapping : -1,
        minValue : 1,
        maxValue : 40,
        fineStepSize : 0.01f,
        unit : ":1",
        decimals : 1
    };
    params[DuckCompressorModule::THRESHOLD] = {
        name : "Thresh",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = -30.0f}, // of the key
        knobMapping : 2,
        midiCCMapping : -1,
        minValue : -80,
        maxValue : 0,
        fineStepSize : 0.01f,
        unit : " dB",
        decimals : 1
    };
    params[DuckCompressorModule::ATTACK] = {
        name : "Attack",
        valueType : ParameterValueType::Float,
        valueCurve : ParameterValueCurve::Log,
        valueBinCount : 0,
        defaultValue : {.float_value = 5.0f},
        knobMapping : 3,
        midiCCMapping : -1,
        minValue : 1,
        maxValue : 100,
        fineStepSize : 0.01f,
        unit : " ms",
        decimals : 1
    };
    params[DuckCompressorModule::RELEASE] = {
        name : "Release",
        valueType : ParameterValueType::Float,
        valueCurve : ParameterValueCurve::Log,
        valueBinCount : 0,
        defaultValue : {.float_value = 150.0f},
        knobMapping : 4,
        midiCCMapping : -1,
        minValue : 10,
        maxValue : 2000,
        fineStepSize : 0.01f,
        unit : " ms",
        decimals : 0
    };
    params[DuckCompressorModule::KEY_INPUT] = {
        name : "Key Input",
        valueType : ParameterValueType::Binned,
        valueBinCount : 2,
        valueBinNames : s_keyNames,
        defaultValue : {.uint_value = 1}, // Right: the right input is the key and is not heard (side chain). Left: the audio is its own key (a normal compressor).
        knobMapping : -1,
        midiCCMapping : -1
    };
    return params;
}();

// Default Constructor
DuckCompressorModule::DuckCompressorModule() : BaseEffectModule() {
    m_name = "DuckComp";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
DuckCompressorModule::~DuckCompressorModule() {}

void DuckCompressorModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    m_simpleComp.initRuntime();
    m_simpleComp.setSampleRate(sample_rate);
    m_gainReductiondB = 0.0f;
    m_grShown = 0.0f;

    // ParameterChanged() only fires when a value changes, so apply every parameter (the stored value or the default) here
    for (int p = 0; p < PARAM_COUNT; p++) {
        ParameterChanged(p);
    }
}

void DuckCompressorModule::ParameterChanged(int parameter_id) {
    switch (parameter_id) {
    case LEVEL:
        m_levelGain = std::pow(10.0f, std::max(-24.0f, std::min(24.0f, GetParameterAsFloat(LEVEL))) / 20.0f);
        break;
    case RATIO:
        // The Simple Compressor takes the slope, not the n of n:1: 1/4 compresses 4:1 (a ratio above 1 would expand and make the audio louder)
        m_simpleComp.setRatio(1.0f / std::max(1.0f, GetParameterAsFloat(RATIO)));
        break;
    case THRESHOLD:
        m_simpleComp.setThresh(std::max(-80.0f, std::min(0.0f, GetParameterAsFloat(THRESHOLD))));
        break;
    case ATTACK:
        m_simpleComp.setAttack(std::max(0.5f, GetParameterAsFloat(ATTACK)));
        break;
    case RELEASE:
        m_simpleComp.setRelease(std::max(5.0f, GetParameterAsFloat(RELEASE)));
        break;
    default:
        break;
    }
}

// The audio is compressed by the level of the key; with a key that is the audio itself it is a normal compressor
void DuckCompressorModule::ProcessSamples(const float *audio, const float *key, float *outL, float *outR, size_t n) {
    const float level = m_levelGain;
    float deepest = 0.0f;
    for (size_t i = 0; i < n; i++) {
        float a = audio[i];
        float unused = 0.0f;
        m_simpleComp.process(a, unused, std::fabs(key[i])); // the gain reduction goes on a only; the key is not in the output
        outL[i] = outR[i] = a * level;
        deepest = std::min(deepest, m_simpleComp.getGainReductiondB());
    }
    m_gainReductiondB = m_simpleComp.getGainReductiondB();
    // for the display: the deepest reduction of the block, which falls back by a twentieth of the way per block (about 1 s at 48 samples a block)
    if (deepest < m_grShown) m_grShown = deepest;
    else m_grShown += (0.0f - m_grShown) * 0.05f * static_cast<float>(n) / 48.0f;
}

void DuckCompressorModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);
    float out;
    ProcessSamples(&m_audioLeft, &m_audioLeft, &out, &m_audioRight, 1);
    m_audioLeft = m_audioRight;
}

void DuckCompressorModule::ProcessStereo(float inL, float inR) {
    BaseEffectModule::ProcessStereo(inL, inR);
    float out;
    ProcessSamples(&m_audioLeft, KeyIsRight() ? &m_audioRight : &m_audioLeft, &out, &m_audioRight, 1);
    m_audioLeft = m_audioRight;
}

void DuckCompressorModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    ProcessSamples(in, in, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void DuckCompressorModule::ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    ProcessSamples(inL, KeyIsRight() ? inR : inL, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

float DuckCompressorModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);
    // The second LED shows the gain reduction: dark when the audio is untouched, full at 12 dB or more
    if (led_id == 1) return value * std::min(1.0f, -m_gainReductiondB / 12.0f);
    return value;
}

// The gain reduction as "-12.3" (no float printf on the target)
static void formatGainReduction(char *buffer, size_t size, float dB) {
    const int tenths = static_cast<int>(std::lround(std::fabs(dB) * 10.0f));
    if (tenths == 0) snprintf(buffer, size, "0.0");
    else snprintf(buffer, size, "-%d.%d", tenths / 10, tenths % 10);
}

void DuckCompressorModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn, bool isEditing) {
    BaseEffectModule::DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn, isEditing);
    // The gain reduction in the lower half of the page (with Show Debug on it is in the debug text instead, where the space is used)
    if (GetShowDebug()) return;
    char text[24];
    formatGainReduction(text, sizeof(text), m_grShown);
    char line[32];
    snprintf(line, sizeof(line), "%s dB", text);
    display.WriteStringAligned(line, Font_11x18, boundsToDrawIn.WithTrimmedTop(boundsToDrawIn.GetHeight() / 2), Alignment::centered, true);
}

void DuckCompressorModule::GetDebugText(char *buffer, size_t size) const {
    char text[24];
    formatGainReduction(text, sizeof(text), m_grShown);
    snprintf(buffer, size, "GR %s dB", text);
}
