#include "nam_a2_module.h"
#include "../Util/audio_utilities.h"
#include "ImpulseResponse/ir_data.h"
#include "Nam/model_data_nam_a2.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <q/fx/biquad.hpp>

using namespace bkshepherd;

constexpr int IR_LENGTH = 1024;

// ---------------------------------------------------------------------------
// EQ configuration – identical band centres to the existing NAM module
// ---------------------------------------------------------------------------
constexpr uint8_t NUM_FILTERS_A2 = 3;

const float centerFrequencyA2[NUM_FILTERS_A2] = {110.f, 900.f, 4000.f};
const float q_a2[NUM_FILTERS_A2] = {0.7f, 0.7f, 0.7f};

// Constructed with (gain_dB, frequency, sample_rate, q).
// The sample-rate value here is a reasonable default; Init() calls .config()
// with the true sample rate before any audio is processed.
cycfi::q::peaking filter_a2[NUM_FILTERS_A2] = {
    {0.f, centerFrequencyA2[0], 48000.f, q_a2[0]},
    {0.f, centerFrequencyA2[1], 48000.f, q_a2[1]},
    {0.f, centerFrequencyA2[2], 48000.f, q_a2[2]},
};

// ---------------------------------------------------------------------------
// Parameter metadata
// ---------------------------------------------------------------------------
// Display names for the MODEL parameter, sourced directly from the model table
// so the two can't drift out of sync. Add models in model_data_nam_a2.h only.
static auto s_modelBinNames = [] {
    std::array<const char *, nam_a2_models::kNamA2ModelCount> names{};
    for (int i = 0; i < nam_a2_models::kNamA2ModelCount; ++i) {
        names[i] = nam_a2_models::kNamA2Models[i].name;
    }
    return names;
}();

static const char *s_irCabNames[3] = {"Off", "Rhythm", "Lead"};

static const auto s_metaData = [] {
    std::array<ParameterMetaData, NamA2Module::PARAM_COUNT> params{};

    params[NamA2Module::GAIN] = {
        name : "Gain",
        valueType : ParameterValueType::Float,
        valueCurve : ParameterValueCurve::Log,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : 22,
    };

    params[NamA2Module::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : 23,
    };

    params[NamA2Module::MODEL] = {
        name : "Model",
        valueType : ParameterValueType::Binned,
        valueBinCount : nam_a2_models::kNamA2ModelCount,
        valueBinNames : s_modelBinNames.data(),
        defaultValue : {.uint_value = 0},
        knobMapping : 2,
        midiCCMapping : 28,
    };

    params[NamA2Module::BASS] = {
        name : "Bass",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 3,
        midiCCMapping : 24,
        minValue : -10,
        maxValue : 10,
    };

    params[NamA2Module::MID] = {
        name : "Mid",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 4,
        midiCCMapping : 25,
        minValue : -10,
        maxValue : 10,
    };

    params[NamA2Module::TREBLE] = {
        name : "Treble",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 5,
        midiCCMapping : 26,
        minValue : -10,
        maxValue : 10,
    };

    params[NamA2Module::EQ] = {
        name : "EQ",
        valueType : ParameterValueType::Bool,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : 27,
    };

    params[NamA2Module::IR_CAB] = {
        name : "IR Cab",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_irCabNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 29,
    };

    return params;
}();

// A2Player's only per-instance member is A2State, whose history buffer is ~76 KB.
// NAM_A2_STATE_DATA places this instance in the on-chip, cacheable RAM_D2 SRAM
// (.sram_d2_bss, see nam_a2_sections.lds) instead of the cramped 128 KB DTCMRAM
// (and instead of the AXI-SRAM, which is mostly full of program code). The hot
// weights and work buffers are static members already pinned to DTCMRAM
// (NAM_A2_HOT_DATA), so only the large, latency-tolerant history moves out.
NAM_A2_STATE_DATA static nam_a2_daisy::A2Player s_nam_a2_model;

