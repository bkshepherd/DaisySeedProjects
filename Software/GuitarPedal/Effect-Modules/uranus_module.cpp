// Uranus: granular delay and synth, adapted from the Uranus pedal of the FunBox project (https://github.com/GuitarML/Funbox, MIT licence, Copyright (c) 2024 Keith Bloemer).
// The grain player (Util/granularplayermod.*) is a modified version of DaisySP's GranularPlayer and the FM voices (Util/fm_voices.h, operator.*) are shared with the FM Keys
// module. See Uranus/README.md for the licences and the list of changes.
#include "uranus_module.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

using namespace bkshepherd;
using namespace daisysp;

namespace {
// Half a second of audio at 48 kHz, continually updated with the input (and the feedback). The grain players read it at random places, which is much too slow in SDRAM, so it is
// in the on-chip RAM that is otherwise unused (see nam_a2_sections.lds).
constexpr int kMaxSample = 24000;
#define URANUS_FAST_DATA __attribute__((section(".sram_d2_bss")))
URANUS_FAST_DATA float s_audio[kMaxSample];

VoiceManagerFm<12> s_voices;

// Output makeup gain. Calibrated with tests/host/run_level_tests.sh so that the default settings give about +2 dB, like the other effects.
constexpr float kOutputGain = 1.75f;

const char *s_grainModeNames[3] = {"Oct Up", "Oct Down", "Up+Down"};
const char *s_grainEnvNames[3] = {"Cos", "SlowAtk", "FastAtk"};
const char *s_synthNames[3] = {"Off", "Granular", "FM"};

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

// The FM controls are 0..1 like a MIDI controller; ratios go in steps of 0.5 from 0.5 to 16.5
float ratioFromMagnitude(float v) { return (float)static_cast<int>(v * 32.0f) / 2.0f + 0.5f; }
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, UranusModule::PARAM_COUNT> params{};

