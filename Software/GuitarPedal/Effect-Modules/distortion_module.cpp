#include "distortion_module.h"
#include <algorithm>
#include <q/fx/biquad.hpp>
#include <array>

using namespace bkshepherd;

static const char *s_clippingOptions[6] = {"Hard Clip", "Soft Clip", "Fuzz", "Tube", "Multi Stage", "Diode Clip"};

constexpr float preFilterCutoffBase = 140.0f;
constexpr float preFilterCutoffMax = 300.0f;
constexpr float postFilterCutoff = 8000.0f;

constexpr uint8_t oversamplingFactor = 16;
// The pre-filter's cutoff follows the input level, but recalculating its coefficients (a sine and a cosine) for every sample is expensive: it is done
// every kPreFilterUpdateInterval samples (6 kHz at 48 kHz, well above how fast the level of a note moves).
constexpr uint8_t kPreFilterUpdateInterval = 8;

static const auto s_metaData = [] {
    std::array<ParameterMetaData, DistortionModule::PARAM_COUNT> params{};

    params[DistortionModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 1,
        displayFn : [](float v) { return ParamDisplay::Db(v); }
    };

    params[DistortionModule::GAIN] = {
        name : "Gain",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : -1,
        unit : " dB",
        decimals : 1,
        displayFn : [](float v) { return ParamDisplay::Db(ParamDisplay::Lin(v, 1.0f, 20.0f)); }
    };

    params[DistortionModule::TONE] = {
        name : "Tone",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 2,
        midiCCMapping : -1,
    };

    params[DistortionModule::DIST_TYPE] = {
        name : "Dist Type",
        valueType : ParameterValueType::Binned,
        valueBinCount : 6,
        valueBinNames : s_clippingOptions,
        defaultValue : {.uint_value = 0},
        knobMapping : 3,
        midiCCMapping : -1
    };

    params[DistortionModule::INTENSITY] = {
        name : "Intensity",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 4,
        midiCCMapping : -1,
    };

    params[DistortionModule::OVERSAMP] = {
        name : "Oversamp",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 1},
        knobMapping : 5,
        midiCCMapping : -1
    };

    return params;
}();

// Default Constructor
DistortionModule::DistortionModule() : BaseEffectModule() {
    // Set the name of the effect
    m_name = "Distortion";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
DistortionModule::~DistortionModule() {
    // No Code Needed
}

void DistortionModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_tone.Init(sample_rate);

    // Pivot between 500 Hz and 2 kHz as the tone amount changes
    m_tone.SetFreq(500.0f + 1500.0f * GetParameterAsFloat(TONE));

    m_oversampling = GetParameterAsBool(OVERSAMP);
    m_preFilterCountdown = 0;
    InitializeFilters();
}

void DistortionModule::InitializeFilters() {
    m_preFilter.config(preFilterCutoffBase, GetSampleRate());

    if (m_oversampling) {
        m_postFilter.config(postFilterCutoff, GetSampleRate() * oversamplingFactor);
    } else {
        m_postFilter.config(postFilterCutoff, GetSampleRate());
    }

    m_upsamplingLowpassFilter.config(GetSampleRate() / (2.0f * static_cast<float>(oversamplingFactor)), GetSampleRate());
}

