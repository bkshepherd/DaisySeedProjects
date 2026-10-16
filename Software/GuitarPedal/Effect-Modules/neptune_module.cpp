// Neptune: stereo reverb and delay, adapted from the Neptune pedal of the FunBox project (https://github.com/GuitarML/Funbox, MIT licence, Copyright (c) 2024 Keith Bloemer),
// which is built on the CloudSeed reverb (https://github.com/ValdemarOrn/CloudSeed, MIT) and has a reverse delay from Veno-Echo (https://github.com/AdamFulford/Veno-Echo,
// MIT). See Neptune/README.md for the licences and the list of changes.
#include "neptune_module.h"
#include "Neptune/delayline_oct.h"
#include "Neptune/delayline_reverse.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;
using namespace daisysp;

namespace {
// Output makeup gain. Calibrated with tests/host/run_level_tests.sh so that the default settings give about +2 dB, like the other effects.
constexpr float kOutputGain = 1.38f;
constexpr int kApplyIntervalSamples = 48;

constexpr size_t kMaxDelay = 48000 * 4;     // 4 seconds at 48 kHz
constexpr size_t kMaxDelayRev = 48000 * 8;  // the reverse delay needs twice as much
constexpr float kDelayMinSeconds = 0.05f;   // 2400 samples
constexpr float kDelaySpanSeconds = 3.95f;  // up to 4 seconds

// The delay line samples are in SDRAM (the objects, with their bookkeeping, are in the fast RAM; see delayline_oct.h)
float DSY_SDRAM_BSS s_octData[2][kMaxDelay];
float DSY_SDRAM_BSS s_revData[2][kMaxDelayRev];

struct DelayChannel {
    DelayLineOct<float, kMaxDelay> oct;
    DelayLineReverse<float, kMaxDelayRev> rev;
    Tone lowpass; // 3 kHz low-pass on the delayed signal: tames the octave mode, and fades the repeats of the other modes (one per channel; the original shared one)
    float currentDelay;
    float delayTarget;
    float feedback;
    bool active; // when off (Delay Time at the bottom) nothing new is written, so the repeats die out
    bool reverseMode;

    void Init(float sampleRate, float *octSamples, float *revSamples) {
        oct.Init(octSamples);
        rev.Init(revSamples);
        lowpass.Init(sampleRate);
        lowpass.SetFreq(3000.0f);
        currentDelay = delayTarget = 2400.0f;
        feedback = 0.0f;
        active = true;
        reverseMode = false;
    }

    float Process(float in) {
        fonepole(currentDelay, delayTarget, 0.0002f); // glide to the target delay
        oct.SetDelay(std::max(currentDelay, 1.0f));
        rev.SetDelay1(currentDelay);
        const float forward = oct.Read();
        const float backward = reverseMode ? rev.ReadRev() : 0.0f; // (the original reads it all the time; only the reverse mode needs it)
        const float read = lowpass.Process(forward);
        // Writing the filtered read to both lines allows the octave and the reverse to be combined
        const float write = feedback * read + (active ? in : 0.0f);
        oct.Write(write);
        rev.Write(write);
        return reverseMode ? backward : read;
    }
};
DelayChannel s_delay[2];

const char *s_reverbNames[3] = {"Chorus", "MedSpace", "RubiKa"};
const char *s_routingNames[3] = {"Send", "Parallel", "Series"};
const char *s_delayModeNames[3] = {"Normal", "Octave", "Reverse"};

// A computationally cheap, mostly energy-constant crossfade (https://signalsmith-audio.co.uk/writing/2021/cheap-energy-crossfade/): the gains of the two sides
void crossfade(float x, float &gainOfX, float &gainOfRest) {
    const float x2 = 1.0f - x;
    const float a = x * x2;
    const float b = a * (1.0f + 1.4186f * a);
    const float c = b + x;
    const float d = b + x2;
    gainOfX = c * c;
    gainOfRest = d * d;
}
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, NeptuneModule::PARAM_COUNT> params{};

