// Pluto: dual looper, adapted from the Pluto pedal of the FunBox project (https://github.com/GuitarML/Funbox, MIT licence, Copyright (c) 2024 Keith Bloemer). See Pluto/README.md
// for the licence text and the list of changes.
#include "pluto_module.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

using namespace bkshepherd;
using namespace daisysp;

namespace {
// A minute of audio for each looper at 48 kHz: 11.5 MB each, in SDRAM. No clearing at startup is needed (see Util/var_speed_looper.h).
constexpr size_t kLoopSize = 48000 * 60;
float DSY_SDRAM_BSS s_loopData[2][kLoopSize];
ReverbSc DSY_SDRAM_BSS s_verb;

constexpr int kDoubleTapSamples = 48000; // a second press within this many samples pauses
constexpr float kLedFreq = 1.5f;         // Hz, the pulse of the LED

const char *s_speedModeNames[2] = {"Smooth", "Stepped"};
const char *s_effectNames[3] = {"Stability", "Filter", "Reverb"};
const char *s_loopModeNames[3] = {"Normal", "One-time", "Fripp"};
const char *s_targetNames[3] = {"A + B", "A", "B"};

// The playback speed of the Speed knobs: smooth, -2x to +2x with 1x at noon (the first half goes from -2 to 1)
float smoothSpeed(float v) { return v <= 0.5f ? v * 6.0f - 2.0f : v * 2.0f; }

// ... or in steps
float steppedSpeed(float v) {
    if (v < 0.05f) return -2.0f;
    if (v <= 0.15f) return -1.5f;
    if (v <= 0.25f) return -1.0f;
    if (v <= 0.35f) return -0.5f;
    if (v <= 0.45f) return 0.5f;
    if (v <= 0.55f) return 1.0f; // noon
    if (v <= 0.7f) return 1.5f;
    if (v <= 0.8f) return 2.0f;
    if (v <= 0.9f) return 2.5f;
    return 3.0f;
}
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, PlutoModule::PARAM_COUNT> params{};

    params[PlutoModule::LEVEL_A] = {
        name : "Level A",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // the loop's volume; 0.5 is unity
        knobMapping : 0,
        midiCCMapping : 14
    };
    params[PlutoModule::MOD_A] = {
        name : "Mod A",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f}, // what it modifies depends on the Effect: Stability (speed variations) / Filter (low pass to the left, high pass to the right) / Reverb (decay time)
        knobMapping : 1,
        midiCCMapping : 15
    };
    params[PlutoModule::LEVEL_B] = {
        name : "Level B",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 2,
        midiCCMapping : 16
    };
    params[PlutoModule::SPEED_A] = {
        name : "Speed A",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // -2x (reverse) to +2x, 1x at noon; the stepped mode goes to 3x
        knobMapping : 3,
        midiCCMapping : 17
    };
    params[PlutoModule::MOD_B] = {
        name : "Mod B",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f}, // for Reverb: the damping
        knobMapping : 4,
        midiCCMapping : 18
    };
    params[PlutoModule::SPEED_B] = {
        name : "Speed B",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 5,
        midiCCMapping : 19
    };
    params[PlutoModule::SPEED_MODE] = { // the original's switch 1
        name : "Speed Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 2,
        valueBinNames : s_speedModeNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 21
    };
    params[PlutoModule::EFFECT] = { // the original's switch 2
        name : "Effect",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_effectNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 22
    };
    params[PlutoModule::LOOP_MODE] = { // the original's switch 3
        name : "Loop Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_loopModeNames,
        defaultValue : {.uint_value = 0}, // Normal: overdub as long as it records / One-time: records one pass / Fripp: recording acts like a delay, with a decay on every pass
        knobMapping : -1,
        midiCCMapping : 23
    };
    params[PlutoModule::FOOT_TARGET] = { // there is one footswitch for the loops here (the original had one for each looper)
        name : "Footswitch",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_targetNames,
        defaultValue : {.uint_value = 0}, // which looper(s) the footswitch records, pauses and clears
        knobMapping : -1,
        midiCCMapping : -1
    };
    return params;
}();

PlutoModule::PlutoModule()
    : BaseEffectModule(), m_verb(&s_verb), m_sampleRate(48000.0f), m_ledPhase(0.0f), m_ledBrightness(0.0f) {
    m_name = "Pluto";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
    for (int i = 0; i < 2; i++) {
        m_currentSpeed[i] = 1.0f;
        m_paused[i] = false;
        m_checkDoubleTap[i] = false;
        m_doubleTapSamples[i] = 0;
    }
}

