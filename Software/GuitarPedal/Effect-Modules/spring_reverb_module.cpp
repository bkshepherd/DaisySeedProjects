#include "spring_reverb_module.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;
using namespace distdsp;

namespace {
const char *s_tankNames[SpringReverbModule::TANK_COUNT] = {"Short", "Medium", "Long"};
const char *s_outputNames[SpringReverbModule::OUTPUT_COUNT] = {"Stereo", "Mono"};
const char *s_springCountNames[SpringReverbModule::kMaxSprings - SpringReverbModule::kMinSprings + 1] = {"2", "3", "4", "5", "6"};

// The time of a round trip through each spring of the Medium tank (the spacing of its echoes at 1 kHz), in seconds: the springs are of different lengths, so that
// their echoes do not line up (a tank of several springs is much less like a delay than one spring). The Short and the Long tank scale them.
constexpr float kSpringSeconds[SpringReverbModule::kMaxSprings] = {0.034f, 0.041f, 0.0475f, 0.054f, 0.0295f, 0.0385f};
constexpr float kTankScale[SpringReverbModule::TANK_COUNT] = {0.7f, 1.0f, 1.35f};

// The dispersion: groups of allpass sections with the coefficients kDispersionCoefs, from the fit of tools/fit_spring_dispersion.py: the delay of the chain
// above its 4 ms at DC follows 15.5 ms * (1 / sqrt(1 - (f / 4 kHz)^2) - 1), that of a low-pass ladder (a helical spring below its transition frequency):
// nothing at the lows, 0.5 ms at 1 kHz, 2.4 ms at 2 kHz, 8 ms at 3 kHz, 20 ms at 3.6 kHz, a maximum at 4 kHz: the lows of an echo arrive first and the
// highs last (an upward sweep). The other springs have a few sections more or less, so that they do not sweep alike.
constexpr float kDispersionCoefs[4] = {0.30f, 0.70f, 0.80f, 0.88f};
constexpr int kDispersionCounts[4] = {53, 4, 13, 21};
constexpr float kSectionScale[SpringReverbModule::kMaxSprings] = {1.0f, 0.91f, 0.96f, 0.87f, 1.04f, 0.83f};

constexpr float kReferenceHz = 1000.0f;    // the loop time is the round trip of this frequency (the chain delays the highs more)
constexpr float kInputHighpassHz = 120.0f; // the driver of the tank
constexpr float kInputLowpassHz = 4200.0f; // a spring does not carry the highs: the main echoes end at the transition frequency, where the stretched
                                           // allpass chain repeats and mirrors, so a 4th-order low-pass (which also keeps what is above 8 kHz out of the 16 kHz springs)
constexpr float kInterpolationHz = 4600.0f; // the low-pass that interpolates the springs back to the full rate (4th order)
constexpr float kDampingHz = 6000.0f;      // each round trip loses some of the highs
constexpr float kOutputHighpassHz = 100.0f;
constexpr float kWetGain = 0.76f;          // the wet gain at Mix 100%: calibrated so that the reverb adds as much energy as the dry signal (on average over the six open
                                           // strings, tests/host/levels/test_level_spring_reverb.cpp), so the default Mix of 30% returns about 10 dB below the dry signal
constexpr float kSmoothing = 0.002f;       // per sample at the full rate: about 10 ms at 48 kHz
constexpr float kDelaySlew = 0.0024f;      // per run of the springs (16 kHz): the delay line follows a change of the tank or of Drip within about 25 ms
constexpr float kFadeIn = 0.0006f;         // per run of the springs: a spring that is switched on fades in over about 100 ms (it starts from silence, and its start-up, a
                                           // dispersed burst, the highs first, would click if it were faded in quickly)
constexpr float kFadeOut = 0.006f;         // and one that is switched off fades out in about 10 ms

float DwellDb(float v) { return ParamDisplay::Lin(v, -12.0f, 24.0f); }
float DecaySeconds(float v) { return ParamDisplay::Exp(v, 0.5f, 6.0f); }
float ToneHz(float v) { return ParamDisplay::Exp(v, 1500.0f, 12000.0f); }
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, SpringReverbModule::PARAM_COUNT> params{};

