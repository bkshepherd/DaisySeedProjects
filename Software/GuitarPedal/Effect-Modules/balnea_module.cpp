// Balnea: digital delay and pitch shifter, adapted from the Balnea pedal of the FunBox project (https://github.com/GuitarML/Funbox, MIT licence, Copyright (c) 2024 Keith Bloemer).
// The reverse delay line is by Adam Fulford (MIT). See Balnea/README.md for the licences and the list of changes.
#include "balnea_module.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

using namespace bkshepherd;
using namespace daisysp;

namespace {
// The delay lines hold up to 16384 samples, which are read at a speed of about 1/16 .. 1: the delay times up to 2 seconds come from reading slowly. A reverse line holds twice that,
// as its two heads move in opposite directions. Sequential access, so SDRAM is fine (and it is not zeroed at startup: Init() clears the lines).
constexpr size_t kLineSize = 4096 * 4;
constexpr size_t kRevSize = kLineSize * 2;
float DSY_SDRAM_BSS s_lineData[2][kLineSize];
float DSY_SDRAM_BSS s_revData[2][kRevSize];

constexpr float kMinTimeMs = 100.0f, kMaxTimeMs = 2000.0f; // the range of the Time parameter
constexpr int kMaxTapSamples = 48000 * 2; // a tap that is not followed by another within 2 s ends the series (2 s is also the longest time)
constexpr int kSelfOscSamples = 14400;    // 300 ms
constexpr int kModInterval = 8;           // the wow and flutter noise is slow: it is advanced once per this many samples
// Output makeup gain. Calibrated with tests/host/run_level_tests.sh so that the default settings give about +3 dB, like the other delays.
constexpr float kOutputGain = 1.25f;

const char *s_stageNames[3] = {"1", "3/4", "1/2"};
const float s_stageMultiplier[3] = {1.0f, 0.75f, 0.5f};
const char *s_scaleNames[3] = {"Octaves", "Dissonant", "Chromatic"};

// The interval of the INT knobs, 11 steps, in the three scales: octaves and fifths / dissonant / chromatic (a speed multiplier: 2 is an octave up)
const float s_interval[3][11] = {
    {0.25f, 0.33334f, 0.5f, 0.66667f, 0.74915f, 1.0f, 1.3348f, 1.5f, 2.0f, 3.0f, 4.0f},
    {0.39685f, 0.5297f, 0.5946f, 0.8409f, 0.8909f, 1.0f, 1.12246f, 1.1892f, 1.6818f, 1.8877f, 2.5198f},
    {0.7492f, 0.7937f, 0.8409f, 0.8909f, 0.9439f, 1.0f, 1.0595f, 1.1225f, 1.1892f, 1.2599f, 1.3348f},
};

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
    std::array<ParameterMetaData, BalneaModule::PARAM_COUNT> params{};

    params[BalneaModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : 14,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };
    params[BalneaModule::TONE] = {
        name : "Tone",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8f}, // the low pass filter of the repeats, 100 Hz .. 14 kHz
        knobMapping : 1,
        midiCCMapping : 15,
        unit : " Hz",
        decimals : 0,
        displayFn : [](float v) { return 100.0f + v * v * 14000.0f; }
    };
    params[BalneaModule::REGEN] = {
        name : "Regen",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // feedback
        knobMapping : 2,
        midiCCMapping : 16,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };
    params[BalneaModule::GLIDE] = {
        name : "Glide",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f}, // how slowly the pitch slides from one interval to the next
        knobMapping : 3,
        midiCCMapping : 17
    };
    params[BalneaModule::INT1] = {
        name : "Interval 1",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // 11 steps; the middle is no change
        knobMapping : 4,
        midiCCMapping : 18
    };
    params[BalneaModule::INT2] = {
        name : "Interval 2",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // in the middle the third stage is skipped
        knobMapping : 5,
        midiCCMapping : 19
    };
    params[BalneaModule::TIME] = { // tap tempo sets it
        name : "Time",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 500.0f},
        knobMapping : -1,
        midiCCMapping : -1,
        minValue : 100,
        maxValue : 2000,
        fineStepSize : 5.0f / 1900.0f, // a fraction of the range (100..2000 ms), so 5 ms; a value in ms would jump straight to the min or max
        unit : " ms",
        decimals : 0
    };
    params[BalneaModule::MOD_RATE] = { // the original's alternate function of the Interval 1 knob
        name : "Mod Rate",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f}, // the speed of the tape wow and flutter
        knobMapping : -1,
        midiCCMapping : 20
    };
    params[BalneaModule::MOD_DEPTH] = { // the alternate function of the Interval 2 knob
        name : "Mod Depth",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f}, // 0: no wow and flutter
        knobMapping : -1,
        midiCCMapping : 21,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };
    params[BalneaModule::BASE_STAGE] = { // the original's switches: how long the sequencer stays at each speed, as a part of the delay time
        name : "Base Stage",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_stageNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : -1
    };
    params[BalneaModule::INT1_STAGE] = {
        name : "Int 1 Stage",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_stageNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : -1
    };
    params[BalneaModule::INT2_STAGE] = {
        name : "Int 2 Stage",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_stageNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : -1
    };
    params[BalneaModule::SCALE] = { // the original's dip switches 3 and 4
        name : "Scale",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_scaleNames,
        defaultValue : {.uint_value = 0}, // the intervals of the two Interval knobs
        knobMapping : -1,
        midiCCMapping : -1
    };
    params[BalneaModule::REVERSE] = { // the original's dip switch 2
        name : "Reverse",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : -1
    };
    params[BalneaModule::TAP_SEQ] = { // the original's alternate function of the tap footswitch
        name : "Tap Sequencer",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0}, // on: every tap steps the sequencer to its next stage, and the tempo is not tapped
        knobMapping : -1,
        midiCCMapping : -1
    };
    return params;
}();

