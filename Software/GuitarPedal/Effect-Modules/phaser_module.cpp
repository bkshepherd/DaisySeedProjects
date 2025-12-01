#include "phaser_module.h"

using namespace bkshepherd;

static const int s_paramCount = 6;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {
        // 0 Mix (dry/wet)
        name : "Mix",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : -1
    },
    {
        // 1 Rate
        name : "Rate",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.25f}, // ~2 Hz after mapping
        knobMapping : 1,
        midiCCMapping : -1
    },
    {
        // 2 Depth
        name : "Depth",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 1.0f},
        knobMapping : 2,
        midiCCMapping : -1
    },
    {
        // 3 Feedback
        name : "Feedback",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.25f},
        knobMapping : 3,
        midiCCMapping : -1
    },
    {
        // 4 Center
        name : "Center",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.35f}, // maps ≈ 400–600 Hz region
        knobMapping : 4,
        midiCCMapping : -1
    },
    {
        // 5 Stages (4/6/8)
        name : "Stages",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : (const char *[]){"4", "6", "8"},
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : -1
    },
};

// Default Constructor
PhaserModule::PhaserModule() : BaseEffectModule() {
    // Set the name of the effect
    m_name = "Phaser";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
PhaserModule::~PhaserModule() {
    // No Code Needed
}

// Simple one‑pole smoother for rate & depth to avoid zipper noise.
inline float SmoothParam(float current, float target) {
    const float alpha = 0.02f; // adjust for responsiveness
    return current + alpha * (target - current);
}

void PhaserModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_phaserL.Init(sample_rate);
    m_phaserR.Init(sample_rate);
    m_phaserL.SetPoles(4);
    m_phaserR.SetPoles(4);
    m_smoothedRate = 1.0f;
    m_smoothedDepth = 1.0f;
    for (int i = 1; i < 6; ++i)
        ParameterChanged(i);
}

// Map helper
static float MapExp(float norm, float minV, float maxV, float curve = 3.0f) {
    // Emphasize low end of rate control
    float x = powf(norm, curve);
    return minV + x * (maxV - minV);
}

void PhaserModule::ParameterChanged(int parameter_id) {
    switch (parameter_id) {
    case 1: { // Rate
        // 0..1 -> 0.10–8 Hz (exp)
        float rate = MapExp(GetParameterAsFloat(1), 0.10f, 8.0f);
        m_targetRate = rate;
        break;
    }
    case 2: {                                   // Depth
        m_targetDepth = GetParameterAsFloat(2); // 0..1
        break;
    }
    case 3: {                                     // Feedback
        float fb = GetParameterAsFloat(3) * 0.6f; // cap at 0.6
        m_phaserL.SetFeedback(fb);
        m_phaserR.SetFeedback(-fb * 0.9f); // slight inversion for stereo spread
        break;
    }
    case 4: { // Center
        // 0..1 -> 300–1200 Hz (mild span typical guitar)
        float cf = 300.0f + GetParameterAsFloat(4) * (1200.0f - 300.0f);
        m_phaserL.SetFreq(cf);
        m_phaserR.SetFreq(cf * 1.07f); // tiny offset
        break;
    }
    case 5: { // Stages
        uint32_t idx = GetParameterAsBinnedValue(5);
        int poles = (idx == 0 ? 4 : (idx == 1 ? 6 : 8));
        m_phaserL.SetPoles(poles);
        m_phaserR.SetPoles(poles);
        break;
    }
    default:
        break;
    }
}

void PhaserModule::ProcessMono(float in) {
    // Smooth dynamic params
    m_smoothedRate = SmoothParam(m_smoothedRate, m_targetRate);
    m_smoothedDepth = SmoothParam(m_smoothedDepth, m_targetDepth);
    m_phaserL.SetLfoFreq(m_smoothedRate);
    m_phaserR.SetLfoFreq(m_smoothedRate);
    m_phaserL.SetLfoDepth(m_smoothedDepth);
    m_phaserR.SetLfoDepth(m_smoothedDepth * 0.95f);

    float wetL = m_phaserL.Process(in);
    float wetR = m_phaserR.Process(in);

    float mix = GetParameterAsFloat(0); // 0..1
    // Preserve approximate level
    float dryGain = 1.0f - mix;
    float wetGain = mix;

    float outL = in * dryGain + wetL * wetGain;
    float outR = in * dryGain + wetR * wetGain;

    m_audioLeft = outL;
    m_audioRight = outR;
}

void PhaserModule::ProcessStereo(float inL, float inR) {
    // Process channels independently for better stereo motion
    m_smoothedRate = SmoothParam(m_smoothedRate, m_targetRate);
    m_smoothedDepth = SmoothParam(m_smoothedDepth, m_targetDepth);
    m_phaserL.SetLfoFreq(m_smoothedRate);
    m_phaserR.SetLfoFreq(m_smoothedRate);
    m_phaserL.SetLfoDepth(m_smoothedDepth);
    m_phaserR.SetLfoDepth(m_smoothedDepth * 0.95f);

    float wetL = m_phaserL.Process(inL);
    float wetR = m_phaserR.Process(inR);

    float mix = GetParameterAsFloat(0);
    float outL = inL * (1.0f - mix) + wetL * mix;
    float outR = inR * (1.0f - mix) + wetR * mix;

    m_audioLeft = outL;
    m_audioRight = outR;
}

float PhaserModule::GetBrightnessForLED(int led_id) const {
    float v = BaseEffectModule::GetBrightnessForLED(led_id);
    if (led_id == 1) {
        float depthNorm = GetParameterAsFloat(2);
        float rateNorm = GetParameterAsFloat(1);
        return v * (0.3f + 0.7f * depthNorm) * (0.4f + 0.6f * rateNorm);
    }
    return v;
}