    params[SpringReverbModule::DWELL] = {
        name : "Dwell",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : -1,
        unit : "",
        decimals : 1,
        displayFn : [](float v) { return ParamDisplay::Lin(v, 0.0f, 10.0f); }
    };

    params[SpringReverbModule::DECAY] = {
        name : "Decay",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // 1.7 s
        knobMapping : 1,
        midiCCMapping : -1,
        unit : " s",
        decimals : 1,
        displayFn : [](float v) { return DecaySeconds(v); }
    };

    params[SpringReverbModule::TONE] = {
        name : "Tone",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 2,
        midiCCMapping : -1,
        unit : " Hz",
        decimals : 0,
        displayFn : [](float v) { return ToneHz(v); }
    };

    params[SpringReverbModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : 3,
        midiCCMapping : -1,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[SpringReverbModule::DRIP] = {
        name : "Drip",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 1.0f}, // the dispersion of a real spring (the fit of kDispersionCounts and kDispersionCoefs)
        knobMapping : 4,
        midiCCMapping : -1,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[SpringReverbModule::TANK] = {
        name : "Tank",
        valueType : ParameterValueType::Binned,
        valueBinCount : SpringReverbModule::TANK_COUNT,
        valueBinNames : s_tankNames,
        defaultValue : {.uint_value = SpringReverbModule::MEDIUM_TANK},
        knobMapping : 5,
        midiCCMapping : -1
    };

    params[SpringReverbModule::OUTPUT] = {
        name : "Output",
        valueType : ParameterValueType::Binned,
        valueBinCount : SpringReverbModule::OUTPUT_COUNT,
        valueBinNames : s_outputNames,
        defaultValue : {.uint_value = SpringReverbModule::MONO_OUT},
        knobMapping : -1,
        midiCCMapping : -1
    };

    params[SpringReverbModule::SPRINGS] = {
        name : "Springs",
        valueType : ParameterValueType::Binned,
        valueBinCount : SpringReverbModule::kMaxSprings - SpringReverbModule::kMinSprings + 1,
        valueBinNames : s_springCountNames,
        defaultValue : {.uint_value = 2}, // 4 springs (raw values are 0-based: 0 is 2 springs)
        knobMapping : -1,
        midiCCMapping : -1
    };

    return params;
}();

float SpringReverbModule::Spring::Process(float in) {
    delay += (delayTarget - delay) * kDelaySlew;
    float fb = damp.LP(line.Read(delay)) * gain;
    if (std::fabs(fb) < 1.0e-18f) fb = 0.0f; // no denormals in the tail
    const float c = chain.Process(in * inGain + fb);
    line.Write(c);
    return c;
}

void SpringReverbModule::Spring::Reset() {
    chain.Reset();
    line.Reset();
    damp.Reset();
}

SpringReverbModule::SpringReverbModule()
    : BaseEffectModule(), m_sampleRate(48000.0f), m_springRate(16000.0f), m_dirty(true), m_dwellTarget(1.0f), m_dwell(1.0f), m_wetTarget(0.0f), m_wet(0.0f), m_monoTarget(1.0f),
      m_mono(1.0f), m_springCount(4), m_phase(0), m_weightA(2.0f), m_weightB(2.0f) {
    m_name = "Spring Reverb";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
    for (float &a : m_activity) a = 0.0f;
}

SpringReverbModule::~SpringReverbModule() {}

void SpringReverbModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    m_springRate = sample_rate / kDecimation;
    m_hpIn.SetHighpass(kInputHighpassHz, 0.707f, sample_rate);
    m_lpIn.SetLowpass(kInputLowpassHz, 0.5412f, sample_rate); // 4th-order Butterworth low-passes in two sections
    m_lpIn2.SetLowpass(kInputLowpassHz, 1.3066f, sample_rate);
    for (int c = 0; c < 2; c++) {
        m_interp1[c].SetLowpass(kInterpolationHz, 0.5412f, sample_rate);
        m_interp2[c].SetLowpass(kInterpolationHz, 1.3066f, sample_rate);
        m_hpOut[c].SetHighpass(kOutputHighpassHz, 0.707f, sample_rate);
    }
    for (Spring &s : m_spring) s.damp.SetCutoff(kDampingHz, m_springRate);
    ClearState();
    for (float &a : m_activity) a = 0.0f; // all idle: Recompute starts the springs that run from silence
    Recompute();
    m_dirty = false;
    m_dwell = m_dwellTarget; // start at the knob positions
    m_wet = m_wetTarget;
    m_mono = m_monoTarget;
    m_weightA = m_weightB = 0.0f;
    for (int s = 0; s < kMaxSprings; s++) {
        m_spring[s].delay = m_spring[s].delayTarget;
        m_activity[s] = s < m_springCount ? 1.0f : 0.0f;
        (s % 2 == 0 ? m_weightA : m_weightB) += m_activity[s] * m_activity[s];
    }
}

