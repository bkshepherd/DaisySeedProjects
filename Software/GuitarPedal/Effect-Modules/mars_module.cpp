// Mars: amp sim and delay, adapted from the Mars pedal of the FunBox project (https://github.com/GuitarML/Funbox, MIT licence, Copyright (c) 2024 Keith Bloemer). See Mars/README.md
// for the licence text and the list of changes.
#include "mars_module.h"
#include "Mars/mars_ir_data.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

using namespace bkshepherd;
using namespace daisysp;

namespace {
// Two seconds of delay, and room after it for the second tap, which is up to 3/4 of the delay time further back. In SDRAM (not zeroed at startup: Init() clears it).
constexpr size_t kMaxDelay = 48000 * 2;
constexpr size_t kBufferSize = kMaxDelay + kMaxDelay * 3 / 4 + 4;
float DSY_SDRAM_BSS s_delayData[kBufferSize];
mars::DelayLine2Tap s_delay;

constexpr size_t kIrBlock = 128; // the FIR filter's block limit
// Output makeup gain for each model (the models differ by up to 10 dB in level). Calibrated with tests/host/run_level_tests.sh so that the default settings give about +3 dB
// with every model, like the other amp models.
constexpr float kModelGain[3] = {1.15f, 0.37f, 0.47f};
// ... and for each cabinet (the impulse responses are not normalised and differ by up to 7 dB), when the cabinet is on
constexpr float kCabGain[3] = {1.0f, 1.25f, 2.27f};

const char *s_modelNames[3] = {"Matchless", "Klon", "Mesa IIC"};
const char *s_irNames[3] = {"Proteus", "US Deluxe", "Vox Bright"};
const char *s_delayModeNames[3] = {"Normal", "Dotted 8th", "Triplet"};

// A computationally cheap, mostly energy-constant crossfade (https://signalsmith-audio.co.uk/writing/2021/cheap-energy-crossfade/): the gains of the wet and the dry signal
void crossfade(float x, float &wet, float &dry) {
    const float x2 = 1.0f - x;
    const float a = x * x2;
    const float b = a * (1.0f + 1.4186f * a);
    const float c = b + x;
    const float d = b + x2;
    wet = c * c;
    dry = d * d;
}
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, MarsModule::PARAM_COUNT> params{};

    params[MarsModule::GAIN] = {
        name : "Gain",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.375f}, // the input gain of the neural model: how hard it is driven, 0.1x to 2.5x (1x at the default)
        knobMapping : 0,
        midiCCMapping : 14
    };
    params[MarsModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f}, // between the amp and the delay
        knobMapping : 1,
        midiCCMapping : 15
    };
    params[MarsModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // the overall volume
        knobMapping : 2,
        midiCCMapping : 16
    };
    params[MarsModule::FILTER] = {
        name : "Filter",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8f}, // a low pass to the left and a high pass to the right, on a cube taper (as the original's knob), so the filter is open from 0.8 on
        knobMapping : 3,
        midiCCMapping : 17
    };
    params[MarsModule::DELAY_TIME] = {
        name : "Delay Time",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.35f}, // 0 is off, then 50 ms to 1 second over the first three quarters of the control and up to 2 seconds beyond
        knobMapping : 4,
        midiCCMapping : 18
    };
    params[MarsModule::DELAY_FEEDBACK] = {
        name : "Delay Feedback",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : 5,
        midiCCMapping : 19
    };
    params[MarsModule::MODEL] = { // the original's switch 1
        name : "Amp Model",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_modelNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 21
    };
    params[MarsModule::IR] = { // the original's switch 2
        name : "Cabinet",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_irNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 22
    };
    params[MarsModule::DELAY_MODE] = { // the original's switch 3
        name : "Delay Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_delayModeNames,
        defaultValue : {.uint_value = 0}, // Normal / a second repeat at a dotted eighth / a second repeat at a triplet
        knobMapping : -1,
        midiCCMapping : 23
    };
    params[MarsModule::AMP_ON] = { // the original's dip switch 1
        name : "Amp",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : -1
    };
    params[MarsModule::IR_ON] = { // the original's dip switch 2
        name : "Cabinet On",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : -1
    };
    return params;
}();

MarsModule::MarsModule() : BaseEffectModule(), m_levelAdjust(1.0f), m_modelGain(1.0f), m_cabGain(1.0f), m_delay(&s_delay), m_currentDelay(2400.0f), m_secondTap(false) {
    m_name = "Mars";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

MarsModule::~MarsModule() {}

void MarsModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_delay->Init(s_delayData, kBufferSize);
    m_lp.Init(sample_rate);
    m_hp.Init(sample_rate);
    m_bal.Init(sample_rate);
    m_currentDelay = 2400.0f;
    // ParameterChanged only fires on a change, so apply the stored (or default) selections here
    ApplyModel();
    ApplyIr();
    ApplyDelayMode();
}

