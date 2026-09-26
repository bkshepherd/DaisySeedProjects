#include "dual_echo_module.h"
#include "../Util/audio_utilities.h"
#include <array>
#include <cmath>

using namespace bkshepherd;

static const char *s_rhythmNames[DualEchoModule::RHYTHM_COUNT] = {"Quarter", "Dotted8", "Halo", "BBD", "Tape"};

static DelayLine<float, DUAL_ECHO_MAX_DELAY_SAMPLES> DSY_SDRAM_BSS s_dualEchoLineA;
static DelayLine<float, DUAL_ECHO_MAX_DELAY_SAMPLES> DSY_SDRAM_BSS s_dualEchoLineB;

static constexpr float kMinTimeMs = 5.0f;
static constexpr float kMaxTimeMs = 1500.0f;
static constexpr float kHeadroomSamples = 2048.0f;

static const auto s_metaData = [] {
    std::array<ParameterMetaData, DualEchoModule::PARAM_COUNT> params{};

    params[DualEchoModule::TIME] = {
        name : "Time",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.54f},
        knobMapping : 0,
        midiCCMapping : 75
    };

    // Knobs 0-4 follow the Halo panel order: Time, Level, Feedback, Rate, Depth.
    params[DualEchoModule::FEEDBACK] = {
        name : "Feedback",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.35f},
        knobMapping : 2,
        midiCCMapping : 76
    };

    params[DualEchoModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f},
        knobMapping : 1,
        midiCCMapping : 77
    };

    params[DualEchoModule::DEPTH] = {
        name : "Depth",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : 4,
        midiCCMapping : 78
    };

    params[DualEchoModule::RATE] = {
        name : "Rate",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : 3,
        midiCCMapping : 79
    };

    params[DualEchoModule::TONE] = {
        name : "Tone",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.7f},
        knobMapping : 5,
        midiCCMapping : 80
    };

    params[DualEchoModule::RHYTHM] = {
        name : "Rhythm",
        valueType : ParameterValueType::Binned,
        valueBinCount : DualEchoModule::RHYTHM_COUNT,
        valueBinNames : s_rhythmNames,
        defaultValue : {.uint_value = DualEchoModule::RHYTHM_HALO},
        knobMapping : -1,
        midiCCMapping : 81
    };

    params[DualEchoModule::SATURATE] = {
        name : "Saturate",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : -1,
        midiCCMapping : 82
    };

    params[DualEchoModule::LOW_CUT] = {
        name : "Low Cut",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 83
    };

    params[DualEchoModule::SPREAD] = {
        name : "Spread",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 84
    };

    // Halo rhythm only: how much of the quarter-note echo feeds the dotted-8th (two delays in series).
    params[DualEchoModule::CASCADE] = {
        name : "Cascade",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 85
    };

    return params;
}();

// ratioA, ratioB, tapeHeads, monoFeed, triangleLfo, antiPhase, twoPoleLp, tapeWow, toneMaxHz, satBase, wetScale
static const DualEchoModule::ModeConfig s_modes[DualEchoModule::RHYTHM_COUNT] = {
    {1.0f, 1.0f, false, false, false, false, false, false, 8750.0f, 0.0f, 1.0f},  // Quarter
    {0.75f, 0.75f, false, false, false, false, false, false, 8750.0f, 0.0f, 1.0f}, // Dotted8
    {1.0f, 0.75f, false, true, false, false, false, false, 8750.0f, 0.0f, 1.2f},  // Halo
    {1.0f, 1.0f, false, false, true, true, true, false, 4500.0f, 0.25f, 1.0f},    // BBD
    {1.0f, 1.0f, true, false, false, false, false, true, 7000.0f, 0.2f, 1.0f}     // Tape
};

