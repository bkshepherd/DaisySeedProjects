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

static const auto s_metaData = [] {
    std::array<ParameterMetaData, CrusherModule::PARAM_COUNT> params{};

    params[CrusherModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 1.0f},
        knobMapping : 0,
        midiCCMapping : 14
    }; // Output level (linear gain from silent to unity).

    params[CrusherModule::BITS] = {
        name : "Bits",
        valueType : ParameterValueType::Binned,
        valueBinCount : 32,
        defaultValue : {.uint_value = 32},
        knobMapping : 1,
        midiCCMapping : 15
    }; // Bit depth for quantization (lower bits = more digital distortion).

    params[CrusherModule::RATE] = {
        name : "Rate",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 1.0f},
        knobMapping : 2,
        midiCCMapping : 16
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
        midiCCMapping : 17
    }; // Random variation of the SRR hold interval (adds instability/grit to the alias texture).

    params[CrusherModule::CUTOFF] = {
        name : "Cutoff",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 1.0f},
        knobMapping : 4,
        midiCCMapping : 18
    }; // Post-crusher low-pass cutoff to tame high-frequency aliasing.

    params[CrusherModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 1.0f},
        knobMapping : 5,
        midiCCMapping : 19
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
        // Defer the heavy q pitch-detector allocation until the feature is
        // actually enabled.
        m_pitchDetector.Init(m_rateMax);
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

    float level = GetParameterAsFloat(LEVEL);
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

    float level = GetParameterAsFloat(LEVEL);
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