void SpringReverbModule::ClearState() {
    for (Spring &s : m_spring) s.Reset();
    for (Biquad *f : {&m_hpIn, &m_lpIn, &m_lpIn2, &m_interp1[0], &m_interp1[1], &m_interp2[0], &m_interp2[1], &m_hpOut[0], &m_hpOut[1], &m_lpOut[0], &m_lpOut[1]}) f->Reset();
    m_dwellClip.Reset();
    m_phase = 0;
}

// Switching the reverb on starts from silence: a chain slot that was switched off is not processed, so its tank would still hold what was in it
// (the same is true without Trails). Not while the tail is still ringing out (Trails).
void SpringReverbModule::SetEnabled(bool isEnabled) {
    const bool wasEnabled = IsEnabled();
    BaseEffectModule::SetEnabled(isEnabled);
    if (isEnabled && !wasEnabled && !IsTrailing()) {
        ClearState();
    }
}

void SpringReverbModule::ParameterChanged(int /*parameter_id*/) { m_dirty = true; }

void SpringReverbModule::Recompute() {
    const int tank = std::min(std::max(GetParameterAsBinnedValue(TANK) - 1, 0), static_cast<int>(TANK_COUNT) - 1);
    const int springs = std::min(std::max(GetParameterAsBinnedValue(SPRINGS) - 1, 0), kMaxSprings - kMinSprings) + kMinSprings;
    const float drip = GetParameterAsFloat(DRIP);
    const float decay = DecaySeconds(GetParameterAsFloat(DECAY));

    m_monoTarget = GetParameterAsBinnedValue(OUTPUT) - 1 == MONO_OUT ? 1.0f : 0.0f;
    m_dwellTarget = dbToGain(DwellDb(GetParameterAsFloat(DWELL)));
    // The reverb is added to the dry signal, as the reverb control of an amp does: the dry level never changes
    m_wetTarget = GetParameterAsFloat(MIX) * kWetGain;
    m_springCount = springs;

    for (int i = 0; i < kMaxSprings; i++) {
        Spring &s = m_spring[i];
        int counts[4];
        float coefs[4];
        for (int g = 0; g < 4; g++) {
            counts[g] = static_cast<int>(kDispersionCounts[g] * kSectionScale[i] + 0.5f);
            coefs[g] = springdsp::DripCoefficient(kDispersionCoefs[g], drip); // 0: a plain delay, 1: the full dispersion
        }
        s.chain.Set(counts, coefs, 4);
        const float roundTrip = kSpringSeconds[i] * kTankScale[tank];
        // The loop is the delay line and the chain: the delay line is what is left of the round trip after the chain's delay at the reference frequency
        const float line = (roundTrip - s.chain.GroupDelay(kReferenceHz, m_springRate)) * m_springRate;
        s.delayTarget = std::min(std::max(line, 4.0f), static_cast<float>(kMaxDelay) - 4.0f);
        s.gain = std::pow(10.0f, -3.0f * roundTrip / decay); // -60 dB after the decay time
        s.inGain = std::sqrt(1.0f - s.gain * s.gain);       // the energy of the whole tail is that of the input, whatever the decay
        // A spring that was idle and is switched on starts from silence (one that is still fading out keeps what is in it)
        if (i < springs && m_activity[i] <= 1.0e-4f) {
            s.Reset();
            s.delay = s.delayTarget;
            m_activity[i] = 0.0f;
        }
    }
    const float toneHz = ToneHz(GetParameterAsFloat(TONE));
    for (int c = 0; c < 2; c++) m_lpOut[c].SetLowpass(toneHz, 0.707f, m_sampleRate);
}