PlutoModule::~PlutoModule() {}

void PlutoModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    for (int i = 0; i < 2; i++) {
        m_looper[i].Init(s_loopData[i], kLoopSize);
        m_lp[i].Init(sample_rate);
        m_hp[i].Init(sample_rate);
        m_bal[i].Init(sample_rate);
        m_rand[i].Init(sample_rate);
        m_currentSpeed[i] = 1.0f;
        m_paused[i] = false;
        m_checkDoubleTap[i] = false;
        m_doubleTapSamples[i] = 0;
    }
    m_verb->Init(sample_rate);
    m_ledPhase = 0.0f;
    m_ledBrightness = 0.0f;
    ApplyLoopMode();
}

void PlutoModule::ApplyLoopMode() {
    const int mode = GetParameterAsBinnedValue(LOOP_MODE) - 1;
    const auto m = mode == 0 ? varspeed::Looper::Mode::NORMAL : mode == 1 ? varspeed::Looper::Mode::ONETIME_DUB : varspeed::Looper::Mode::FRIPPERTRONICS;
    m_looper[0].SetMode(m);
    m_looper[1].SetMode(m);
}

void PlutoModule::ParameterChanged(int parameter_id) {
    if (parameter_id == LOOP_MODE) ApplyLoopMode();
}

bool PlutoModule::Selected(int looper) const {
    const int target = GetParameterAsBinnedValue(FOOT_TARGET) - 1; // 0: both, 1: A, 2: B
    return target == 0 || target == looper + 1;
}

// Tap: start or stop recording (not while paused); a second tap within a second pauses or resumes playback
void PlutoModule::AlternateFootswitchPressed() {
    for (int i = 0; i < 2; i++) {
        if (!Selected(i)) continue;
        if (!m_paused[i]) m_looper[i].TrigRecord();
        if (m_checkDoubleTap[i]) {
            if (m_doubleTapSamples[i] <= kDoubleTapSamples) {
                if (m_looper[i].Recording()) m_looper[i].TrigRecord(); // not recording when paused
                m_paused[i] = !m_paused[i];
                m_doubleTapSamples[i] = 0; // also when tapped three times
                m_checkDoubleTap[i] = false;
            }
        } else {
            m_checkDoubleTap[i] = true;
        }
    }
}

// Hold: clear the loop
void PlutoModule::AlternateFootswitchHeldFor1Second() {
    for (int i = 0; i < 2; i++) {
        if (!Selected(i)) continue;
        m_paused[i] = false;
        m_looper[i].Clear();
    }
}

