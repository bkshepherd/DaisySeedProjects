// SPDX-License-Identifier: GPL-3.0-or-later
//
// Earth: a plate reverb with an octave effect, adapted from the Earth pedal of the FunBox project by GuitarML (https://github.com/GuitarML/Funbox,
// software/Earth, GPL-3.0-or-later, Copyright (c) 2024 Keith Bloemer). See earth_module.h and Earth/README.md for the licence, the acknowledgements and
// the list of changes.
#include "earth_module.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <span>

using namespace bkshepherd;
namespace q = cycfi::q;
using namespace q::literals;

namespace {

// SDRAM for the reverb's delay lines (the engine carves every InterpDelay buffer from it); sized with headroom for the largest Size at 48 kHz and the
// 700 ms pre-delay.
constexpr size_t kArenaFloatCount = 524288; // 2 MiB / sizeof(float)
float DSY_SDRAM_BSS s_earthArena[kArenaFloatCount];
constexpr unsigned int kMaxPreDelaySamples = 33600; // 700 ms at 48 kHz

// The original runs its reverb engine at 48 kHz, but that engine never wired up its maximum sample rate, so the tank ran with 32 kHz time scales (about two
// thirds as long as designed). The vendored engine here has that fixed, so the Size and the modulation depth are scaled by 32000 / sample rate to keep Earth's
// room sizes. (Checked against Earth's own engine in impulse responses: the tank's first echo lands on the same sample. Its input diffusion network is
// not covered by Size and stays at the true 48 kHz scale, about 1.5 times longer than the original's.)
constexpr float kOriginalTankRate = 32000.0f;

const float kSizeScale[3] = {1.0f, 2.0f, 4.0f}; // small, medium, big
constexpr float kOdMin = 0.4f;                  // overdrive drive at rest, and the target the swell returns to
constexpr float kOdMax = 0.6f;                  // overdrive drive while the footswitch is held
constexpr float kWetLevel = 0.4f;               // overall level of the reverb relative to the dry signal, as in the original
// Output makeup gain, so that the effect is not quieter than bypass at its default settings (calibrated with tests/host/run_level_tests.sh)
constexpr float kOutputGain = 1.4f; // +2.9 dB: about +2 dB at the default settings (it measured about -1 dB without it)

const char *s_sizeNames[3] = {"Small", "Medium", "Big"};
const char *m_octaveNames[3] = {"None", "Up", "Up+Down"};
const char *s_fsActionNames[3] = {"Freeze", "Overdrive", "Octave"};

} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, EarthModule::PARAM_COUNT> params{};

    params[EarthModule::PRE_DELAY] = { // 0..1 maps to 0..700 ms
        name : "PreDelay",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 0,
        midiCCMapping : 14
    };

    params[EarthModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f},
        knobMapping : 1,
        midiCCMapping : 15
    };

    params[EarthModule::DECAY] = {
        name : "Decay",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8f},
        knobMapping : 2,
        midiCCMapping : 16
    };

    params[EarthModule::MOD_DEPTH] = {
        name : "Mod Depth",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : 3,
        midiCCMapping : 17
    };

    params[EarthModule::MOD_SPEED] = {
        name : "Mod Speed",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : 4,
        midiCCMapping : 18
    };

    params[EarthModule::FILTER] = { // a low-pass on the reverb input: 0 is open, 1 is dark
        name : "Filter",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.2f},
        knobMapping : 5,
        midiCCMapping : 19
    };

    params[EarthModule::SIZE] = {
        name : "Size",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_sizeNames,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : 20
    };

    params[EarthModule::OCTAVE] = {
        name : "Octave",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : m_octaveNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 21
    };

    params[EarthModule::FS_ACTION] = {
        name : "FS Action",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_fsActionNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 22
    };

    params[EarthModule::IN_DIFFUSION] = {
        name : "In Diffuse",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : 23
    };

    params[EarthModule::OCT_ONLY] = { // with an octave on, send only the octave (no dry signal) to the reverb
        name : "Oct Only",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 24
    };

    return params;
}();

