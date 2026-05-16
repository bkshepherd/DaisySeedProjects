#include "tape_delay_module.h"
#include "../Util/audio_utilities.h"
#include <array>
#include <cmath>

using namespace bkshepherd;

static const char *s_modeNames[4] = {"Fixed", "Multi", "Single", "LoFi"};
static const char *s_divisionNames[3] = {"Quarter", "Dotted8", "Triplet"};
static const char *s_headConfigNames[3] = {"Head1", "Head2", "Head3"};

DelayLine<float, TAPE_MAX_DELAY_SAMPLES> DSY_SDRAM_BSS s_tapeDelayLineL;
DelayLine<float, TAPE_MAX_DELAY_SAMPLES> DSY_SDRAM_BSS s_tapeDelayLineR;
ReverbSc DSY_SDRAM_BSS s_tapeReverb;

static const auto s_metaData = [] {
    std::array<ParameterMetaData, TapeDelayModule::PARAM_COUNT> params{};

    params[TapeDelayModule::TIME] = {
        name : "Time",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.25f},
        knobMapping : 0,
        midiCCMapping : 60
    };

    params[TapeDelayModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f},
        knobMapping : 1,
        midiCCMapping : 61
    };

    params[TapeDelayModule::REPEATS] = {
        name : "Repeats",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.15f},
        knobMapping : 2,
        midiCCMapping : 62
    };

    params[TapeDelayModule::MODE] = {
        name : "Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 4,
        valueBinNames : s_modeNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 3,
        midiCCMapping : 63
    };

    params[TapeDelayModule::WOW_FLUTTER] = {
        name : "WowFlutter",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : 4,
        midiCCMapping : 64
    };

    params[TapeDelayModule::TAPE_AGE] = {
        name : "Tape Age",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.35f},
        knobMapping : 5,
        midiCCMapping : 65
    };

    params[TapeDelayModule::TAP_DIV] = {
        name : "Tap Div",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_divisionNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 66
    };

    params[TapeDelayModule::TAPE_BIAS] = {
        name : "Tape Bias",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : -1,
        midiCCMapping : 67
    };

    params[TapeDelayModule::IMPERFECTIONS] = {
        name : "Glitches",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.2f},
        knobMapping : -1,
        midiCCMapping : 68
    };

    params[TapeDelayModule::LOW_END_CONTOUR] = {
        name : "Low End",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f},
        knobMapping : -1,
        midiCCMapping : 72
    };

    params[TapeDelayModule::REVERB_MIX] = {
        name : "Reverb",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : -1,
        midiCCMapping : 73
    };

    params[TapeDelayModule::HEAD_CONFIG] = {
        name : "Head Config",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_headConfigNames,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : 74
    };

    return params;
}();

