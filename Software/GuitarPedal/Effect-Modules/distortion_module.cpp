#include "distortion_module.h"
#include <algorithm>
#include <q/fx/biquad.hpp>

using namespace bkshepherd;

static const char *s_clippingOptions[6] = {"Hard Clip", "Soft Clip", "Fuzz", "Tube", "Multi Stage", "Diode Clip"};

constexpr float preFilterCutoffBase = 140.0f;
constexpr float preFilterCutoffMax = 300.0f;
constexpr float postFilterCutoff = 8000.0f;
cycfi::q::highpass preFilter(preFilterCutoffBase, 48000); // Dummy values that get overwritten in Init
cycfi::q::lowpass postFilter(postFilterCutoff, 48000);    // Dummy values that get overwritten in Init
cycfi::q::lowpass upsamplingLowpassFilter(0.0f, 48000);   // Dummy values that get overwritten in Init

constexpr uint8_t oversamplingFactor = 16;

static const int s_paramCount = 6;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : -1
    },
    {
        name : "Gain",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : -1,
    },
    {
        name : "Tone",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 2,
        midiCCMapping : -1,
    },
    {
        name : "Dist Type",
        valueType : ParameterValueType::Binned,
        valueBinCount : 6,
        valueBinNames : s_clippingOptions,
        defaultValue : {.uint_value = 0},
        knobMapping : 3,
        midiCCMapping : -1
    },
    {
        name : "Intensity",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 4,
        midiCCMapping : -1,
    },
    {
        name : "Oversamp",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 1},
        knobMapping : 5,
        midiCCMapping : -1
    },
};

// Default Constructor
DistortionModule::DistortionModule() : BaseEffectModule() {
    // Set the name of the effect
    m_name = "Distortion";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
DistortionModule::~DistortionModule() {
    // No Code Needed
}

void DistortionModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_tone.Init(sample_rate);

    // Pivot between 500 Hz and 2 kHz as the tone amount changes
    m_tone.SetFreq(500.0f + 1500.0f * GetParameterAsFloat(2));

    m_oversampling = GetParameterAsBool(5);
    InitializeFilters();
}

void DistortionModule::InitializeFilters() {
    preFilter.config(preFilterCutoffBase, GetSampleRate());

    if (m_oversampling) {
        postFilter.config(postFilterCutoff, GetSampleRate() * oversamplingFactor);
    } else {
        postFilter.config(postFilterCutoff, GetSampleRate());
    }

    upsamplingLowpassFilter.config(GetSampleRate() / (2.0f * static_cast<float>(oversamplingFactor)), GetSampleRate());
}

void DistortionModule::ParameterChanged(int parameter_id) {
    if (parameter_id == 5) {
        m_oversampling = GetParameterAsBool(5);
        InitializeFilters();
    } else if (parameter_id == 2) {
        // Pivot between 500 Hz and 2 kHz as the tone amount changes
        m_tone.SetFreq(500.0f + 1500.0f * GetParameterAsFloat(2));
    }
}

float hardClipping(float input, float threshold) { return std::clamp(input, -threshold, threshold); }

float diodeClipping(float input, float threshold) {
    if (input > threshold)
        return threshold - std::exp(-(input - threshold));
    else if (input < -threshold)
        return -threshold + std::exp(input + threshold);
    return input;
}

float softClipping(float input, float gain) { return std::tanh(input * gain); }

float fuzzEffect(float input, float intensity) {
    // Symmetrical clipping with extreme compression
    float fuzzed = softClipping(input, intensity);

    // Introduce a slight asymmetry for a classic fuzz character and adds harmonic content
    fuzzed += 0.05f * std::sin(input * 20.0f);

    // Dynamic response: Adjust the intensity based on the input signal's amplitude
    const float dynamicIntensity = intensity * (1.0f + 0.5f * std::abs(input));
    fuzzed = softClipping(fuzzed, dynamicIntensity);

    return fuzzed;
}

float tubeSaturation(float input, float gain) { return std::atan(input * gain); }

float multiStage(float sample, float drive, float intensity) {
    // First stage
    const float stage1 = softClipping(sample, drive * intensity * 2.0f);

    // Second stage
    const float stage2 = softClipping(stage1, drive * intensity);

    // Power amp, mimic second tube clipping, possibly negative feedback
    const float result = tubeSaturation(stage2, drive * intensity);

    return result;
}

float dynamicPreFilterCutoff(float inputEnergy) {
    return preFilterCutoffBase + (preFilterCutoffMax - preFilterCutoffBase) * std::tanh(inputEnergy);
}

