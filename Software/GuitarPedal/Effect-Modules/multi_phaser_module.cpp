#include "multi_phaser_module.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;
using M = MultiPhaserModule;

namespace {
const char *s_routingNames[M::ROUTING_COUNT] = {"Series", "Parallel"};
const char *s_outputNames[M::OUTPUT_COUNT] = {"Mono", "Stereo"};
const char *s_onNames[2] = {"Off", "On"};
const char *s_stageNames[6] = {"2", "4", "6", "8", "10", "12"};
const char *s_waveNames[M::WAVE_COUNT] = {"Sine", "Triangle", "Random"};

// The names in the menu: "P1 Rate" and so on
const char *s_unitNames[M::kUnits][M::U_COUNT] = {
    {"P1 On", "P1 Rate", "P1 Depth", "P1 Feedback", "P1 Stages", "P1 Center", "P1 Wave", "P1 Phase"},
    {"P2 On", "P2 Rate", "P2 Depth", "P2 Feedback", "P2 Stages", "P2 Center", "P2 Wave", "P2 Phase"},
    {"P3 On", "P3 Rate", "P3 Depth", "P3 Feedback", "P3 Stages", "P3 Center", "P3 Wave", "P3 Phase"},
    {"P4 On", "P4 Rate", "P4 Depth", "P4 Feedback", "P4 Stages", "P4 Center", "P4 Wave", "P4 Phase"}};

// The defaults of the four phasers: two on, with different speeds and centres, so that the sweeps drift against each other; the others are ready for use.
struct UnitDefaults {
    int on;
    float rate, depth, feedback, center, phase;
    int stages, wave;
};
constexpr UnitDefaults kDefaults[M::kUnits] = {{1, 0.45f, 0.8f, 0.65f, 0.50f, 0.0f, 1, M::SINE},       // 0.5 Hz, +30% feedback, 4 stages, 630 Hz
                                               {1, 0.35f, 0.7f, 0.50f, 0.60f, 0.25f, 2, M::TRIANGLE},  // 0.3 Hz, no feedback, 6 stages, 900 Hz
                                               {0, 0.55f, 0.6f, 0.60f, 0.40f, 0.5f, 3, M::SINE},       // 0.9 Hz, 8 stages, 400 Hz
                                               {0, 0.25f, 0.9f, 0.50f, 0.70f, 0.75f, 0, M::RANDOM}};   // 0.15 Hz, 2 stages, 1.3 kHz

constexpr float kRateMinHz = 0.05f, kRateMaxHz = 10.0f;
constexpr float kCenterMinHz = 100.0f, kCenterMaxHz = 4000.0f;
constexpr float kSweepOctaves = 2.5f;      // each way from the centre at Depth 100%
constexpr float kFeedbackMax = 0.85f;
constexpr float kStereoOffset = 0.25f;     // the right channel's oscillators, in cycles
constexpr float kPi = 3.14159265358979f;
constexpr float kParamSmoothing = 0.06f;   // per control update (16 samples): about 15 ms
constexpr float kSmoothing = 0.002f;       // per sample: about 10 ms at 48 kHz
constexpr int kSettleSamples = 2880;       // after a change that cannot be made smoothly (the routing, the number of stages) the phased signal stays off for 60 ms while the filters settle

float RateHz(float v) { return ParamDisplay::Exp(v, kRateMinHz, kRateMaxHz); }
float CenterHz(float v) { return ParamDisplay::Exp(v, kCenterMinHz, kCenterMaxHz); }
float FeedbackValue(float v) { return (v * 2.0f - 1.0f) * kFeedbackMax; }
// The feedback raises the level of the chain (for noise by 1 / (1 - fb^2) in energy): the output of a phaser is scaled so that its energy for noise
// is that of a phaser without feedback
float FeedbackGain(float fb) { return std::sqrt(2.0f * (1.0f - fb * fb) / (2.0f - fb * fb)); }
// The sum of the input and the allpass chain takes level off (for noise 3 dB per phaser, a guitar-like signal 1.5 to 5 dB): each phaser makes it up, so that a
// default phaser is level-neutral on a guitar signal (the rule of the project: the default level +0 to +3 dB, never attenuating)

// The oscillators have their zero crossing, rising, at phase 0
float LfoValue(int wave, float p, float rnd) {
    p -= std::floor(p);
    switch (wave) {
    case M::TRIANGLE:
        return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);
    case M::RANDOM:
        return rnd;
    default:
        return std::sin(2.0f * kPi * p);
    }
}
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, M::PARAM_COUNT> params{};

    params[M::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 1.0f},
        knobMapping : 0,
        midiCCMapping : -1,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[M::ROUTING] = {
        name : "Routing",
        valueType : ParameterValueType::Binned,
        valueBinCount : M::ROUTING_COUNT,
        valueBinNames : s_routingNames,
        defaultValue : {.uint_value = M::SERIES},
        knobMapping : -1,
        midiCCMapping : -1
    };

    params[M::OUTPUT] = {
        name : "Output",
        valueType : ParameterValueType::Binned,
        valueBinCount : M::OUTPUT_COUNT,
        valueBinNames : s_outputNames,
        defaultValue : {.uint_value = M::MONO_OUT},
        knobMapping : -1,
        midiCCMapping : -1
    };

    // The knobs are the controls of the first phaser (Rate, Depth, Feedback) and the Rate and Depth of the second
    const int knobs[M::kUnits][M::U_COUNT] = {{-1, 1, 2, 3, -1, -1, -1, -1}, {-1, 4, 5, -1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1, -1, -1, -1}};
    for (int u = 0; u < M::kUnits; u++) {
        const UnitDefaults &d = kDefaults[u];
        params[M::UnitParamId(u, M::U_ON)] = {
            name : s_unitNames[u][M::U_ON],
            valueType : ParameterValueType::Binned,
            valueBinCount : 2,
            valueBinNames : s_onNames,
            defaultValue : {.uint_value = static_cast<uint32_t>(d.on)},
            knobMapping : knobs[u][M::U_ON],
            midiCCMapping : -1
        };
        params[M::UnitParamId(u, M::U_RATE)] = {
            name : s_unitNames[u][M::U_RATE],
            valueType : ParameterValueType::Float,
            valueBinCount : 0,
            defaultValue : {.float_value = d.rate},
            knobMapping : knobs[u][M::U_RATE],
            midiCCMapping : -1,
            unit : " Hz",
            decimals : 2,
            displayFn : [](float v) { return RateHz(v); }
        };
        params[M::UnitParamId(u, M::U_DEPTH)] = {
            name : s_unitNames[u][M::U_DEPTH],
            valueType : ParameterValueType::Float,
            valueBinCount : 0,
            defaultValue : {.float_value = d.depth},
            knobMapping : knobs[u][M::U_DEPTH],
            midiCCMapping : -1,
            unit : " %",
            decimals : 0,
            displayFn : [](float v) { return ParamDisplay::Percent(v); }
        };
        params[M::UnitParamId(u, M::U_FEEDBACK)] = {
            name : s_unitNames[u][M::U_FEEDBACK],
            valueType : ParameterValueType::Float,
            valueBinCount : 0,
            defaultValue : {.float_value = d.feedback},
            knobMapping : knobs[u][M::U_FEEDBACK],
            midiCCMapping : -1,
            unit : " %",
            decimals : 0,
            showSign : true,
            displayFn : [](float v) { return FeedbackValue(v) * 100.0f; }
        };
        params[M::UnitParamId(u, M::U_STAGES)] = {
            name : s_unitNames[u][M::U_STAGES],
            valueType : ParameterValueType::Binned,
            valueBinCount : 6,
            valueBinNames : s_stageNames,
            defaultValue : {.uint_value = static_cast<uint32_t>(d.stages)},
            knobMapping : knobs[u][M::U_STAGES],
            midiCCMapping : -1
        };
        params[M::UnitParamId(u, M::U_CENTER)] = {
            name : s_unitNames[u][M::U_CENTER],
            valueType : ParameterValueType::Float,
            valueBinCount : 0,
            defaultValue : {.float_value = d.center},
            knobMapping : knobs[u][M::U_CENTER],
            midiCCMapping : -1,
            unit : " Hz",
            decimals : 0,
            displayFn : [](float v) { return CenterHz(v); }
        };
        params[M::UnitParamId(u, M::U_WAVE)] = {
            name : s_unitNames[u][M::U_WAVE],
            valueType : ParameterValueType::Binned,
            valueBinCount : M::WAVE_COUNT,
            valueBinNames : s_waveNames,
            defaultValue : {.uint_value = static_cast<uint32_t>(d.wave)},
            knobMapping : knobs[u][M::U_WAVE],
            midiCCMapping : -1
        };
        params[M::UnitParamId(u, M::U_PHASE)] = {
            name : s_unitNames[u][M::U_PHASE],
            valueType : ParameterValueType::Float,
            valueBinCount : 0,
            defaultValue : {.float_value = d.phase},
            knobMapping : knobs[u][M::U_PHASE],
            midiCCMapping : -1,
            unit : " deg",
            decimals : 0,
            displayFn : [](float v) { return ParamDisplay::Lin(v, 0.0f, 360.0f); }
        };
    }
    return params;
}();