void SpringReverbModule::Process(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    if (m_dirty) {
        m_dirty = false;
        Recompute();
    }

    for (size_t i = 0; i < n; i++) {
        const float l = inL[i], r = inR[i]; // read before writing: the buffers may be the same
        m_dwell += (m_dwellTarget - m_dwell) * kSmoothing;
        m_wet += (m_wetTarget - m_wet) * kSmoothing;
        m_mono += (m_monoTarget - m_mono) * kSmoothing;

        float x = m_hpIn.Process(0.5f * (l + r));
        x = m_dwellClip.Process(x * m_dwell * (1.0f / 1.5f)); // the clip has a gain of 1.5 for small signals: a gain of m_dwell, up to a ceiling of 1
        x = m_lpIn2.Process(m_lpIn.Process(x));

        // The springs run on every third sample, each on the sample that is there (the signal is low-passed below 4.2 kHz); the sum goes back to the full rate with
        // zeros in between and the interpolating low-pass
        float uA = 0.0f, uB = 0.0f;
        if (m_phase == 0) {
            float sumA = 0.0f, sumB = 0.0f, weightA = 0.0f, weightB = 0.0f;
            for (int s = 0; s < kMaxSprings; s++) {
                float &a = m_activity[s];
                if (s < m_springCount) {
                    a += (1.0f - a) * kFadeIn;
                } else if (a > 0.0f) {
                    a -= a * kFadeOut;
                    if (a < 1.0e-4f) a = 0.0f; // faded out: the spring is not processed any more
                }
                if (a > 0.0f) {
                    const float y = m_spring[s].Process(x) * a;
                    if (s % 2 == 0) {
                        sumA += y;
                        weightA += a * a;
                    } else {
                        sumB += y;
                        weightB += a * a;
                    }
                }
            }
            m_weightA = weightA;
            m_weightB = weightB;
            // every channel is normalised by the number of springs it has: the same level for any count
            uA = weightA > 0.0f ? kDecimation * sumA / std::sqrt(weightA) : 0.0f;
            uB = weightB > 0.0f ? kDecimation * sumB / std::sqrt(weightB) : 0.0f;
        }
        m_phase = m_phase + 1 == kDecimation ? 0 : m_phase + 1;

        const float fA = m_lpOut[0].Process(m_hpOut[0].Process(m_interp2[0].Process(m_interp1[0].Process(uA))));
        const float fB = m_lpOut[1].Process(m_hpOut[1].Process(m_interp2[1].Process(m_interp1[1].Process(uB))));
        // Mono: all the springs on both channels. The filters are linear and the same on both, so the sum of all the springs is the weighted sum of the two channels
        const float total = m_weightA + m_weightB;
        const float mono = total > 0.0f ? (std::sqrt(m_weightA) * fA + std::sqrt(m_weightB) * fB) / std::sqrt(total) : 0.0f;
        const float wetL = fA + (mono - fA) * m_mono;
        const float wetR = fB + (mono - fB) * m_mono;
        outL[i] = l + wetL * m_wet;
        outR[i] = r + wetR * m_wet;
    }
}

void SpringReverbModule::ProcessMono(float in) {
    float outL, outR;
    Process(&in, &in, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void SpringReverbModule::ProcessStereo(float inL, float inR) {
    float outL, outR;
    Process(&inL, &inR, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void SpringReverbModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    Process(in, in, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void SpringReverbModule::ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    Process(inL, inR, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}
