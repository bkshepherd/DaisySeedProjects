#include "flanger_module.h"

using namespace bkshepherd;

static const int s_paramCount = 5;
static const ParameterMetaData s_metaData[s_paramCount] = {{
                                                               name : "Mix",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.65f},
                                                               knobMapping : 0,
                                                               midiCCMapping : 20
                                                           },
                                                           {
                                                               name : "Manual",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.5f},
                                                               knobMapping : 1,
                                                               midiCCMapping : 21
                                                           },
                                                           {
                                                               name : "Rate",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.25f},
                                                               knobMapping : 2,
                                                               midiCCMapping : 22
                                                           },
                                                           {
                                                               name : "Depth",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.3f},
                                                               knobMapping : 3,
                                                               midiCCMapping : 23
                                                           },
                                                           {
                                                               name : "Feedback",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.25f},
                                                               knobMapping : 4,
                                                               midiCCMapping : 24
                                                           }};

// Default Constructor
FlangerModule::FlangerModule() : BaseEffectModule(), m_lfoFreqMin(0.05f), m_lfoFreqMax(5.0f) {
    // Set the name of the effect
    m_name = "Flanger";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
FlangerModule::~FlangerModule() {
    // No Code Needed
}

void FlangerModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_flanger.Init(sample_rate);

    // Seed smoothed params from current UI values
    const float manualNorm = GetParameterAsFloat(1);
    const float rateNorm = GetParameterAsFloat(2);
    const float depthNorm = GetParameterAsFloat(3);
    const float fbNorm = GetParameterAsFloat(4);
    const float mix = GetParameterAsFloat(0);

    m_delaySm = manualNorm;
    m_rateSm = m_lfoFreqMin + (rateNorm * rateNorm) * (m_lfoFreqMax - m_lfoFreqMin);
    m_depthSm = depthNorm;
    m_fbSm = fbNorm;
    m_mixSm = mix;

    // Push once so DSP starts from the same values
    m_flanger.SetDelay(m_delaySm);
    m_flanger.SetLfoFreq(m_rateSm);
    m_flanger.SetLfoDepth(m_depthSm);
    m_flanger.SetFeedback(m_fbSm);
}

void FlangerModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    const float x = m_audioLeft;

    // Targets from UI
    const float manualNorm = GetParameterAsFloat(1);
    const float rateNorm = GetParameterAsFloat(2);
    const float depthNorm = GetParameterAsFloat(3);
    const float fbNorm = GetParameterAsFloat(4);
    const float mix = GetParameterAsFloat(0);

    // Map and smooth
    const float rateHzTgt = m_lfoFreqMin + (rateNorm * rateNorm) * (m_lfoFreqMax - m_lfoFreqMin);

    // Alphas tuned for responsiveness vs. zipper noise
    m_delaySm = Smooth(m_delaySm, manualNorm, 0.01f); // slightly slower
    m_rateSm = Smooth(m_rateSm, rateHzTgt, 0.02f);
    m_depthSm = Smooth(m_depthSm, depthNorm, 0.02f);
    m_fbSm = Smooth(m_fbSm, fbNorm, 0.02f);
    m_mixSm = Smooth(m_mixSm, mix, 0.02f);

    // Apply to DaisySP
    m_flanger.SetDelay(m_delaySm);
    m_flanger.SetLfoFreq(m_rateSm);
    m_flanger.SetLfoDepth(m_depthSm);
    m_flanger.SetFeedback(m_fbSm);

    // Headroom
    const float pre = 0.9f;

    // DaisySP Flanger returns equal mix: y_eq = 0.5*(x + wet)
    const float y_eq = m_flanger.Process(x * pre);
    const float wet = 2.0f * y_eq - (x * pre);

    // Equal-power mix
    const float t = m_mixSm; // 0..1
    const float dryG = cosf(t * 1.57079632679f);
    const float wetG = sinf(t * 1.57079632679f);

    const float y = (x * pre) * dryG + wet * wetG;

    const float post = 0.95f;
    m_audioLeft = y * post;
    m_audioRight = m_audioLeft;
}

void FlangerModule::ProcessStereo(float inL, float inR) {
    // Treat as mono effect; feed the average
    const float monoIn = 0.5f * (inL + inR);
    ProcessMono(monoIn);
}

float FlangerModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * GetParameterAsFloat(0);
    }

    return value;
}