// One phaser, one channel: the allpass chain with its feedback; the result is the sum of the input and the output of the chain, halved (the notches).
// Every stage is H(z) = (a + z^-1) / (1 + a z^-1), a = (tan(pi f / fs) - 1) / (tan(pi f / fs) + 1): a quarter turn of phase at f, no change of level.
float MultiPhaserModule::Unit::Process(int ch, float x) {
    const float a = this->a[ch];
    this->a[ch] += da[ch];
    float y = x + fbState[ch];
    float *st = s[ch];
    for (int k = 0; k < stagesActive; k++) {
        const float o = a * y + st[k];
        st[k] = y - a * o;
        y = o;
    }
    float fb = y * feedbackS;
    if (std::fabs(fb) < 1.0e-18f) fb = 0.0f; // no denormals when it rings out
    fbState[ch] = fb;
    return 0.5f * gain * (x + y);
}

void MultiPhaserModule::Unit::Clear() {
    for (int c = 0; c < 2; c++) {
        for (float &v : s[c]) v = 0.0f;
        fbState[c] = 0.0f;
    }
}

MultiPhaserModule::MultiPhaserModule()
    : BaseEffectModule(), m_sampleRate(48000.0f), m_dirty(true), m_mixTarget(1.0f), m_mix(1.0f), m_stereoTarget(0.0f), m_stereo(0.0f), m_routingTarget(SERIES), m_routing(SERIES),
      m_duck(1.0f), m_settle(0), m_tick(0) {
    m_name = "Multi Phaser";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

MultiPhaserModule::~MultiPhaserModule() {}

void MultiPhaserModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    for (int u = 0; u < kUnits; u++) {
        m_unit[u] = Unit();
        m_unit[u].rng = 0x9E3779B9u * (u + 1);
    }
    Recompute();
    m_dirty = false;
    m_mix = m_mixTarget; // start at the knob positions
    m_stereo = m_stereoTarget;
    m_routing = m_routingTarget;
    m_duck = 1.0f;
    m_settle = 0;
    m_tick = 0;
    for (Unit &u : m_unit) {
        u.stagesActive = u.stages;
        u.rateS = u.rate;
        u.depthS = u.depth;
        u.feedbackS = u.feedback;
        u.centerS = u.center;
        u.act = u.on ? 1.0f : 0.0f;
        u.lfoPhase = 0.0f;
    }
    ControlUpdate(m_stereo > 0.0f); // the coefficients: the first update sets them without a ramp
    for (Unit &u : m_unit)
        for (int c = 0; c < 2; c++) u.da[c] = 0.0f;
}

