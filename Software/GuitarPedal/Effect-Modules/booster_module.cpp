#include "booster_module.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;
using namespace distdsp;

namespace {
const char *s_typeNames[BoosterModule::TYPE_COUNT] = {"Clean", "Treble", "Mid", "Warm"};

struct TypeData {
    float ceiling; // where the output soft-clips, relative to full scale
    float bias;    // the asymmetry of the clip (a germanium transistor, a tube): the fraction of its range the clip is shifted by
    bool adaa;     // anti-aliased clip: for the types where the clipping is part of the sound (the others only limit at their ceiling)
};
constexpr TypeData kType[BoosterModule::TYPE_COUNT] = {{1.0f, 0.0f, false}, {0.5f, 0.25f, true}, {0.8f, 0.0f, false}, {0.6f, 0.2f, true}};

constexpr float kGainMaxDb = 24.0f;
constexpr float kSmoothing = 0.002f; // per sample: about 10 ms at 48 kHz

float GainDb(float v) { return ParamDisplay::Lin(v, 0.0f, kGainMaxDb); }
float LevelDb(float v) { return ParamDisplay::Lin(v, -24.0f, 6.0f); }
float TiltDb(float v) { return (v - 0.5f) * 16.0f; }                   // Clean: -8 dB (dark) to +8 dB (bright) between the lows and the highs
float TrebleCornerHz(float v) { return ParamDisplay::Exp(v, 200.0f, 3200.0f); } // Treble: below this the bass is cut (800 Hz at 50%)
float MidCenterHz(float v) { return ParamDisplay::Exp(v, 400.0f, 3200.0f); }    // Mid: where the bell is (1.1 kHz at 50%)
float WarmCutHz(float v) { return ParamDisplay::Exp(v, 2000.0f, 12000.0f); }    // Warm: the high cut (4.9 kHz at 50%)

void SetIdentity(Biquad &b) { b.Set(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f); }
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, BoosterModule::PARAM_COUNT> params{};

    params[BoosterModule::GAIN] = {
        name : "Gain",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // 12 dB
        knobMapping : 0,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 1,
        displayFn : [](float v) { return GainDb(v); }
    };

    params[BoosterModule::TONE] = {
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

    params[BoosterModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8f}, // 0 dB
        knobMapping : 2,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 1,
        showSign : true,
        displayFn : [](float v) { return LevelDb(v); }
    };

    params[BoosterModule::TYPE] = {
        name : "Type",
        valueType : ParameterValueType::Binned,
        valueBinCount : BoosterModule::TYPE_COUNT,
        valueBinNames : s_typeNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 3,
        midiCCMapping : -1
    };

    return params;
}();

BoosterModule::BoosterModule()
    : MonoEffectModule(), m_sampleRate(48000.0f), m_dirty(true), m_type(0), m_gainTarget(1.0f), m_gain(1.0f), m_outTarget(1.0f), m_out(1.0f), m_ceiling(1.0f),
      m_invScale(1.0f / 1.5f), m_bias(0.0f), m_sBias(0.0f), m_comp(1.0f), m_adaa(false) {
    m_name = "Booster";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

BoosterModule::~BoosterModule() {}

void BoosterModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    m_hpIn.SetCutoff(10.0f, sample_rate); // only takes away DC: a boost should not thin the low strings
    m_dc.SetCutoff(10.0f, sample_rate);
    for (OnePole *f : {&m_hpIn, &m_dc}) f->Reset();
    for (Biquad *f : {&m_b1, &m_b2, &m_b3}) f->Reset();
    m_soft.Reset();
    Recompute();
    m_dirty = false;
    m_gain = m_gainTarget; // start at the knob positions
    m_out = m_outTarget;
    Settle();
}

void BoosterModule::ParameterChanged(int /*parameter_id*/) { m_dirty = true; }

void BoosterModule::Recompute() {
    const int type = std::min(std::max(GetParameterAsBinnedValue(TYPE) - 1, 0), static_cast<int>(TYPE_COUNT) - 1);
    const float gainDb = GainDb(GetParameterAsFloat(GAIN));
    const float tone = GetParameterAsFloat(TONE);
    const TypeData &t = kType[type];

    if (type != m_type) {
        m_type = type;
        m_soft.Reset(); // the anti-aliasing remembers the previous sample: not one from before the other type was playing
    }
    m_ceiling = t.ceiling;
    m_bias = t.bias;
    m_adaa = t.adaa;
    m_invScale = 1.0f / (1.5f * t.ceiling);
    m_sBias = SoftClipAdaa::f(t.bias);
    m_comp = 1.0f / (1.0f - t.bias * t.bias);
    m_outTarget = dbToGain(LevelDb(GetParameterAsFloat(LEVEL)));

    SetIdentity(m_b1);
    SetIdentity(m_b2);
    SetIdentity(m_b3);
    m_gainTarget = dbToGain(gainDb);
    switch (type) {
    case TREBLE:
        m_b1.SetHighpass(TrebleCornerHz(tone), 0.707f, m_sampleRate);
        m_b3.SetLowpass(11000.0f, 0.707f, m_sampleRate);
        break;
    case MID:
        m_b1.SetPeaking(MidCenterHz(tone), gainDb, 0.9f, m_sampleRate); // the gain is in the bell
        m_gainTarget = 1.0f;
        break;
    case WARM:
        m_b1.SetLowShelf(200.0f, 3.0f, m_sampleRate);
        m_b3.SetLowpass(WarmCutHz(tone), 0.707f, m_sampleRate);
        break;
    default: // CLEAN: a tilt, the lows against the highs, around 1 kHz
        m_b1.SetLowShelf(300.0f, -0.5f * TiltDb(tone), m_sampleRate);
        m_b2.SetHighShelf(3000.0f, 0.5f * TiltDb(tone), m_sampleRate);
        break;
    }
}

void BoosterModule::Process(const float *in, float *out, size_t n) {
    if (m_dirty) {
        m_dirty = false;
        Recompute();
    }

    for (size_t i = 0; i < n; i++) {
        m_gain += (m_gainTarget - m_gain) * kSmoothing;
        m_out += (m_outTarget - m_out) * kSmoothing;

        float y = m_hpIn.HP(in[i]);
        y = m_b2.Process(m_b1.Process(y)) * m_gain;
        const float u = y * m_invScale + m_bias;
        const float s = m_adaa ? m_soft.Process(u) : SoftClipAdaa::f(u);
        y = m_ceiling * (s - m_sBias) * m_comp;
        y = m_b3.Process(m_dc.HP(y)); // an asymmetric clip leaves an offset
        out[i] = y * m_out;
    }
}