static inline float FastTanh(float x) {
    if (x > 3.0f) {
        return 1.0f;
    }
    if (x < -3.0f) {
        return -1.0f;
    }
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Transparent below |x| = 1, soft asymptote to 2.
static inline float SafetyClip(float x) {
    if (x > 1.0f) {
        return 1.0f + FastTanh(x - 1.0f);
    }
    if (x < -1.0f) {
        return -1.0f - FastTanh(-x - 1.0f);
    }
    return x;
}

// Returns sin(2*pi*p) for p in [0, 1).
static inline float FastSin(float p) {
    const float x = 2.0f * p - 1.0f;
    float y = 4.0f * x * (1.0f - fabsf(x));
    y = 0.225f * (y * fabsf(y) - y) + y;
    return -y;
}

static inline float Triangle(float p) { return 4.0f * fabsf(p - 0.5f) - 1.0f; }

static inline float Clamp(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

DualEchoModule::DualEchoModule()
    : BaseEffectModule(), m_sr(48000.0f), m_mode(s_modes[RHYTHM_HALO]), m_timeTargetSamples(21600.0f),
      m_timeSamples(21600.0f), m_ratioA(1.0f), m_ratioB(0.75f), m_feedback(0.0f), m_cascade(0.0f), m_level(0.0f),
      m_spread(0.0f), m_lfoRateHz(1.0f), m_modAmpTarget(0.0f), m_modAmp(0.0f), m_flutterAmp(0.0f), m_drive(1.0f),
      m_biasOffset(0.0f), m_bias(0.0f), m_satNorm(1.0f), m_compK(0.0f), m_hold(0.0f), m_holdActive(false),
      m_envAttack(0.0f), m_envRelease(0.0f), m_holdCoef(0.0f), m_ledPhase(0.0f) {
    m_name = "Dual Echo";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));

    m_voiceA.line = nullptr;
    m_voiceB.line = nullptr;
}

DualEchoModule::~DualEchoModule() {}

void DualEchoModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_sr = sample_rate;

    m_voiceA.line = &s_dualEchoLineA;
    m_voiceB.line = &s_dualEchoLineB;

    Voice *voices[2] = {&m_voiceA, &m_voiceB};
    for (Voice *v : voices) {
        v->lp1.Init();
        v->lp2.Init();
        v->hp.Init();
        v->hp.SetFilterMode(OnePole::FILTER_MODE_HIGH_PASS);
    }

    m_envAttack = 1.0f - expf(-1.0f / (0.005f * m_sr));
    m_envRelease = 1.0f - expf(-1.0f / (0.15f * m_sr));
    m_holdCoef = 1.0f - expf(-1.0f / (0.01f * m_sr));

    UpdateMode();
    UpdateTimeTarget();
    UpdateModulation();
    ResetState();
}

void DualEchoModule::UpdateMode() {
    int idx = GetParameterAsBinnedValue(RHYTHM) - 1;
    if (idx < 0 || idx >= RHYTHM_COUNT) {
        idx = RHYTHM_HALO;
    }
    m_mode = s_modes[idx];
    UpdateFilters();
    UpdateSaturation();
}

void DualEchoModule::UpdateFilters() {
    // Ranges from the Halo manual: Tone 1000-8750 Hz, single-order HPF up to 300 Hz. Saturate also darkens.
    float sat = Clamp(GetParameterAsFloat(SATURATE) + m_mode.satBase, 0.0f, 1.0f);
    float toneHz = fminf(1000.0f * powf(8.75f, GetParameterAsFloat(TONE)), m_mode.toneMaxHz) * (1.0f - 0.45f * sat);
    float lowHz = 10.0f * powf(30.0f, GetParameterAsFloat(LOW_CUT));

    Voice *voices[2] = {&m_voiceA, &m_voiceB};
    for (Voice *v : voices) {
        v->lp1.SetFrequency(toneHz / m_sr);
        v->lp2.SetFrequency(toneHz / m_sr);
        v->hp.SetFrequency(lowHz / m_sr);
    }
}