void MultiPhaserModule::ClearState() {
    for (Unit &u : m_unit) u.Clear();
}

// Switching it on starts from silence (not while the tail of a phaser's feedback is still ringing out with Trails)
void MultiPhaserModule::SetEnabled(bool isEnabled) {
    const bool wasEnabled = IsEnabled();
    BaseEffectModule::SetEnabled(isEnabled);
    if (isEnabled && !wasEnabled && !IsTrailing()) ClearState();
}

void MultiPhaserModule::ParameterChanged(int /*parameter_id*/) { m_dirty = true; }

void MultiPhaserModule::Recompute() {
    m_mixTarget = GetParameterAsFloat(MIX);
    m_routingTarget = std::min(std::max(GetParameterAsBinnedValue(ROUTING) - 1, 0), static_cast<int>(ROUTING_COUNT) - 1);
    const float stereoTarget = GetParameterAsBinnedValue(OUTPUT) - 1 == STEREO_OUT ? 1.0f : 0.0f;
    if (stereoTarget > 0.5f && m_stereoTarget < 0.5f && m_stereo < 1.0e-4f) {
        // the right channel starts from the left one's position and from silence
        for (Unit &u : m_unit) {
            for (int k = 0; k < kMaxStages; k++) u.s[1][k] = 0.0f;
            u.fbState[1] = 0.0f;
            u.a[1] = u.a[0];
            u.da[1] = u.da[0];
        }
    }
    m_stereoTarget = stereoTarget;

    for (int i = 0; i < kUnits; i++) {
        Unit &u = m_unit[i];
        const bool wasOn = u.on;
        u.on = GetParameterAsBinnedValue(UnitParamId(i, U_ON)) - 1 == 1;
        u.rate = RateHz(GetParameterAsFloat(UnitParamId(i, U_RATE)));
        u.depth = std::min(std::max(GetParameterAsFloat(UnitParamId(i, U_DEPTH)), 0.0f), 1.0f);
        u.feedback = FeedbackValue(std::min(std::max(GetParameterAsFloat(UnitParamId(i, U_FEEDBACK)), 0.0f), 1.0f));
        u.stages = 2 * std::min(std::max(GetParameterAsBinnedValue(UnitParamId(i, U_STAGES)), 1), kMaxStages / 2);
        u.center = CenterHz(GetParameterAsFloat(UnitParamId(i, U_CENTER)));
        u.wave = std::min(std::max(GetParameterAsBinnedValue(UnitParamId(i, U_WAVE)) - 1, 0), static_cast<int>(WAVE_COUNT) - 1);
        u.phase = GetParameterAsFloat(UnitParamId(i, U_PHASE));
        if (u.on && !wasOn && u.act < 1.0e-4f) {
            // a phaser that was off starts from silence, at its own settings
            u.Clear();
            u.rateS = u.rate;
            u.depthS = u.depth;
            u.feedbackS = u.feedback;
            u.centerS = u.center;
            u.act = 0.0f;
            u.a[0] = u.a[1] = 0.0f;
            u.da[0] = u.da[1] = 0.0f;
            u.fresh = true;
            u.stagesActive = u.stages;
            u.settle = 0;
        }
    }
}