EarthModule::EarthModule()
    : BaseEffectModule(), m_arenaExhausted(false), m_octave(kOctaveSampleRate / resample_factor), m_eq1(-11, 140_Hz, kOctaveSampleRate),
      m_eq2(5, 160_Hz, kOctaveSampleRate), m_binCounter(0), m_odOn(false), m_odSwell(kOdMin), m_odTarget(kOdMin), m_preDelay(0.0f), m_modDepth(0.0f), m_modSpeed(0.0f),
      m_decay(0.0f), m_sampleRate(48000.0f), m_rateRatio(kOriginalTankRate / 48000.0f), m_footswitchHeld(false), m_freeze(false), m_octaveMomentary(false) {
    m_name = "Earth";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

EarthModule::~EarthModule() {}

void EarthModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    m_sampleRate = sample_rate;
    m_rateRatio = kOriginalTankRate / sample_rate;

    // Arm the SDRAM arena while the engine is constructed (every delay line it owns carves its buffer from it), then detach it.
    InterpDelayArena::set(s_earthArena, kArenaFloatCount);
    m_dattorro = std::make_unique<Dattorro>(sample_rate, 16.0f, 4.0f, kMaxPreDelaySamples); // sample rate, max LFO depth, max time scale, pre-delay length
    m_arenaExhausted = InterpDelayArena::exhausted();
    InterpDelayArena::set(nullptr, 0);

    // Fixed settings of the original that are not controls
    m_dattorro->setInputFilterLowCutoffPitch(0.0f);
    m_dattorro->setTankDiffusion(0.7f);
    m_dattorro->setTankFilterLowCutFrequency(0.0f);
    m_dattorro->setTankFilterHighCutFrequency(10.0f);
    m_dattorro->setTankModShape(0.5f);

    for (int i = 0; i < 2; i++) {
        m_overdrive[i].Init();
        m_overdrive[i].SetDrive(kOdMin);
    }
    m_odOn = false;
    m_odSwell = m_odTarget = kOdMin;

    for (size_t j = 0; j < resample_factor; ++j) {
        m_buff[j] = 0.0f;
        m_buffOut[j] = 0.0f;
    }
    m_binCounter = 0;

    // The smoothed values start at the controls, so there is no glide from zero at startup
    m_preDelay = GetParameterAsFloat(PRE_DELAY) * 0.7f;
    m_modDepth = GetParameterAsFloat(MOD_DEPTH);
    m_modSpeed = GetParameterAsFloat(MOD_SPEED);
    m_decay = GetParameterAsFloat(DECAY);

    // ParameterChanged only fires on a change, so apply the defaults to the engine here
    ApplyEngineSettings();
    m_dattorro->setPreDelay(m_preDelay);
    m_dattorro->setTankModDepth(m_modDepth * 8.0f * m_rateRatio);
    m_dattorro->setTankModSpeed(0.3f + m_modSpeed * 15.0f);
    m_dattorro->setDecay(m_decay);
    m_dattorro->clear();
}

void EarthModule::ParameterChanged(int parameter_id) {
    if (parameter_id == SIZE || parameter_id == FILTER || parameter_id == IN_DIFFUSION) {
        ApplyEngineSettings();
    }
}

// The controls that are handed to the engine only when they change (the others are smoothed and handed over once per call)
void EarthModule::ApplyEngineSettings() {
    if (!m_dattorro) return;
    const int size = std::min(2, std::max(0, GetParameterAsBinnedValue(SIZE) - 1));
    m_dattorro->setTimeScale(kSizeScale[size] * m_rateRatio);
    m_dattorro->setInputFilterHighCutoffPitch(10.0f - 7.0f * GetParameterAsFloat(FILTER)); // a low-pass on the input works best (original comment)
    m_dattorro->enableInputDiffusion(GetParameterAsBool(IN_DIFFUSION));
}

// Moves the smoothed values towards the controls by n samples (the original does one step per sample), then hands them to the engine once
void EarthModule::UpdateSmoothedParameters(size_t n) {
    const float preDelayTarget = GetParameterAsFloat(PRE_DELAY) * 0.7f;
    const float modDepthTarget = GetParameterAsFloat(MOD_DEPTH);
    const float modSpeedTarget = GetParameterAsFloat(MOD_SPEED);
    const float decayTarget = m_freeze ? 1.0f : GetParameterAsFloat(DECAY); // frozen: the decay glides to 1.0 and back

    for (size_t i = 0; i < n; i++) {
        fonepole(m_preDelay, preDelayTarget, 0.0002f);
        fonepole(m_modDepth, modDepthTarget, 0.0002f);
        fonepole(m_modSpeed, modSpeedTarget, 0.0002f);
        fonepole(m_decay, decayTarget, 0.0002f);

        if (m_odOn) {
            fonepole(m_odSwell, m_odTarget, 0.000015f); // gradually swell the overdrive up, and back down after releasing the footswitch
            if (m_odSwell < kOdMin + 0.01f && !m_footswitchHeld) {
                m_odOn = false; // off once the drive has dropped back down
            }
        }
    }

    m_dattorro->setPreDelay(m_preDelay);
    m_dattorro->setTankModDepth(m_modDepth * 8.0f * m_rateRatio);
    m_dattorro->setTankModSpeed(0.3f + m_modSpeed * 15.0f);
    m_dattorro->setDecay(m_decay);
    if (m_odOn) {
        m_overdrive[0].SetDrive(m_odSwell);
        m_overdrive[1].SetDrive(m_odSwell);
    }
}