    params[NeptuneModule::DECAY] = {
        name : "Decay",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : 14,
        unit : " s",
        decimals : 2,
        // CloudSeed LineDecay (T60 of the delay lines): 0.05 + Response3Dec(v) * 59.95 s
        displayFn : [](float v) { return 0.05f + (powf(1000.0f, v) - 1.0f) / 999.0f * 59.95f; }
    };
    params[NeptuneModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f}, // dry / effect
        knobMapping : 1,
        midiCCMapping : 15,
        unit : " %",
        decimals : 0,
        // crossfade position
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };
    params[NeptuneModule::DELAY_TIME] = {
        name : "Dly Time",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f}, // 50 ms .. 4 s; the delay is off at the bottom of the range
        knobMapping : 2,
        midiCCMapping : 16,
        unit : " s",
        decimals : 2,
        // as in UpdateDelays (the delay is off at the bottom of the range)
        displayFn : [](float v) { return kDelayMinSeconds + kDelaySpanSeconds * v; }
    };
    params[NeptuneModule::MOD_AMT] = {
        name : "Mod",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f}, // amount of modulation of the reverb
        knobMapping : 3,
        midiCCMapping : 17,
        unit : " ms",
        decimals : 2,
        // CloudSeed LineModAmount: v * 2.5 ms
        displayFn : [](float v) { return v * 2.5f; }
    };
    params[NeptuneModule::LOWPASS] = {
        name : "LowPass",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8f}, // low-pass on the reverb, cubed (as the original's knob taper)
        knobMapping : 4,
        midiCCMapping : 18,
        unit : " Hz",
        decimals : 0,
        // CloudSeed PostCutoffFrequency of v^3: 400 + Response4Oct(v^3) * 19600 Hz
        displayFn : [](float v) { return 400.0f + (powf(16.0f, v * v * v) - 1.0f) / 15.0f * 19600.0f; }
    };
    params[NeptuneModule::DELAY_FDBK] = {
        name : "Dly Fdbk",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : 5,
        midiCCMapping : 19,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };
    params[NeptuneModule::REVERB_MODE] = {
        name : "Rev Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_reverbNames,
        defaultValue : {.uint_value = 1}, // Medium Space
        knobMapping : -1,
        midiCCMapping : 20
    };
    params[NeptuneModule::ROUTING] = {
        name : "Routing",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_routingNames,
        defaultValue : {.uint_value = 1}, // Send: the delay also feeds the reverb; Parallel: reverb and delay side by side; Series: the delay only goes into the reverb
        knobMapping : -1,
        midiCCMapping : 21
    };
    params[NeptuneModule::DELAY_MODE] = {
        name : "Dly Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_delayModeNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 22
    };
    params[NeptuneModule::STEREO_IN] = { // off = mono in (the left input feeds both sides), on = true stereo
        name : "Stereo In",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 23
    };
    params[NeptuneModule::PRE_DELAY] = { // the original's "alternate" control for the Decay knob
        name : "PreDelay",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : -1,
        midiCCMapping : 24,
        unit : " ms",
        decimals : 0,
        // engine: (int)(0.95 * v * 1000) ms
        displayFn : [](float v) { return floorf(v * 0.95f * 1000.0f); }
    };
    params[NeptuneModule::MOD_RATE] = { // the original's "alternate" control for the Mod knob
        name : "Mod Rate",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.2f},
        knobMapping : -1,
        midiCCMapping : 25,
        unit : " Hz",
        decimals : 2,
        // CloudSeed LineModRate: Response2Dec(v) * 5 Hz (per line scaled 0.7..1.0)
        displayFn : [](float v) { return (powf(100.0f, v) - 1.0f) / 99.0f * 5.0f; }
    };
    params[NeptuneModule::REV_DELAY_MIX] = { // the original's "alternate" control for the Mix knob: the balance between the reverb and the delay
        name : "Rev/Delay",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 26
    };
    return params;
}();

