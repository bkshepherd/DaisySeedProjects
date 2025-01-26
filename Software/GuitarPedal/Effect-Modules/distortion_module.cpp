#include "distortion_module.h"
#include <q/fx/lowpass.hpp>

using namespace bkshepherd;

static const char *s_clippingOptions[5] = {"Hard Clip", "Soft Clip", "Fuzz", "Tube", "Multi Stage"};

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
        valueBinCount : 5,
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
    // Set the frequency for the tilt pivot for the tone control
    m_tone.SetFreq(1000.0f);
}

void DistortionModule::ParameterChanged(int parameter_id) {}

float hardClipping(float input, float threshold) {
    if (input > threshold) {
        return threshold;
    } else if (input < -threshold) {
        return -threshold;
    } else {
        return input;
    }
}

float softClipping(float input, float gain) { return std::tanh(input * gain); }

float fuzzEffect(float input, float intensity) {
    // Pre-filter: High-pass to remove low-frequency content
    static float prevInput = 0.0f;
    float highPassed = input - prevInput;
    prevInput = input;

    // Symmetrical clipping with extreme compression
    float fuzzed = std::tanh(highPassed * intensity);

    // Introduce a slight asymmetry for a classic fuzz character and adds harmonic content
    fuzzed += 0.05f * std::sin(highPassed * 20.0f);

    // Dynamic response: Adjust the intensity based on the input signal's amplitude
    float dynamicIntensity = intensity * (1.0f + 0.5f * std::abs(input));
    fuzzed = std::tanh(fuzzed * dynamicIntensity);

    // Post-filter: Low-pass to smooth out harsh high frequencies
    static float prevOutput = 0.0f;
    float alpha = 0.1f; // Adjust alpha for desired smoothing
    float smoothed = alpha * fuzzed + (1.0f - alpha) * prevOutput;
    prevOutput = smoothed;

    return smoothed;
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

// Helper functions for oversampling
std::vector<float> upsample(const std::vector<float> &input, int factor, float sample_rate) {
    std::vector<float> output(input.size() * factor, 0.0f);
    cycfi::q::one_pole_lowpass lowpass_filter(sample_rate / (2.0f * factor));

    for (size_t i = 0; i < input.size(); ++i) {
        output[i * factor] = input[i]; // Insert input samples, leaving zeros in between
    }

    // Apply the one-pole low-pass filter to smooth interpolated samples
    for (size_t i = 1; i < output.size(); ++i) {
        output[i] = lowpass_filter(output[i]);
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
    case 0: // Soft Clipping
        sample = softClipping(sample, gain);
        break;
    case 1: // Hard Clipping
        sample = hardClipping(sample, 0.9f);
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
    }
}

void DistortionModule::ProcessMono(float in) {
    const float gain = m_gainMin + (GetParameterAsFloat(1) * (m_gainMax - m_gainMin));
    const int clippingType = GetParameterAsBinnedValue(3) - 1;
    const float intensity = GetParameterAsFloat(4);

    float distorted = in;
    if (GetParameterAsBool(5)) {
        const uint8_t oversamplingFactor = 32;
        // Prepare signal for oversampling
        std::vector<float> monoInput = {distorted};
        std::vector<float> oversampledInput = upsample(monoInput, oversamplingFactor, GetSampleRate()); // 4x oversampling

        // Apply gain and distortion processing
        for (float &sample : oversampledInput) {
            processDistortion(sample, gain, clippingType, intensity);
        }

        // Downsample back to original sample rate
        const std::vector<float> downsampledOutput = downsample(oversampledInput, oversamplingFactor);
        distorted = downsampledOutput[0];
    } else {
        processDistortion(distorted, gain, clippingType, intensity);
    }

    // Normalize the volume between the types of distortion
    switch (clippingType) {
    case 0:
        distorted *= 0.8f;
        break;
    case 1:
        distorted *= 1.0f;
        break;
    case 2:
        distorted *= 2.8f;
        break;
    case 3:
        distorted *= 0.9f;
        break;
    case 4:
        distorted *= 0.3f;
        break;
    }

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
    float lp = m_tone.Process(input);

    // Compute the high-passed portion
    float hp = input - lp;

    // Crossfade: toneAmount=0 => all LP (bassier), toneAmount=1 => all HP (treblier)
    return lp * (1.f - toneAmount) + hp * toneAmount;
}

float DistortionModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    return value;
}