void DualEchoModule::UpdateSaturation() {
    float sat = Clamp(GetParameterAsFloat(SATURATE) + m_mode.satBase, 0.0f, 1.0f);
    m_drive = 1.0f + 5.0f * sat;
    // Small bias makes the curve asymmetric for even harmonics; offset/norm keep zero-in at zero-out and unity slope.
    m_bias = 0.2f * sat;
    m_biasOffset = FastTanh(m_bias);
    m_satNorm = 1.0f / (m_drive * (1.0f - m_biasOffset * m_biasOffset));
    m_compK = 3.0f * sat;
}

void DualEchoModule::UpdateModulation() {
    m_lfoRateHz = 0.1f * powf(100.0f, GetParameterAsFloat(RATE));

    // Depth sets peak pitch deviation, so amplitude is rate-compensated.
    float depth = GetParameterAsFloat(DEPTH);
    float cents = 40.0f * depth * sqrtf(depth);
    float ratio = powf(2.0f, cents / 1200.0f) - 1.0f;
    m_modAmpTarget = fminf(ratio * m_sr / (TWOPI_F * m_lfoRateHz), 0.01f * m_sr);
    m_flutterAmp = 2.5f * (0.3f + depth) * (m_sr / 48000.0f);
}

void DualEchoModule::UpdateTimeTarget() {
    float t = GetParameterAsFloat(TIME);
    float ms = kMinTimeMs + (kMaxTimeMs - kMinTimeMs) * t * t;
    float maxSamples = static_cast<float>(DUAL_ECHO_MAX_DELAY_SAMPLES) - kHeadroomSamples;
    m_timeTargetSamples = Clamp(ms * 0.001f * m_sr, 2.0f, maxSamples);
}

void DualEchoModule::ResetState() {
    Voice *voices[2] = {&m_voiceA, &m_voiceB};
    for (Voice *v : voices) {
        if (v->line != nullptr) {
            v->line->Init();
        }
        v->lp1.Reset();
        v->lp2.Reset();
        v->hp.Reset();
        v->tapeMod.Init(m_sr);
        v->lfoPhase = 0.0f;
        v->env = 0.0f;
    }

    // Decorrelate voice B's noise-based drift from voice A.
    for (int i = 0; i < 2048; ++i) {
        m_voiceB.tapeMod.GetTapeSpeed(1.0f, 7.0f, 0.0f, 0.0f);
    }

    m_timeSamples = m_timeTargetSamples;
    m_ratioA = m_mode.ratioA;
    m_ratioB = m_mode.ratioB;
    m_modAmp = m_modAmpTarget;
    m_feedback = GetParameterAsFloat(FEEDBACK);
    m_cascade = GetParameterAsFloat(CASCADE);
    m_level = GetParameterAsFloat(LEVEL);
    m_spread = GetParameterAsFloat(SPREAD);
    m_hold = 0.0f;
    m_holdActive = false;
    m_ledPhase = 0.0f;
}

void DualEchoModule::ParameterChanged(int parameter_id) {
    switch (parameter_id) {
    case TIME:
        UpdateTimeTarget();
        break;
    case DEPTH:
    case RATE:
        UpdateModulation();
        break;
    case TONE:
    case LOW_CUT:
        UpdateFilters();
        break;
    case SATURATE:
        UpdateSaturation();
        UpdateFilters();
        break;
    case RHYTHM:
        UpdateMode();
        break;
    default:
        break;
    }
}

float DualEchoModule::ReadVoice(Voice &v, float ratio, float modSamples) const {
    const float maxDelay = static_cast<float>(DUAL_ECHO_MAX_DELAY_SAMPLES) - 4.0f;
    const float base = m_timeSamples * ratio;
    const float d = Clamp(base + modSamples, 2.0f, maxDelay);

    if (!m_mode.tapeHeads) {
        return v.line->ReadHermite(d);
    }

    const float d1 = Clamp(base * 0.5f + modSamples, 2.0f, maxDelay);
    return (0.6f * v.line->ReadHermite(d1) + v.line->ReadHermite(d)) * (1.0f / 1.6f);
}