void DistortionModule::ParameterChanged(int parameter_id) {
    if (parameter_id == OVERSAMP) {
        m_oversampling = GetParameterAsBool(OVERSAMP);
        InitializeFilters();
    } else if (parameter_id == TONE) {
        // Pivot between 500 Hz and 2 kHz as the tone amount changes
        m_tone.SetFreq(500.0f + 1500.0f * GetParameterAsFloat(TONE));
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

// Per-type output compensation applied after clipping. Tuned with tests/host/run_level_tests.sh so that every distortion type gives
// about +3 dB RMS gain at default settings for a -37 dBFS peak sine (what the pedal's level readout shows for normal playing): about +6 dB.
void normalizeVolume(float &sample, int clippingType) {
    switch (clippingType) {
    case 0: // Hard Clipping
        sample *= 1.709f;
        break;
    case 1: // Soft Clipping
        sample *= 0.1836f;
        break;
    case 2: // Fuzz
        sample *= 0.1061f;
        break;
    case 3: // Tube Saturation
        sample *= 0.3531f;
        break;
    case 4: // Multi-stage
        sample *= 0.0647f;
        break;
    case 5: // Diode Clipping
        sample *= 1.709f;
        break;
    }
}

void DistortionModule::ProcessMono(float in) {
    float distorted = in;

    // Apply high-pass filter to remove excessive low frequencies
    if (m_preFilterCountdown == 0) {
        m_preFilter.config(dynamicPreFilterCutoff(std::abs(distorted)), GetSampleRate());
        m_preFilterCountdown = kPreFilterUpdateInterval;
    }
    --m_preFilterCountdown;
    distorted = m_preFilter(distorted);

    const float gain = m_gainMin + (GetParameterAsFloat(GAIN) * (m_gainMax - m_gainMin));
    const int clippingType = GetParameterAsBinnedValue(DIST_TYPE) - 1;
    const float intensity = GetParameterAsFloat(INTENSITY);

    // Reduce signal amplitude before clipping
    distorted = distorted * 0.5f;

    if (m_oversampling) {
        // Zero-stuff to the oversampled rate, smooth the inserted zeros with
        // the upsampling low-pass, distort and post-filter each oversampled
        // sample, then decimate by taking the first sample. Uses a fixed
        // stack buffer; this runs per audio sample so it must not allocate.
        float oversampled[oversamplingFactor];
        oversampled[0] = distorted;
        for (int i = 1; i < oversamplingFactor; ++i) {
            oversampled[i] = m_upsamplingLowpassFilter(0.0f);
        }

        // Apply gain and distortion processing
        for (float &sample : oversampled) {
            processDistortion(sample, gain, clippingType, intensity);

            // Post-filter: Low-pass to smooth out harsh high frequencies
            sample = m_postFilter(sample);
        }

        // Downsample back to original sample rate (decimate: every
        // oversamplingFactor-th sample, i.e. index 0 of this frame)
        distorted = oversampled[0];

        // Apply gain compensation for oversampling
        distorted *= oversamplingFactor;
    } else {
        processDistortion(distorted, gain, clippingType, intensity);

        // Post-filter: Low-pass to smooth out harsh high frequencies
        distorted = m_postFilter(distorted);
    }

    // Normalize the volume between the types of distortion
    normalizeVolume(distorted, clippingType);

    // Apply tilt-tone filter
    const float filter_out = ProcessTiltToneControl(distorted);

    const float level = m_levelMin + (GetParameterAsFloat(LEVEL) * (m_levelMax - m_levelMin));
    m_audioLeft = filter_out * level;
    m_audioRight = m_audioLeft;
}

void DistortionModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);
}

