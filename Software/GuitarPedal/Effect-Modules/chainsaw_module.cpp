#include "chainsaw_module.h"
#include <array>
#include <cmath>

using namespace bkshepherd;
using namespace distdsp;

namespace {
// The Color Mix EQ: the HM-2's gyrator frequencies (Low 87 Hz; High two stagger-tuned peaks, together a wide hump in the upper mids). The Qs and
// the +-15 dB range are estimates: published analyses give the frequencies, not the bandwidth or the boost.
constexpr float kLowHz = 87.0f, kLowQ = 0.9f;
constexpr float kHigh1Hz = 960.0f, kHigh2Hz = 1280.0f, kHighQ = 1.6f;
constexpr float kEqRangeDb = 15.0f;

constexpr float kAsymBias = 0.2f; // the op-amp stage clips asymmetrically
constexpr float kDiodeDrive = 3.0f; // how far the op-amp's output drives the diodes to ground into clipping
constexpr float kGateDeadBand = 0.02f; // the back-to-back diodes in the signal path, relative to the clipped level
constexpr float kMakeup = 0.0281f; // the level: matched to the Preamp's Crunch when both are saturated (see tests/host/levels/test_level_distortion_family.cpp)
constexpr float kSmoothing = 0.002f;

float EqDb(float v) { return ParamDisplay::Lin(v, -kEqRangeDb, kEqRangeDb); }
float LevelDb(float v) { return ParamDisplay::Lin(v, -24.0f, 6.0f); }
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, ChainsawModule::PARAM_COUNT> params{};

    params[ChainsawModule::DIST] = {
        name : "Dist",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8f},
        knobMapping : 0,
        midiCCMapping : -1,
        unit : "",
        decimals : 1,
        displayFn : [](float v) { return ParamDisplay::Lin(v, 0.0f, 10.0f); }
    };

    params[ChainsawModule::LOW] = {
        name : "Low",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8333f}, // +10 dB
        knobMapping : 1,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 1,
        showSign : true,
        displayFn : [](float v) { return EqDb(v); }
    };

    params[ChainsawModule::HIGH] = {
        name : "High",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8333f}, // +10 dB
        knobMapping : 2,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 1,
        showSign : true,
        displayFn : [](float v) { return EqDb(v); }
    };

    params[ChainsawModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8f},
        knobMapping : 3,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 1,
        showSign : true,
        displayFn : [](float v) { return LevelDb(v); }
    };

    return params;
}();

ChainsawModule::ChainsawModule()
    : MonoEffectModule(), m_sampleRate(48000.0f), m_dirty(true), m_preTarget(1.0f), m_pre(1.0f), m_gainTarget(1.0f), m_gain(1.0f), m_outTarget(1.0f), m_out(1.0f) {
    m_name = "Chainsaw";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

ChainsawModule::~ChainsawModule() {}

void ChainsawModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    m_hpIn.SetCutoff(80.0f, sample_rate);
    m_lpPre.SetCutoff(6000.0f, sample_rate);
    m_hpCouple.SetCutoff(30.0f, sample_rate);
    m_lpOp.SetCutoff(5000.0f, sample_rate);
    m_dc.SetCutoff(20.0f, sample_rate);
    m_outLp.SetLowpass(7500.0f, 0.707f, sample_rate);
    for (OnePole *f : {&m_hpIn, &m_lpPre, &m_hpCouple, &m_lpOp, &m_dc}) f->Reset();
    for (Biquad *f : {&m_low, &m_high1, &m_high2, &m_outLp}) f->Reset();
    m_preClip.Reset();
    m_opClip.Reset();
    m_diodes.Reset();
    m_gate.d = kGateDeadBand;
    m_gate.Reset();
    Recompute();
    m_dirty = false;
    m_pre = m_preTarget;
    m_gain = m_gainTarget;
    m_out = m_outTarget;
    Settle();
}

void ChainsawModule::ParameterChanged(int /*parameter_id*/) { m_dirty = true; }

void ChainsawModule::Recompute() {
    const float dist = GetParameterAsFloat(DIST);
    m_preTarget = 1.0f + 7.0f * dist;                  // the transistor pre-gain stage
    m_gainTarget = 10.0f + 140.0f * dist * dist;       // the op-amp clipping stage (20 dB at the minimum, as on the pedal: never clean)
    m_outTarget = dbToGain(LevelDb(GetParameterAsFloat(LEVEL))) * kMakeup;
    m_low.SetPeaking(kLowHz, EqDb(GetParameterAsFloat(LOW)), kLowQ, m_sampleRate);
    const float highDb = EqDb(GetParameterAsFloat(HIGH));
    m_high1.SetPeaking(kHigh1Hz, highDb, kHighQ, m_sampleRate);
    m_high2.SetPeaking(kHigh2Hz, highDb, kHighQ, m_sampleRate);
}

void ChainsawModule::Process(const float *in, float *out, size_t n) {
    if (m_dirty) {
        m_dirty = false;
        Recompute();
    }

    for (size_t i = 0; i < n; i++) {
        m_pre += (m_preTarget - m_pre) * kSmoothing;
        m_gain += (m_gainTarget - m_gain) * kSmoothing;
        m_out += (m_outTarget - m_out) * kSmoothing;

        float y = m_hpIn.HP(in[i]);
        y = m_lpPre.LP(m_preClip.Process(y * m_pre)); // the transistor stage
        y = m_hpCouple.HP(y);
        y = m_lpOp.LP(y * m_gain);                    // the op-amp's gain, band-limited
        y = m_opClip.Process(y + kAsymBias);          // asymmetric clipping
        y = m_dc.HP(y);
        y = m_diodes.Process(y * kDiodeDrive);        // the diodes to ground
        y = m_gate.Process(y);                        // the back-to-back diodes in the signal path
        y = m_low.Process(y);                         // the Color Mix EQ, after the clipping
        y = m_high1.Process(y);
        y = m_high2.Process(y);
        y = m_outLp.Process(y);
        out[i] = y * m_out;
    }
}