float DualEchoModule::VoiceModulation(Voice &v, float rateScale, float phaseOffset, float ampSamples, float width,
                                      float &modRight) {
    if (m_mode.tapeWow) {
        modRight = v.tapeMod.GetTapeSpeed(m_lfoRateHz * 0.5f * rateScale, 7.0f * rateScale, ampSamples, m_flutterAmp, 1.0f);
        return modRight;
    }

    v.lfoPhase += m_lfoRateHz * rateScale / m_sr;
    if (v.lfoPhase >= 1.0f) {
        v.lfoPhase -= 1.0f;
    }
    float p = v.lfoPhase + phaseOffset;
    if (p >= 1.0f) {
        p -= 1.0f;
    }
    float pR = p + 0.25f * width;
    if (pR >= 1.0f) {
        pR -= 1.0f;
    }

    const float lfo = m_mode.triangleLfo ? Triangle(p) : FastSin(p);
    const float lfoR = m_mode.triangleLfo ? Triangle(pR) : FastSin(pR);
    const float drift = v.tapeMod.GetTapeSpeed(0.25f * rateScale, 1.0f, 0.15f * ampSamples, 0.0f, 0.0f);
    modRight = lfoR * ampSamples + drift * (1.0f - 2.0f * width);
    return lfo * ampSamples + drift;
}

float DualEchoModule::ProcessLoop(Voice &v, float loopIn, float hold) {
    float x = v.hp.Process(loopIn);
    x = v.lp1.Process(x);
    if (m_mode.twoPoleLp) {
        x = v.lp2.Process(x);
    }
    // Hold opens the filters so the frozen loop doesn't darken away.
    x += (loopIn - x) * hold;

    const float a = fabsf(x);
    v.env += (a > v.env ? m_envAttack : m_envRelease) * (a - v.env);
    const float compressed = x / (1.0f + m_compK * v.env);
    const float sat = (FastTanh(compressed * m_drive + m_bias) - m_biasOffset) * m_satNorm;

    return SafetyClip(sat + (x - sat) * hold);
}