    params[UranusModule::SIZE] = {
        name : "Size",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f}, // grain size 1 .. 300 ms
        knobMapping : 0,
        midiCCMapping : 14
    };
    params[UranusModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : 15
    };
    params[UranusModule::PITCH] = {
        name : "Pitch",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // transposition of the grains, -13 .. +13 semitones in steps of one; no transposition in the middle
        knobMapping : 2,
        midiCCMapping : 16
    };
    params[UranusModule::FEEDBACK] = {
        name : "Feedback",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f}, // of the audio the grains are cut from
        knobMapping : 3,
        midiCCMapping : 17
    };
    params[UranusModule::WIDTH] = {
        name : "Width",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // how far back in time grains can start, 0 .. 50 ms
        knobMapping : 4,
        midiCCMapping : 18
    };
    params[UranusModule::SPEED] = {
        name : "Speed",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.75f}, // playback speed of the grains, -2 .. +2 (normal at 0.75, stopped in the middle, backwards below)
        knobMapping : 5,
        midiCCMapping : 19
    };
    params[UranusModule::GRAIN_MODE] = {
        name : "Grain Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_grainModeNames,
        defaultValue : {.uint_value = 0}, // the two grain players play at: unchanged and an octave up / unchanged and an octave down / an octave down and an octave up
        knobMapping : -1,
        midiCCMapping : -1
    };
    params[UranusModule::GRAIN_ENV] = {
        name : "Grain Env",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_grainEnvNames,
        defaultValue : {.uint_value = 0}, // smooth cosine / slow attack, fast decay / fast attack
        knobMapping : -1,
        midiCCMapping : -1
    };
    params[UranusModule::SYNTH_MODE] = {
        name : "Synth Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_synthNames,
        defaultValue : {.uint_value = 0}, // Off: audio in only / Granular: a MIDI key plays the grains (ADSR, from middle C) / FM: a polyphonic FM synth replaces the input
        knobMapping : -1,
        midiCCMapping : -1
    };
    params[UranusModule::HOLD] = { // stops writing new audio: the grains keep playing what is in the buffer. The secondary footswitch toggles it.
        name : "Hold",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : -1
    };
    params[UranusModule::SPREAD] = { // the original's alternate function of the Mix knob
        name : "Spread",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f}, // stereo spread of every grain
        knobMapping : -1,
        midiCCMapping : -1
    };
    params[UranusModule::LFO_DEPTH] = { // the alternate function of the Pitch knob
        name : "LFO Depth",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f}, // pitch modulation, 0 .. 1 semitone
        knobMapping : -1,
        midiCCMapping : -1
    };
    params[UranusModule::LFO_RATE] = { // the alternate function of the Speed knob
        name : "LFO Rate",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.2f}, // 0 .. 6 Hz, exponential
        knobMapping : -1,
        midiCCMapping : -1
    };
    // FM synth and envelope controls, MIDI CC 20 .. 31 as on the pedal. The envelope controls (CC 28 .. 31) are the granular synth's ADSR and the FM carrier's.
    params[UranusModule::FM_CAR_LEVEL] = {
        name : "Car Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.25f}, // the carrier's level is 4 x this: 0.5 by default
        knobMapping : -1,
        midiCCMapping : 20
    };
    params[UranusModule::FM_CAR_RATIO] = {
        name : "Car Ratio",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.04f}, // ratio 0.5 .. 16.5 in steps of 0.5: 1.0
        knobMapping : -1,
        midiCCMapping : 21
    };
    params[UranusModule::FM_MOD_LEVEL] = {
        name : "Mod Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.63f}, // exponential: 0.1
        knobMapping : -1,
        midiCCMapping : 22
    };
    params[UranusModule::FM_MOD_RATIO] = {
        name : "Mod Ratio",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.98f}, // 16.0
        knobMapping : -1,
        midiCCMapping : 23
    };
    params[UranusModule::MOD_ATTACK] = {
        name : "Mod Attack",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.07f}, // exponential: 5 ms
        knobMapping : -1,
        midiCCMapping : 24
    };
    params[UranusModule::MOD_DECAY] = {
        name : "Mod Decay",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.07f},
        knobMapping : -1,
        midiCCMapping : 25
    };
    params[UranusModule::MOD_SUSTAIN] = {
        name : "Mod Sustain",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 26
    };
    params[UranusModule::MOD_RELEASE] = {
        name : "Mod Release",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.2f}, // seconds
        knobMapping : -1,
        midiCCMapping : 27
    };
    params[UranusModule::ENV_ATTACK] = {
        name : "Env Attack",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.07f},
        knobMapping : -1,
        midiCCMapping : 28
    };
    params[UranusModule::ENV_DECAY] = {
        name : "Env Decay",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.07f},
        knobMapping : -1,
        midiCCMapping : 29
    };
    params[UranusModule::ENV_SUSTAIN] = {
        name : "Env Sustain",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 30
    };
    params[UranusModule::ENV_RELEASE] = {
        name : "Env Release",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.2f},
        knobMapping : -1,
        midiCCMapping : 31
    };
    return params;
}();

UranusModule::UranusModule()
    : BaseEffectModule(), m_voices(&s_voices), m_sampleRate(48000.0f), m_sampleIndex(0), m_currentGrainSize(0.0f), m_gate(false), m_note(60.0f), m_velocity(0.0f) {
    m_name = "Uranus";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

UranusModule::~UranusModule() {}

void UranusModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sampleRate = sample_rate;
    m_sampleIndex = 0;
    m_gate = false;
    m_note = 60.0f; // middle C: no transposition
    m_velocity = 0.0f;

    // The buffer is not zeroed at startup
    std::memset(s_audio, 0, sizeof(s_audio));

    // Two grain players, a quarter of a cycle apart, so the grains are offset from each other (more would not keep up on the Daisy)
    m_swarm[0].Init(s_audio, kMaxSample, sample_rate, 0.0f, 0.5f);
    m_swarm[1].Init(s_audio, kMaxSample, sample_rate, 0.25f, 0.75f);

    m_env.Init(sample_rate);
    m_lfo.Init(sample_rate);
    m_lfo.SetWaveform(Oscillator::WAVE_SIN);
    m_lfo.SetAmp(1.0f);
    m_lfo.SetFreq(0.25f);

    m_voices->Init(sample_rate);

    m_currentGrainSize = GetParameterAsFloat(SIZE) * 300.0f + 1.0f;

    // ParameterChanged only fires on a change, so push every control's default (or stored value) here
    for (int p = 0; p < PARAM_COUNT; p++) ApplyParameter(p);
}