TapeDelayModule::TapeDelayModule()
    : BaseEffectModule(), m_timeMinSamples(2400.0f), m_timeMaxSamples(192000.0f), m_mixWet(0.5f), m_mixDry(0.5f),
    m_currentDelaySamples(24000.0f), m_ageLpMin(1200.0f), m_ageLpMax(18000.0f),
      m_contourHpMin(20.0f), m_contourHpMax(350.0f), m_wowRateMin(0.15f), m_wowRateMax(2.0f), m_flutterRateMin(2.0f),
                        m_flutterRateMax(14.0f), m_wowDepthMaxSamples(55.0f), m_flutterDepthMaxSamples(16.0f), m_dropoutGain(1.0f),
            m_dropoutGainTarget(1.0f), m_crinkleOffset(0.0f), m_crinkleOffsetTarget(0.0f), m_dropoutSamplesRemaining(0.0f),
            m_crinkleSamplesRemaining(0.0f), m_imperfectionCooldownSamples(0.0f), m_randState(0xA57E1B3Fu), m_ledValue(0.0f),
            m_sampleRate(48000.0f), m_delayL(nullptr), m_delayR(nullptr), m_reverb(nullptr) {
    m_name = "Tape Delay";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

TapeDelayModule::~TapeDelayModule() {}

void TapeDelayModule::UpdateMix() {
    float mixKnob = m_smoothedMix;
    float x2 = 1.0f - mixKnob;
    float A = mixKnob * x2;
    float B = A * (1.0f + 1.4186f * A);
    float C = B + mixKnob;
    float D = B + x2;

    m_mixWet = C * C;
    m_mixDry = D * D;
}

bool TapeDelayModule::IsLoFiMode() const {
    // Binned values in this framework are 1..N, so convert to zero-based here.
    return (GetParameterAsBinnedValue(MODE) - 1) == 3;
}

float TapeDelayModule::ApplySafetyLimiter(float sample) const {
    constexpr float drive = 1.6f;
    return tanhf(sample * drive) / tanhf(drive);
}

void TapeDelayModule::UpdateDelayTimeAndLed() {
    float time = m_smoothedTime;

    if (IsLoFiMode()) {
        constexpr float lofiMinSamples = 24.0f;   // 0.5 ms @ 48k
        constexpr float lofiMaxSamples = 480.0f;  // 10 ms @ 48k
        float t = time * time;
        m_currentDelaySamples = lofiMinSamples + (lofiMaxSamples - lofiMinSamples) * t;

        float wobble = GetParameterAsFloat(WOW_FLUTTER);
        m_ledOsc.SetFreq(0.5f + wobble * 4.0f);
    } else {
        m_currentDelaySamples = m_timeMinSamples + (m_timeMaxSamples - m_timeMinSamples) * time;

        float ledFreq = m_sampleRate / fmaxf(1.0f, m_currentDelaySamples);
        m_ledOsc.SetFreq(ledFreq * 0.5f);
    }

    m_ledValue = m_ledOsc.Process();
}

float TapeDelayModule::GetDivisionMultiplier() const {
    // Match tape-machine semantics: Tap Div only applies to Single mode.
    // Fixed and Multi derive rhythm from head selections.
    int mode = GetParameterAsBinnedValue(MODE) - 1;
    if (mode != 2) {
        return 1.0f;
    }

    int div = GetParameterAsBinnedValue(TAP_DIV) - 1;
    switch (div) {
    case 1:
        return 0.75f;
    case 2:
        return 2.0f / 3.0f;
    default:
        return 1.0f;
    }
}

float TapeDelayModule::GetWowFlutterOffset() {
    float wowFlutter = GetParameterAsFloat(WOW_FLUTTER);
    float extreme = fmaxf(0.0f, (wowFlutter - 0.65f) / 0.35f);
    float extremeShaped = extreme * extreme;

    float wowRate = m_wowRateMin + (m_wowRateMax - m_wowRateMin) * wowFlutter;
    float flutterRate = m_flutterRateMin + (m_flutterRateMax - m_flutterRateMin) * wowFlutter;

    // Push upper range into unstable/broken-cassette territory.
    wowRate += 2.2f * extremeShaped;
    flutterRate += 18.0f * extremeShaped;

    float wowDepth;
    float flutterDepth;
    if (IsLoFiMode()) {
        wowDepth = wowFlutter * wowFlutter * 9.0f + 20.0f * extremeShaped;
        flutterDepth = wowFlutter * 2.8f + 8.0f * extremeShaped;
    } else {
        wowDepth = wowFlutter * wowFlutter * m_wowDepthMaxSamples + 40.0f * extremeShaped;
        flutterDepth = wowFlutter * m_flutterDepthMaxSamples + 22.0f * extremeShaped;
    }

    float flutterMix = 0.2f + 0.5f * extremeShaped;
    return m_tapeMod.GetTapeSpeed(wowRate, flutterRate, wowDepth, flutterDepth, flutterMix);
}

float TapeDelayModule::Random01() {
    m_randState = 1664525u * m_randState + 1013904223u;
    return static_cast<float>(m_randState & 0x00FFFFFFu) / 16777215.0f;
}

void TapeDelayModule::UpdateImperfections(float amount, float &dropoutGain, float &crinkleOffset) {
    float amt = fmaxf(0.0f, fminf(amount, 1.0f));

    float dropoutRateHz = 0.02f + 0.8f * amt;
    float crinkleRateHz = 0.05f + 1.4f * amt;
    float spliceRateHz = 0.002f + 0.08f * amt;

    float dropoutChance = dropoutRateHz / m_sampleRate;
    float crinkleChance = crinkleRateHz / m_sampleRate;
    float spliceChance = spliceRateHz / m_sampleRate;

    if (m_dropoutSamplesRemaining <= 0.0f && Random01() < dropoutChance) {
        float durationMs = 8.0f + Random01() * (20.0f + 90.0f * amt);
        m_dropoutSamplesRemaining = durationMs * 0.001f * m_sampleRate;
        float depth = amt * (0.08f + 0.60f * Random01());
        m_dropoutGainTarget = 1.0f - depth;
    }

    if (m_crinkleSamplesRemaining <= 0.0f && Random01() < crinkleChance) {
        float durationMs = 2.0f + Random01() * (6.0f + 24.0f * amt);
        m_crinkleSamplesRemaining = durationMs * 0.001f * m_sampleRate;
        float maxOffset = 0.35f + amt * 6.0f;
        m_crinkleOffsetTarget = (Random01() * 2.0f - 1.0f) * maxOffset;
    }

    if (m_imperfectionCooldownSamples > 0.0f) {
        m_imperfectionCooldownSamples -= 1.0f;
    } else if (Random01() < spliceChance) {
        // Rare abrupt splice-like jump layered on top of smoothed crinkle motion.
        float spliceOffset = (Random01() * 2.0f - 1.0f) * (0.7f + 9.0f * amt);
        m_crinkleOffset = spliceOffset;
        m_crinkleOffsetTarget = spliceOffset;
        m_crinkleSamplesRemaining = (1.0f + Random01() * 5.0f) * 0.001f * m_sampleRate;
        m_imperfectionCooldownSamples = (120.0f + Random01() * 600.0f) * 0.001f * m_sampleRate;
    }

    if (m_dropoutSamplesRemaining > 0.0f) {
        m_dropoutSamplesRemaining -= 1.0f;
    } else {
        m_dropoutGainTarget = 1.0f;
    }

    if (m_crinkleSamplesRemaining > 0.0f) {
        m_crinkleSamplesRemaining -= 1.0f;
    } else {
        m_crinkleOffsetTarget = 0.0f;
    }

    fonepole(m_dropoutGain, m_dropoutGainTarget, 0.003f);
    fonepole(m_crinkleOffset, m_crinkleOffsetTarget, 0.02f);

    dropoutGain = fmaxf(0.0f, fminf(m_dropoutGain, 1.0f));
    crinkleOffset = m_crinkleOffset;
}

void TapeDelayModule::GetHeadMix(float baseSamples, DelayLine<float, TAPE_MAX_DELAY_SAMPLES> &delay, float &out) const {
    // Binned values in this framework are 1..N, so convert to zero-based here.
    int mode = GetParameterAsBinnedValue(MODE) - 1;
    int headCfg = GetParameterAsBinnedValue(HEAD_CONFIG) - 1;

    if (mode == 3) {
        out = delay.Read(baseSamples);
        return;
    }

    float h1 = baseSamples * 0.5f;
    float h2 = baseSamples * 1.0f;
    float h3 = baseSamples * 1.5f;

    // Fixed machine: one playback head selected at a time.
    if (mode == 0) {
        if (headCfg == 0) {
            out = delay.Read(h1);
        } else if (headCfg == 1) {
            out = delay.Read(h2);
        } else {
            out = delay.Read(h3);
        }
        return;
    }

    // Multi machine: two playback heads selected as 1+2, 2+3, or 1+3.
    if (mode == 1) {
        if (headCfg == 0) {
            out = 0.5f * delay.Read(h1) + 0.5f * delay.Read(h2);
        } else if (headCfg == 1) {
            out = 0.5f * delay.Read(h2) + 0.5f * delay.Read(h3);
        } else {
            out = 0.5f * delay.Read(h1) + 0.5f * delay.Read(h3);
        }
        return;
    }

    // Single machine: use Head Config as tape speed/fidelity variants.
    // A=fast/cleaner, B=normal, C=slow/darker feel (longer effective delay).
    float singleSamples = baseSamples;
    if (headCfg == 0) {
        singleSamples = baseSamples * 0.75f;
    } else if (headCfg == 2) {
        singleSamples = baseSamples * 1.35f;
    }

    singleSamples = fmaxf(1.0f, fminf(singleSamples, static_cast<float>(TAPE_MAX_DELAY_SAMPLES - 2)));
    out = delay.Read(singleSamples);
}

float TapeDelayModule::ProcessChannel(float input, float speedMod, float dropoutGain, float crinkleOffset, float age,
                                      bool isLoFi,
                                      DelayLine<float, TAPE_MAX_DELAY_SAMPLES> &delay, Tone &tone, Svf &hp) {
    float division = isLoFi ? 1.0f : GetDivisionMultiplier();
    float baseSamples = m_currentDelaySamples * division + speedMod + crinkleOffset;
    float minDelay = isLoFi ? 2.0f : 1.0f;
    baseSamples = fmaxf(minDelay, fminf(baseSamples, static_cast<float>(TAPE_MAX_DELAY_SAMPLES - 2)));

    float wet = 0.0f;
    GetHeadMix(baseSamples, delay, wet);

    float ageShaped = age * age;
    float lpFreq;
    if (isLoFi) {
        constexpr float lofiLpMin = 900.0f;
        constexpr float lofiLpMax = 12000.0f;
        lpFreq = lofiLpMax - (lofiLpMax - lofiLpMin) * ageShaped;
    } else {
        lpFreq = m_ageLpMax - (m_ageLpMax - m_ageLpMin) * ageShaped;
    }
    tone.SetFreq(lpFreq);

    float contour = GetParameterAsFloat(LOW_END_CONTOUR);
    float hpFreq;
    if (isLoFi) {
        constexpr float lofiHpMin = 30.0f;
        constexpr float lofiHpMax = 700.0f;
        hpFreq = lofiHpMin + (lofiHpMax - lofiHpMin) * contour;
    } else {
        hpFreq = m_contourHpMin + (m_contourHpMax - m_contourHpMin) * contour;
    }
    hp.SetFreq(hpFreq);

    float filteredWet = tone.Process(wet);
    hp.Process(filteredWet);
    filteredWet = hp.High();

    float repeats = m_smoothedRepeats;
    float feedback;
    if (isLoFi) {
        feedback = 0.0f; // LoFi mode uses a very short delay at high drive - feedback is dangerous
    } else {
        feedback = 0.003f + repeats * 0.20f;
    }

    float bias = GetParameterAsFloat(TAPE_BIAS);
    float drive = isLoFi ? (1.0f + bias * 7.0f) : (1.0f + bias * 4.0f);
    float playbackWet = ApplySafetyLimiter(filteredWet * dropoutGain);

    // Dynamically back off feedback when wet energy rises to prevent dangerous level build-up.
    float wetEnergyDamping = 1.0f / (1.0f + 0.65f * fabsf(playbackWet));
    feedback *= wetEnergyDamping;

    // Normalise by drive (not tanhf(drive)) so small-signal gain is unity.
    // tanhf(x*d)/tanhf(d) has gain = d/tanhf(d) > 1, causing the wet to sound louder than dry.
    // tanhf(x*d)/d has derivative 1 at the origin — unity gain — and softclips harder as drive rises.
    float sat = tanhf((input + feedback * playbackWet) * drive) / drive;

    delay.Write(sat);

    return playbackWet;
}

void TapeDelayModule::ResetInternalState() {
    if (m_delayL != nullptr) {
        m_delayL->Init();
    }
    if (m_delayR != nullptr) {
        m_delayR->Init();
    }
    if (m_reverb != nullptr) {
        m_reverb->Init(m_sampleRate);
        m_reverb->SetFeedback(0.85f);
        m_reverb->SetLpFreq(9000.0f);
    }

    m_tapeMod.Init(m_sampleRate);

    m_dropoutGain = 1.0f;
    m_dropoutGainTarget = 1.0f;
    m_crinkleOffset = 0.0f;
    m_crinkleOffsetTarget = 0.0f;
    m_dropoutSamplesRemaining = 0.0f;
    m_crinkleSamplesRemaining = 0.0f;
    m_imperfectionCooldownSamples = 0.0f;
    m_smoothedTime = GetParameterAsFloat(TIME);
    m_smoothedMix = GetParameterAsFloat(MIX);
    m_smoothedRepeats = GetParameterAsFloat(REPEATS);
}

void TapeDelayModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    m_sampleRate = sample_rate;

    m_delayL = &s_tapeDelayLineL;
    m_delayR = &s_tapeDelayLineR;
    m_reverb = &s_tapeReverb;

    m_delayL->Init();
    m_delayR->Init();

    m_toneL.Init(sample_rate);
    m_toneR.Init(sample_rate);

    m_hpL.Init(sample_rate);
    m_hpR.Init(sample_rate);
    m_hpL.SetRes(0.1f);
    m_hpR.SetRes(0.1f);

    m_reverb->Init(sample_rate);
    m_reverb->SetFeedback(0.85f);
    m_reverb->SetLpFreq(9000.0f);

    m_tapeMod.Init(sample_rate);

    m_ledOsc.Init(sample_rate);
    m_ledOsc.SetWaveform(Oscillator::WAVE_SQUARE);
    m_ledOsc.SetAmp(1.0f);
    m_ledOsc.SetFreq(2.0f);

    UpdateMix();
    ResetInternalState();
}

