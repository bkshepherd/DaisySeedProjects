#include "octave_fuzz_module.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;
using namespace distdsp;

namespace {
constexpr float kMakeup = 0.0356f; // level match at default settings (see tests/host/levels/test_level_distortion_family.cpp)
constexpr float kSmoothing = 0.002f;
constexpr float kPreGain = 8.0f; // the gain in front of the rectifier

float ToneHz(float v) { return ParamDisplay::Exp(v, 800.0f, 8000.0f); }
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, OctaveFuzzModule::PARAM_COUNT> params{};

    params[OctaveFuzzModule::FUZZ] = {
        name : "Fuzz",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.6f},
        knobMapping : 0,
        midiCCMapping : -1,
        unit : "",
        decimals : 1,
        displayFn : [](float v) { return ParamDisplay::Lin(v, 0.0f, 10.0f); }
    };

    params[OctaveFuzzModule::OCTAVE] = {
        name : "Octave",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : -1,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[OctaveFuzzModule::TONE] = {
        name : "Tone",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.6f},
        knobMapping : 2,
        midiCCMapping : -1,
        unit : " Hz",
        decimals : 0,
        displayFn : [](float v) { return ToneHz(v); }
    };

    params[OctaveFuzzModule::LEVEL] = {
        name : "Level",
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

    return params;
}();

OctaveFuzzModule::OctaveFuzzModule()
    : MonoEffectModule(), m_sampleRate(48000.0f), m_dirty(true), m_fuzzTarget(1.0f), m_fuzz(1.0f), m_octaveTarget(0.5f), m_octave(0.5f), m_outTarget(1.0f),
      m_out(1.0f) {
    m_name = "Octa Fuzz";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

OctaveFuzzModule::~OctaveFuzzModule() {}

void OctaveFuzzModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    m_hpIn.SetCutoff(60.0f, sample_rate); // the rectifier tracks best without the lowest notes' body
    m_rectDc.SetCutoff(40.0f, sample_rate);
    m_dc.SetCutoff(20.0f, sample_rate);
    for (OnePole *f : {&m_hpIn, &m_rectDc, &m_tone, &m_dc}) f->Reset();
    m_clip.Reset();
    Recompute();
    m_dirty = false;
    m_fuzz = m_fuzzTarget;
    m_octave = m_octaveTarget;
    m_out = m_outTarget;
    Settle();
}

void OctaveFuzzModule::ParameterChanged(int /*parameter_id*/) { m_dirty = true; }

void OctaveFuzzModule::Recompute() {
    const float fuzz = GetParameterAsFloat(FUZZ);
    m_fuzzTarget = 1.0f + 30.0f * fuzz * fuzz;
    m_octaveTarget = GetParameterAsFloat(OCTAVE);
    m_outTarget = dbToGain(ParamDisplay::Lin(GetParameterAsFloat(LEVEL), -24.0f, 6.0f)) * kMakeup;
    m_tone.SetCutoff(ToneHz(GetParameterAsFloat(TONE)), m_sampleRate);
}

void OctaveFuzzModule::Process(const float *in, float *out, size_t n) {
    if (m_dirty) {
        m_dirty = false;
        Recompute();
    }

    for (size_t i = 0; i < n; i++) {
        m_fuzz += (m_fuzzTarget - m_fuzz) * kSmoothing;
        m_octave += (m_octaveTarget - m_octave) * kSmoothing;
        m_out += (m_outTarget - m_out) * kSmoothing;

        const float u = m_hpIn.HP(in[i]) * kPreGain;
        const float rect = m_rectDc.HP(std::fabs(u) * 2.0f); // full-wave rectified (an octave up), without its offset
        const float mixed = u * (1.0f - m_octave) + rect * m_octave;
        float y = m_clip.Process(mixed * m_fuzz);
        y = m_dc.HP(y);
        y = m_tone.LP(y);
        out[i] = y * m_out;
    }
}
