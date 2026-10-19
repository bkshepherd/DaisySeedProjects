#include "wavefolder_module.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;
using namespace distdsp;

namespace {
constexpr float kMakeup = 0.0454f; // level match at default settings (see tests/host/levels/test_level_distortion_family.cpp)
constexpr float kSmoothing = 0.002f;
constexpr float kMaxBias = 0.3f; // the largest bias, relative to full scale

constexpr float kTopBoost = 12.0f; // extra fold at the top of the Fold knob (see distdsp::topBoost)

// multiples of the input that reach the first fold peak at 1.0
float FoldAmount(float v) { return ParamDisplay::Exp(v, 0.25f, 10.0f) * topBoost(v, 0.5f, kTopBoost); }
float ToneHz(float v) { return ParamDisplay::Exp(v, 1000.0f, 16000.0f); }
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, WavefolderModule::PARAM_COUNT> params{};

    params[WavefolderModule::FOLD] = {
        name : "Fold",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.81832f}, // 30x
        knobMapping : 0,
        midiCCMapping : -1,
        unit : " x",
        decimals : 1,
        displayFn : [](float v) { return FoldAmount(v); }
    };

    params[WavefolderModule::SYMMETRY] = {
        name : "Symmetry",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : -1,
        unit : " %",
        decimals : 0,
        showSign : true,
        displayFn : [](float v) { return ParamDisplay::Lin(v, -100.0f, 100.0f); }
    };

    params[WavefolderModule::TONE] = {
        name : "Tone",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.7f},
        knobMapping : 2,
        midiCCMapping : -1,
        unit : " Hz",
        decimals : 0,
        displayFn : [](float v) { return ToneHz(v); }
    };

    params[WavefolderModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 1.0f},
        knobMapping : 3,
        midiCCMapping : -1,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[WavefolderModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8f},
        knobMapping : 4,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 1,
        showSign : true,
        displayFn : [](float v) { return ParamDisplay::Lin(v, -24.0f, 6.0f); }
    };

    return params;
}();

WavefolderModule::WavefolderModule()
    : MonoEffectModule(), m_sampleRate(48000.0f), m_dirty(true), m_foldTarget(1.0f), m_fold(1.0f), m_biasTarget(0.0f), m_bias(0.0f), m_mixTarget(1.0f), m_mix(1.0f),
      m_outTarget(1.0f), m_out(1.0f), m_x1(0.0f) {
    m_name = "Wavefolder";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

WavefolderModule::~WavefolderModule() {}

void WavefolderModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    m_hpIn.SetCutoff(30.0f, sample_rate);
    m_dc.SetCutoff(20.0f, sample_rate);
    for (OnePole *f : {&m_hpIn, &m_dc, &m_tone}) f->Reset();
    m_x1 = 0.0f;
    Recompute();
    m_dirty = false;
    m_fold = m_foldTarget;
    m_bias = m_biasTarget;
    m_mix = m_mixTarget;
    m_out = m_outTarget;
    Settle();
}

void WavefolderModule::ParameterChanged(int /*parameter_id*/) { m_dirty = true; }

void WavefolderModule::Recompute() {
    m_foldTarget = FoldAmount(GetParameterAsFloat(FOLD)) * (kPi * 0.5f);
    m_biasTarget = (GetParameterAsFloat(SYMMETRY) - 0.5f) * 2.0f * kMaxBias;
    m_mixTarget = GetParameterAsFloat(MIX);
    m_outTarget = dbToGain(ParamDisplay::Lin(GetParameterAsFloat(LEVEL), -24.0f, 6.0f)) * kMakeup;
    m_tone.SetCutoff(ToneHz(GetParameterAsFloat(TONE)), m_sampleRate);
}

void WavefolderModule::Process(const float *in, float *out, size_t n) {
    if (m_dirty) {
        m_dirty = false;
        Recompute();
    }

    for (size_t i = 0; i < n; i++) {
        m_fold += (m_foldTarget - m_fold) * kSmoothing;
        m_bias += (m_biasTarget - m_bias) * kSmoothing;
        m_mix += (m_mixTarget - m_mix) * kSmoothing;
        m_out += (m_outTarget - m_out) * kSmoothing;

        const float dry = m_hpIn.HP(in[i]);
        const float x = dry + m_bias;
        // y = sin(a x), anti-aliased: the average of the fold over the step from the previous sample, with the antiderivative -cos(a x) / a
        const float d = x - m_x1;
        float wet;
        if (std::fabs(d) < 1.0e-4f) {
            wet = std::sin(m_fold * 0.5f * (x + m_x1));
        } else {
            wet = (std::cos(m_fold * m_x1) - std::cos(m_fold * x)) / (m_fold * d);
        }
        m_x1 = x;
        wet = m_dc.HP(wet); // the bias leaves an offset
        wet = m_tone.LP(wet);
        out[i] = (dry * (1.0f - m_mix) + wet * m_mix) * m_out;
    }
}