void UranusModule::ParameterChanged(int parameter_id) { ApplyParameter(parameter_id); }

void UranusModule::ApplyParameter(int parameter_id) {
    const float v = GetParameterAsFloat(parameter_id);
    switch (parameter_id) {
    case GRAIN_ENV: {
        const int mode = GetParameterAsBinnedValue(GRAIN_ENV) - 1;
        for (int i = 0; i < 2; i++) m_swarm[i].setEnvelopeMode(mode);
        break;
    }
    case SPREAD:
        for (int i = 0; i < 2; i++) m_swarm[i].setStereoSpread(v);
        break;
    case LFO_RATE:
        m_lfo.SetFreq(v * v * 6.0f); // exponential, 0 .. 6 Hz
        break;
    case FM_CAR_LEVEL:
        m_voices->SetCarrierLevel(v / 0.25f * 0.5f); // 0.25 is 0.5: the original's CC 20 gives value / 0.5
        break;
    case FM_CAR_RATIO:
        m_voices->SetCarrierRatio(ratioFromMagnitude(v));
        break;
    case FM_MOD_LEVEL:
        m_voices->SetModulatorLevel((v / 2.0f) * (v / 2.0f)); // exponential and reduced maximum level
        break;
    case FM_MOD_RATIO:
        m_voices->SetModulatorRatio(ratioFromMagnitude(v));
        break;
    case MOD_ATTACK:
        m_voices->SetModAttack(v * v + 0.0001f);
        break;
    case MOD_DECAY:
        m_voices->SetModDecay(v * v + 0.0001f);
        break;
    case MOD_SUSTAIN:
        m_voices->SetModSustain(v + 0.001f);
        break;
    case MOD_RELEASE:
        m_voices->SetModRelease(v + 0.001f);
        break;
    case ENV_ATTACK: // the granular synth's envelope and the FM carrier's
        m_env.SetTime(ADSR_SEG_ATTACK, v * v + 0.0001f);
        m_voices->SetCarrierAttack(v * v + 0.0001f);
        break;
    case ENV_DECAY:
        m_env.SetTime(ADSR_SEG_DECAY, v * v + 0.0001f);
        m_voices->SetCarrierDecay(v * v + 0.0001f);
        break;
    case ENV_SUSTAIN:
        m_env.SetSustainLevel(v + 0.01f);
        m_voices->SetCarrierSustain(v + 0.001f);
        break;
    case ENV_RELEASE:
        m_env.SetTime(ADSR_SEG_RELEASE, v + 0.001f);
        m_voices->SetCarrierRelease(v + 0.001f);
        break;
    default:
        break; // the rest is read where it is used
    }
}

void UranusModule::AlternateFootswitchPressed() { SetParameterRaw(HOLD, GetParameterAsBool(HOLD) ? 0 : 1); }

void UranusModule::OnNoteOn(float notenumber, float velocity) {
    // A note on with velocity 0 is a note off
    if (velocity == 0.0f) {
        OnNoteOff(notenumber, velocity);
        return;
    }
    const int synth = GetParameterAsBinnedValue(SYNTH_MODE) - 1;
    if (synth == 1) {
        m_note = notenumber; // middle C (60) is neutral: the grains are transposed up or down from there
        m_velocity = velocity;
        m_gate = true;
    } else if (synth == 2) {
        m_voices->OnNoteOn(notenumber, velocity);
    }
}

