#include "crusher_module.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;

static constexpr float s_pitchTrackMultipliers[8] = {
    1.0f,   // unison
    1.5f,   // fifth
    2.0f,   // +1 octave
    3.0f,   // +1 octave fifth
    4.0f,   // +2 octaves
    6.0f,   // +2 octaves fifth
    8.0f,   // +3 octaves
    16.0f,  // +4 octaves
};

static constexpr float s_rateKnobUnlockThreshold = 1.0f / 256.0f;

// Level match at default settings: +6 dB at the reference level, like the other distortion effects (see tests/host/levels/test_level_crusher.cpp)
static constexpr float s_makeup = 1.8708f;

static float LevelDb(float v) { return ParamDisplay::Lin(v, -24.0f, 6.0f); }
static float LevelGain(float v) { return powf(10.0f, LevelDb(v) / 20.0f) * s_makeup; }

static const auto s_metaData = [] {
    std::array<ParameterMetaData, CrusherModule::PARAM_COUNT> params{};

    params[CrusherModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8f},
        knobMapping : 0,
        midiCCMapping : 14,
        unit : " dB",
        decimals : 1,
        showSign : true,
        displayFn : [](float v) { return LevelDb(v); }
    }; // Output level, -24 to +6 dB around the level-matched default (0 dB).

    params[CrusherModule::BITS] = {
        name : "Bits",
        valueType : ParameterValueType::Binned,
        valueBinCount : 32,
        defaultValue : {.uint_value = 7}, // raw values are 0-based: 7 = bin 8 (8 bits)
        knobMapping : 1,
        midiCCMapping : 15
    }; // Bit depth for quantization (lower bits = more digital distortion).

    params[CrusherModule::RATE] = {
        name : "Rate",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.70978f}, // 8000 Hz on the 100 Hz - 48 kHz exponential scale
        knobMapping : 2,
        midiCCMapping : 16,
        unit : " Hz",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Exp(v, 100.0f, 48000.0f); }
    }; // Sample-rate reduction frequency. In pitch-tracking mode this selects the harmonic bin multiplier.

    params[CrusherModule::PITCH_TRACKING] = {
        name : "Pitch Tracking",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : -1
    }; // When enabled, SRR frequency follows detected input pitch (RATE chooses multiplier bins: unison, fifth, octaves).

    params[CrusherModule::JITTER] = {
        name : "Jitter",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 3,
        midiCCMapping : 17,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    }; // Random variation of the SRR hold interval (adds instability/grit to the alias texture).

    params[CrusherModule::CUTOFF] = {
        name : "Cutoff",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.28205f}, // 6000 Hz on the linear 500 Hz - 20 kHz scale
        knobMapping : 4,
        midiCCMapping : 18,
        unit : " Hz",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Lin(v, 500.0f, 20000.0f); }
    }; // Post-crusher low-pass cutoff to tame high-frequency aliasing.

    params[CrusherModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 1.0f},
        knobMapping : 5,
        midiCCMapping : 19,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    }; // Dry/wet blend between input and crushed signal.

    return params;
}();

// Default Constructor
CrusherModule::CrusherModule() :
    BaseEffectModule(), m_rateMin(100.0f), m_rateMax(48000.0f), m_cutoffMin(500.0f), m_cutoffMax(20000.0f),
    m_lpFilter{cycfi::q::lowpass(m_cutoffMax, 48000.0f), cycfi::q::lowpass(m_cutoffMax, 48000.0f)},
    m_manualRateControl(1.0f), m_lastTrackedRate(100.0f), m_rateKnobLatchValue(1.0f), m_hasTrackedRate(false), m_manualRateLocked(false),
    m_restoringManualRate(false) {
    // Set the name of the effect
    m_name = "Crusher";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
CrusherModule::~CrusherModule() {
    // No Code Needed
}

void CrusherModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_rateMax = sample_rate;
    m_manualRateControl = GetParameterAsFloat(RATE);
    m_lastTrackedRate = m_rateMin;
    m_rateKnobLatchValue = m_manualRateControl;
    m_hasTrackedRate = false;
    m_manualRateLocked = false;
    m_restoringManualRate = false;

    m_bitcrusherL.Init(sample_rate);
    m_bitcrusherR.Init(sample_rate);

    // Initialize the pitch detector here rather than lazily in the audio
    // path: its setup allocates, which is not safe in the audio callback
    if (!m_pitchDetector.IsInitialized()) {
        m_pitchDetector.Init(m_rateMax);
    }
}