void DualEchoModule::ProcessSample(float inL, float inR, float &outL, float &outR) {
    fonepole(m_timeSamples, m_timeTargetSamples, 0.0003f);
    fonepole(m_ratioA, m_mode.ratioA, 0.0005f);
    fonepole(m_ratioB, m_mode.ratioB, 0.0005f);
    fonepole(m_modAmp, m_modAmpTarget, 0.001f);
    fonepole(m_feedback, GetParameterAsFloat(FEEDBACK), 0.002f);
    fonepole(m_cascade, GetParameterAsFloat(CASCADE), 0.002f);
    fonepole(m_level, GetParameterAsFloat(LEVEL), 0.002f);
    fonepole(m_spread, GetParameterAsFloat(SPREAD), 0.002f);
    fonepole(m_hold, m_holdActive ? 1.0f : 0.0f, m_holdCoef);
    const float hold = m_hold;
    const bool halo = m_mode.monoFeed;

    float fb = m_feedback * 0.99f;
    fb += (0.999f - fb) * hold;
    const float cascade = halo ? m_cascade : 0.0f;

    const float shortestHead = m_timeSamples * fminf(m_ratioA, m_ratioB) * (m_mode.tapeHeads ? 0.5f : 1.0f);
    const float amp = fminf(m_modAmp * (1.0f - 0.5f * hold), fmaxf(0.0f, 0.45f * shortestHead - 2.0f));
    // In Halo the image comes from each side drifting differently, not from ping-pong panning.
    const float width = halo ? m_spread : 0.0f;
    float modAR, modBR;
    const float modA = VoiceModulation(m_voiceA, 1.0f, 0.0f, amp, width, modAR);
    const float modB = VoiceModulation(m_voiceB, m_mode.antiPhase ? 1.0f : 1.07f, m_mode.antiPhase ? 0.5f : 0.25f, amp,
                                       width, modBR);

    const float wetA = ReadVoice(m_voiceA, m_ratioA, modA);
    const float wetB = ReadVoice(m_voiceB, m_ratioB, modB);
    const float wetAR = halo ? ReadVoice(m_voiceA, m_ratioA, modAR) : wetA;
    const float wetBR = halo ? ReadVoice(m_voiceB, m_ratioB, modBR) : wetB;

    float inA = inL;
    float inB = inR;
    if (halo) {
        inA = inB = 0.5f * (inL + inR);
    }
    const float gate = 1.0f - hold;
    // Cascade feeds the quarter echo forward into the dotted-8th line, like two pedals in series.
    const float loopA = inA * gate + fb * wetA;
    const float loopB = (inB + cascade * wetA) * gate + fb * wetB;

    m_voiceA.line->Write(ProcessLoop(m_voiceA, loopA, hold));
    m_voiceB.line->Write(ProcessLoop(m_voiceB, loopB, hold));

    const float s = m_spread;
    const float gain = 0.5f * m_mode.wetScale * m_level * 1.2f;
    float wetL, wetR;
    if (halo) {
        const float pan = 0.25f * s;
        wetL = (wetA * (1.0f + pan) + wetB * (1.0f - pan)) * gain;
        wetR = (wetAR * (1.0f - pan) + wetBR * (1.0f + pan)) * gain;
    } else {
        wetL = (wetA * (1.0f + s) + wetB * (1.0f - s)) * gain;
        wetR = (wetA * (1.0f - s) + wetB * (1.0f + s)) * gain;
    }

    outL = inL + SafetyClip(wetL);
    outR = inR + SafetyClip(wetR);

    m_ledPhase += 1.0f / fmaxf(1.0f, m_timeSamples);
    if (m_ledPhase >= 1.0f) {
        m_ledPhase -= 1.0f;
    }
}

void DualEchoModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);
    float outL, outR;
    ProcessSample(in, in, outL, outR);
    m_audioLeft = 0.5f * (outL + outR);
    m_audioRight = m_audioLeft;
}

void DualEchoModule::ProcessStereo(float inL, float inR) {
    BaseEffectModule::ProcessStereo(inL, inR);
    ProcessSample(inL, inR, m_audioLeft, m_audioRight);
}

void DualEchoModule::SetEnabled(bool isEnabled) {
    bool wasEnabled = IsEnabled();
    BaseEffectModule::SetEnabled(isEnabled);

    if (isEnabled && !wasEnabled) {
        ResetState();
    } else if (!isEnabled) {
        m_holdActive = false;
    }
}

void DualEchoModule::SetTempo(uint32_t bpm) {
    float freq = tempo_to_freq(bpm);
    if (freq <= 0.0f) {
        return;
    }
    float ms = 1000.0f / freq;
    float t = sqrtf(Clamp((ms - kMinTimeMs) / (kMaxTimeMs - kMinTimeMs), 0.0f, 1.0f));
    SetParameterAsMagnitude(TIME, t);
}

float DualEchoModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        if (m_holdActive) {
            return value;
        }
        return m_ledPhase < 0.5f ? value : 0.0f;
    }

    return value;
}

void DualEchoModule::AlternateFootswitchHeldFor1Second() { m_holdActive = true; }

void DualEchoModule::AlternateFootswitchReleased() { m_holdActive = false; }

void DualEchoModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                            bool isEditing) {
    BaseEffectModule::DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn, isEditing);

    if (m_holdActive) {
        display.SetCursor(boundsToDrawIn.GetRight() - 34, 2);
        display.WriteString("HOLD", Font_6x8, true);
    }
}
