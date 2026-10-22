#include "muff_fuzz_module.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;
using namespace distdsp;

namespace {
const char *s_modeNames[3] = {"Classic", "Fat", "Bright"};
constexpr float kToneCornerHz[3] = {1500.0f, 800.0f, 3000.0f};
constexpr float kMakeup[3] = {0.1580f, 0.1580f, 0.1580f}; // the level: matched to the Preamp's Crunch when both are saturated (see tests/host/levels/test_level_distortion_family.cpp)
constexpr float kSmoothing = 0.002f;
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, MuffFuzzModule::PARAM_COUNT> params{};

    params[MuffFuzzModule::SUSTAIN] = {
        name : "Sustain",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.6f},
        knobMapping : 0,
        midiCCMapping : -1,
        unit : "",
        decimals : 1,
        displayFn : [](float v) { return ParamDisplay::Lin(v, 0.0f, 10.0f); }
    };

    params[MuffFuzzModule::TONE] = {
        name : "Tone",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : -1,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[MuffFuzzModule::VOLUME] = {
        name : "Volume",
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

    params[MuffFuzzModule::MODE] = {
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

MuffFuzzModule::MuffFuzzModule()
    : MonoEffectModule(), m_sampleRate(48000.0f), m_dirty(true), m_sustainTarget(1.0f), m_sustain(1.0f), m_toneTarget(0.5f), m_tone(0.5f), m_outTarget(1.0f),
      m_out(1.0f) {
    m_name = "Muff Fuzz";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

MuffFuzzModule::~MuffFuzzModule() {}

void MuffFuzzModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    m_hpIn.SetCutoff(40.0f, sample_rate);
    m_c1.SetCutoff(35.0f, sample_rate);
    m_c2.SetCutoff(35.0f, sample_rate);
    m_c3.SetCutoff(35.0f, sample_rate);
    m_lpA.SetCutoff(7000.0f, sample_rate);
    m_lpB.SetCutoff(6000.0f, sample_rate);
    m_lpC.SetCutoff(5500.0f, sample_rate);
    m_dc.SetCutoff(20.0f, sample_rate);
    for (OnePole *f : {&m_hpIn, &m_c1, &m_c2, &m_c3, &m_lpA, &m_lpB, &m_lpC, &m_toneSplit, &m_dc}) f->Reset();
    m_clipA.Reset();
    m_clipB.Reset();
    m_clipC.Reset();
    Recompute();
    m_dirty = false;
    m_sustain = m_sustainTarget;
    m_tone = m_toneTarget;
    m_out = m_outTarget;
    Settle();
}

void MuffFuzzModule::ParameterChanged(int /*parameter_id*/) { m_dirty = true; }

void MuffFuzzModule::Recompute() {
    const int mode = std::min(std::max(GetParameterAsBinnedValue(MODE) - 1, 0), 2);
    const float sustain = GetParameterAsFloat(SUSTAIN);
    m_sustainTarget = 3.0f + 17.0f * sustain * sustain; // the gain of each of the two clipping stages
    m_toneTarget = GetParameterAsFloat(TONE);
    m_outTarget = dbToGain(ParamDisplay::Lin(GetParameterAsFloat(VOLUME), -24.0f, 6.0f)) * kMakeup[mode];
    m_toneSplit.SetCutoff(kToneCornerHz[mode], m_sampleRate);
}

void MuffFuzzModule::Process(const float *in, float *out, size_t n) {
    if (m_dirty) {
        m_dirty = false;
        Recompute();
    }

    for (size_t i = 0; i < n; i++) {
        m_sustain += (m_sustainTarget - m_sustain) * kSmoothing;
        m_tone += (m_toneTarget - m_tone) * kSmoothing;
        m_out += (m_outTarget - m_out) * kSmoothing;

        float y = m_hpIn.HP(in[i]);
        y = m_lpA.LP(m_clipA.Process(y * 6.0f)); // the input booster: a soft clip
        y = m_c1.HP(y);
        y = m_lpB.LP(m_clipB.Process(y * m_sustain)); // the first clipping stage
        y = m_c2.HP(y);
        y = m_lpC.LP(m_clipC.Process(y * m_sustain)); // the second
        y = m_c3.HP(y);
        // The tone: low-pass and high-pass of the same corner, crossfaded (the mids are scooped at the centre)
        const float lo = m_toneSplit.LP(y);
        const float hi = y - lo;
        y = lo * (1.0f - m_tone) + hi * m_tone;
        y = m_dc.HP(y);
        out[i] = y * m_out;
    }
}
