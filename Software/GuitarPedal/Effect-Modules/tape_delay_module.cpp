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
        midiCCMapping : 60,
        unit : " ms",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Lin(v, 50.0f, 4000.0f); }
    };

    params[TapeDelayModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f},
        knobMapping : 1,
        midiCCMapping : 61,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[TapeDelayModule::REPEATS] = {
        name : "Repeats",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.15f},
        knobMapping : 2,
        midiCCMapping : 62,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v * 0.98f); }
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
        midiCCMapping : 65,
        unit : " Hz",
        decimals : 0,
        displayFn : [](float v) { return 18000.0f - (18000.0f - 1200.0f) * v * v; }
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
        midiCCMapping : 68,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[TapeDelayModule::LOW_END_CONTOUR] = {
        name : "Low End",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f},
        knobMapping : -1,
        midiCCMapping : 72,
        unit : " Hz",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Lin(v, 20.0f, 350.0f); }
    };

    params[TapeDelayModule::REVERB_MIX] = {
        name : "Reverb",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : -1,
        midiCCMapping : 73,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
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
            m_crinkleSamplesRemaining(0.0f), m_imperfectionCooldownSamples(0.0f), m_randState(0xA57E1B3Fu), m_smoothedTime(0.0f), m_smoothedMix(0.0f), m_smoothedRepeats(0.0f), m_ledValue(0.0f),
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
    // Unity small-signal gain, soft asymptote — see in-loop saturator note below.
    constexpr float drive = 1.6f;
    return tanhf(sample * drive) / drive;
}

