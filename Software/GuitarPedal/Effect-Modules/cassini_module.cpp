// Cassini: chromatic sampler, adapted from the Cassini pedal of the FunBox project (https://github.com/GuitarML/Funbox, MIT licence, Copyright (c) 2024 Keith Bloemer). See
// Cassini/README.md for the licence text and the list of changes.
#include "cassini_module.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

using namespace bkshepherd;
using namespace daisysp;

namespace {
constexpr int kMaxSample = 48000 * 20; // 20 seconds, three banks: 11.5 MB in SDRAM
float DSY_SDRAM_BSS s_sample[CassiniModule::kBanks][kMaxSample];
constexpr size_t kLoopSize = 48000 * 60; // the looper holds a minute: 11.5 MB
float DSY_SDRAM_BSS s_loopData[kLoopSize];
ReverbSc DSY_SDRAM_BSS s_verb;

constexpr float kMiddleC = 261.6256f;
constexpr int kDoubleTapSamples = 48000;
constexpr float kLedFreq = 1.5f;

const char *s_speedModeNames[2] = {"Smooth", "Stepped"};
const char *s_sampleModeNames[2] = {"One-shot", "Looped"};
const char *s_bankNames[3] = {"1", "2", "3"};
const char *s_footNames[2] = {"Sample", "Looper"};

float smoothSpeed(float v) { return v <= 0.5f ? v * 6.0f - 2.0f : v * 2.0f; }

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
    std::array<ParameterMetaData, CassiniModule::PARAM_COUNT> params{};

    params[CassiniModule::TRIM_START] = {
        name : "Trim Start",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f}, // where the sample starts, as a fraction of its length
        knobMapping : 0,
        midiCCMapping : 14
    };
    params[CassiniModule::TRIM_END] = {
        name : "Trim End",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 1.0f}, // where it ends; before the start it plays backwards
        knobMapping : 1,
        midiCCMapping : 15
    };
    params[CassiniModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // the keys' volume
        knobMapping : 2,
        midiCCMapping : 16
    };
    params[CassiniModule::LOOP_SPEED] = {
        name : "Loop Speed",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // the looper's speed: -2x (reverse) to +2x, 1x at noon
        knobMapping : 3,
        midiCCMapping : 17
    };
    params[CassiniModule::FILTER] = {
        name : "Filter",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.52f}, // low pass to the left of the middle, high pass to the right (almost open here)
        knobMapping : 4,
        midiCCMapping : 18
    };
    params[CassiniModule::REVERB] = {
        name : "Reverb",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f}, // decay time
        knobMapping : 5,
        midiCCMapping : 19
    };
    params[CassiniModule::SPEED_MODE] = { // the original's switch 1
        name : "Speed Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 2,
        valueBinNames : s_speedModeNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : -1
    };
    params[CassiniModule::SAMPLE_MODE] = { // the original's switch 2
        name : "Sample Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 2,
        valueBinNames : s_sampleModeNames,
        defaultValue : {.uint_value = 1}, // one-shot: a key plays the sample once / looped: it repeats while the key is down
        knobMapping : -1,
        midiCCMapping : -1
    };
    params[CassiniModule::BANK] = { // the original's switch 3
        name : "Bank",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_bankNames,
        defaultValue : {.uint_value = 0}, // the sample that is recorded and played
        knobMapping : -1,
        midiCCMapping : -1
    };
    params[CassiniModule::FOOT_MODE] = { // the original had a footswitch for each
        name : "Footswitch",
        valueType : ParameterValueType::Binned,
        valueBinCount : 2,
        valueBinNames : s_footNames,
        defaultValue : {.uint_value = 0}, // Sample: hold to record the sample of the bank / Looper: tap to record, tap again to loop, tap twice to pause, hold to clear
        knobMapping : -1,
        midiCCMapping : -1
    };
    return params;
}();

void CassiniModule::Voice::Init(float sampleRate) {
    m_active = false;
    m_gate = false;
    m_firstSample = true;
    m_speed = 1.0f;
    m_playhead = 0.0f;
    m_env.Init(sampleRate);
    m_env.SetSustainLevel(1.0f);
    m_env.SetTime(ADSR_SEG_ATTACK, 0.5f);
    m_env.SetTime(ADSR_SEG_DECAY, 0.05f);
    m_env.SetTime(ADSR_SEG_RELEASE, 0.5f);
}

void CassiniModule::Voice::OnNoteOn(float note, float velocity) {
    m_note = note;
    m_velocity = velocity;
    m_active = true;
    m_gate = true;
    m_speed = mtof(note) / kMiddleC; // middle C plays the sample at its own pitch
    m_firstSample = true;
}