// Block versions. The same arithmetic as ProcessMono (the output is identical, see tests/host/test_distortion_block.cpp), but the parameters are read once
// per block, and the clipping type and the oversampling switch are chosen once per block instead of for every oversampled sample (16 per sample).
namespace {
template <int TYPE>
inline float clipSample(float sample, float gain, float intensity) { // processDistortion() for one type
    sample *= gain;
    if constexpr (TYPE == 0 || TYPE == 5) {
        sample = hardClipping(sample, 1.0f - intensity);
    } else if constexpr (TYPE == 1) {
        sample = softClipping(sample, gain);
    } else if constexpr (TYPE == 2) {
        sample = fuzzEffect(sample, intensity * 10.0f);
    } else if constexpr (TYPE == 3) {
        sample = tubeSaturation(sample, intensity * 10.0f);
    } else {
        sample = multiStage(sample, gain, intensity);
    }
    return sample;
}

template <int TYPE>
constexpr float volumeCompensation() { // normalizeVolume() for one type
    if constexpr (TYPE == 0 || TYPE == 5) return 1.709f;
    else if constexpr (TYPE == 1) return 0.1836f;
    else if constexpr (TYPE == 2) return 0.1061f;
    else if constexpr (TYPE == 3) return 0.3531f;
    else return 0.0647f;
}

struct DistortionFilters {
    cycfi::q::highpass &pre;
    cycfi::q::lowpass &post;
    cycfi::q::lowpass &upsampling;
};

template <int TYPE, bool OVERSAMPLE>
void distortBlock(DistortionFilters f, Tone &tone, uint8_t &preFilterCountdown, const float *in, float *out, size_t n, float sampleRate, float gain, float intensity, float toneAmount,
                  float level) {
    for (size_t i = 0; i < n; i++) {
        float distorted = in[i];

        // The pre-filter's cutoff follows the signal's energy (updated every few samples, as in ProcessMono)
        if (preFilterCountdown == 0) {
            f.pre.config(dynamicPreFilterCutoff(std::abs(distorted)), sampleRate);
            preFilterCountdown = kPreFilterUpdateInterval;
        }
        --preFilterCountdown;
        distorted = f.pre(distorted);

        // Reduce signal amplitude before clipping
        distorted = distorted * 0.5f;

        if constexpr (OVERSAMPLE) {
            float oversampled[oversamplingFactor];
            oversampled[0] = distorted;
            for (int k = 1; k < oversamplingFactor; ++k) {
                oversampled[k] = f.upsampling(0.0f);
            }
            for (float &sample : oversampled) {
                sample = clipSample<TYPE>(sample, gain, intensity);
                sample = f.post(sample);
            }
            distorted = oversampled[0];
            distorted *= oversamplingFactor;
        } else {
            distorted = clipSample<TYPE>(distorted, gain, intensity);
            distorted = f.post(distorted);
        }

        distorted *= volumeCompensation<TYPE>();

        // The tilt tone control
        const float lp = tone.Process(distorted);
        const float hp = distorted - lp;
        const float filter_out = lp * (1.f - toneAmount) + hp * toneAmount;

        out[i] = filter_out * level;
    }
}
} // namespace

void DistortionModule::ProcessBlockInternal(const float *in, float *out, size_t n) {
    const float gain = m_gainMin + (GetParameterAsFloat(GAIN) * (m_gainMax - m_gainMin));
    const int clippingType = GetParameterAsBinnedValue(DIST_TYPE) - 1;
    const float intensity = GetParameterAsFloat(INTENSITY);
    const float toneAmount = GetParameterAsFloat(TONE);
    const float level = m_levelMin + (GetParameterAsFloat(LEVEL) * (m_levelMax - m_levelMin));
    const float sampleRate = GetSampleRate();
    const bool oversampling = m_oversampling;
    const DistortionFilters filters{m_preFilter, m_postFilter, m_upsamplingLowpassFilter};

#define DISTORTION_BLOCK_CASE(T)                                                                                                                       \
    case T:                                                                                                                                            \
        if (oversampling) {                                                                                                                            \
            distortBlock<T, true>(filters, m_tone, m_preFilterCountdown, in, out, n, sampleRate, gain, intensity, toneAmount, level);                                                \
        } else {                                                                                                                                       \
            distortBlock<T, false>(filters, m_tone, m_preFilterCountdown, in, out, n, sampleRate, gain, intensity, toneAmount, level);                                               \
        }                                                                                                                                              \
        break;
    switch (clippingType) {
        DISTORTION_BLOCK_CASE(0)
        DISTORTION_BLOCK_CASE(1)
        DISTORTION_BLOCK_CASE(2)
        DISTORTION_BLOCK_CASE(3)
        DISTORTION_BLOCK_CASE(4)
        DISTORTION_BLOCK_CASE(5)
    default: // not reachable: the type is a choice of six
        distortBlock<0, true>(filters, m_tone, m_preFilterCountdown, in, out, n, sampleRate, gain, intensity, toneAmount, level);
        break;
    }
#undef DISTORTION_BLOCK_CASE
}

void DistortionModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    ProcessBlockInternal(in, outL, n);
    for (size_t i = 0; i < n; i++) outR[i] = outL[i];
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

// As ProcessStereo: the effect is mono, the left input is processed and the result goes to both outputs
void DistortionModule::ProcessStereoBlock(const float *inL, const float * /*inR*/, float *outL, float *outR, size_t n) { ProcessMonoBlock(inL, outL, outR, n); }

float DistortionModule::ProcessTiltToneControl(float input) {
    const float toneAmount = GetParameterAsFloat(TONE);

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