NeptuneModule::NeptuneModule()
    : BaseEffectModule(), m_reverb(nullptr), m_pending(0), m_reverbModeChanged(false), m_sinceApply(0), m_wetMix(0.5f), m_dryMix(0.5f), m_delayMix(0.703f), m_reverbMix(0.703f),
      m_modeRamp(1.0f), m_modeTrigger(false) {
    m_name = "Neptune";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

NeptuneModule::~NeptuneModule() {
    // The reverb's delay lines are in the shared pool, which cannot be freed; the module lives for the whole run.
}

void NeptuneModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    AudioLib::ValueTables::Init();
    CloudSeed::FastSin::Init();

    m_reverb = new CloudSeed::ReverbController(sample_rate);
    m_reverb->ClearBuffers();

    for (int c = 0; c < 2; c++) s_delay[c].Init(sample_rate, s_octData[c], s_revData[c]);
    UpdateDelays();
    for (int c = 0; c < 2; c++) s_delay[c].currentDelay = s_delay[c].delayTarget; // start at the setting, not gliding up from nothing
    ApplyDelayMode();

    // ParameterChanged only fires on a change, so load the reverb mode and push every control's default (or stored value) here
    ApplyReverbMode();
    CalculateMix();
    m_pending = 0;
    m_reverbModeChanged = false;
    m_modeRamp = 1.0f;
    m_modeTrigger = false;
}

void NeptuneModule::ParameterChanged(int parameter_id) {
    switch (parameter_id) {
    case MIX:
    case REV_DELAY_MIX:
        CalculateMix(); // cheap, and the balance should follow the knob at once
        break;
    case REVERB_MODE:
        // Loading another preset clears the reverb: the output is muted, the preset is loaded a moment later (on the audio path) and the sound comes back
        m_modeRamp = 0.0f;
        m_modeTrigger = true;
        break;
    case DELAY_MODE:
        ApplyDelayMode();
        break;
    case DELAY_TIME:
    case DELAY_FDBK:
    case ROUTING:
    case STEREO_IN:
        break; // read directly where they are used
    default:
        m_pending.fetch_or(1u << parameter_id);
        break;
    }
}

void NeptuneModule::ApplyParameter(int parameter_id) {
    if (m_reverb == nullptr) return;
    switch (parameter_id) {
    case DECAY:
        m_reverb->SetParameter(::Parameter2::LineDecay, GetParameterAsFloat(DECAY));
        break;
    case MOD_AMT:
        m_reverb->SetParameter(::Parameter2::LineModAmount, GetParameterAsFloat(MOD_AMT));
        break;
    case MOD_RATE:
        m_reverb->SetParameter(::Parameter2::LineModRate, GetParameterAsFloat(MOD_RATE));
        break;
    case LOWPASS: {
        const float v = GetParameterAsFloat(LOWPASS);
        m_reverb->SetParameter(::Parameter2::PostCutoffFrequency, v * v * v); // the original's knob has a cubic taper
        break;
    }
    case PRE_DELAY:
        m_reverb->SetParameter(::Parameter2::PreDelay, GetParameterAsFloat(PRE_DELAY) * 0.95f); // (the CloudSeed module found 1.0 freezes the pedal)
        break;
    default:
        break;
    }
}

void NeptuneModule::ApplyReverbMode() {
    if (m_reverb == nullptr) return;
    const int mode = GetParameterAsBinnedValue(REVERB_MODE) - 1;
    m_reverb->ClearBuffers();
    if (mode == 0) {
        m_reverb->initFactoryChorus();
    } else if (mode == 2) {
        m_reverb->initFactoryRubiKaFields();
    } else {
        m_reverb->initFactoryMediumSpace();
    }
    // The original's versions of these presets differ from this repository's in the output stage: a higher main output, and Medium Space has a higher early output
    m_reverb->SetParameter(::Parameter2::MainOut, 0.95f);
    if (mode != 0 && mode != 2) m_reverb->SetParameter(::Parameter2::EarlyOut, 0.8f);
    m_reverb->SetParameter(::Parameter2::LineCount, 2);
    // The controls apply over the preset (the original forces this after a mode change)
    for (int p : {DECAY, MOD_AMT, MOD_RATE, LOWPASS, PRE_DELAY}) ApplyParameter(p);
}

void NeptuneModule::ApplyDelayMode() {
    const int mode = GetParameterAsBinnedValue(DELAY_MODE) - 1;
    for (int c = 0; c < 2; c++) {
        s_delay[c].oct.setOctave(mode == 1);
        s_delay[c].reverseMode = (mode == 2);
    }
}

