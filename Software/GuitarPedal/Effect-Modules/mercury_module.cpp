// Mercury: amp sim with NAM pico models, adapted from the Mercury pedal of the FunBox project (https://github.com/GuitarML/Funbox, MIT licence, Copyright (c) 2024 Keith Bloemer), with
// the network of RTNeural-NAM (https://github.com/jatinchowdhury18/RTNeural-NAM, BSD-3-Clause). See Mercury/README.md for the licence texts and the list of changes.
#include "mercury_module.h"
#include "Mercury/wavenet_pico.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;

namespace {
// The network's history (33 KB) is read and written for every sample: it is in the fast on-chip RAM (DTCM is the default for the uninitialised data under BOOT_SRAM)
mercury::PicoWavenet<> s_net;

// The EQ bands: centre frequency and Q, -10 .. +10 dB each (the same as the original)
constexpr float kBandFreq[4] = {180.0f, 1200.0f, 4000.0f, 8000.0f};
constexpr float kBandQ[4] = {0.7f, 0.6f, 0.5f, 0.5f};
// Output makeup gain. Calibrated with tests/host/run_level_tests.sh so that the default settings give about +3 dB with every model, like the other amp models.
constexpr float kModelGain[mercury::kModelCount] = {0.87f, 1.88f, 2.06f, 0.49f, 0.78f, 0.32f, 0.38f, 0.44f, 0.52f};

const char *s_modelNames[mercury::kModelCount] = {"Matchless SC30", "Twin Reverb", "Dumble Low", "Dumble High", "Ethos Pedal", "JCM800", "Mesa IIB", "Splawn", "PRS Archon"};
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, MercuryModule::PARAM_COUNT> params{};

    params[MercuryModule::GAIN] = {
        name : "Gain",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.375f}, // the input gain of the model: how hard it is driven, 0.1x to 2.5x (1x at the default)
        knobMapping : 0,
        midiCCMapping : 14,
        unit : " dB",
        decimals : 1,
        displayFn : [](float v) { return ParamDisplay::Db(ParamDisplay::Lin(v, 0.1f, 2.5f)); }
    };
    params[MercuryModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : 15,
        unit : " dB",
        decimals : 1,
        displayFn : [](float v) { return ParamDisplay::Db(v); }
    };
    params[MercuryModule::PRESENCE] = {
        name : "Presence",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 2,
        midiCCMapping : 16,
        minValue : -10,
        maxValue : 10,
        fineStepSize : 0.1f,
        unit : " dB",
        decimals : 1,
        showSign : true
    };
    params[MercuryModule::BASS] = {
        name : "Bass",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 3,
        midiCCMapping : 17,
        minValue : -10,
        maxValue : 10,
        fineStepSize : 0.1f,
        unit : " dB",
        decimals : 1,
        showSign : true
    };
    params[MercuryModule::MID] = {
        name : "Mid",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 4,
        midiCCMapping : 18,
        minValue : -10,
        maxValue : 10,
        fineStepSize : 0.1f,
        unit : " dB",
        decimals : 1,
        showSign : true
    };
    params[MercuryModule::TREBLE] = {
        name : "Treble",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 5,
        midiCCMapping : 19,
        minValue : -10,
        maxValue : 10,
        fineStepSize : 0.1f,
        unit : " dB",
        decimals : 1,
        showSign : true
    };
    params[MercuryModule::MODEL] = { // the original's switches 1 and 2: low gain, medium gain and high gain models, three each
        name : "Amp Model",
        valueType : ParameterValueType::Binned,
        valueBinCount : mercury::kModelCount,
        valueBinNames : s_modelNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 21
    };
    params[MercuryModule::EQ] = {
        name : "EQ",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : -1
    };
    return params;
}();

MercuryModule::MercuryModule()
    : BaseEffectModule(), m_eq{{0.0f, kBandFreq[0], 48000.0f, kBandQ[0]}, {0.0f, kBandFreq[1], 48000.0f, kBandQ[1]}, {0.0f, kBandFreq[2], 48000.0f, kBandQ[2]}, {0.0f, kBandFreq[3], 48000.0f, kBandQ[3]}},
      m_sampleRate(48000.0f), m_gain(1.0f), m_level(0.5f), m_eqOn(true), m_currentModel(-1), m_modelGain(1.0f), m_muteOutput(false) {
    m_name = "Mercury";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

MercuryModule::~MercuryModule() {}

// The bands: bass, mid, treble and presence are the parameters 3, 4, 5 and 2
void MercuryModule::ConfigureBand(int band) {
    static const int kParam[4] = {BASS, MID, TREBLE, PRESENCE};
    m_eq[band].config(GetParameterAsFloat(kParam[band]), kBandFreq[band], m_sampleRate, kBandQ[band]);
}

void MercuryModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    // ParameterChanged only fires on a change, so apply the stored (or default) values here
    m_gain = 0.1f + GetParameterAsFloat(GAIN) * 2.4f; // 0.1x .. 2.5x
    m_level = GetParameterAsFloat(LEVEL);
    m_eqOn = GetParameterAsBool(EQ);
    for (int b = 0; b < 4; b++) ConfigureBand(b);
    m_currentModel = -1;
    SelectModel(); // loads the weights and lets the network settle: not to be called from the audio callback
}

void MercuryModule::SelectModel() {
    const int index = GetParameterAsBinnedValue(MODEL) - 1;
    if (index == m_currentModel || index < 0 || index >= mercury::kModelCount) return;
    m_muteOutput = true;
    s_net.LoadWeights(mercury::kModels[index].weights);
    s_net.Prewarm();
    m_currentModel = index;
    m_modelGain = mercury::kModels[index].levelAdjust * 0.4f * kModelGain[index]; // the original's 0.4 and the model's level adjustment
    m_muteOutput = false;
}

void MercuryModule::ParameterChanged(int parameter_id) {
    switch (parameter_id) {
    case GAIN:
        m_gain = 0.1f + GetParameterAsFloat(GAIN) * 2.4f;
        break;
    case LEVEL:
        m_level = GetParameterAsFloat(LEVEL);
        break;
    case EQ:
        m_eqOn = GetParameterAsBool(EQ);
        break;
    case BASS:
        ConfigureBand(0);
        break;
    case MID:
        ConfigureBand(1);
        break;
    case TREBLE:
        ConfigureBand(2);
        break;
    case PRESENCE:
        ConfigureBand(3);
        break;
    case MODEL:
        SelectModel();
        break;
    default:
        break;
    }
}

float MercuryModule::ProcessSample(float in) {
    if (m_muteOutput) return 0.0f;
    float out = s_net.Process(in * m_gain) * m_modelGain;
    if (m_eqOn)
        for (int b = 0; b < 4; b++) out = m_eq[b](out);
    return out * m_level;
}

void MercuryModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);
    m_audioLeft = m_audioRight = ProcessSample(in);
}

void MercuryModule::ProcessStereo(float inL, float /*inR*/) { ProcessMono(inL); } // mono: the left input only, as in the original

void MercuryModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    for (size_t i = 0; i < n; i++) outL[i] = outR[i] = ProcessSample(in[i]);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void MercuryModule::ProcessStereoBlock(const float *inL, const float * /*inR*/, float *outL, float *outR, size_t n) { ProcessMonoBlock(inL, outL, outR, n); }