// Every kControlPeriod samples: the oscillators advance, the settings are followed, and the next coefficients are worked out; the coefficients
// are interpolated in between
void MultiPhaserModule::ControlUpdate(bool stereo) {
    const float tickSeconds = kControlPeriod / m_sampleRate;
    for (Unit &u : m_unit) {
        if (!u.on && u.act <= 0.0f) continue;
        u.rateS += (u.rate - u.rateS) * kParamSmoothing;
        u.depthS += (u.depth - u.depthS) * kParamSmoothing;
        u.feedbackS += (u.feedback - u.feedbackS) * kParamSmoothing;
        u.gain = M::kMakeup * FeedbackGain(u.feedbackS);
        u.centerS += (u.center - u.centerS) * kParamSmoothing;

        u.lfoPhase += u.rateS * tickSeconds;
        if (u.lfoPhase >= 1.0f) {
            u.lfoPhase -= std::floor(u.lfoPhase);
            u.rng ^= u.rng << 13; // a new random position for every cycle
            u.rng ^= u.rng >> 17;
            u.rng ^= u.rng << 5;
            u.rndTarget = (static_cast<float>(u.rng >> 8) / 8388608.0f) - 1.0f;
        }
        u.rndValue += (u.rndTarget - u.rndValue) * (1.0f - std::exp(-4.0f * u.rateS * tickSeconds)); // reached in about a quarter of a cycle

        for (int ch = 0; ch < (stereo ? 2 : 1); ch++) {
            const float p = u.lfoPhase + u.phase + (ch == 1 ? kStereoOffset : 0.0f);
            const float lfo = LfoValue(u.wave, p, ch == 1 ? -u.rndValue : u.rndValue);
            if (ch == 0) u.lfo = lfo;
            float fc = u.centerS * std::exp2(u.depthS * kSweepOctaves * lfo);
            fc = std::min(std::max(fc, 30.0f), 12000.0f);
            const float t = std::tan(kPi * fc / m_sampleRate);
            const float target = (t - 1.0f) / (t + 1.0f);
            if (u.fresh) u.a[ch] = target;
            u.da[ch] = (target - u.a[ch]) / kControlPeriod;
        }
        u.fresh = false;
    }
}

