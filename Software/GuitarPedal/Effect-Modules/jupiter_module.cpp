// Jupiter: stereo reverb, adapted from the Jupiter pedal of the FunBox project (https://github.com/GuitarML/Funbox, MIT licence, Copyright (c) 2024 Keith Bloemer),
// which is built on the CloudSeed reverb (https://github.com/ValdemarOrn/CloudSeed, MIT). See Jupiter/README.md for the licences and the list of changes.
#include "jupiter_module.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;

namespace {
// Output makeup gain. Calibrated with tests/host/run_level_tests.sh so that the default settings give about +2 dB, like the other reverbs.
constexpr float kOutputGain = 1.33f;
constexpr int kApplyIntervalSamples = 48;

const char *s_lushNames[3] = {"Low", "Medium", "High"};
const char *s_modNames[3] = {"Low", "Medium", "High"};
const char *s_preDelayNames[3] = {"Off", "100 ms", "200 ms"};
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, JupiterModule::PARAM_COUNT> params{};

    params[JupiterModule::DECAY] = {
        name : "Decay",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // about 2 seconds, as at noon on the pedal
        knobMapping : 0,
        midiCCMapping : 14
    };
    params[JupiterModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f},
        knobMapping : 1,
        midiCCMapping : 15
    };
    params[JupiterModule::LOW_GAIN] = {
        name : "Low Gain",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.7f}, // low shelf cut: fully cut at 0, no cut at 1; a mild cut by default so Low Freq is audible
        knobMapping : 2,
        midiCCMapping : 16
    };
    params[JupiterModule::LOW_FREQ] = {
        name : "Low Freq",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // 20 Hz .. 1 kHz, about 200 Hz in the middle
        knobMapping : 3,
        midiCCMapping : 17
    };
    params[JupiterModule::HIGH_GAIN] = {
        name : "High Gain",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // high shelf cut: strongly cut at 0, no cut at 1; a mild cut by default so High Freq is audible
        knobMapping : 4,
        midiCCMapping : 18
    };
    params[JupiterModule::HIGH_FREQ] = {
        name : "High Freq",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // 400 Hz .. 20 kHz, about 4.3 kHz in the middle
        knobMapping : 5,
        midiCCMapping : 19
    };
    params[JupiterModule::LUSH] = {
        name : "Lush",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_lushNames,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : 20
    };
    params[JupiterModule::MODULATION] = {
        name : "Modulation",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_modNames,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : 21
    };
    params[JupiterModule::PRE_DELAY] = {
        name : "PreDelay",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_preDelayNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 22
    };
    params[JupiterModule::STEREO_IN] = { // off = mono in (the left input feeds both sides), on = true stereo
        name : "Stereo In",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 23
    };
    return params;
}();

JupiterModule::JupiterModule() : BaseEffectModule(), m_reverb(nullptr), m_pending(0), m_sinceApply(0), m_wetMix(0.5f), m_dryMix(0.5f) {
    m_name = "Jupiter";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

JupiterModule::~JupiterModule() {
    // The reverb's delay lines are in the shared pool, which cannot be freed; the module lives for the whole run.
}

void JupiterModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    AudioLib::ValueTables::Init();
    CloudSeed::FastSin::Init();

    m_reverb = new CloudSeed::ReverbController(sample_rate);
    m_reverb->ClearBuffers();
    m_reverb->initFactoryRubiKaFields();
    // The starting point is the original Jupiter's version of the RubiKa Fields preset, which differs from this repository's in the output stage: both post-reverb
    // shelves on (they are the Low/High Freq/Gain controls; this repository's preset has them off) and the cutoff filter off (it has it on). The shelves would do
    // nothing otherwise.
    m_reverb->SetParameter(::Parameter2::LowShelfEnabled, 1.0f);
    m_reverb->SetParameter(::Parameter2::HighShelfEnabled, 1.0f);
    m_reverb->SetParameter(::Parameter2::CutoffEnabled, 0.0f);
    m_reverb->SetParameter(::Parameter2::MainOut, 0.95f);

    // ParameterChanged only fires on a change, so push every control's default (or stored value) here
    for (int p = 0; p < PARAM_COUNT; p++) {
        ApplyParameter(p);
    }
    CalculateMix();
    m_pending = 0;
}

void JupiterModule::ParameterChanged(int parameter_id) {
    if (parameter_id == MIX) {
        CalculateMix(); // cheap, and the dry/wet balance should follow the knob at once
    } else if (parameter_id != STEREO_IN) {
        m_pending.fetch_or(1u << parameter_id);
    }
}