void TapeDelayModule::ParameterChanged(int parameter_id) {
    if (parameter_id == MIX) {
        UpdateMix();
    }
}

void TapeDelayModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    // Smooth critical parameters to avoid zipper noise from knob/automation changes
    fonepole(m_smoothedTime, GetParameterAsFloat(TIME), 0.02f);
    fonepole(m_smoothedMix, GetParameterAsFloat(MIX), 0.02f);
    fonepole(m_smoothedRepeats, fminf(GetParameterAsFloat(REPEATS), 0.65f), 0.02f);

    UpdateDelayTimeAndLed();
    bool isLoFi = IsLoFiMode();

    float speedMod = GetWowFlutterOffset();
    float dropoutGain = 1.0f;
    float crinkleOffset = 0.0f;
    float age = GetParameterAsFloat(TAPE_AGE);
    UpdateImperfections(GetParameterAsFloat(IMPERFECTIONS), dropoutGain, crinkleOffset);

    float wetL = ProcessChannel(m_audioLeft, speedMod, dropoutGain, crinkleOffset, age, isLoFi, *m_delayL, m_toneL, m_hpL);
    float wetR = ProcessChannel(m_audioRight, speedMod, dropoutGain, crinkleOffset, age, isLoFi, *m_delayR, m_toneR, m_hpR);

    float reverbMix = GetParameterAsFloat(REVERB_MIX);
    float reverbL = 0.0f;
    float reverbR = 0.0f;
    m_reverb->Process(wetL, wetR, &reverbL, &reverbR);

    float wetOutL = wetL * (1.0f - reverbMix) + reverbL * reverbMix;
    float wetOutR = wetR * (1.0f - reverbMix) + reverbR * reverbMix;

    wetOutL = ApplySafetyLimiter(wetOutL * 0.9f);
    wetOutR = ApplySafetyLimiter(wetOutR * 0.9f);

    m_audioLeft = wetOutL * m_mixWet + m_audioLeft * m_mixDry;
    m_audioRight = wetOutR * m_mixWet + m_audioRight * m_mixDry;
}