void MultiPhaserModule::Process(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    if (m_dirty) {
        m_dirty = false;
        Recompute();
    }

    for (size_t i = 0; i < n; i++) {
        const float l = inL[i], r = inR[i]; // read before writing: the buffers may be the same
        const float x = 0.5f * (l + r);

        m_mix += (m_mixTarget - m_mix) * kSmoothing;
        m_stereo += (m_stereoTarget - m_stereo) * kSmoothing;
        if (m_stereo < 1.0e-4f && m_stereoTarget == 0.0f) m_stereo = 0.0f;
        if (m_routingTarget != m_routing) {
            // the routing changes while the phased signal is faded out, and the filters get time to settle at the new input
            m_duck -= m_duck * kSmoothing * 2.0f;
            if (m_duck < 0.01f) {
                m_duck = 0.0f;
                m_routing = m_routingTarget;
                m_settle = kSettleSamples;
            }
        } else if (m_settle > 0) {
            m_settle--;
        } else if (m_duck < 1.0f) {
            m_duck += (1.0f - m_duck) * kSmoothing * 2.0f + 1.0e-5f;
            if (m_duck > 0.9999f) m_duck = 1.0f;
        }
        for (Unit &u : m_unit) {
            if (!u.on && u.act <= 0.0f) {
                u.stagesActive = u.stages;
                continue;
            }
            const bool stagesChange = u.stagesActive != u.stages;
            const bool wanted = u.on && !stagesChange && u.settle == 0;
            u.act += ((wanted ? 1.0f : 0.0f) - u.act) * kSmoothing;
            if (u.settle > 0 && !stagesChange) u.settle--;
            if (!wanted && u.act < 1.0e-4f) {
                u.act = 0.0f;
                if (stagesChange) {
                    u.stagesActive = u.stages; // faded out: the number of stages is changed, from silence
                    u.settle = kSettleSamples;
                    u.Clear();
                } else if (!u.on) {
                    u.Clear();
                }
            }
        }

        const bool stereo = m_stereo > 0.0f;
        if (m_tick == 0) ControlUpdate(stereo);
        m_tick = m_tick + 1 == kControlPeriod ? 0 : m_tick + 1;

        float chain[2] = {x, x};
        float total = 0.0f;
        for (const Unit &u : m_unit) total += u.act;
        const float parallelScale = total > 1.0f ? 1.0f / total : 1.0f;
        for (int ch = 0; ch < (stereo ? 2 : 1); ch++) {
            float c = x;
            for (Unit &u : m_unit) {
                if (u.act <= 0.0f) continue;
                if (m_routing == SERIES) {
                    const float p = u.Process(ch, c);
                    c += u.act * (p - c);
                } else {
                    const float p = u.Process(ch, x);
                    c += u.act * parallelScale * (p - x);
                }
            }
            chain[ch] = c;
        }
        if (!stereo) chain[1] = chain[0];
        else chain[1] = chain[0] + (chain[1] - chain[0]) * m_stereo;

        const float wet = m_mix * m_duck;
        outL[i] = x + wet * (chain[0] - x);
        outR[i] = x + wet * (chain[1] - x);
    }
}

void MultiPhaserModule::ProcessMono(float in) {
    float outL, outR;
    Process(&in, &in, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void MultiPhaserModule::ProcessStereo(float inL, float inR) {
    float outL, outR;
    Process(&inL, &inR, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void MultiPhaserModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    Process(in, in, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void MultiPhaserModule::ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    Process(inL, inR, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

float MultiPhaserModule::GetBrightnessForLED(int led_id) const {
    const float base = BaseEffectModule::GetBrightnessForLED(led_id);
    if (led_id == 1) {
        for (const Unit &u : m_unit)
            if (u.on) return base * (0.2f + 0.8f * (0.5f + 0.5f * u.lfo)); // the first phaser that is on
    }
    return base;
}