void JupiterModule::ApplyParameter(int parameter_id) {
    if (m_reverb == nullptr) return;
    switch (parameter_id) {
    case DECAY:
        m_reverb->SetParameter(::Parameter2::LineDecay, GetParameterAsFloat(DECAY) * 0.65f + 0.35f); // 0.35 .. 1.0
        break;
    case LOW_FREQ:
        m_reverb->SetParameter(::Parameter2::PostLowShelfFrequency, GetParameterAsFloat(LOW_FREQ));
        break;
    case HIGH_FREQ:
        m_reverb->SetParameter(::Parameter2::PostHighShelfFrequency, GetParameterAsFloat(HIGH_FREQ));
        break;
    case LOW_GAIN:
        m_reverb->SetParameter(::Parameter2::PostLowShelfGain, GetParameterAsFloat(LOW_GAIN));
        break;
    case HIGH_GAIN:
        m_reverb->SetParameter(::Parameter2::PostHighShelfGain, GetParameterAsFloat(HIGH_GAIN) * 0.7f + 0.3f);
        break;
    case LUSH: {
        // The original's three "lushness" settings
        const int mode = GetParameterAsBinnedValue(LUSH) - 1;
        if (mode == 0) {
            m_reverb->SetParameter(::Parameter2::LateDiffusionEnabled, 0.0f);
            m_reverb->SetParameter(::Parameter2::LateDiffusionStages, 0.4f);
            m_reverb->SetParameter(::Parameter2::LateStageTap, 0.0f);
            m_reverb->SetParameter(::Parameter2::Interpolation, 0.0f);
            m_reverb->SetParameter(::Parameter2::LineCount, 1);
        } else if (mode == 2) {
            m_reverb->SetParameter(::Parameter2::LateDiffusionEnabled, 1.0f);
            m_reverb->SetParameter(::Parameter2::LateDiffusionStages, 1.0f);
            m_reverb->SetParameter(::Parameter2::LateStageTap, 1.0f);
            m_reverb->SetParameter(::Parameter2::Interpolation, 1.0f);
            m_reverb->SetParameter(::Parameter2::LineCount, 2);
        } else {
            m_reverb->SetParameter(::Parameter2::LateDiffusionEnabled, 1.0f);
            m_reverb->SetParameter(::Parameter2::LateDiffusionStages, 0.2f);
            m_reverb->SetParameter(::Parameter2::LateStageTap, 1.0f);
            m_reverb->SetParameter(::Parameter2::Interpolation, 0.0f);
            m_reverb->SetParameter(::Parameter2::LineCount, 2);
        }
        break;
    }
    case MODULATION: {
        const int mode = GetParameterAsBinnedValue(MODULATION) - 1;
        m_reverb->SetParameter(::Parameter2::LineModAmount, mode == 0 ? 0.1f : (mode == 2 ? 0.9f : 0.5f));
        m_reverb->SetParameter(::Parameter2::LineModRate, mode == 0 ? 0.1f : (mode == 2 ? 0.6f : 0.3f));
        break;
    }
    case PRE_DELAY: {
        const int mode = GetParameterAsBinnedValue(PRE_DELAY) - 1;
        m_reverb->SetParameter(::Parameter2::PreDelay, mode == 0 ? 0.0f : (mode == 2 ? 0.2f : 0.1f)); // 0 / 100 ms / 200 ms
        break;
    }
    default:
        break;
    }
}

// Called from the audio path: at most one control per call, so a burst of knob movement is spread over many blocks instead of landing in one
void JupiterModule::ApplyPending(size_t n) {
    // at most once per 48 samples (the pedal's block size), however the samples are divided into calls
    if (m_sinceApply != 0) {
        m_sinceApply += (int)n;
        if (m_sinceApply >= kApplyIntervalSamples) m_sinceApply = 0;
        return;
    }
    m_sinceApply = (int)n >= kApplyIntervalSamples ? 0 : (int)n;
    const uint32_t pending = m_pending.load();
    if (pending == 0) return;
    const int bit = __builtin_ctz(pending);
    m_pending.fetch_and(~(1u << bit));
    ApplyParameter(bit);
}

void JupiterModule::CalculateMix() {
    // A computationally cheap, mostly energy-constant crossfade (https://signalsmith-audio.co.uk/writing/2021/cheap-energy-crossfade/), as in the original
    const float mix = GetParameterAsFloat(MIX);
    const float x2 = 1.0f - mix;
    const float a = mix * x2;
    const float b = a * (1.0f + 1.4186f * a);
    const float c = b + mix;
    const float d = b + x2;
    m_wetMix = c * c;
    m_dryMix = d * d;
}

void JupiterModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);
    ApplyPending(1);
    float outL, outR;
    ProcessBlockInternal(&m_audioLeft, &m_audioLeft, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void JupiterModule::ProcessStereo(float inL, float inR) {
    BaseEffectModule::ProcessStereo(inL, inR);
    ApplyPending(1);
    const float left = m_audioLeft;
    const float right = GetParameterAsBool(STEREO_IN) ? m_audioRight : m_audioLeft;
    float outL, outR;
    ProcessBlockInternal(&left, &right, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void JupiterModule::ProcessBlockInternal(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    static float wetL[CloudSeed::ReverbController::MaxBlockSize];
    static float wetR[CloudSeed::ReverbController::MaxBlockSize];

    // The engine copies its inputs before writing its outputs, but its API takes non-const pointers
    m_reverb->Process(const_cast<float *>(inL), const_cast<float *>(inR), wetL, wetR, static_cast<int>(n));

    for (size_t i = 0; i < n; i++) {
        const float l = (inL[i] * m_dryMix + wetL[i] * m_wetMix) * kOutputGain;
        const float r = (inR[i] * m_dryMix + wetR[i] * m_wetMix) * kOutputGain;
        outL[i] = l;
        outR[i] = r;
    }
}

void JupiterModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    constexpr size_t kMax = CloudSeed::ReverbController::MaxBlockSize;
    ApplyPending(n);
    for (size_t start = 0; start < n; start += kMax) {
        const size_t len = (n - start < kMax) ? (n - start) : kMax;
        ProcessBlockInternal(in + start, in + start, outL + start, outR + start, len);
    }
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void JupiterModule::ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    constexpr size_t kMax = CloudSeed::ReverbController::MaxBlockSize;
    ApplyPending(n);
    // With Stereo In off, the right input is a copy of the left (the original's MISO mode)
    const float *right = GetParameterAsBool(STEREO_IN) ? inR : inL;
    for (size_t start = 0; start < n; start += kMax) {
        const size_t len = (n - start < kMax) ? (n - start) : kMax;
        ProcessBlockInternal(inL + start, right + start, outL + start, outR + start, len);
    }
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}