void CrusherModule::AlternateFootswitchPressed() {
    SetParameterAsBool(PITCH_TRACKING, !GetParameterAsBool(PITCH_TRACKING));
}

void CrusherModule::ParameterChanged(int parameter_id) {
    if(parameter_id == RATE) {
        const float rateControl = GetParameterAsFloat(RATE);

        if(m_restoringManualRate) {
            m_restoringManualRate = false;
            return;
        }

        if(GetParameterAsBool(PITCH_TRACKING)) {
            return;
        }

        if(m_manualRateLocked) {
            if(fabsf(rateControl - m_rateKnobLatchValue) <= s_rateKnobUnlockThreshold) {
                if(fabsf(rateControl - m_manualRateControl) > s_rateKnobUnlockThreshold) {
                    m_restoringManualRate = true;
                    SetParameterAsFloat(RATE, m_manualRateControl);
                }
                return;
            }

            m_manualRateLocked = false;
        }

        m_manualRateControl = rateControl;
        return;
    }

    if(parameter_id == PITCH_TRACKING) {
        if(GetParameterAsBool(PITCH_TRACKING)) {
            m_hasTrackedRate = false;
            return;
        }

        m_manualRateLocked = true;
        m_rateKnobLatchValue = GetParameterAsFloat(RATE);

        if(m_hasTrackedRate) {
            m_manualRateControl = GetRateControlForFrequency(m_lastTrackedRate);
        }

        if(fabsf(GetParameterAsFloat(RATE) - m_manualRateControl) > s_rateKnobUnlockThreshold) {
            m_restoringManualRate = true;
            SetParameterAsFloat(RATE, m_manualRateControl);
        }
    }
}

float CrusherModule::GetRateControlForFrequency(float rate) const {
    const float clampedRate = std::clamp(rate, m_rateMin, m_rateMax);
    const float rateRange = m_rateMax / m_rateMin;

    if(rateRange <= 1.0f) {
        return 0.0f;
    }

    return std::clamp(logf(clampedRate / m_rateMin) / logf(rateRange), 0.0f, 1.0f);
}

float CrusherModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if(led_id == 1) {
        return value * (GetParameterAsBool(PITCH_TRACKING) ? 1.0f : 0.0f);
    }

    return value;
}

float CrusherModule::GetSrrRate(float detectorInput) {
    const float manualRate = m_rateMin * powf(m_rateMax / m_rateMin, m_manualRateControl);

    if(!GetParameterAsBool(PITCH_TRACKING)) {
        return manualRate;
    }

    if(!m_pitchDetector.IsInitialized()) {
        // The detector is initialized in Init(); if that somehow didn't
        // happen, fall back to the manual rate rather than allocating here
        return manualRate;
    }

    const float rateControl = GetParameterAsFloat(RATE);
    const int multiplierIndex =
        std::clamp(static_cast<int>(rateControl * static_cast<float>(std::size(s_pitchTrackMultipliers))), 0, static_cast<int>(std::size(s_pitchTrackMultipliers)) - 1);
    const float trackedRate = m_pitchDetector.Process(detectorInput) * s_pitchTrackMultipliers[multiplierIndex];
    if(trackedRate > 0.0f) {
        m_lastTrackedRate = std::clamp(trackedRate, m_rateMin, m_rateMax);
        m_hasTrackedRate = true;
        return m_lastTrackedRate;
    }

    return m_rateMin * powf(m_rateMax / m_rateMin, rateControl);
}

void CrusherModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    float level = LevelGain(GetParameterAsFloat(LEVEL));
    float bits = (float)GetParameterAsBinnedValue(BITS);
    float rate = GetSrrRate(in);
    float jitter = GetParameterAsFloat(JITTER);
    float cutoff = m_cutoffMin + GetParameterAsFloat(CUTOFF) * (m_cutoffMax - m_cutoffMin);
    float mix = GetParameterAsFloat(MIX);

    m_lpFilter[0].config(cutoff, m_rateMax);
    m_bitcrusherL.setNumberOfBits(bits);
    m_bitcrusherL.setTargetSampleRate(rate);
    m_bitcrusherL.setJitter(jitter);

    float crushed = m_bitcrusherL.Process(in);
    float wet = m_lpFilter[0](crushed);
    float out = ((1.0f - mix) * in) + (mix * wet);

    m_audioRight = m_audioLeft = out * level;
}