BalneaModule::BalneaModule()
    : BaseEffectModule(), m_sampleRate(48000.0f), m_delayTarget(4096.0f), m_baseSpeed(1.0f), m_baseDelaySamples(24000), m_toneFreq(-1.0f), m_modLeft(0.0f), m_modRight(0.0f),
      m_modCounter(0), m_stage(0), m_stageTimer(0), m_advanceStage(false), m_tapCount(0), m_tapTimer(0), m_tapTimeout(kMaxTapSamples), m_footDown(false), m_footHeldSamples(0), m_selfOsc(false),
      m_resetSequence(false), m_ledTimer(0), m_ledOn(false) {
    m_name = "Balnea";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

BalneaModule::~BalneaModule() {}

void BalneaModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;

    for (int i = 0; i < 2; i++) {
        m_tap[i] = Tap();
        m_tap[i].line.Init(s_lineData[i], kLineSize);
        m_tap[i].rev.Init(s_revData[i], kRevSize);
        m_tap[i].lpf.Init(sample_rate);
        m_tap[i].lpf.SetRes(0.5f);
        m_tap[i].lpf.SetDrive(0.55f);
        m_tap[i].lpf.SetFreq(1300.0f);
        m_tone[i].Init(sample_rate);
        m_tone[i].SetRes(0.8f);
        m_tone[i].SetFreq(10000.0f);
    }
    m_toneFreq = -1.0f;
    m_tape.Init(sample_rate / (2.0f * kModInterval)); // the original advanced the noise twice per sample (once per delay line)
    m_modLeft = m_modRight = 0.0f;
    m_modCounter = 0;
    m_voices.Init(sample_rate);
    m_voices.SetCutoff(1300.0f);

    m_stage = 0;
    m_stageTimer = 0;
    m_advanceStage = false;
    m_tapCount = 0;
    m_tapTimer = 0;
    m_tapTimeout = kMaxTapSamples;
    m_footDown = false;
    m_footHeldSamples = 0;
    m_selfOsc = false;
    m_resetSequence = false;
    m_ledTimer = 0;
    m_ledOn = false;

    // ParameterChanged only fires on a change, so apply the time (the stored one, or the default) here
    ApplyTime(GetParameterAsFloat(TIME));
    for (int i = 0; i < 2; i++) m_tap[i].currentDelay = m_delayTarget;
}

void BalneaModule::ParameterChanged(int parameter_id) {
    if (parameter_id == TIME) ApplyTime(GetParameterAsFloat(TIME));
}

// The delay lines have a few fixed lengths; the time is the speed they are read at: the tapped time chooses the longest length that is below it, and the speed makes up the rest
void BalneaModule::ApplyTime(float ms) {
    ms = std::min(ms, kMaxTimeMs); // a longer time saved with an older version
    const float samples = std::max(ms, 1.0f) * m_sampleRate / 1000.0f;
    if (samples >= 65536.0f) m_delayTarget = 16384.0f;
    else if (samples >= 32768.0f) m_delayTarget = 8192.0f;
    else if (samples >= 16384.0f) m_delayTarget = 4096.0f;
    else if (samples >= 8192.0f) m_delayTarget = 2048.0f;
    else m_delayTarget = 1024.0f;
    m_baseSpeed = m_delayTarget / samples;
    m_baseDelaySamples = static_cast<int>(samples);
    for (int i = 0; i < 2; i++) m_tap[i].speed = m_baseSpeed;
    m_resetSequence = true;
}