// The A2 model requires exactly kBlockSize (48) samples per process_block_48 call.
// This project's hardware block size (guitar_pedal.cpp: blockSize = 48) matches
// exactly, so ProcessMono accumulates 48 samples and fires once per audio callback,
// with one block (~1 ms) of latency. If the hardware block size is ever changed,
// the double-buffer in ProcessMono still works correctly but latency will change:
//   - hardware block < 48: fires every ceil(48/hwBlock) callbacks
//   - hardware block > 48: fires multiple times per callback
// Either way is glitch-free, just adjust expectations for latency.
static_assert(nam_a2_daisy::kBlockSize == 48, "A2 kBlockSize changed — verify hardware blockSize in guitar_pedal.cpp still matches");

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
NamA2Module::NamA2Module()
    : BaseEffectModule(), m_bufferIndex(0), m_gainMin(0.0f), m_gainMax(2.0f), m_levelMin(0.0f), m_levelMax(2.0f), m_gain(1.0f),
      m_level(1.0f), m_eqEnabled(true), m_irEnabled(false), m_cachedEffectMagnitudeValue(1.0f), m_currentModelIndex(-1),
      m_currentModelGain(1.0f), m_currentIRindex(-1), m_muteOutput(false), m_model(&s_nam_a2_model) {
    m_name = "NAM";

    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));

    // Zero both buffers so the first kBlockSize output samples are silence
    // (one block of startup latency ≈ 1 ms at 48 kHz).
    for (int i = 0; i < kBlockSize; ++i) {
        m_inputBuffer[i] = 0.0f;
        m_outputBuffer[i] = 0.0f;
        m_irOutputBuffer[i] = 0.0f;
    }
}

NamA2Module::~NamA2Module() {}

// ---------------------------------------------------------------------------
// SelectIR
// ---------------------------------------------------------------------------
void NamA2Module::SelectIR() {
    int binValue = GetParameterAsBinnedValue(IR_CAB);
    if (binValue <= 1) {
        // Off
        m_irEnabled = false;
        m_currentIRindex = -1;
        return;
    }
    const int irIndex = binValue - 2; // 0-based index into ir_collection
    if (irIndex < 0 || irIndex >= static_cast<int>(ir_collection.size())) {
        return;
    }
    if (irIndex != m_currentIRindex) {
        // This runs on the main loop and the audio callback can preempt it,
        // so keep the IR disabled while the coefficients/state are rewritten.
        m_irEnabled = false;
        mIR.setImpulseResponse(ir_collection[irIndex].data(), IR_LENGTH, true);
        m_currentIRindex = irIndex;
    }
    m_irEnabled = true;
}

// ---------------------------------------------------------------------------
// SelectModel
// ---------------------------------------------------------------------------
void NamA2Module::SelectModel() {
    const int modelIndex = GetParameterAsBinnedValue(MODEL) - 1;
    if (modelIndex == m_currentModelIndex)
        return;
    if (modelIndex < 0 || modelIndex >= nam_a2_models::kNamA2ModelCount)
        return;

    m_muteOutput = true;

    const auto &entry = nam_a2_models::kNamA2Models[modelIndex];
    m_model->load_weights(entry.weights, nam_a2_daisy::kA2WeightCount);
    m_currentModelIndex = modelIndex;
    // Loudness match across models: raw A2 weight exports drop NAM's loudness
    // metadata, so per-model level varies. outputGain (1.0f = unity) rescales.
    m_currentModelGain = entry.outputGain;

    // Flush the double-buffer so the previous model's samples don't bleed
    // through on the first block after switching.
    m_bufferIndex = 0;
    for (int i = 0; i < kBlockSize; ++i) {
        m_inputBuffer[i] = 0.0f;
        m_outputBuffer[i] = 0.0f;
    }

    m_muteOutput = false;
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
void NamA2Module::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    // Prime the cached gain/level/EQ values (ParameterChanged only fires on change).
    UpdateCachedParameters();

    // Load the default model and prewarm — must NOT be called from the audio callback.
    SelectModel();

    // Pre-load a valid IR so the FIR instance is never uninitialized if the
    // audio callback observes m_irEnabled mid-switch (default IR is Off, so
    // nothing else guarantees setImpulseResponse ran before first enable).
    mIR.init(ir_collection[0].data(), IR_LENGTH, true);
    m_currentIRindex = 0;

    // Select the default IR (may be Off).
    SelectIR();

    // Configure EQ filters with the actual sample rate.
    filter_a2[0].config(GetParameterAsFloat(BASS), centerFrequencyA2[0], sample_rate, q_a2[0]);
    filter_a2[1].config(GetParameterAsFloat(MID), centerFrequencyA2[1], sample_rate, q_a2[1]);
    filter_a2[2].config(GetParameterAsFloat(TREBLE), centerFrequencyA2[2], sample_rate, q_a2[2]);
}