void MarsModule::ApplyModel() {
    const mars::Model &model = mars::kModels[GetParameterAsBinnedValue(MODEL) - 1];
    m_gru.SetModel(&model);
    m_levelAdjust = model.levelAdjust;
    m_modelGain = kModelGain[GetParameterAsBinnedValue(MODEL) - 1];
}

void MarsModule::ApplyIr() {
    m_ir.init(mars::kIrs[GetParameterAsBinnedValue(IR) - 1], static_cast<uint32_t>(mars::kIrLength), false);
    m_cabGain = kCabGain[GetParameterAsBinnedValue(IR) - 1];
}

void MarsModule::ApplyDelayMode() {
    const int mode = GetParameterAsBinnedValue(DELAY_MODE) - 1;
    m_secondTap = mode != 0;
    if (mode == 1) m_delay->Set2ndTapFraction(0.75f);      // dotted eighth
    else if (mode == 2) m_delay->Set2ndTapFraction(0.6666667f); // triplet
}

void MarsModule::ParameterChanged(int parameter_id) {
    if (parameter_id == MODEL) ApplyModel();
    else if (parameter_id == IR) ApplyIr();
    else if (parameter_id == DELAY_MODE) ApplyDelayMode();
}

void MarsModule::ProcessSamples(const float *in, float *outL, float *outR, size_t n) {
    // The controls are read once per call
    const float gain = 0.1f + GetParameterAsFloat(GAIN) * 2.4f; // 0.1x .. 2.5x
    const float level = GetParameterAsFloat(LEVEL);
    const float filterKnob = GetParameterAsFloat(FILTER);
    const float feedback = GetParameterAsFloat(DELAY_FEEDBACK);
    const float delayKnob = GetParameterAsFloat(DELAY_TIME);
    const bool ampOn = GetParameterAsBool(AMP_ON);
    const bool irOn = GetParameterAsBool(IR_ON);
    float wetMix, dryMix;
    crossfade(GetParameterAsFloat(MIX), wetMix, dryMix);

    // The filter: a cube taper (the original's knob), low pass below the middle, high pass above
    const float vfilter = filterKnob * filterKnob * filterKnob;
    const bool lowPass = vfilter <= 0.5f;
    if (lowPass) {
        float freq = vfilter * 39800.0f + 100.0f;
        m_lp.SetFreq(freq);
    } else {
        float freq = (vfilter - 0.5f) * 800.0f + 40.0f;
        m_hp.SetFreq(freq);
    }

    // The delay: off below 1% (it only stops taking new input), then 50 ms to 1 s over 75% of the control, and 1 to 2 s over the rest
    const bool delayActive = delayKnob >= 0.01f;
    const float delayTarget = delayKnob <= 0.75f ? 2400.0f + delayKnob * 60800.0f : 48000.0f + (delayKnob - 0.75f) * 192000.0f;

    float pre[kIrBlock], post[kIrBlock];
    for (size_t start = 0; start < n; start += kIrBlock) {
        const size_t m = std::min(kIrBlock, n - start);
        for (size_t i = 0; i < m; i++) {
            const float x = in[start + i] * gain;
            // gain -> neural model (with a skip connection) -> filter -> delay -> mix -> cabinet
            float amp = x;
            if (ampOn) amp = (m_gru.Process(x) + x) * m_levelAdjust;

            float filtered = lowPass ? m_lp.Process(amp) : m_hp.Process(amp);
            float unfiltered = amp;
            const float balanced = m_bal.Process(filtered, unfiltered);

            fonepole(m_currentDelay, delayTarget, 0.0002f);
            m_delay->SetDelay(m_currentDelay);
            const float read = m_delay->Read();
            const float secondTap = m_secondTap ? m_delay->ReadSecondTap() : 0.0f;
            m_delay->Write(delayActive ? feedback * read + balanced : feedback * read); // not active: no new sound goes into the delay
            const float delayOut = read + secondTap;

            pre[i] = balanced * dryMix + delayOut * wetMix;
        }
        if (irOn) {
            m_ir.processBlock(pre, post, static_cast<uint32_t>(m));
            for (size_t i = 0; i < m; i++) post[i] *= 0.2f;
        } else {
            for (size_t i = 0; i < m; i++) post[i] = pre[i];
        }
        for (size_t i = 0; i < m; i++) outL[start + i] = outR[start + i] = post[i] * level * 0.4f * m_modelGain * (irOn ? m_cabGain : 1.0f);
    }
}

void MarsModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);
    float outL, outR;
    ProcessSamples(&m_audioLeft, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void MarsModule::ProcessStereo(float inL, float /*inR*/) { ProcessMono(inL); } // mono: the left input only, as in the original

void MarsModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    ProcessSamples(in, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void MarsModule::ProcessStereoBlock(const float *inL, const float * /*inR*/, float *outL, float *outR, size_t n) { ProcessMonoBlock(inL, outL, outR, n); }