void CrusherModule::ProcessStereo(float inL, float inR) {
    BaseEffectModule::ProcessStereo(inL, inR);

    float level = LevelGain(GetParameterAsFloat(LEVEL));
    float bits = (float)GetParameterAsBinnedValue(BITS);
    float rate = GetSrrRate(0.5f * (inL + inR));
    float jitter = GetParameterAsFloat(JITTER);
    float cutoff = m_cutoffMin + GetParameterAsFloat(CUTOFF) * (m_cutoffMax - m_cutoffMin);
    float mix = GetParameterAsFloat(MIX);

    m_lpFilter[0].config(cutoff, m_rateMax);
    m_lpFilter[1].config(cutoff, m_rateMax);

    m_bitcrusherL.setNumberOfBits(bits);
    m_bitcrusherL.setTargetSampleRate(rate);
    m_bitcrusherL.setJitter(jitter);
    m_bitcrusherR.setNumberOfBits(bits);
    m_bitcrusherR.setTargetSampleRate(rate);
    m_bitcrusherR.setJitter(jitter);

    float crushedL = m_bitcrusherL.Process(inL);
    float crushedR = m_bitcrusherR.Process(inR);
    float wetL = m_lpFilter[0](crushedL);
    float wetR = m_lpFilter[1](crushedR);

    m_audioLeft = (((1.0f - mix) * inL) + (mix * wetL)) * level;
    m_audioRight = (((1.0f - mix) * inR) + (mix * wetR)) * level;
}

// Block versions: the same arithmetic as ProcessMono/ProcessStereo (the output is identical, see tests/host/test_block_conversions.cpp). The parameters, the
// filter coefficients and the bitcrusher settings are worked out once per block instead of once per sample; so is the manual sample rate (a pow() per sample),
// except while pitch tracking is on, where the rate follows the detected pitch and is set for every sample.
void CrusherModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    const float level = LevelGain(GetParameterAsFloat(LEVEL));
    const float bits = (float)GetParameterAsBinnedValue(BITS);
    const float jitter = GetParameterAsFloat(JITTER);
    const float cutoff = m_cutoffMin + GetParameterAsFloat(CUTOFF) * (m_cutoffMax - m_cutoffMin);
    const float mix = GetParameterAsFloat(MIX);
    const bool tracking = GetParameterAsBool(PITCH_TRACKING);

    m_lpFilter[0].config(cutoff, m_rateMax);
    m_bitcrusherL.setNumberOfBits(bits);
    m_bitcrusherL.setJitter(jitter);
    if (!tracking) m_bitcrusherL.setTargetSampleRate(GetSrrRate(0.0f)); // the rate does not depend on the input

    for (size_t i = 0; i < n; i++) {
        const float x = in[i];
        if (tracking) m_bitcrusherL.setTargetSampleRate(GetSrrRate(x));
        const float crushed = m_bitcrusherL.Process(x);
        const float wet = m_lpFilter[0](crushed);
        const float out = (((1.0f - mix) * x) + (mix * wet)) * level;
        outL[i] = out;
        outR[i] = out;
    }
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void CrusherModule::ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    const float level = LevelGain(GetParameterAsFloat(LEVEL));
    const float bits = (float)GetParameterAsBinnedValue(BITS);
    const float jitter = GetParameterAsFloat(JITTER);
    const float cutoff = m_cutoffMin + GetParameterAsFloat(CUTOFF) * (m_cutoffMax - m_cutoffMin);
    const float mix = GetParameterAsFloat(MIX);
    const bool tracking = GetParameterAsBool(PITCH_TRACKING);

    m_lpFilter[0].config(cutoff, m_rateMax);
    m_lpFilter[1].config(cutoff, m_rateMax);
    m_bitcrusherL.setNumberOfBits(bits);
    m_bitcrusherL.setJitter(jitter);
    m_bitcrusherR.setNumberOfBits(bits);
    m_bitcrusherR.setJitter(jitter);
    if (!tracking) {
        const float rate = GetSrrRate(0.0f);
        m_bitcrusherL.setTargetSampleRate(rate);
        m_bitcrusherR.setTargetSampleRate(rate);
    }

    for (size_t i = 0; i < n; i++) {
        // Read both inputs before writing so in-place processing is safe
        const float l = inL[i];
        const float r = inR[i];
        if (tracking) {
            const float rate = GetSrrRate(0.5f * (l + r));
            m_bitcrusherL.setTargetSampleRate(rate);
            m_bitcrusherR.setTargetSampleRate(rate);
        }
        const float wetL = m_lpFilter[0](m_bitcrusherL.Process(l));
        const float wetR = m_lpFilter[1](m_bitcrusherR.Process(r));
        outL[i] = (((1.0f - mix) * l) + (mix * wetL)) * level;
        outR[i] = (((1.0f - mix) * r) + (mix * wetR)) * level;
    }
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}