float BalneaModule::IntervalMultiplier(int knobIndex, int scale) const {
    return s_interval[scale][std::min(std::max(knobIndex, 0), 10)];
}

void BalneaModule::AlternateFootswitchPressed() {
    m_footHeldSamples = 0;
    m_footDown = true;
}

// A tap on release (so that holding is not a tap): the second tap sets the time; holding for 300 ms makes the delay self-oscillate
void BalneaModule::AlternateFootswitchReleased() {
    m_footDown = false;
    if (m_selfOsc) {
        m_selfOsc = false;
        return;
    }
    if (GetParameterAsBool(TAP_SEQ)) {
        m_tapCount = 0;
        m_advanceStage = true;
        return;
    }
    // Three taps set the time, from the interval between the last two (the first tap only starts, and the first interval is dropped, as it
    // is usually off). Further taps in the same series, each within twice the time of the one before, keep adjusting it.
    const int interval = m_tapTimer;
    m_tapTimer = 0;
    if (m_tapCount == 0) {
        m_tapCount = 1;
        m_tapTimeout = kMaxTapSamples;
    } else if (m_tapCount == 1) {
        m_tapCount = 2;
        m_tapTimeout = kMaxTapSamples;
    } else {
        const float ms = std::min(std::max(interval * 1000.0f / m_sampleRate, kMinTimeMs), kMaxTimeMs);
        SetParameterAsFloat(TIME, ms);
        ApplyTime(ms);
        m_tapTimeout = std::min(kMaxTapSamples, 2 * interval);
    }
}

void BalneaModule::OnNoteOn(float notenumber, float velocity) {
    if (velocity == 0.0f) m_voices.OnNoteOff(notenumber, velocity); // a note on with velocity 0 is a note off
    else m_voices.OnNoteOn(notenumber, velocity);
}

void BalneaModule::OnNoteOff(float notenumber, float velocity) { m_voices.OnNoteOff(notenumber, velocity); }

// One delay line: it is read at a variable speed (a sample rate reducer without interpolation, which is what transposes), filtered, and fed back
float BalneaModule::ProcessTap(Tap &t, float in, float modSpeed, float glide, float feedback, bool reverse) {
    fonepole(t.currentDelay, m_delayTarget, 0.0002f);
    t.line.SetDelay(t.currentDelay);
    t.rev.SetDelay1(t.currentDelay * 2.0f); // doubled: the read and write heads go in opposite directions

    if (glide < 0.05f) t.currentSpeed = t.speed;
    else fonepole(t.currentSpeed, t.speed, 0.001f * (1.0f - glide));

    t.incCounter += t.currentSpeed + modSpeed;
    float out;
    if (t.incCounter >= 1.0f) {
        t.incCounter -= 1.0f;
        t.currentSample = reverse ? t.rev.ReadRev() : t.line.Read();
        t.lpf.Process(t.currentSample);
        out = t.lpf.Low() * 0.51f; // this factor is very sensitive to feedback: 0.6 is too much
        t.line.Write(feedback * out + in);
        t.rev.Write(feedback * out + in);
    } else {
        t.lpf.Process(t.currentSample); // the filter runs at the pedal's sample rate
        out = t.lpf.Low() * 0.51f;
    }
    return out;
}

