#include "bass_dist_module.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;
using namespace distdsp;

namespace {
const char *s_clipNames[3] = {"Soft", "Hard", "Fuzz"};
constexpr float kFuzzBias = 0.3f;
constexpr float kMakeup[3] = {0.1304f, 0.1941f, 0.1432f}; // level match of the clip types at default settings (see tests/host/levels/test_level_distortion_family.cpp)
constexpr float kSmoothing = 0.002f;

float CrossoverHz(float v) { return ParamDisplay::Exp(v, 60.0f, 400.0f); }
float ToneHz(float v) { return ParamDisplay::Exp(v, 1500.0f, 12000.0f); }
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, BassDistModule::PARAM_COUNT> params{};

    params[BassDistModule::DRIVE] = {
        name : "Drive",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : -1,
        unit : "",
        decimals : 1,
        displayFn : [](float v) { return ParamDisplay::Lin(v, 0.0f, 10.0f); }
    };

    params[BassDistModule::CROSSOVER] = {
        name : "Crossover",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f},
        knobMapping : 1,
        midiCCMapping : -1,
        unit : " Hz",
        decimals : 0,
        displayFn : [](float v) { return CrossoverHz(v); }
    };

    params[BassDistModule::CLIP] = {
        name : "Clip",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_clipNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 2,
        midiCCMapping : -1
    };

    params[BassDistModule::LOWS] = {
        name : "Lows",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8f},
        knobMapping : 3,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 1,
        showSign : true,
        displayFn : [](float v) { return ParamDisplay::Lin(v, -24.0f, 6.0f); }
    };

    params[BassDistModule::TONE] = {
        name : "Tone",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.6f},
        knobMapping : 4,
        midiCCMapping : -1,
        unit : " Hz",
        decimals : 0,
        displayFn : [](float v) { return ToneHz(v); }
    };

    params[BassDistModule::LEVEL] = {
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

    params[BassDistModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 1.0f},
        knobMapping : -1,
        midiCCMapping : -1,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    return params;
}();

BassDistModule::BassDistModule()
    : MonoEffectModule(), m_sampleRate(48000.0f), m_dirty(true), m_clipType(0), m_driveTarget(1.0f), m_drive(1.0f), m_lowsTarget(1.0f), m_lows(1.0f), m_mixTarget(1.0f),
      m_mix(1.0f), m_outTarget(1.0f), m_out(1.0f) {
    m_name = "Bass Dist";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

BassDistModule::~BassDistModule() {}

void BassDistModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    m_hpIn.SetCutoff(25.0f, sample_rate);
    m_dc.SetCutoff(20.0f, sample_rate);
    for (OnePole *f : {&m_hpIn, &m_tone, &m_dc}) f->Reset();
    m_low1.Reset();
    m_low2.Reset();
    m_high1.Reset();
    m_high2.Reset();
    m_soft.Reset();
    m_hard.Reset();
    Recompute();
    m_dirty = false;
    m_drive = m_driveTarget;
    m_lows = m_lowsTarget;
    m_mix = m_mixTarget;
    m_out = m_outTarget;
    Settle();
}

void BassDistModule::ParameterChanged(int /*parameter_id*/) { m_dirty = true; }

void BassDistModule::Recompute() {
    m_clipType = std::min(std::max(GetParameterAsBinnedValue(CLIP) - 1, 0), 2);
    const float drive = GetParameterAsFloat(DRIVE);
    m_driveTarget = 1.0f + 40.0f * drive * drive;
    m_lowsTarget = dbToGain(ParamDisplay::Lin(GetParameterAsFloat(LOWS), -24.0f, 6.0f));
    m_mixTarget = GetParameterAsFloat(MIX);
    m_outTarget = dbToGain(ParamDisplay::Lin(GetParameterAsFloat(LEVEL), -24.0f, 6.0f)) * kMakeup[m_clipType];
    const float fc = CrossoverHz(GetParameterAsFloat(CROSSOVER));
    m_low1.SetLowpass(fc, 0.7071f, m_sampleRate); // two second-order Butterworth filters in series make a fourth-order Linkwitz-Riley filter
    m_low2.SetLowpass(fc, 0.7071f, m_sampleRate);
    m_high1.SetHighpass(fc, 0.7071f, m_sampleRate);
    m_high2.SetHighpass(fc, 0.7071f, m_sampleRate);
    m_tone.SetCutoff(ToneHz(GetParameterAsFloat(TONE)), m_sampleRate);
}

void BassDistModule::Process(const float *in, float *out, size_t n) {
    if (m_dirty) {
        m_dirty = false;
        Recompute();
    }

    for (size_t i = 0; i < n; i++) {
        m_drive += (m_driveTarget - m_drive) * kSmoothing;
        m_lows += (m_lowsTarget - m_lows) * kSmoothing;
        m_mix += (m_mixTarget - m_mix) * kSmoothing;
        m_out += (m_outTarget - m_out) * kSmoothing;

        const float x = m_hpIn.HP(in[i]);
        const float low = m_low2.Process(m_low1.Process(x));
        const float high = m_high2.Process(m_high1.Process(x)); // the bands add up to the input with a flat magnitude (they are in phase at the crossover)
        const float v = high * m_drive;
        float wet;
        switch (m_clipType) {
        case 1:  wet = m_hard.Process(v); break;
        case 2:  wet = m_soft.Process(v + kFuzzBias); break; // a biased soft clip: a fuzzier, asymmetric sound
        default: wet = m_soft.Process(v); break;
        }
        wet = m_tone.LP(m_dc.HP(wet));
        out[i] = (low * m_lows + high * (1.0f - m_mix) + wet * m_mix) * m_out;
    }
}