void NeptuneModule::UpdateDelays() {
    const float v = GetParameterAsFloat(DELAY_TIME);
    const float feedback = GetParameterAsFloat(DELAY_FDBK);
    const float target = GetSampleRate() * (kDelayMinSeconds + kDelaySpanSeconds * v);
    for (int c = 0; c < 2; c++) {
        s_delay[c].active = v > 0.01f; // the delay is off at the bottom of the range
        s_delay[c].delayTarget = std::min(target, (float)(kMaxDelay - 1));
        s_delay[c].feedback = feedback;
    }
}

// Called from the audio path: at most one control per 48 samples, so a burst of knob movement is spread over many blocks instead of landing in one
void NeptuneModule::ApplyPending(size_t n) {
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

void NeptuneModule::CalculateMix() {
    crossfade(GetParameterAsFloat(MIX), m_wetMix, m_dryMix);
    crossfade(GetParameterAsFloat(REV_DELAY_MIX), m_delayMix, m_reverbMix);
}

void NeptuneModule::ProcessSamples(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    constexpr size_t kMax = CloudSeed::ReverbController::MaxBlockSize;
    ApplyPending(n);
    UpdateDelays();
    const int routing = GetParameterAsBinnedValue(ROUTING) - 1; // 0 = Send, 1 = Parallel, 2 = Series

    for (size_t start = 0; start < n; start += kMax) {
        const size_t len = (n - start < kMax) ? (n - start) : kMax;
        float delayL[kMax], delayR[kMax], revInL[kMax], revInR[kMax], wetL[kMax], wetR[kMax];

        for (size_t j = 0; j < len; j++) {
            const float l = inL[start + j];
            const float r = inR[start + j];
            delayL[j] = s_delay[0].Process(l) * m_delayMix;
            delayR[j] = s_delay[1].Process(r) * m_delayMix;
            // Send and Series feed the delay into the reverb; Parallel does not
            revInL[j] = routing != 1 ? l + delayL[j] : l;
            revInR[j] = routing != 1 ? r + delayR[j] : r;
        }

        m_reverb->Process(revInL, revInR, wetL, wetR, static_cast<int>(len));

        for (size_t j = 0; j < len; j++) {
            const float l = inL[start + j];
            const float r = inR[start + j];
            // Series: only the reverb is heard; the others add the delay. The wet gain is applied twice, as in the original.
            const float balancedL = (wetL[j] * (m_reverbMix * 1.2f) + (routing != 2 ? delayL[j] : 0.0f)) * m_wetMix;
            const float balancedR = (wetR[j] * (m_reverbMix * 1.2f) + (routing != 2 ? delayR[j] : 0.0f)) * m_wetMix;
            float yL = (l * m_dryMix + balancedL * m_wetMix) * kOutputGain;
            float yR = (r * m_dryMix + balancedR * m_wetMix) * kOutputGain;

            if (m_modeRamp < 1.0f) { // muted while the reverb mode changes
                m_modeRamp += 0.001f;
                yL = yR = 0.0f;
                if (m_modeRamp > 0.1f && m_modeTrigger) {
                    m_modeTrigger = false;
                    ApplyReverbMode();
                }
            }
            outL[start + j] = yL;
            outR[start + j] = yR;
        }
    }
}

void NeptuneModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);
    float outL, outR;
    ProcessSamples(&m_audioLeft, &m_audioLeft, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void NeptuneModule::ProcessStereo(float inL, float inR) {
    BaseEffectModule::ProcessStereo(inL, inR);
    const float left = m_audioLeft;
    const float right = GetParameterAsBool(STEREO_IN) ? m_audioRight : m_audioLeft;
    float outL, outR;
    ProcessSamples(&left, &right, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void NeptuneModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    ProcessSamples(in, in, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void NeptuneModule::ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    // With Stereo In off, the right input is a copy of the left (the original's MISO mode)
    ProcessSamples(inL, GetParameterAsBool(STEREO_IN) ? inR : inL, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}