void BalneaModule::ProcessSamples(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    // The controls are read once per call
    float wetMix, dryMix;
    crossfade(GetParameterAsFloat(MIX), wetMix, dryMix);
    const float tone = GetParameterAsFloat(TONE);
    const float toneFreq = 100.0f + tone * tone * 14000.0f;
    if (toneFreq != m_toneFreq) {
        m_toneFreq = toneFreq;
        m_tone[0].SetFreq(toneFreq);
        m_tone[1].SetFreq(toneFreq);
    }
    const float regen = GetParameterAsFloat(REGEN);
    const float glide = GetParameterAsFloat(GLIDE);
    const int scale = GetParameterAsBinnedValue(SCALE) - 1;
    const float int1 = IntervalMultiplier(static_cast<int>(GetParameterAsFloat(INT1) * 11.0f), scale);
    const float int2 = IntervalMultiplier(static_cast<int>(GetParameterAsFloat(INT2) * 11.0f), scale);
    const float modRate = GetParameterAsFloat(MOD_RATE);
    const float modDepth = GetParameterAsFloat(MOD_DEPTH);
    const bool reverse = GetParameterAsBool(REVERSE);
    const bool tapSeq = GetParameterAsBool(TAP_SEQ);
    const float stageMult[3] = {s_stageMultiplier[GetParameterAsBinnedValue(BASE_STAGE) - 1], s_stageMultiplier[GetParameterAsBinnedValue(INT1_STAGE) - 1],
                                s_stageMultiplier[GetParameterAsBinnedValue(INT2_STAGE) - 1]};

    for (size_t i = 0; i < n; i++) {
        // The synth is mixed into the input
        const float synth = m_voices.Process() * 0.04f;
        const float leftIn = inL[i] + synth;
        const float rightIn = inR[i] + synth;

        if (m_resetSequence) { // a new time: start the sequence over
            m_resetSequence = false;
            m_stageTimer = 0;
            m_stage = 0;
            m_ledTimer = 0;
        }

        // Tap tempo
        if (m_tapCount > 0 && !tapSeq) {
            if (++m_tapTimer >= m_tapTimeout) { // no further tap: the series is over
                m_tapCount = 0;
                m_tapTimer = 0;
            }
        }
        if (m_footDown && !m_selfOsc && ++m_footHeldSamples >= kSelfOscSamples) m_selfOsc = true;
        const float feedback = m_selfOsc ? 1.1f : regen;

        // LED 2 blinks on and off once per delay time
        if (++m_ledTimer >= m_baseDelaySamples / 2) {
            m_ledTimer = 0;
            m_ledOn = !m_ledOn;
        }

        // The sequencer: one stage per delay time (or per tap), at the base speed, then with the first interval, then with both
        if ((m_stageTimer >= static_cast<int>(m_baseDelaySamples * stageMult[m_stage]) && !tapSeq) || (tapSeq && m_advanceStage)) {
            m_advanceStage = false;
            if (++m_stage == 3) m_stage = 0;
            if (m_stage == 2 && int2 == 1.0f) m_stage = 0; // no second interval: skip the stage
            const float speed = m_stage == 0 ? m_baseSpeed : m_stage == 1 ? int1 * m_baseSpeed : int1 * int2 * m_baseSpeed;
            m_tap[0].speed = m_tap[1].speed = speed;
            m_stageTimer = 0;
        }
        m_stageTimer++;

        // Wow and flutter move the read speed
        if (modDepth > 0.0f) {
            if (m_modCounter == 0) {
                m_tape.Process(0.2f + 2.0f * modRate, 2.0f + 5.0f * modRate, 2.0f * modDepth, 2.0f * modDepth, 0.7f);
                m_modLeft = m_tape.GetTapeSpeedLeft() * 0.01f;
                m_modRight = m_tape.GetTapeSpeedRight() * 0.01f;
            }
            if (++m_modCounter >= kModInterval) m_modCounter = 0;
        } else {
            m_modLeft = m_modRight = 0.0f;
        }

        const float wetL = ProcessTap(m_tap[0], leftIn, m_modLeft, glide, feedback, reverse);
        m_tone[0].Process(wetL);
        const float wetR = ProcessTap(m_tap[1], rightIn, m_modRight, glide, feedback, reverse);
        m_tone[1].Process(wetR);

        outL[i] = (leftIn * dryMix + m_tone[0].Low() * wetMix * 1.1f) * kOutputGain;
        outR[i] = (rightIn * dryMix + m_tone[1].Low() * wetMix * 1.1f) * kOutputGain;
    }
}

void BalneaModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);
    float outL, outR;
    ProcessSamples(&m_audioLeft, &m_audioLeft, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void BalneaModule::ProcessStereo(float inL, float inR) {
    BaseEffectModule::ProcessStereo(inL, inR);
    float outL, outR;
    ProcessSamples(&m_audioLeft, &m_audioRight, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void BalneaModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    ProcessSamples(in, in, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void BalneaModule::ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    ProcessSamples(inL, inR, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

float BalneaModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);
    if (led_id == 1) return value * (m_ledOn ? (m_selfOsc ? 1.0f : 0.5f) : 0.0f); // the second LED blinks with the delay time
    return value;
}
