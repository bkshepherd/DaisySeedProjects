#include "nam_module.h"
#include "../Util/audio_utilities.h"
#include "../dependencies/NeuralAmpModelerCore/NAM/activations.h"
#include "../dependencies/NeuralAmpModelerCore/NAM/dsp.h"
#include "../dependencies/nam-binary-loader/namb/get_dsp_namb.h"
#include "Nam/1_namb.h" // Embedded .namb model as C array
#include <memory>
#include <q/fx/biquad.hpp>

using namespace bkshepherd;

constexpr uint8_t NUM_FILTERS_NAM = 3;

const float minGain = -10.f;
const float maxGain = 10.f;

const float centerFrequencyNam[NUM_FILTERS_NAM] = {110.f, 900.f, 4000.f}; // Experiment with these freqs and q values
const float q_nam[NUM_FILTERS_NAM] = {.7f, .7f, .7f};

cycfi::q::peaking filter_nam[NUM_FILTERS_NAM] = {{0, centerFrequencyNam[0], 48000, q_nam[0]},
                                                 {0, centerFrequencyNam[1], 48000, q_nam[1]},
                                                 {0, centerFrequencyNam[2], 48000, q_nam[2]}};

// This must match the length of the model_collection_nam array in model_data_nam.h
const size_t k_numModels = 10;

static const char *s_modelBinNames[k_numModels] = {
    "Mesa", "Match30", "DumHighG", "DumLowG", "Ethos", "Splawn", "PRSArch", "JCM800", "SansAmp", "BE-100",
};

static const int s_paramCount = 8;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {
        name : "Gain",
        valueType : ParameterValueType::Float,
        valueCurve : ParameterValueCurve::Log,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : 14,
    },
    {name : "Level", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 1, midiCCMapping : 15},
    {
        name : "Model",
        valueType : ParameterValueType::Binned,
        valueBinCount : k_numModels,
        valueBinNames : s_modelBinNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 2,
        midiCCMapping : 16
    },
    {
        name : "Bass",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 3,
        midiCCMapping : 17,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    },
    {
        name : "Mid",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 4,
        midiCCMapping : 18,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    },
    {
        name : "Treble",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 5,
        midiCCMapping : 19,
        minValue : static_cast<int>(minGain),
        maxValue : static_cast<int>(maxGain)
    },
    {
        name : "NeuralModel",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : 20
    },
    {
        name : "EQ",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : 21
    },

};

// NeuralAmpModelerCore model instance
static std::unique_ptr<nam::DSP> nam_model;
static constexpr size_t kMaxBlockSize = 48; // adjust as needed

// NOTES:
// nano models:
//   Seems to run (verify sound) for Samplerate 32kHz, Blocksize 64, 1 sample at a time
//   Freezes at Samplerate 48kHz, Blocksize 64, 1 sample at a time
//   Runs at samplerate 32kHz, Blocksize 48, 1 sample, verify sound

// Default Constructor
NamModule::NamModule()
    : BaseEffectModule(), m_gainMin(0.0f), m_gainMax(2.0f), m_levelMin(0.0f), m_levelMax(2.0f), m_cachedEffectMagnitudeValue(1.0f) {
    // Set the name of the effect
    m_name = "NAM";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
NamModule::~NamModule() {
    // No Code Needed
}

void NamModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    SelectModel();

    filter_nam[0].config(GetParameterAsFloat(3), centerFrequencyNam[0], sample_rate, q_nam[0]);
    filter_nam[1].config(GetParameterAsFloat(4), centerFrequencyNam[1], sample_rate, q_nam[1]);
    filter_nam[2].config(GetParameterAsFloat(5), centerFrequencyNam[2], sample_rate, q_nam[2]);
}

void NamModule::ParameterChanged(int parameter_id) {
    if (parameter_id == 2) { // Change Model
        SelectModel();
    } else if (parameter_id == 3) {
        filter_nam[0].config(GetParameterAsFloat(3), centerFrequencyNam[0], GetSampleRate(), q_nam[0]);
    } else if (parameter_id == 4) {
        filter_nam[1].config(GetParameterAsFloat(4), centerFrequencyNam[1], GetSampleRate(), q_nam[1]);
    } else if (parameter_id == 5) {
        filter_nam[2].config(GetParameterAsFloat(5), centerFrequencyNam[2], GetSampleRate(), q_nam[2]);
    }
}

void NamModule::SelectModel() {
    const int modelIndex = GetParameterAsBinnedValue(2) - 1;

    if (m_currentModelindex != modelIndex) {
        // Temporarily disable output as we switch models
        m_muteOutput = true;
        // Directly load the embedded .namb model from 1_namb.h
        nam_model = nam::get_dsp_namb(__1_namb, __1_namb_len);
        if (nam_model) {
            nam_model->ResetAndPrewarm(GetSampleRate(), kMaxBlockSize);
        }
        m_currentModelindex = modelIndex;
        // Re-enable output
        m_muteOutput = false;
    }
}

void NamModule::ProcessMono(float in) {
    // When switching models, we stop processing temporarily and mute the output
    if (m_muteOutput) {
        m_audioLeft = m_audioRight = 0.0f;
        return;
    }

    BaseEffectModule::ProcessMono(in);

    float ampOut;
    float input_arr[1] = {0.0}; // Neural Net Input
    input_arr[0] = m_audioLeft * (m_gainMin + (m_gainMax - m_gainMin) * GetParameterAsFloat(0));

    if (GetParameterAsBool(6) && nam_model) {
        float *input_ptr = input_arr;
        float *output_ptr = &ampOut;
        nam_model->process(&input_ptr, &output_ptr, 1); // process 1 sample
    } else {
        ampOut = input_arr[0];
    }

    // Apply 3 band EQ
    if (GetParameterAsBool(7)) {
        for (uint8_t i = 0; i < NUM_FILTERS_NAM; i++) {
            ampOut = filter_nam[i](ampOut);
        }
    }

    const float level = m_levelMin + (GetParameterAsFloat(1) * (m_levelMax - m_levelMin));

    m_audioRight = m_audioLeft = ampOut * level;
}

void NamModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);

    // NOTE: Running the Neural Nets in stereo is currently not feasible due to processing limitations, this will remain a MONO ONLY
    // effect for now.
    //       The left channel output is copied to the right output, but the right input is ignored in this effect module.

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    // BaseEffectModule::ProcessStereo(m_audioLeft, inR);

    // m_audioRight = m_audioLeft;

    // Use the same magnitude as already calculated for the Left Audio
    // m_audioRight = m_audioRight * m_cachedEffectMagnitudeValue;
}

float NamModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