void TapeDelayModule::UpdateDelayTimeAndLed(bool isLoFi, float wobble) {
    float time = m_smoothedTime;

    if (isLoFi) {
        constexpr float lofiMinSamples = 24.0f;   // 0.5 ms @ 48k
        constexpr float lofiMaxSamples = 480.0f;  // 10 ms @ 48k
        float t = time * time;
        m_currentDelaySamples = lofiMinSamples + (lofiMaxSamples - lofiMinSamples) * t;

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

TapeDelayModule::WowParams TapeDelayModule::ComputeWowParams(float rateScale, bool isLoFi) const {
    float wowFlutter = GetParameterAsFloat(WOW_FLUTTER);
    float extreme = fmaxf(0.0f, (wowFlutter - 0.65f) / 0.35f);
    float extremeShaped = extreme * extreme;

    float wowRate = m_wowRateMin + (m_wowRateMax - m_wowRateMin) * wowFlutter;
    float flutterRate = m_flutterRateMin + (m_flutterRateMax - m_flutterRateMin) * wowFlutter;

    // Push upper range into unstable/broken-cassette territory.
    wowRate += 2.2f * extremeShaped;
    flutterRate += 18.0f * extremeShaped;

    // Per-channel rate scaling so L/R modulation diverges and widens the stereo image.
    wowRate *= rateScale;
    flutterRate *= rateScale;

    float wowDepth;
    float flutterDepth;
    if (isLoFi) {
        wowDepth = wowFlutter * wowFlutter * 9.0f + 20.0f * extremeShaped;
        flutterDepth = wowFlutter * 2.8f + 8.0f * extremeShaped;
    } else {
        wowDepth = wowFlutter * wowFlutter * m_wowDepthMaxSamples + 40.0f * extremeShaped;
        flutterDepth = wowFlutter * m_flutterDepthMaxSamples + 22.0f * extremeShaped;
    }

    float flutterMix = 0.2f + 0.5f * extremeShaped;
    return WowParams{wowRate, flutterRate, wowDepth, flutterDepth, flutterMix};
}

float TapeDelayModule::Random01() {
    m_randState = 1664525u * m_randState + 1013904223u;
    return static_cast<float>(m_randState & 0x00FFFFFFu) / 16777215.0f;
}

TapeDelayModule::ImperfectionParams TapeDelayModule::ComputeImperfectionParams(float amount) const {
    float amt = fmaxf(0.0f, fminf(amount, 1.0f));

    float dropoutRateHz = 0.02f + 0.8f * amt;
    float crinkleRateHz = 0.05f + 1.4f * amt;
    float spliceRateHz = 0.002f + 0.08f * amt;

    float dropoutChance = dropoutRateHz / m_sampleRate;
    float crinkleChance = crinkleRateHz / m_sampleRate;
    float spliceChance = spliceRateHz / m_sampleRate;
    return ImperfectionParams{amt, dropoutChance, crinkleChance, spliceChance};
}

void TapeDelayModule::UpdateImperfections(const ImperfectionParams &p, float &dropoutGain, float &crinkleOffset) {
    const float amt = p.amt;
    const float dropoutChance = p.dropoutChance;
    const float crinkleChance = p.crinkleChance;
    const float spliceChance = p.spliceChance;

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

void TapeDelayModule::GetHeadMix(float baseSamples, DelayLine<float, TAPE_MAX_DELAY_SAMPLES> &delay, float &out, int mode,
                                 int headCfg) const {
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

float TapeDelayModule::ProcessChannel(float input, float speedMod, float dropoutGain, float crinkleOffset, const ChannelParams &cp,
                                      DelayLine<float, TAPE_MAX_DELAY_SAMPLES> &delay, Tone &tone, Svf &hp) {
    // The tone and high-pass frequencies depend only on parameters, so ProcessSamples() sets them once per call.
    float baseSamples = m_currentDelaySamples * cp.division + speedMod + crinkleOffset;
    baseSamples = fmaxf(cp.minDelay, fminf(baseSamples, static_cast<float>(TAPE_MAX_DELAY_SAMPLES - 2)));

    float wet = 0.0f;
    GetHeadMix(baseSamples, delay, wet, cp.mode, cp.headCfg);

    float filteredWet = tone.Process(wet);
    hp.Process(filteredWet);
    filteredWet = hp.High();

    float repeats = m_smoothedRepeats;
    float feedback;
    if (cp.isLoFi) {
        feedback = 0.0f; // LoFi mode uses a very short delay at high drive - feedback is dangerous
    } else {
        // Allow up to slight self-oscillation; the in-loop tanh saturator bounds runaway.
        feedback = repeats * 0.98f;
    }

    float drive = cp.drive;
    float playbackWet = ApplySafetyLimiter(filteredWet * dropoutGain);

    // Gentle program-dependent compression: tames sustained energy without killing repeats.
    float wetEnergyDamping = 1.0f / (1.0f + 0.2f * fabsf(playbackWet));
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

    m_tapeModL.Init(m_sampleRate);
    m_tapeModR.Init(m_sampleRate);
    // Decorrelate the right channel modulator so L/R wow/flutter diverge from sample 0.
    for (int i = 0; i < 2048; ++i) {
        m_tapeModR.GetTapeSpeed(1.0f, 7.0f, 0.0f, 0.0f);
    }

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

    m_tapeModL.Init(sample_rate);
    m_tapeModR.Init(sample_rate);

    m_ledOsc.Init(sample_rate);
    m_ledOsc.SetWaveform(Oscillator::WAVE_SQUARE);
    m_ledOsc.SetAmp(1.0f);
    m_ledOsc.SetFreq(2.0f);

    // The smoothed values come from the parameters, and the wet/dry balance is derived from the smoothed mix, so the order matters.
    ResetInternalState();
    UpdateMix();
}

void TapeDelayModule::ProcessSamples(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    // Everything that depends only on the parameters is computed once per call instead of once per sample. Expressions are kept exactly
    // as they were in the per-sample code so the results are bit-identical.
    const bool isLoFi = IsLoFiMode();

    ChannelParams cp;
    cp.isLoFi = isLoFi;
    cp.division = isLoFi ? 1.0f : GetDivisionMultiplier();
    cp.minDelay = isLoFi ? 2.0f : 1.0f;
    // Binned values in this framework are 1..N, so convert to zero-based here.
    cp.mode = GetParameterAsBinnedValue(MODE) - 1;
    cp.headCfg = GetParameterAsBinnedValue(HEAD_CONFIG) - 1;

    const float age = GetParameterAsFloat(TAPE_AGE);
    float ageShaped = age * age;
    float lpFreq;
    if (isLoFi) {
        constexpr float lofiLpMin = 900.0f;
        constexpr float lofiLpMax = 12000.0f;
        lpFreq = lofiLpMax - (lofiLpMax - lofiLpMin) * ageShaped;
    } else {
        lpFreq = m_ageLpMax - (m_ageLpMax - m_ageLpMin) * ageShaped;
    }

    float contour = GetParameterAsFloat(LOW_END_CONTOUR);
    float hpFreq;
    if (isLoFi) {
        constexpr float lofiHpMin = 30.0f;
        constexpr float lofiHpMax = 700.0f;
        hpFreq = lofiHpMin + (lofiHpMax - lofiHpMin) * contour;
    } else {
        hpFreq = m_contourHpMin + (m_contourHpMax - m_contourHpMin) * contour;
    }

    // SetFreq only recomputes filter coefficients (a few trig/pow calls), so once per call is the same as once per sample.
    m_toneL.SetFreq(lpFreq);
    m_toneR.SetFreq(lpFreq);
    m_hpL.SetFreq(hpFreq);
    m_hpR.SetFreq(hpFreq);

    const float bias = GetParameterAsFloat(TAPE_BIAS);
    cp.drive = isLoFi ? (1.0f + bias * 7.0f) : (1.0f + bias * 4.0f);

    const WowParams wowL = ComputeWowParams(1.0f, isLoFi);
    const WowParams wowR = ComputeWowParams(1.07f, isLoFi);
    const ImperfectionParams imp = ComputeImperfectionParams(GetParameterAsFloat(IMPERFECTIONS));
    const float wobble = isLoFi ? GetParameterAsFloat(WOW_FLUTTER) : 0.0f;

    const float timeParam = GetParameterAsFloat(TIME);
    const float mixParam = GetParameterAsFloat(MIX);
    const float repeatsParam = GetParameterAsFloat(REPEATS);
    const float reverbMix = GetParameterAsFloat(REVERB_MIX);

    for (size_t i = 0; i < n; i++) {
        const float dryL = inL[i];
        const float dryR = inR[i];

        // Tape-style motor glide on Time sweeps; faster smoothing on mix/repeats just kills zipper noise.
        fonepole(m_smoothedTime, timeParam, 0.0005f);
        fonepole(m_smoothedMix, mixParam, 0.02f);
        fonepole(m_smoothedRepeats, repeatsParam, 0.02f);
        UpdateMix(); // wet/dry follows the smoothed mix every sample, so it always converges to the Mix parameter

        UpdateDelayTimeAndLed(isLoFi, wobble);

        float speedModL = m_tapeModL.GetTapeSpeed(wowL.wowRate, wowL.flutterRate, wowL.wowDepth, wowL.flutterDepth, wowL.flutterMix);
        float speedModR = m_tapeModR.GetTapeSpeed(wowR.wowRate, wowR.flutterRate, wowR.wowDepth, wowR.flutterDepth, wowR.flutterMix);
        float dropoutGain = 1.0f;
        float crinkleOffset = 0.0f;
        UpdateImperfections(imp, dropoutGain, crinkleOffset);

        float wetL = ProcessChannel(dryL, speedModL, dropoutGain, crinkleOffset, cp, *m_delayL, m_toneL, m_hpL);
        float wetR = ProcessChannel(dryR, speedModR, dropoutGain, crinkleOffset, cp, *m_delayR, m_toneR, m_hpR);

        float reverbL = 0.0f;
        float reverbR = 0.0f;
        m_reverb->Process(wetL, wetR, &reverbL, &reverbR);

        float wetOutL = wetL * (1.0f - reverbMix) + reverbL * reverbMix;
        float wetOutR = wetR * (1.0f - reverbMix) + reverbR * reverbMix;

        wetOutL = ApplySafetyLimiter(wetOutL * 0.9f);
        wetOutR = ApplySafetyLimiter(wetOutR * 0.9f);

        outL[i] = wetOutL * m_mixWet + dryL * m_mixDry;
        outR[i] = wetOutR * m_mixWet + dryR * m_mixDry;
    }
}

void TapeDelayModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    float outL, outR;
    ProcessSamples(&in, &in, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void TapeDelayModule::ProcessStereo(float inL, float inR) {
    BaseEffectModule::ProcessStereo(inL, inR);

    float outL, outR;
    ProcessSamples(&inL, &inR, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void TapeDelayModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    ProcessSamples(in, in, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void TapeDelayModule::ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    ProcessSamples(inL, inR, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void TapeDelayModule::SetEnabled(bool isEnabled) {
    bool wasEnabled = IsEnabled();
    BaseEffectModule::SetEnabled(isEnabled);

    // Re-entering the effect should always start from silence to avoid stale, loud tails.
    // (but not while the tail is still ringing: Trails)
    if (isEnabled && !wasEnabled && !IsTrailing()) {
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
