#include "tape_sat_module.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;
using namespace distdsp;

namespace {
const char *s_modeNames[2] = {"Tape", "Trafo"}; // (Trafo = transformer)
struct ModeData {
    float shelfHz;  // the corner of the low boost
    float bias;     // the asymmetry of the saturation
    float warmthX;  // a multiplier for the Warmth control
};
constexpr ModeData kMode[2] = {{90.0f, 0.04f, 1.0f}, {55.0f, 0.0f, 1.5f}};
constexpr float kMakeup[2] = {1.4859f, 1.4825f}; // level match of the modes at default settings (see tests/host/levels/test_level_distortion_family.cpp)
constexpr float kSmoothing = 0.002f;

float DriveDb(float v) { return ParamDisplay::Lin(v, 0.0f, 36.0f); }
float WarmthDb(float v) { return ParamDisplay::Lin(v, 0.0f, 12.0f); }
float ToneHz(float v) { return ParamDisplay::Exp(v, 2000.0f, 16000.0f); }
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, TapeSatModule::PARAM_COUNT> params{};

    params[TapeSatModule::DRIVE] = {
        name : "Drive",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8f},
        knobMapping : 0,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 0,
        displayFn : [](float v) { return DriveDb(v); }
    };

    params[TapeSatModule::WARMTH] = {
        name : "Warmth",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f},
        knobMapping : 1,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 1,
        displayFn : [](float v) { return WarmthDb(v); }
    };

    params[TapeSatModule::TONE] = {
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

    params[TapeSatModule::MIX] = {
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

    params[TapeSatModule::LEVEL] = {
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

    params[TapeSatModule::MODE] = {
        name : "Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 2,
        valueBinNames : s_modeNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 5,
        midiCCMapping : -1
    };

    return params;
}();

TapeSatModule::TapeSatModule()
    : MonoEffectModule(), m_sampleRate(48000.0f), m_dirty(true), m_mode(0), m_driveTarget(1.0f), m_drive(1.0f), m_mixTarget(1.0f), m_mix(1.0f), m_outTarget(1.0f),
      m_out(1.0f) {
    m_name = "Tape Sat";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

TapeSatModule::~TapeSatModule() {}

void TapeSatModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    m_hpIn.SetCutoff(20.0f, sample_rate);
    m_dc.SetCutoff(15.0f, sample_rate);
    for (OnePole *f : {&m_hpIn, &m_tone, &m_dc}) f->Reset();
    m_pre.Reset();
    m_post.Reset();
    m_clip.Reset();
    Recompute();
    m_dirty = false;
    m_drive = m_driveTarget;
    m_mix = m_mixTarget;
    m_out = m_outTarget;
    Settle();
}

void TapeSatModule::ParameterChanged(int /*parameter_id*/) { m_dirty = true; }

void TapeSatModule::Recompute() {
    m_mode = std::min(std::max(GetParameterAsBinnedValue(MODE) - 1, 0), 1);
    const ModeData &md = kMode[m_mode];
    m_driveTarget = dbToGain(DriveDb(GetParameterAsFloat(DRIVE)));
    m_mixTarget = GetParameterAsFloat(MIX);
    m_outTarget = dbToGain(ParamDisplay::Lin(GetParameterAsFloat(LEVEL), -24.0f, 6.0f)) * kMakeup[m_mode];
    const float warmth = WarmthDb(GetParameterAsFloat(WARMTH)) * md.warmthX;
    m_pre.SetLowShelf(md.shelfHz, warmth, m_sampleRate);   // boost the lows before the saturation ...
    m_post.SetLowShelf(md.shelfHz, -warmth, m_sampleRate); // ... and take the boost out again after it
    m_tone.SetCutoff(ToneHz(GetParameterAsFloat(TONE)), m_sampleRate);
}

void TapeSatModule::Process(const float *in, float *out, size_t n) {
    if (m_dirty) {
        m_dirty = false;
        Recompute();
    }
    const float bias = kMode[m_mode].bias;

    for (size_t i = 0; i < n; i++) {
        m_drive += (m_driveTarget - m_drive) * kSmoothing;
        m_mix += (m_mixTarget - m_mix) * kSmoothing;
        m_out += (m_outTarget - m_out) * kSmoothing;

        const float dry = m_hpIn.HP(in[i]);
        const float v = m_pre.Process(dry) * m_drive;
        // Unity gain for small signals: the soft clip has a slope of 1.5 at the origin, and a bias adds a small offset that is taken out again
        float wet = m_clip.Process(v + bias) * (1.0f / (1.5f * m_drive));
        wet = m_dc.HP(wet);
        wet = m_post.Process(wet);
        wet = m_tone.LP(wet);
        out[i] = (dry * (1.0f - m_mix) + wet * m_mix) * m_out;
    }
}