void TapeDelayModule::ProcessStereo(float inL, float inR) {
    BaseEffectModule::ProcessStereo(inL, inR);

    // Smooth critical parameters to avoid zipper noise from knob/automation changes
    fonepole(m_smoothedTime, GetParameterAsFloat(TIME), 0.02f);
    fonepole(m_smoothedMix, GetParameterAsFloat(MIX), 0.02f);
    fonepole(m_smoothedRepeats, fminf(GetParameterAsFloat(REPEATS), 0.65f), 0.02f);

    UpdateDelayTimeAndLed();
    bool isLoFi = IsLoFiMode();

    float speedMod = GetWowFlutterOffset();
    float dropoutGain = 1.0f;
    float crinkleOffset = 0.0f;
    float age = GetParameterAsFloat(TAPE_AGE);
    UpdateImperfections(GetParameterAsFloat(IMPERFECTIONS), dropoutGain, crinkleOffset);

    float wetL = ProcessChannel(m_audioLeft, speedMod, dropoutGain, crinkleOffset, age, isLoFi, *m_delayL, m_toneL, m_hpL);
    float wetR = ProcessChannel(m_audioRight, speedMod, dropoutGain, crinkleOffset, age, isLoFi, *m_delayR, m_toneR, m_hpR);

    float reverbMix = GetParameterAsFloat(REVERB_MIX);
    float reverbL = 0.0f;
    float reverbR = 0.0f;
    m_reverb->Process(wetL, wetR, &reverbL, &reverbR);

    float wetOutL = wetL * (1.0f - reverbMix) + reverbL * reverbMix;
    float wetOutR = wetR * (1.0f - reverbMix) + reverbR * reverbMix;

    wetOutL = ApplySafetyLimiter(wetOutL * 0.9f);
    wetOutR = ApplySafetyLimiter(wetOutR * 0.9f);

    m_audioLeft = wetOutL * m_mixWet + m_audioLeft * m_mixDry;
    m_audioRight = wetOutR * m_mixWet + m_audioRight * m_mixDry;
}

