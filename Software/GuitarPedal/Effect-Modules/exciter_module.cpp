#include "exciter_module.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;
using namespace distdsp;

namespace {
constexpr float kMakeup = 0.7413f; // level match at default settings (see tests/host/levels/test_level_distortion_family.cpp)
constexpr float kSmoothing = 0.002f;
constexpr float kWetGain = 0.5f; // how much of the shaper's output is added at full Mix and full harmonic levels

float DriveDb(float v) { return ParamDisplay::Lin(v, 0.0f, 40.0f); }
float FocusHz(float v) { return ParamDisplay::Exp(v, 500.0f, 8000.0f); }

ParameterMetaData Harmonic(const char *name, float def, int knob) {
    ParameterMetaData m{};
    m.name = name;
    m.valueType = ParameterValueType::Float;
    m.valueBinCount = 0;
    m.defaultValue.float_value = def;
    m.knobMapping = knob;
    m.midiCCMapping = -1;
    m.unit = " %";
    m.decimals = 0;
    m.displayFn = [](float v) { return ParamDisplay::Percent(v); };
    return m;
}
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, ExciterModule::PARAM_COUNT> params{};

    params[ExciterModule::DRIVE] = {
        name : "Drive",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.7f},
        knobMapping : 0,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 0,
        displayFn : [](float v) { return DriveDb(v); }
    };
    params[ExciterModule::H2] = Harmonic("2nd", 0.5f, 1);
    params[ExciterModule::H3] = Harmonic("3rd", 0.3f, 2);
    params[ExciterModule::H4] = Harmonic("4th", 0.0f, -1);
    params[ExciterModule::H5] = Harmonic("5th", 0.0f, -1);

    params[ExciterModule::FOCUS] = {
        name : "Focus",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : 3,
        midiCCMapping : -1,
        unit : " Hz",
        decimals : 0,
        displayFn : [](float v) { return FocusHz(v); }
    };

    params[ExciterModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 4,
        midiCCMapping : -1,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[ExciterModule::LEVEL] = {
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

ExciterModule::ExciterModule()
    : MonoEffectModule(), m_sampleRate(48000.0f), m_dirty(true), m_driveTarget(1.0f), m_drive(1.0f), m_h2Target(0.0f), m_h2(0.0f), m_h3Target(0.0f), m_h3(0.0f),
      m_h4Target(0.0f), m_h4(0.0f), m_h5Target(0.0f), m_h5(0.0f), m_mixTarget(0.5f), m_mix(0.5f), m_outTarget(1.0f), m_out(1.0f) {
    m_name = "Exciter";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

ExciterModule::~ExciterModule() {}

void ExciterModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    m_hpIn.SetCutoff(30.0f, sample_rate);
    m_dc.SetCutoff(20.0f, sample_rate);
    for (OnePole *f : {&m_hpIn, &m_focus, &m_dc}) f->Reset();
    Recompute();
    m_dirty = false;
    m_drive = m_driveTarget;
    m_h2 = m_h2Target; m_h3 = m_h3Target; m_h4 = m_h4Target; m_h5 = m_h5Target;
    m_mix = m_mixTarget;
    m_out = m_outTarget;
    Settle();
}

void ExciterModule::ParameterChanged(int /*parameter_id*/) { m_dirty = true; }

void ExciterModule::Recompute() {
    m_driveTarget = dbToGain(DriveDb(GetParameterAsFloat(DRIVE)));
    m_h2Target = GetParameterAsFloat(H2);
    m_h3Target = GetParameterAsFloat(H3);
    m_h4Target = GetParameterAsFloat(H4);
    m_h5Target = GetParameterAsFloat(H5);
    m_mixTarget = GetParameterAsFloat(MIX);
    m_outTarget = dbToGain(ParamDisplay::Lin(GetParameterAsFloat(LEVEL), -24.0f, 6.0f)) * kMakeup;
    m_focus.SetCutoff(FocusHz(GetParameterAsFloat(FOCUS)), m_sampleRate);
}

void ExciterModule::Process(const float *in, float *out, size_t n) {
    if (m_dirty) {
        m_dirty = false;
        Recompute();
    }

    for (size_t i = 0; i < n; i++) {
        m_drive += (m_driveTarget - m_drive) * kSmoothing;
        m_h2 += (m_h2Target - m_h2) * kSmoothing;
        m_h3 += (m_h3Target - m_h3) * kSmoothing;
        m_h4 += (m_h4Target - m_h4) * kSmoothing;
        m_h5 += (m_h5Target - m_h5) * kSmoothing;
        m_mix += (m_mixTarget - m_mix) * kSmoothing;
        m_out += (m_outTarget - m_out) * kSmoothing;

        const float x = m_hpIn.HP(in[i]);
        // The part above Focus is driven into the shaper (Chebyshev polynomials are defined on -1 .. 1)
        const float s = clampf(m_focus.HP(x) * m_drive, -1.0f, 1.0f);
        const float s2 = s * s;
        const float t2 = 2.0f * s2 - 1.0f;
        const float t3 = s * (4.0f * s2 - 3.0f);
        const float t4 = 8.0f * s2 * (s2 - 1.0f) + 1.0f;
        const float t5 = s * (16.0f * s2 * s2 - 20.0f * s2 + 5.0f);
        float wet = m_h2 * t2 + m_h3 * t3 + m_h4 * t4 + m_h5 * t5;
        wet = m_dc.HP(wet); // T2 and T4 have an offset
        out[i] = (x + m_mix * kWetGain * wet) * m_out;
    }
}