// Helper functions for oversampling
std::vector<float> upsample(const std::vector<float> &input, int factor, float sample_rate) {
    std::vector<float> output(input.size() * factor, 0.0f);

    for (size_t i = 0; i < input.size(); ++i) {
        // Insert input samples, leaving zeros in between
        output[i * factor] = input[i];
    }

    // Apply the low-pass filter to smooth interpolated samples
    for (size_t i = 1; i < output.size(); ++i) {
        output[i] = upsamplingLowpassFilter(output[i]);
    }

    return output;
}

std::vector<float> downsample(const std::vector<float> &input, int factor) {
    std::vector<float> output(input.size() / factor);
    for (size_t i = 0; i < output.size(); ++i) {
        output[i] = input[i * factor]; // Take every nth sample
    }
    return output;
}

void processDistortion(float &sample,           // Sample to process
                       const float &gain,       // Gain
                       const int &clippingType, // Clipping type
                       const float &intensity)  // Intensity
{
    sample *= gain;

    switch (clippingType) {
    case 0: // Hard Clipping
        sample = hardClipping(sample, 1.0f - intensity);
        break;
    case 1: // Soft Clipping
        sample = softClipping(sample, gain);
        break;
    case 2: // Fuzz
        sample = fuzzEffect(sample, intensity * 10.0f);
        break;
    case 3: // Tube Saturation
        sample = tubeSaturation(sample, intensity * 10.0f);
        break;
    case 4: // Multi-stage
        sample = multiStage(sample, gain, intensity);
        break;
    case 5: // Diode Clipping
        sample = hardClipping(sample, 1.0f - intensity);
        break;
    }
}

void normalizeVolume(float &sample, int clippingType) {
    switch (clippingType) {
    case 0: // Hard Clipping
        sample *= 1.8f;
        break;
    case 1: // Soft Clipping
        sample *= 0.8f;
        break;
    case 2: // Fuzz
        sample *= 1.0f;
        break;
    case 3: // Tube Saturation
        sample *= 0.9f;
        break;
    case 4: // Multi-stage
        sample *= 0.5f;
        break;
    case 5: // Diode Clipping
        sample *= 1.8f;
        break;
    }
}

void DistortionModule::ProcessMono(float in) {
    float distorted = in;

    // Apply high-pass filter to remove excessive low frequencies
    const float energy = std::abs(distorted);
    preFilter.config(dynamicPreFilterCutoff(energy), GetSampleRate());
    distorted = preFilter(distorted);

    const float gain = m_gainMin + (GetParameterAsFloat(1) * (m_gainMax - m_gainMin));
    const int clippingType = GetParameterAsBinnedValue(3) - 1;
    const float intensity = GetParameterAsFloat(4);

    // Reduce signal amplitude before clipping
    distorted = distorted * 0.5f;

    if (m_oversampling) {
        // Prepare signal for oversampling
        std::vector<float> monoInput = {distorted};
        std::vector<float> oversampledInput = upsample(monoInput, oversamplingFactor, GetSampleRate());

        // Apply gain and distortion processing
        for (float &sample : oversampledInput) {
            processDistortion(sample, gain, clippingType, intensity);

            // Post-filter: Low-pass to smooth out harsh high frequencies
            sample = postFilter(sample);
        }

        // Downsample back to original sample rate
        const std::vector<float> downsampledOutput = downsample(oversampledInput, oversamplingFactor);
        distorted = downsampledOutput[0];

        // Apply gain compensation for oversampling
        distorted *= oversamplingFactor;
    } else {
        processDistortion(distorted, gain, clippingType, intensity);

        // Post-filter: Low-pass to smooth out harsh high frequencies
        distorted = postFilter(distorted);
    }

    // Normalize the volume between the types of distortion
    normalizeVolume(distorted, clippingType);

    // Apply tilt-tone filter
    const float filter_out = ProcessTiltToneControl(distorted);

    const float level = m_levelMin + (GetParameterAsFloat(0) * (m_levelMax - m_levelMin));
    m_audioLeft = filter_out * level;
    m_audioRight = m_audioLeft;
}

void DistortionModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);
}

float DistortionModule::ProcessTiltToneControl(float input) {
    const float toneAmount = GetParameterAsFloat(2);

    // Process input with one-pole low-pass
    const float lp = m_tone.Process(input);

    // Compute the high-passed portion
    const float hp = input - lp;

    // Crossfade: toneAmount=0 => all LP (more bass), toneAmount=1 => all HP (more treble)
    return lp * (1.f - toneAmount) + hp * toneAmount;
}

float DistortionModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    return value;
}