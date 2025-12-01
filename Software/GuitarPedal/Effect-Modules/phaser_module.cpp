#include "phaser_module.h"
#include <math.h>

using namespace bkshepherd;

static const int s_paramCount = 4;
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
        defaultValue : {.float_value = 0.25f},
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

// Exponential mapper for Rate
static inline float MapExp(float norm, float minV, float maxV, float curve = 3.0f) {
    float x = powf(norm, curve);
    return minV + x * (maxV - minV);
}

// Equal-power dry/wet gains to prevent level bumps
static inline void EqualPowerMix(float mix, float &dryGain, float &wetGain) {
    const float t = mix;                // 0..1
    dryGain = cosf(t * 1.57079632679f); // pi/2
    wetGain = sinf(t * 1.57079632679f);
}

void PhaserModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    // Init SimplePhaser
    m_phaser.init(sample_rate);
    m_phaser.set_range(300.0f, 1500.0f); // guitar‑friendly sweep
    m_phaser.set_depth(0.95f);
    m_phaser.set_feedback(0.20f);

    m_smoothedRate = 1.0f;
    m_smoothedDepth = 0.95f;

    // Push Rate/Depth/Feedback from UI once
    for (int i = 1; i <= 3; ++i)
        ParameterChanged(i);
}

void PhaserModule::ParameterChanged(int parameter_id) {
    switch (parameter_id) {
    case 1: { // Rate: 0..1 -> 0.10–8 Hz (exp mapping favors slow)
        float rate = MapExp(GetParameterAsFloat(1), 0.10f, 8.0f);
        m_targetRate = rate;
        break;
    }
    case 2: { // Depth
        m_targetDepth = fminf(fmaxf(GetParameterAsFloat(2), 0.0f), 0.98f);
        m_phaser.set_depth(m_targetDepth);
        break;
    }
    case 3: { // Feedback
        float fb = GetParameterAsFloat(3) * 0.45f;
        m_phaser.set_feedback(fb);
        break;
    }
    default:
        break;
    }
}

void PhaserModule::ProcessMono(float in) {
    // Smooth live-changed params
    m_smoothedRate = SmoothParam(m_smoothedRate, m_targetRate);
    m_smoothedDepth = SmoothParam(m_smoothedDepth, m_targetDepth);
    m_phaser.set_lfo_freq(m_smoothedRate);
    m_phaser.set_depth(m_smoothedDepth);

    // Small pre-trim for headroom (dry+wet can boost peaks)
    const float pre = 0.85f;
    float x = in * pre;

    // SimplePhaser returns 0.5*(x + wet). Recover wet = 2*out - x.
    float sp_out = m_phaser.process(x);
    float wet = 2.0f * sp_out - x;

    float dryGain, wetGain;
    EqualPowerMix(GetParameterAsFloat(0), dryGain, wetGain);

    const float post = 0.95f;
    const float out = (x * dryGain + wet * wetGain) * post;

    // --- Audio output ---
    m_audioLeft = out;
    m_audioRight = out;

    // --- LED envelope (rectify + smooth) ---
    float mag = fabsf(out); // 0..~1
    // simple one-pole low-pass for LED env
    const float ledAlpha = 0.01f; // adjust for LED "speed"
    m_ledEnv += ledAlpha * (mag - m_ledEnv);
}

void PhaserModule::ProcessStereo(float inL, float inR) {
    const float monoIn = 0.5f * (inL + inR);
    ProcessMono(monoIn);
}

float PhaserModule::GetBrightnessForLED(int led_id) const {
    float base = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        // Clamp env just to be safe
        float env = m_ledEnv;
        if (env < 0.0f)
            env = 0.0f;
        if (env > 1.0f)
            env = 1.0f;

        // Add a minimum so LED never fully goes dark
        float shaped = 0.2f + 0.8f * env; // 0.2 .. 1.0

        return base * shaped;
    }

    return base;
}