void UranusModule::OnNoteOff(float notenumber, float velocity) {
    const int synth = GetParameterAsBinnedValue(SYNTH_MODE) - 1;
    if (synth == 1) {
        m_gate = false;
    } else if (synth == 2) {
        m_voices->OnNoteOff(notenumber, velocity);
    }
}

void UranusModule::ProcessSamples(const float *in, float *outL, float *outR, size_t n) {
    // The controls are read once per call
    const float grainSize = GetParameterAsFloat(SIZE) * 300.0f + 1.0f; // 1 .. 300 ms
    const float width = GetParameterAsFloat(WIDTH) * 50.0f;            // 0 .. 50 ms
    const float feedback = std::max(GetParameterAsFloat(FEEDBACK), 0.01f);
    const float speed = GetParameterAsFloat(SPEED) * 4.0f - 2.0f;      // -2 .. +2
    const float lfoDepth = GetParameterAsFloat(LFO_DEPTH) * 100.0f;    // cents
    const int grainMode = GetParameterAsBinnedValue(GRAIN_MODE) - 1;
    const int synth = GetParameterAsBinnedValue(SYNTH_MODE) - 1;
    const bool hold = GetParameterAsBool(HOLD);
    float wetMix, dryMix;
    crossfade(GetParameterAsFloat(MIX), wetMix, dryMix);

    // Transposition in cents: the knob in semitones, plus what the two grain players add (an octave up and/or down)
    float pitch = static_cast<float>(static_cast<int>(GetParameterAsFloat(PITCH) * 26.0f - 13.0f)) * 100.0f;
    if (synth == 1) pitch = (m_note - 60.0f) * 100.0f; // the key sets the pitch: note 61 is +100 cents
    const float pitch1 = (grainMode == 2) ? -1200.0f : 0.0f;
    const float pitch2 = (grainMode == 1) ? -1200.0f : 1200.0f;

    for (size_t i = 0; i < n; i++) {
        float input = in[i];

        // FM synth mode: the synth replaces the audio input
        if (synth == 2) input = m_voices->Process() * 0.05f; // reduced volume

        // Not in hold: the input (and the feedback of what is there) goes into the buffer, which is a ring
        if (!hold) {
            s_audio[m_sampleIndex] = s_audio[m_sampleIndex] * feedback + input;
            if (++m_sampleIndex >= kMaxSample) m_sampleIndex = 0;
        }

        fonepole(m_currentGrainSize, grainSize, 0.0001f); // glide to the grain size
        const float pitchMod = m_lfo.Process() * lfoDepth; // +- depth cents

        m_swarm[0].Process(speed, pitch + pitch1 + pitchMod, m_currentGrainSize, width);
        m_swarm[1].Process(speed, pitch + pitch2 + pitchMod, m_currentGrainSize, width);
        float left = m_swarm[0].getLeftOut() + m_swarm[1].getLeftOut();
        float right = m_swarm[0].getRightOut() + m_swarm[1].getRightOut();

        // Granular synth mode: the key's envelope and velocity over the grains
        if (synth == 1) {
            const float amp = m_env.Process(m_gate) * (m_velocity / 127.0f) * 3.0f; // (3: a volume increase)
            left *= amp;
            right *= amp;
        }

        outL[i] = (left * wetMix + input * dryMix) * kOutputGain;
        outR[i] = (right * wetMix + input * dryMix) * kOutputGain;
    }
}

void UranusModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);
    float outL, outR;
    ProcessSamples(&m_audioLeft, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void UranusModule::ProcessStereo(float inL, float /*inR*/) {
    // Mono in (the left input), stereo out, as in the original
    ProcessMono(inL);
}

void UranusModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    ProcessSamples(in, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void UranusModule::ProcessStereoBlock(const float *inL, const float * /*inR*/, float *outL, float *outR, size_t n) { ProcessMonoBlock(inL, outL, outR, n); }

float UranusModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);
    if (led_id == 1) return value * (GetParameterAsBool(HOLD) ? 1.0f : 0.0f); // the second LED shows Hold
    return value;
}