void TapeDelayModule::SetEnabled(bool isEnabled) {
    bool wasEnabled = IsEnabled();
    BaseEffectModule::SetEnabled(isEnabled);

    // Re-entering the effect should always start from silence to avoid stale, loud tails.
    if (isEnabled && !wasEnabled) {
        ResetInternalState();
    }
}

void TapeDelayModule::SetTempo(uint32_t bpm) {
    if (IsLoFiMode()) {
        return;
    }

    float freq = tempo_to_freq(bpm);
    float delaySamples = m_sampleRate / freq;

    float magnitude = (delaySamples - m_timeMinSamples) / (m_timeMaxSamples - m_timeMinSamples);
    magnitude = fmaxf(0.0f, fminf(1.0f, magnitude));
    SetParameterAsMagnitude(TIME, magnitude);
}

float TapeDelayModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);
    float ledGate = (m_ledValue > 0.45f) ? 1.0f : 0.0f;

    if (led_id == 1) {
        return value * ledGate;
    }

    return value;
}

int TapeDelayModule::GetMappedParameterIDForKnob(int knob_id) const {
    if (m_shiftModeActive) {
        switch (knob_id) {
        case 0: return TAP_DIV;        // Knob 0 -> Tap Division
        case 1: return TAPE_BIAS;      // Knob 1 -> Tape Bias
        case 2: return IMPERFECTIONS;  // Knob 2 -> Imperfections
        case 3: return LOW_END_CONTOUR;// Knob 3 -> Low End Contour
        case 4: return REVERB_MIX;     // Knob 4 -> Reverb Mix
        case 5: return HEAD_CONFIG;    // Knob 5 -> Head Config
        default: return -1;
        }
    }
    return BaseEffectModule::GetMappedParameterIDForKnob(knob_id);
}

void TapeDelayModule::AlternateFootswitchHeldFor1Second() {
    m_shiftModeActive = !m_shiftModeActive;
}

void TapeDelayModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal,
                             Rectangle boundsToDrawIn, bool isEditing) {
    BaseEffectModule::DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn, isEditing);

    if (m_shiftModeActive) {
        display.SetCursor(boundsToDrawIn.GetRight() - 40, 2);
        display.WriteString("SHIFT", Font_6x8, true);
    }
}
