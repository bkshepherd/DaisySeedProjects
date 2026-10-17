#include "test_tone_module.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;

namespace {
// The correction that makes a step in a waveform band-limited (the polyBLEP residual); t is the phase (0 .. 1) relative to the step, dt the phase increment per sample
inline float polyBlep(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

const char *s_typeNames[4] = {"Sine", "Square", "Triangle", "Noise"};

constexpr float kGainSmoothing = 0.002f;      // per sample: about 10 ms at 48 kHz
constexpr float kTypeFadeSeconds = 0.005f;    // the level goes down and up again over this long when the type changes
constexpr float kNoiseToSineRms = 1.2247449f; // a uniform noise of amplitude 1 has an RMS of 0.5774; a sine of peak 1 has 0.7071: scale the noise to the same RMS
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, TestToneModule::PARAM_COUNT> params{};

    params[TestToneModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = -20.0f},
        knobMapping : 0,
        midiCCMapping : 14,
        minValue : -60,
        maxValue : 0,
        fineStepSize : 0.01f,
        unit : " dB",
        decimals : 1
    };

    params[TestToneModule::TYPE] = {
        name : "Type",
        valueType : ParameterValueType::Binned,
        valueBinCount : 4,
        valueBinNames : s_typeNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 1,
        midiCCMapping : 15
    };

    params[TestToneModule::FREQUENCY] = {
        name : "Frequency",
        valueType : ParameterValueType::Float,
        valueCurve : ParameterValueCurve::Log,
        valueBinCount : 0,
        defaultValue : {.float_value = 1000.0f},
        knobMapping : 2,
        midiCCMapping : 16,
        minValue : 20,
        maxValue : 20000,
        fineStepSize : 0.01f,
        unit : " Hz",
        decimals : 0
    };

    return params;
}();

TestToneModule::TestToneModule()
    : BaseEffectModule(), m_phase(0.0f), m_phaseInc(1000.0f / 48000.0f), m_sampleRate(48000.0f), m_type(SINE), m_targetGain(0.1f), m_gain(0.1f), m_fade(1.0f) {
    m_name = "TestTone";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

TestToneModule::~TestToneModule() {}

void TestToneModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    m_phase = 0.0f;
    m_noise.state = 0x2545F491u;

    // ParameterChanged only fires on a change, so the defaults (or the stored values) are applied here
    m_type = std::min(std::max(GetParameterAsBinnedValue(TYPE) - 1, 0), 3);
    ParameterChanged(FREQUENCY);
    ParameterChanged(LEVEL);
    m_gain = m_targetGain; // start at the level, not gliding up from silence
    m_fade = 1.0f;
}

void TestToneModule::ParameterChanged(int parameter_id) {
    if (parameter_id == LEVEL) {
        const float db = std::min(std::max(GetParameterAsFloat(LEVEL), -60.0f), 0.0f);
        m_targetGain = std::pow(10.0f, db / 20.0f);
    } else if (parameter_id == FREQUENCY) {
        const float hz = std::min(std::max(GetParameterAsFloat(FREQUENCY), 1.0f), 0.45f * m_sampleRate);
        m_phaseInc = hz / m_sampleRate;
    }
    // TYPE is read in ProcessSamples (the switch is faded)
}

void TestToneModule::ProcessSamples(float *outL, float *outR, size_t n) {
    const int wanted = std::min(std::max(GetParameterAsBinnedValue(TYPE) - 1, 0), 3);
    const float fadeStep = 1.0f / (kTypeFadeSeconds * m_sampleRate);

    for (size_t i = 0; i < n; i++) {
        // A new type: fade out, switch, fade in
        if (wanted != m_type) {
            m_fade -= fadeStep;
            if (m_fade <= 0.0f) {
                m_fade = 0.0f;
                m_type = wanted;
            }
        } else if (m_fade < 1.0f) {
            m_fade = std::min(1.0f, m_fade + fadeStep);
        }

        m_gain += (m_targetGain - m_gain) * kGainSmoothing;

        float x;
        switch (m_type) {
        case NOISE:
            x = m_noise.randSigned(-1.0f, 1.0f) * kNoiseToSineRms; // (peaks up to 1.22 x the level: above about -1.8 dB the noise is limited to full scale below)
            break;
        case SQUARE: { // band-limited, so the harmonics above half the sample rate do not fold back into the audio band
            x = m_phase < 0.5f ? 1.0f : -1.0f;
            x += polyBlep(m_phase, m_phaseInc);
            const float shifted = m_phase >= 0.5f ? m_phase - 0.5f : m_phase + 0.5f;
            x -= polyBlep(shifted, m_phaseInc);
            break;
        }
        case TRIANGLE:
            x = 4.0f * std::fabs(m_phase - 0.5f) - 1.0f; // +1 at phase 0, -1 at 0.5 (its harmonics fall off as 1/n^2, so it needs no correction)
            break;
        default:
            x = std::sin(6.2831853f * m_phase);
            break;
        }
        m_phase += m_phaseInc;
        if (m_phase >= 1.0f) m_phase -= 1.0f;

        const float y = std::min(1.0f, std::max(-1.0f, x * m_gain * m_fade)); // never beyond full scale
        outL[i] = y;
        outR[i] = y;
    }
}

void TestToneModule::ProcessMono(float /*in*/) {
    float l, r;
    ProcessSamples(&l, &r, 1);
    m_audioLeft = l;
    m_audioRight = r;
}

void TestToneModule::ProcessStereo(float inL, float /*inR*/) { ProcessMono(inL); }

void TestToneModule::ProcessMonoBlock(const float * /*in*/, float *outL, float *outR, size_t n) {
    ProcessSamples(outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void TestToneModule::ProcessStereoBlock(const float *inL, const float * /*inR*/, float *outL, float *outR, size_t n) { ProcessMonoBlock(inL, outL, outR, n); }
