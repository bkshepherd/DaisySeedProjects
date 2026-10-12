#include "ir_module.h"
#include "../Util/audio_utilities.h"
#include "ImpulseResponse/ir_data.h"
#include <array>

using namespace bkshepherd;

static const char *s_irNames_large[2] = {"Rhythm", "Lead"};

static const auto s_metaData = [] {
    std::array<ParameterMetaData, IrModule::PARAM_COUNT> params{};

    params[IrModule::IR] = {
        name : "IR",
        valueType : ParameterValueType::Binned,
        valueBinCount : 2,
        valueBinNames : s_irNames_large,
        defaultValue : {.uint_value = 0},
        knobMapping : 0,
        midiCCMapping : 14
    };

    params[IrModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : 15
    };

    return params;
}();

// Default Constructor
IrModule::IrModule() : BaseEffectModule(), m_levelMin(0.0f), m_levelMax(2.0f), m_cachedEffectMagnitudeValue(1.0f) {
    // Set the name of the effect
    m_name = "IR";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
IrModule::~IrModule() {
    // No Code Needed
}

void IrModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    std::fill(std::begin(m_inputBuffer), std::end(m_inputBuffer), 0.0f);
    std::fill(std::begin(m_outputBuffer), std::end(m_outputBuffer), 0.0f);

    m_bufferIndex = 0;
    m_currentIRindex = -1;

    SelectIR();
}

void IrModule::ParameterChanged(int parameter_id) {
    if (parameter_id == IR) { // Change IR
        SelectIR();
    } else if (parameter_id == LEVEL) { // Level
        // Nothing to cache for now; level is read in ProcessMono().
    }
}

void IrModule::SelectIR() {
    const int binValue = GetParameterAsBinnedValue(IR);
    const int irIndex = binValue - 1;

    if (irIndex < 0 || irIndex >= static_cast<int>(ir_collection.size())) {
        return;
    }

    if (irIndex == m_currentIRindex) {
        return;
    }

    // Avoid stale convolution history from the previous cab.
    std::fill(std::begin(m_inputBuffer), std::end(m_inputBuffer), 0.0f);
    std::fill(std::begin(m_outputBuffer), std::end(m_outputBuffer), 0.0f);
    m_bufferIndex = 0;

    mIR.init(ir_collection[irIndex].data(), static_cast<uint32_t>(ir_collection[irIndex].size()), true);

    m_currentIRindex = irIndex;
}

void IrModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    const float level = m_levelMin + (GetParameterAsFloat(LEVEL) * (m_levelMax - m_levelMin));

    // Read the already-processed sample for this frame.
    // This creates one kBlockSize block of latency
    const float processed = m_outputBuffer[m_bufferIndex];

    // Queue the current input sample for the next IR block.
    //
    // 0.5 is for loudness normalization with the IR
    // Level is applied only at output to avoid double-scaling.
    m_inputBuffer[m_bufferIndex] = m_audioLeft * 0.5f;

    ++m_bufferIndex;

    if (m_bufferIndex >= kBlockSize) {
        mIR.processBlock(m_inputBuffer, m_outputBuffer, kBlockSize);
        m_bufferIndex = 0;
    }

    m_audioLeft = processed * level;
    m_audioRight = m_audioLeft;

    m_cachedEffectMagnitudeValue = fabsf(m_audioLeft);
}

void IrModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);
}

// Block version. The FIR keeps its own history, so when no samples are staged from the per-sample path the incoming block can be
// convolved directly. That skips the staging copies and also removes the one block of latency the per-sample path has.
// Otherwise (unaligned or oversized blocks) fall back to the per-sample path so the staged samples are never orphaned.
void IrModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    if (m_bufferIndex != 0 || n > static_cast<size_t>(kBlockSize)) {
        BaseEffectModule::ProcessMonoBlock(in, outL, outR, n);
        return;
    }

    const float level = m_levelMin + (GetParameterAsFloat(LEVEL) * (m_levelMax - m_levelMin));

    // 0.5 is for loudness normalization with the IR
    for (size_t i = 0; i < n; i++) {
        m_inputBuffer[i] = in[i] * 0.5f;
    }

    // Write straight into the caller's buffer so the staged per-sample state (m_outputBuffer) is left untouched.
    mIR.processBlock(m_inputBuffer, outL, static_cast<uint32_t>(n));

    for (size_t i = 0; i < n; i++) {
        outL[i] *= level;
        outR[i] = outL[i];
    }

    m_audioLeft = m_audioRight = outL[n - 1];
    m_cachedEffectMagnitudeValue = fabsf(m_audioLeft);
}

void IrModule::ProcessStereoBlock(const float *inL, const float * /*inR*/, float *outL, float *outR, size_t n) {
    ProcessMonoBlock(inL, outL, outR, n);
}

float IrModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