void PlutoModule::ProcessSamples(const float *inL, const float *inR, float *outL, float *outR, size_t n, bool stereo) {
    // The controls are read once per call
    const float level[2] = {GetParameterAsFloat(LEVEL_A), GetParameterAsFloat(LEVEL_B)};
    const float mod[2] = {GetParameterAsFloat(MOD_A), GetParameterAsFloat(MOD_B)};
    const float speedKnob[2] = {GetParameterAsFloat(SPEED_A), GetParameterAsFloat(SPEED_B)};
    const bool smooth = GetParameterAsBinnedValue(SPEED_MODE) == 1;
    const int effect = GetParameterAsBinnedValue(EFFECT) - 1; // 0 stability, 1 filter, 2 reverb

    float speedTarget[2], steppedAbs[2];
    for (int k = 0; k < 2; k++) {
        if (smooth) {
            speedTarget[k] = smoothSpeed(speedKnob[k]);
            steppedAbs[k] = 0.0f;
        } else {
            const float s = steppedSpeed(speedKnob[k]);
            m_looper[k].SetReverse(s < 0.0f);
            steppedAbs[k] = std::fabs(s);
            m_looper[k].SetIncrementSize(steppedAbs[k]);
        }
    }

    if (effect == 0) {
        m_rand[0].SetFreq(mod[0] * 4.0f);
        m_rand[1].SetFreq(mod[1] * 4.0f);
    } else if (effect == 2) {
        const float v = mod[0];
        if (v < 0.01f) m_verb->SetFeedback(0.0f);
        else if (v <= 0.1f) m_verb->SetFeedback(v * 6.4f); // 0 .. 0.6 for the first 10% of the knob: a smooth start of the useful decay times
        else m_verb->SetFeedback(v * 0.4f + 0.6f);         // 0.6 .. 1.0
        float inverted = 1.0f - mod[1];                    // the damping: knob left is less damping
        inverted *= inverted;                              // exponential taper
        m_verb->SetLpFreq(600.0f + inverted * (16000.0f - 600.0f));
    } else {
        for (int k = 0; k < 2; k++) {
            if (mod[k] <= 0.5f) {
                m_lp[k].SetFreq(mod[k] * mod[k] * mod[k] * 39800.0f + 80.0f);
            } else {
                const float v = mod[k] - 0.5f;
                float freq = v * v * 500.0f + 40.0f; // (ATone::SetFreq takes a non-const reference)
                m_hp[k].SetFreq(freq);
            }
        }
    }

    for (size_t i = 0; i < n; i++) {
        // Speed: smooth changes (like a tape reel) or the stepped value, and random variations in the Stability effect
        float speedAbs[2];
        for (int k = 0; k < 2; k++) {
            if (smooth) {
                fonepole(m_currentSpeed[k], speedTarget[k], 0.00012f);
                m_looper[k].SetReverse(m_currentSpeed[k] < 0.0f);
                speedAbs[k] = std::fabs(m_currentSpeed[k]);
                m_looper[k].SetIncrementSize(speedAbs[k]);
            } else {
                speedAbs[k] = steppedAbs[k];
            }
            if (effect == 0) m_looper[k].SetIncrementSize(speedAbs[k] + m_rand[k].Process() * mod[k] * 0.05f);
            if (m_checkDoubleTap[k] && ++m_doubleTapSamples[k] > kDoubleTapSamples) { // too late for a double tap
                m_doubleTapSamples[k] = 0;
                m_checkDoubleTap[k] = false;
            }
        }

        // With one input both loopers record it; with two, A is the left channel and B the right
        const float l = inL[i];
        const float r = stereo ? inR[i] : inL[i];

        float loop[2] = {0.0f, 0.0f};
        if (!m_paused[0]) loop[0] = m_looper[0].Process(l) * level[0] * 2.0f;
        if (!m_paused[1]) loop[1] = m_looper[1].Process(r) * level[1] * 2.0f;

        float wetL = 0.0f, wetR = 0.0f;
        if (effect == 2) {
            m_verb->Process(loop[0], loop[1], &wetL, &wetR);
        } else if (effect == 1) {
            for (int k = 0; k < 2; k++) {
                float filtered = mod[k] <= 0.5f ? m_lp[k].Process(loop[k]) : m_hp[k].Process(loop[k]);
                float unfiltered = loop[k];
                loop[k] = m_bal[k].Process(filtered, unfiltered);
            }
        }

        if (stereo) { // loop A on the left and loop B on the right
            outL[i] = l + loop[0] + wetL;
            outR[i] = r + loop[1] + wetR;
        } else {      // both loops on both channels
            outL[i] = l + loop[0] + loop[1] + wetL;
            outR[i] = l + loop[0] + loop[1] + wetR;
        }
    }

    // The LED shows the looper(s) the footswitch controls: it pulses while recording, blinks while paused, and is lit while playing
    m_ledPhase += kLedFreq * static_cast<float>(n) / m_sampleRate;
    m_ledPhase -= std::floor(m_ledPhase);
    const float tri = m_ledPhase < 0.5f ? m_ledPhase * 4.0f - 1.0f : 3.0f - m_ledPhase * 4.0f;
    bool recording = false, paused = false, playing = false;
    for (int k = 0; k < 2; k++) {
        if (!Selected(k)) continue;
        recording |= m_looper[k].Recording();
        paused |= m_paused[k];
        playing |= !m_looper[k].IsEmpty();
    }
    if (recording) m_ledBrightness = tri * 0.5f + 0.5f;
    else if (paused) m_ledBrightness = m_ledPhase < 0.5f ? 1.0f : 0.0f;
    else m_ledBrightness = playing ? 1.0f : 0.0f;
}

void PlutoModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);
    float outL, outR;
    ProcessSamples(&m_audioLeft, &m_audioLeft, &outL, &outR, 1, false);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void PlutoModule::ProcessStereo(float inL, float inR) {
    BaseEffectModule::ProcessStereo(inL, inR);
    float outL, outR;
    ProcessSamples(&m_audioLeft, &m_audioRight, &outL, &outR, 1, true);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void PlutoModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    ProcessSamples(in, in, outL, outR, n, false);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void PlutoModule::ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    ProcessSamples(inL, inR, outL, outR, n, true);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

float PlutoModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);
    if (led_id == 1) return value * m_ledBrightness;
    return value;
}
