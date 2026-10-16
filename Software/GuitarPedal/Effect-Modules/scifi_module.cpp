#include "scifi_module.h"
#include "../Util/audio_utilities.h"
#include <array>

#include <q/fx/biquad.hpp>
#include <q/support/literals.hpp>

// #include "../Util/EffectState.h"
#include "../Util/Multirate.h"
#include "../Util/OctaveGenerator.h"

using namespace bkshepherd;

namespace q = cycfi::q;
using namespace q::literals;

// naming everything _scifi so it wont conflict with other polyoctave module
static Decimator2 decimate_scifi;
static Interpolator interpolate_scifi;
static const auto sample_rate_temp =
    48000; // hard code for now                          // NOTE: the sample_rate must be divisible by the resample_factor (48/6 = 8)
static OctaveGenerator octave_scifi(sample_rate_temp / resample_factor); // resample_factor is defined in Multirate.h and equals 6
static q::highshelf eq1_scifi(-11, 140_Hz, sample_rate_temp);
static q::lowshelf eq2_scifi(5, 160_Hz, sample_rate_temp);

ReverbSc DSY_SDRAM_BSS reverbStereo_scifi;

static const auto s_metaData = [] {
    std::array<ParameterMetaData, SciFiModule::PARAM_COUNT> params{};

    params[SciFiModule::DRY] = {
        name : "Dry",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : 14,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[SciFiModule::OCT_DOWN] = {
        name : "OctDown",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : 15,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[SciFiModule::OCT_UP] = {
        name : "OctUp",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 2,
        midiCCMapping : 16,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[SciFiModule::TIME] = {
        name : "Time",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8f},
        knobMapping : 3,
        midiCCMapping : 17
    };

    params[SciFiModule::DRIVE] = {
        name : "Drive",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 4,
        midiCCMapping : 18
    };

    params[SciFiModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 5,
        midiCCMapping : 19,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[SciFiModule::DAMP] = {
        name : "Damp",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : -1,
        midiCCMapping : 20,
        unit : " Hz",
        decimals : 0,
        displayFn : [](float v) { return 500.0f + (1.0f - v) * (1.0f - v) * (16000.0f - 500.0f); }
    };

    params[SciFiModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 21,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[SciFiModule::OD_MIX] = {
        name : "OD Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 1.0f},
        knobMapping : -1,
        midiCCMapping : 22,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    return params;
}();

// Default Constructor
SciFiModule::SciFiModule()
    : BaseEffectModule(), m_driveMin(0.4f), m_driveMax(0.6f), m_levelMin(0.0f), m_levelMax(1.0f), m_timeMin(0.6f), m_timeMax(1.0f),
      m_lpFreqMin(500.0f), m_lpFreqMax(16000.0f), m_cachedEffectMagnitudeValue(1.0f) {
    // Set the name of the effect
    m_name = "SciFi";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
SciFiModule::~SciFiModule() {
    // No Code Needed
}

void SciFiModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    // Initialize buffers to 0
    for (int j = 0; j < 6; ++j) {
        buff[j] = 0.0;
        buff_out[j] = 0.0;
    }

    m_reverbStereo = &reverbStereo_scifi;
    m_reverbStereo->Init(sample_rate);

    m_overdriveLeft.Init();
    m_overdriveRight.Init();
}

void SciFiModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    float outL, outR;
    ProcessSamples(&in, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void SciFiModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    ProcessSamples(in, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

// SciFi is a mono-in, stereo-out effect: the right input is ignored
void SciFiModule::ProcessStereoBlock(const float *inL, const float * /*inR*/, float *outL, float *outR, size_t n) {
    ProcessMonoBlock(inL, outL, outR, n);
}

void SciFiModule::ProcessSamples(const float *in, float *outL, float *outR, size_t n) {
    // Everything that only depends on the parameters is computed once per call instead of once per sample. The expressions (and so
    // their float/double promotions) are kept exactly as they were in the per-sample code, so results are bit-identical.
    const float dryLevel = GetParameterAsFloat(DRY);
    const float down1Level = GetParameterAsFloat(OCT_DOWN); // Setting 2 oct down and 1 oct down with one parameter
    const float down2Level = GetParameterAsFloat(OCT_DOWN); // Setting 2 oct down and 1 oct down with one parameter
    const float up1Level = GetParameterAsFloat(OCT_UP);

    m_reverbStereo->SetFeedback(m_timeMin + GetParameterAsFloat(TIME) * (m_timeMax - m_timeMin));
    float invertedFreq =
        1.0 - GetParameterAsFloat(DAMP); // Invert the damping param so that knob left is less dampening, knob right is more dampening
    invertedFreq = invertedFreq * invertedFreq; // also square it for exponential taper (more control over lower frequencies)
    m_reverbStereo->SetLpFreq(m_lpFreqMin + invertedFreq * (m_lpFreqMax - m_lpFreqMin));

    const float drive_setting = m_driveMin + (GetParameterAsFloat(DRIVE) * (m_driveMax - m_driveMin));
    m_overdriveLeft.SetDrive(drive_setting);
    m_overdriveRight.SetDrive(drive_setting);
    // reduce volume as od drive goes up (otherwise way too loud)
    const double odVolume = (1.0 - (drive_setting * drive_setting * 2.8 - 0.1296));

    const float odMix = GetParameterAsFloat(OD_MIX);
    const float mix = GetParameterAsFloat(MIX);
    const float level = GetParameterAsFloat(LEVEL);

    for (size_t i = 0; i < n; i++) {
        const float input = in[i];
        // making a workaround for only processing sample by sample instead of block, will add 6 samples of latency
        buff[bin_counter] = input;

        // Process PolyOctave //////////////////////////////

        // do calculation every 6 samples
        if (bin_counter > 4) {

            std::span<const float, resample_factor> in_chunk(&(buff[0]), resample_factor); // std::span is c++ 20 feature

            const auto sample = decimate_scifi(in_chunk);

            float octave_mix = 0;
            octave_scifi.update(sample);
            octave_mix += up1Level * octave_scifi.up1() * 2.0;
            octave_mix += down1Level * octave_scifi.down1() * 2.0;
            octave_mix += down2Level * octave_scifi.down2() * 2.0;

            auto out_chunk = interpolate_scifi(octave_mix);
            for (size_t j = 0; j < out_chunk.size(); ++j) {
                float mixed = eq2_scifi(eq1_scifi(out_chunk[j]));

                mixed += dryLevel * buff[j];

                buff_out[j] = mixed;
            }
        }

        // Increments the buffer index from 0 to 5 (workaround to adapt code)
        bin_counter += 1;
        if (bin_counter > 5)
            bin_counter = 0;

        //////////////////////////////////////////////////////////////
        // Calculate the Reverb
        float sendl, sendr, wetl, wetr; // Reverb Inputs/Outputs

        sendl = sendr = buff_out[bin_counter];

        m_reverbStereo->Process(sendl, sendr, &wetl, &wetr);

        //////////////////////////////////////////////////////////////

        // Overdrive the reverb output
        float od_out_left = m_overdriveLeft.Process(wetl * 0.8) * odVolume;
        float od_out_right = m_overdriveRight.Process(wetr * 0.8) * odVolume;

        // Mix regular reverb with overdriven reverb (default is full overdrive)
        float overdrive_mix_left = od_out_left * odMix * 0.6 + wetl * (1.0 - odMix);
        float overdrive_mix_right = od_out_right * odMix * 0.6 + wetr * (1.0 - odMix);

        // Mix in the dry signal and set overall level
        outL[i] = (overdrive_mix_left * mix + input * (1.0 - mix)) * level;
        outR[i] = (overdrive_mix_right * mix + input * (1.0 - mix)) * level;
    }
}

void SciFiModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect (Scifi is currently a MISO Mono in stereo out effect)
    ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    // BaseEffectModule::ProcessStereo(m_audioLeft, inR);  // Mono only for now

    // Use the same magnitude as already calculated for the Left Audio
    // m_audioRight = m_audioRight * m_cachedEffectMagnitudeValue;
}

float SciFiModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