// The next sample of the recording, interpolated between two samples; at the end of the trimmed part it stops (one-shot) or starts again (looped)
float CassiniModule::Voice::NextSample(const float *sample, int size, int trimStart, int trimEnd, bool oneShot) {
    if (size == 0) return 0.0f;
    if (m_firstSample) {
        m_firstSample = false;
        m_playhead = static_cast<float>(trimStart);
    }
    const bool reverse = trimStart > trimEnd; // the start can be after the end: backwards
    if (!reverse && m_playhead >= static_cast<float>(trimEnd)) {
        m_playhead = static_cast<float>(trimStart);
        if (oneShot) m_active = false;
    }
    // Backwards, the end is at least at position 1: the original stopped at an index below 1 and, as the position rarely reaches the end exactly, stayed there silent
    if (reverse && m_playhead <= static_cast<float>(std::max(trimEnd, 1))) {
        m_playhead = static_cast<float>(trimStart);
        if (oneShot) m_active = false;
    }
    const size_t index = static_cast<size_t>(m_playhead);
    const float fraction = m_playhead - static_cast<float>(index);
    if (reverse && index < 1) return 0.0f;

    const float sample1 = sample[index];
    float sample2;
    if (!reverse) sample2 = (static_cast<float>(index + 1) < static_cast<float>(trimEnd)) ? sample[index + 1] : 0.0f;
    else sample2 = (static_cast<float>(index - 1) > static_cast<float>(trimEnd)) ? sample[index - 1] : 0.0f;
    const float out = sample1 + fraction * (sample2 - sample1);

    m_playhead += reverse ? -m_speed : m_speed;
    return out;
}

float CassiniModule::Voice::Process(const float *sample, int size, int trimStart, int trimEnd, bool oneShot) {
    if (!m_active) return 0.0f;
    const float amp = m_env.Process(m_gate);
    if (!m_env.IsRunning()) {
        m_active = false;
        m_playhead = 0.0f;
    }
    return NextSample(sample, size, trimStart, trimEnd, oneShot) * (m_velocity / 127.0f) * amp;
}

CassiniModule::CassiniModule()
    : BaseEffectModule(), m_verb(&s_verb), m_sampleRate(48000.0f), m_currentSpeed(1.0f), m_paused(false), m_checkDoubleTap(false), m_doubleTapSamples(0), m_recording(false),
      m_recordIndex(0), m_ledPhase(0.0f), m_ledBrightness(0.0f) {
    m_name = "Cassini";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
    for (int &s : m_sampleSize) s = 0;
}

CassiniModule::~CassiniModule() {}

void CassiniModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    for (auto &v : m_voices) v.Init(sample_rate);
    m_looper.Init(s_loopData, kLoopSize);
    m_verb->Init(sample_rate);
    m_verb->SetLpFreq(100000.0f);
    m_verb->SetFeedback(0.0f);
    m_filter.Init(sample_rate);
    m_filter.SetFreq(20000.0f);
    m_rand.Init(sample_rate);
    m_currentSpeed = 1.0f;
    m_paused = false;
    m_checkDoubleTap = false;
    m_doubleTapSamples = 0;
    m_recording = false;
    m_recordIndex = 0;
    for (int &s : m_sampleSize) s = 0; // the banks are empty (their memory is not cleared: only what has been recorded is played)
    m_ledPhase = 0.0f;
    m_ledBrightness = 0.0f;
}

int CassiniModule::SampleBank() const { return GetParameterAsBinnedValue(BANK) - 1; }

// Sample: hold to record the bank's sample. Looper: tap to record, tap again to play (a second tap within a second pauses), hold to clear
void CassiniModule::AlternateFootswitchPressed() {
    if (GetParameterAsBinnedValue(FOOT_MODE) == 1) {
        m_recordIndex = 0;
        m_recording = true;
        return;
    }
    if (!m_paused) m_looper.TrigRecord();
    if (m_checkDoubleTap) {
        if (m_doubleTapSamples <= kDoubleTapSamples) {
            if (m_looper.Recording()) m_looper.TrigRecord();
            m_paused = !m_paused;
            m_doubleTapSamples = 0;
            m_checkDoubleTap = false;
        }
    } else {
        m_checkDoubleTap = true;
    }
}

void CassiniModule::AlternateFootswitchReleased() {
    if (m_recording) {
        m_recording = false;
        m_recordIndex = 0;
    }
}

void CassiniModule::AlternateFootswitchHeldFor1Second() {
    if (GetParameterAsBinnedValue(FOOT_MODE) == 2) {
        m_paused = false;
        m_looper.Clear();
    }
}

void CassiniModule::OnNoteOn(float notenumber, float velocity) {
    if (velocity == 0.0f) {
        OnNoteOff(notenumber, velocity); // a note on with velocity 0 is a note off
        return;
    }
    for (auto &v : m_voices) {
        if (!v.IsActive()) {
            v.OnNoteOn(notenumber, velocity);
            return;
        }
    }
}