void EarthModule::AlternateFootswitchPressed() {
    if (m_footswitchHeld) return;
    m_footswitchHeld = true;
    const int action = GetParameterAsBinnedValue(FS_ACTION) - 1;
    if (action == 0) {
        m_freeze = true;
    } else if (action == 1) {
        m_odTarget = kOdMax;
        m_odOn = true;
    } else {
        m_octaveMomentary = true;
    }
}

void EarthModule::AlternateFootswitchReleased() {
    m_footswitchHeld = false;
    m_freeze = false;
    m_odTarget = kOdMin;
    m_octaveMomentary = false;
}

void EarthModule::ProcessSamples(const float *in, float *outL, float *outR, size_t n) {
    if (m_arenaExhausted || !m_dattorro) {
        // Degrade to dry rather than risk the small shared heap
        for (size_t i = 0; i < n; i++) outL[i] = outR[i] = in[i];
        return;
    }

    UpdateSmoothedParameters(n);

    // A cheap, mostly energy-constant crossfade (from the SignalSmith blog)
    const float mix = GetParameterAsFloat(MIX);
    const float x2 = 1.0f - mix;
    const float A = mix * x2;
    const float B = A * (1.0f + 1.4186f * A);
    const float C = B + mix;
    const float D = B + x2;
    const float wetMix = C * C;
    const float dryMix = D * D;

    const int octaveMode = GetParameterAsBinnedValue(OCTAVE) - 1; // 0 = none, 1 = up, 2 = up and down
    const int fsAction = GetParameterAsBinnedValue(FS_ACTION) - 1;
    const bool octaveOnly = GetParameterAsBool(OCT_ONLY);

    for (size_t i = 0; i < n; i++) {
        const float inputL = in[i];

        // The octave runs before the reverb, in chunks of resample_factor samples (so it adds that much latency to the octave sound)
        m_buff[m_binCounter] = inputL;
        if (m_binCounter > 4 && octaveMode != 0) {
            std::span<const float, resample_factor> in_chunk(&(m_buff[0]), resample_factor);
            const auto sample = m_decimate(in_chunk);

            float octave_mix = 0.0f;
            m_octave.update(sample);
            octave_mix += m_octave.up1() * 2.0f;
            if (octaveMode == 2) {
                octave_mix += m_octave.down1() * 2.0f;
                octave_mix += m_octave.down2() * 2.0f;
            }

            auto out_chunk = m_interpolate(octave_mix);
            for (size_t j = 0; j < out_chunk.size(); ++j) {
                float mixed = m_eq2(m_eq1(out_chunk[j]));
                const float dryLevel = 0.5f;
                if (!octaveOnly || octaveMode == 2) mixed += dryLevel * m_buff[j]; // never leave the dry signal out in "up and down"
                m_buffOut[j] = mixed;
            }
        }
        m_binCounter += 1;
        if (m_binCounter > 5) m_binCounter = 0;

        float reverbIn = inputL;
        if (octaveMode != 0) { // octave up, or up and down
            if ((fsAction == 2 && m_octaveMomentary) || fsAction != 2) {
                reverbIn = m_buffOut[m_binCounter];
            }
        }

        m_dattorro->process(reverbIn, reverbIn);

        const float reverbLeft = m_dattorro->getLeftOutput();
        const float reverbRight = m_dattorro->getRightOutput();
        float effectLeft = reverbLeft;
        float effectRight = reverbRight;
        if (m_odOn) {
            // Momentary overdrive swell. The volume is reduced as the drive goes up (otherwise way too loud).
            const float gain = 1.0f - (m_odSwell * m_odSwell * 2.8f - 0.1296f);
            effectLeft = m_overdrive[0].Process(reverbLeft * 0.25f) * gain;
            effectRight = m_overdrive[1].Process(reverbRight * 0.25f) * gain;
        }

        outL[i] = (inputL * dryMix + effectLeft * wetMix * kWetLevel) * kOutputGain;
        outR[i] = (inputL * dryMix + effectRight * wetMix * kWetLevel) * kOutputGain;
    }
}

void EarthModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    float outL, outR;
    ProcessSamples(&in, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void EarthModule::ProcessStereo(float inL, float /*inR*/) {
    // Mono in (the left input), stereo out
    ProcessMono(inL);
}

void EarthModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    ProcessSamples(in, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void EarthModule::ProcessStereoBlock(const float *inL, const float * /*inR*/, float *outL, float *outR, size_t n) {
    ProcessMonoBlock(inL, outL, outR, n);
}

float EarthModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    // The second LED lights while the footswitch action is held
    if (led_id == 1) {
        return value * (m_footswitchHeld ? 1.0f : 0.0f);
    }

    return value;
}