// ---------------------------------------------------------------------------
// UpdateCachedParameters
// ---------------------------------------------------------------------------
void NamA2Module::UpdateCachedParameters() {
    m_gain = m_gainMin + (m_gainMax - m_gainMin) * GetParameterAsFloat(GAIN);
    m_level = m_levelMin + GetParameterAsFloat(LEVEL) * (m_levelMax - m_levelMin);
    m_eqEnabled = GetParameterAsBool(EQ);
}

// ---------------------------------------------------------------------------
// ParameterChanged
// ---------------------------------------------------------------------------
void NamA2Module::ParameterChanged(int parameter_id) {
    if (parameter_id == MODEL) {
        SelectModel();
    } else if (parameter_id == GAIN || parameter_id == LEVEL || parameter_id == EQ) {
        UpdateCachedParameters();
    } else if (parameter_id == IR_CAB) {
        SelectIR();
    } else if (parameter_id == BASS) {
        filter_a2[0].config(GetParameterAsFloat(BASS), centerFrequencyA2[0], GetSampleRate(), q_a2[0]);
    } else if (parameter_id == MID) {
        filter_a2[1].config(GetParameterAsFloat(MID), centerFrequencyA2[1], GetSampleRate(), q_a2[1]);
    } else if (parameter_id == TREBLE) {
        filter_a2[2].config(GetParameterAsFloat(TREBLE), centerFrequencyA2[2], GetSampleRate(), q_a2[2]);
    }
}

// ---------------------------------------------------------------------------
// ProcessMono
//
// The A2 model requires exactly kBlockSize (48) samples per call.
// We use a double-buffer approach with one block of latency (~1 ms at 48 kHz):
//
//   1. Output m_outputBuffer[n]  – samples processed in the previous call
//   2. Store gainedInput into m_inputBuffer[n]
//   3. When the input buffer is full, call process_block_48 to refill
//      m_outputBuffer for the next kBlockSize output samples.
// ---------------------------------------------------------------------------
void NamA2Module::ProcessMono(float in) {
    if (m_muteOutput) {
        m_audioLeft = m_audioRight = 0.0f;
        return;
    }

    BaseEffectModule::ProcessMono(in);

    m_inputBuffer[m_bufferIndex] = m_audioLeft * m_gain;

    // Read from the previously-processed output block, applying the per-model
    // loudness-match gain and IR level compensation.
    const float irScale = m_irEnabled ? 0.5f : 1.0f;
    float ampOut = m_irOutputBuffer[m_bufferIndex] * m_currentModelGain * irScale;

    ++m_bufferIndex;
    if (m_bufferIndex >= kBlockSize) {
        m_model->process_block_48(m_inputBuffer, m_outputBuffer);

        if (m_irEnabled) {
            mIR.processBlock(m_outputBuffer, m_irOutputBuffer, kBlockSize);
        } else {
            std::copy(m_outputBuffer, m_outputBuffer + kBlockSize, m_irOutputBuffer);
        }

        m_bufferIndex = 0;
    }

    // Apply 3-band EQ post-model.
    if (m_eqEnabled) {
        for (uint8_t i = 0; i < NUM_FILTERS_A2; ++i) {
            ampOut = filter_a2[i](ampOut);
        }
    }

    m_audioLeft = m_audioRight = ampOut * m_level;

    m_cachedEffectMagnitudeValue = fminf(1.0f, fabsf(m_audioLeft));
}

// ---------------------------------------------------------------------------
// ProcessStereo
//
// Running the neural network in stereo is not feasible within CPU constraints;
// we process mono and copy the result to both channels.
// ---------------------------------------------------------------------------
void NamA2Module::ProcessStereo(float inL, float /*inR*/) { ProcessMono(inL); }

// ---------------------------------------------------------------------------
// GetBrightnessForLED
// ---------------------------------------------------------------------------
float NamA2Module::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
