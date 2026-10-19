#include "preamp_module.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;
using namespace distdsp;

namespace {
const char *s_voicingNames[3] = {"Clean", "Crunch", "Lead"};

struct Voicing {
    float gain1Max; // the first stage's gain at Gain = 10
    float gain2;    // the second stage's gain (0: the stage is skipped)
    float midHz;    // the frequency of the Mid control
    float outLpHz;  // the high cut at the end (a stand-in for the speaker and the power amp)
    float bias;     // the asymmetry of the first stage
    float makeup;   // the level: Clean and Crunch +6 dB at the reference input; Lead matched to Crunch at a hot input, where both saturate
                    // (see tests/host/levels/test_level_distortion_family.cpp)
    float topBoost; // extra first-stage gain at the top of the Gain knob (see distdsp::topBoost)
};
constexpr Voicing kVoicing[3] = {{6.0f, 0.0f, 500.0f, 8000.0f, 0.05f, 0.7736f, 38.0f},
                                 {25.0f, 2.5f, 700.0f, 6500.0f, 0.10f, 0.0856f, 0.0f},
                                 {60.0f, 6.0f, 900.0f, 5500.0f, 0.12f, 0.0797f, 0.0f}};

constexpr float kSmoothing = 0.002f; // per sample: about 10 ms at 48 kHz
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, PreampModule::PARAM_COUNT> params{};

    params[PreampModule::GAIN] = {
        name : "Gain",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f},
        knobMapping : 0,
        midiCCMapping : -1,
        unit : "",
        decimals : 1,
        displayFn : [](float v) { return ParamDisplay::Lin(v, 0.0f, 10.0f); }
    };

    params[PreampModule::BASS] = {
        name : "Bass",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 1,
        showSign : true,
        displayFn : [](float v) { return ParamDisplay::Lin(v, -12.0f, 12.0f); }
    };

    params[PreampModule::MID] = {
        name : "Mid",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 2,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 1,
        showSign : true,
        displayFn : [](float v) { return ParamDisplay::Lin(v, -10.0f, 10.0f); }
    };

    params[PreampModule::TREBLE] = {
        name : "Treble",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 3,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 1,
        showSign : true,
        displayFn : [](float v) { return ParamDisplay::Lin(v, -12.0f, 12.0f); }
    };

    params[PreampModule::VOICING] = {
        name : "Voicing",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_voicingNames,
        defaultValue : {.uint_value = 1},
        knobMapping : 4,
        midiCCMapping : -1
    };

    params[PreampModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8f},
        knobMapping : 5,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 1,
        showSign : true,
        displayFn : [](float v) { return ParamDisplay::Lin(v, -24.0f, 6.0f); }
    };

    return params;
}();

PreampModule::PreampModule()
    : MonoEffectModule(), m_sampleRate(48000.0f), m_dirty(true), m_voicing(1), m_gain1Target(1.0f), m_gain1(1.0f), m_outTarget(1.0f), m_out(1.0f) {
    m_name = "Preamp";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

PreampModule::~PreampModule() {}

void PreampModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    m_hpIn.SetCutoff(70.0f, sample_rate);
    m_hpC1.SetCutoff(30.0f, sample_rate);
    m_lp1.SetCutoff(6500.0f, sample_rate);
    m_hp2.SetCutoff(120.0f, sample_rate);
    m_lp2.SetCutoff(5000.0f, sample_rate);
    for (OnePole *f : {&m_hpIn, &m_hpC1, &m_lp1, &m_hp2, &m_lp2}) f->Reset();
    for (Biquad *f : {&m_bass, &m_mid, &m_treble, &m_outLp}) f->Reset();
    m_clip1.Reset();
    m_clip2.Reset();
    Recompute();
    m_dirty = false;
    m_gain1 = m_gain1Target; // start at the knob positions
    m_out = m_outTarget;
    Settle();
}

void PreampModule::ParameterChanged(int /*parameter_id*/) { m_dirty = true; }

void PreampModule::Recompute() {
    m_voicing = std::min(std::max(GetParameterAsBinnedValue(VOICING) - 1, 0), 2);
    const Voicing &v = kVoicing[m_voicing];
    const float gain = GetParameterAsFloat(GAIN);
    m_gain1Target = (1.0f + (v.gain1Max - 1.0f) * gain * gain) * topBoost(gain, 0.4f, v.topBoost);
    m_outTarget = dbToGain(ParamDisplay::Lin(GetParameterAsFloat(LEVEL), -24.0f, 6.0f)) * v.makeup;

    m_bass.SetLowShelf(120.0f, ParamDisplay::Lin(GetParameterAsFloat(BASS), -12.0f, 12.0f), m_sampleRate);
    m_mid.SetPeaking(v.midHz, ParamDisplay::Lin(GetParameterAsFloat(MID), -10.0f, 10.0f), 0.7f, m_sampleRate);
    m_treble.SetHighShelf(3200.0f, ParamDisplay::Lin(GetParameterAsFloat(TREBLE), -12.0f, 12.0f), m_sampleRate);
    m_outLp.SetLowpass(v.outLpHz, 0.707f, m_sampleRate);
}

void PreampModule::Process(const float *in, float *out, size_t n) {
    if (m_dirty) {
        m_dirty = false;
        Recompute();
    }
    const Voicing &v = kVoicing[m_voicing];
    const bool twoStages = v.gain2 > 0.0f;

    for (size_t i = 0; i < n; i++) {
        m_gain1 += (m_gain1Target - m_gain1) * kSmoothing;
        m_out += (m_outTarget - m_out) * kSmoothing;

        float y = m_hpIn.HP(in[i]);
        y = m_clip1.Process(y * m_gain1 + v.bias); // the first gain stage: an asymmetric soft clip
        y = m_hpC1.HP(y);                           // the coupling capacitor takes the bias away
        y = m_lp1.LP(y);
        if (twoStages) {
            y = m_hp2.HP(y);
            y = m_clip2.Process(y * v.gain2);
            y = m_lp2.LP(y);
        }
        y = m_bass.Process(y);
        y = m_mid.Process(y);
        y = m_treble.Process(y);
        y = m_outLp.Process(y);
        out[i] = y * m_out;
    }
}