void CassiniModule::OnNoteOff(float notenumber, float /*velocity*/) {
    for (auto &v : m_voices)
        if (v.IsActive() && v.GetNote() == notenumber) v.OnNoteOff();
}

void CassiniModule::ProcessSamples(const float *in, float *outL, float *outR, size_t n) {
    // The controls are read once per call
    const int bank = SampleBank();
    const float *sample = s_sample[bank];
    const int size = m_sampleSize[bank];
    const float sizeF = static_cast<float>(size);
    const int trimStart = static_cast<int>(GetParameterAsFloat(TRIM_START) * sizeF);
    const int trimEnd = static_cast<int>(GetParameterAsFloat(TRIM_END) * sizeF);
    const bool oneShot = GetParameterAsBinnedValue(SAMPLE_MODE) == 1;
    const float voiceLevel = GetParameterAsFloat(LEVEL) * 2.5f;
    const float filterKnob = GetParameterAsFloat(FILTER);
    const bool smooth = GetParameterAsBinnedValue(SPEED_MODE) == 1;
    const float speedKnob = std::max(GetParameterAsFloat(LOOP_SPEED), 0.01f);

    m_verb->SetFeedback(GetParameterAsFloat(REVERB));
    if (filterKnob <= 0.5f) m_filter.SetFreq(filterKnob * filterKnob * 2.0f * 18000.0f + 150.0f); // exponential, 150 to 18150 Hz
    else m_filter.SetFreq(((filterKnob - 0.5f) * 2.0f) * ((filterKnob - 0.5f) * 2.0f) * 4000.0f);  // exponential, 0 to 4000 Hz

    float speedTarget = 0.0f;
    if (smooth) {
        speedTarget = smoothSpeed(speedKnob);
    } else {
        const float s = steppedSpeed(speedKnob);
        m_looper.SetReverse(s < 0.0f);
        m_looper.SetIncrementSize(std::fabs(s));
    }

    for (size_t i = 0; i < n; i++) {
        const float input = in[i];

        if (smooth) {
            fonepole(m_currentSpeed, speedTarget, 0.00006f);
            m_looper.SetReverse(m_currentSpeed < 0.0f);
            m_looper.SetIncrementSize(std::fabs(m_currentSpeed));
        }
        if (m_checkDoubleTap && ++m_doubleTapSamples > kDoubleTapSamples) {
            m_doubleTapSamples = 0;
            m_checkDoubleTap = false;
        }

        // Record the input while the footswitch is held, up to 20 seconds
        if (m_recording) {
            s_sample[bank][m_recordIndex] = input;
            m_sampleSize[bank] = m_recordIndex;
            if (++m_recordIndex >= kMaxSample) {
                m_recording = false;
                m_recordIndex = 0;
                m_sampleSize[bank] = kMaxSample;
            }
        }

        float sum = 0.0f;
        for (auto &v : m_voices) sum += v.Process(sample, size, trimStart, trimEnd, oneShot);
        sum *= voiceLevel;

        m_filter.Process(sum);
        const float filtered = filterKnob <= 0.5f ? m_filter.Low() : m_filter.High();

        float wetL, wetR;
        m_verb->Process(filtered, filtered, &wetL, &wetR);
        const float audioOut = filtered + wetL;

        const float loopOut = m_paused ? 0.0f : m_looper.Process(audioOut);

        outL[i] = outR[i] = input + audioOut + loopOut;
    }

    // The LED shows the looper in the Looper footswitch mode, and the recording of the sample otherwise
    m_ledPhase += kLedFreq * static_cast<float>(n) / m_sampleRate;
    m_ledPhase -= std::floor(m_ledPhase);
    const float tri = m_ledPhase < 0.5f ? m_ledPhase * 4.0f - 1.0f : 3.0f - m_ledPhase * 4.0f;
    if (m_recording || m_looper.Recording()) m_ledBrightness = tri * 0.5f + 0.5f;
    else if (m_paused) m_ledBrightness = m_ledPhase < 0.5f ? 1.0f : 0.0f;
    else m_ledBrightness = m_looper.IsEmpty() ? 0.0f : 1.0f;
}

void CassiniModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);
    float outL, outR;
    ProcessSamples(&m_audioLeft, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void CassiniModule::ProcessStereo(float inL, float /*inR*/) { ProcessMono(inL); } // mono in, as in the original

void CassiniModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    ProcessSamples(in, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void CassiniModule::ProcessStereoBlock(const float *inL, const float * /*inR*/, float *outL, float *outR, size_t n) { ProcessMonoBlock(inL, outL, outR, n); }

float CassiniModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);
    if (led_id == 1) return value * m_ledBrightness;
    return value;
}
