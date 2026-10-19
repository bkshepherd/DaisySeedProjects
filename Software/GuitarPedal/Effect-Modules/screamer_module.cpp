#include "screamer_module.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;
using namespace distdsp;

namespace {
const char *s_modeNames[3] = {"Classic", "Asymmetric", "Hard"};

constexpr float kEmphasisHz = 720.0f; // the corner of the gain stage's high-pass
constexpr float kOpAmpHz = 9000.0f;   // the op-amp's limited bandwidth at high gain
constexpr float kClipLevel = 0.6f;    // the level of the clipping (the diodes' forward voltage, relative to full scale)
constexpr float kAsymBias = 0.25f;
constexpr float kMakeup[3] = {0.1816f, 0.1930f, 0.2618f}; // level match of the modes at default settings (see tests/host/levels/test_level_distortion_family.cpp)
constexpr float kSmoothing = 0.002f;

float ToneHz(float v) { return ParamDisplay::Exp(v, 500.0f, 8000.0f); }
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, ScreamerModule::PARAM_COUNT> params{};

    params[ScreamerModule::DRIVE] = {
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

    params[ScreamerModule::TONE] = {
        name : "Tone",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.6f},
        knobMapping : 1,
        midiCCMapping : -1,
        unit : " Hz",
        decimals : 0,
        displayFn : [](float v) { return ToneHz(v); }
    };

    params[ScreamerModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8f},
        knobMapping : 2,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 1,
        showSign : true,
        displayFn : [](float v) { return ParamDisplay::Lin(v, -24.0f, 6.0f); }
    };

    params[ScreamerModule::MODE] = {
        name : "Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_modeNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 3,
        midiCCMapping : -1
    };

    return params;
}();

ScreamerModule::ScreamerModule()
    : MonoEffectModule(), m_sampleRate(48000.0f), m_dirty(true), m_mode(0), m_driveTarget(1.0f), m_drive(1.0f), m_outTarget(1.0f), m_out(1.0f) {
    m_name = "Screamer";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

ScreamerModule::~ScreamerModule() {}

void ScreamerModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    m_hpIn.SetCutoff(40.0f, sample_rate);
    m_emph.SetCutoff(kEmphasisHz, sample_rate);
    m_lpOp.SetCutoff(kOpAmpHz, sample_rate);
    m_dc.SetCutoff(20.0f, sample_rate);
    for (OnePole *f : {&m_hpIn, &m_emph, &m_lpOp, &m_tone, &m_dc}) f->Reset();
    m_soft.Reset();
    m_hard.Reset();
    Recompute();
    m_dirty = false;
    m_drive = m_driveTarget;
    m_out = m_outTarget;
    Settle();
}

void ScreamerModule::ParameterChanged(int /*parameter_id*/) { m_dirty = true; }

void ScreamerModule::Recompute() {
    m_mode = std::min(std::max(GetParameterAsBinnedValue(MODE) - 1, 0), 2);
    const float drive = GetParameterAsFloat(DRIVE);
    m_driveTarget = 1.0f + 60.0f * drive * drive; // the gain of the clipped path
    m_outTarget = dbToGain(ParamDisplay::Lin(GetParameterAsFloat(LEVEL), -24.0f, 6.0f)) * kMakeup[m_mode];
    m_tone.SetCutoff(ToneHz(GetParameterAsFloat(TONE)), m_sampleRate);
}

void ScreamerModule::Process(const float *in, float *out, size_t n) {
    if (m_dirty) {
        m_dirty = false;
        Recompute();
    }

    for (size_t i = 0; i < n; i++) {
        m_drive += (m_driveTarget - m_drive) * kSmoothing;
        m_out += (m_outTarget - m_out) * kSmoothing;

        const float x = m_hpIn.HP(in[i]);
        // The clipped path: high-passed, amplified and band-limited like the op-amp stage; the clean signal is added unclipped
        const float u = m_lpOp.LP(m_emph.HP(x) * m_drive) * (1.0f / kClipLevel);
        float c;
        switch (m_mode) {
        case 1:  c = m_soft.Process(u + kAsymBias); break;
        case 2:  c = m_hard.Process(u); break;
        default: c = m_soft.Process(u); break;
        }
        float y = x + c * kClipLevel;
        y = m_dc.HP(y);
        y = m_tone.LP(y);
        out[i] = y * m_out;
    }
}
