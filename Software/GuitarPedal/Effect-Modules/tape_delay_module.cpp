#include "tape_delay_module.h"
#include "../Util/audio_utilities.h"
#include <array>
#include <cmath>

using namespace bkshepherd;

static const char *s_modeNames[3] = {"Single", "Multi", "Fixed"};
static const char *s_divisionNames[3] = {"Quarter", "Dotted8", "Triplet"};
static const char *s_headConfigNames[3] = {"Head1", "Head2", "Head3"};

DelayLine<float, TAPE_MAX_DELAY_SAMPLES> DSY_SDRAM_BSS s_tapeDelayLineL;
DelayLine<float, TAPE_MAX_DELAY_SAMPLES> DSY_SDRAM_BSS s_tapeDelayLineR;

static const auto s_metaData = [] {
    std::array<ParameterMetaData, TapeDelayModule::PARAM_COUNT> params{};

    params[TapeDelayModule::TIME] = {
        name : "Time",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.45f},
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
        defaultValue : {.float_value = 0.45f},
        knobMapping : 2,
        midiCCMapping : 62
    };

    params[TapeDelayModule::MODE] = {
        name : "Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
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
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 67
    };

    params[TapeDelayModule::LOW_END_CONTOUR] = {
        name : "Low End",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f},
        knobMapping : -1,
        midiCCMapping : 68
    };

    params[TapeDelayModule::REVERB_MIX] = {
        name : "Reverb",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : -1,
        midiCCMapping : 69
    };

    params[TapeDelayModule::HEAD_CONFIG] = {
        name : "Head Config",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_headConfigNames,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : 70
    };

    return params;
}();

TapeDelayModule::TapeDelayModule()
    : BaseEffectModule(), m_timeMinSamples(2400.0f), m_timeMaxSamples(192000.0f), m_mixWet(0.5f), m_mixDry(0.5f),
      m_currentDelaySamples(24000.0f), m_lastWetL(0.0f), m_lastWetR(0.0f), m_ageLpMin(1200.0f), m_ageLpMax(18000.0f),
      m_contourHpMin(20.0f), m_contourHpMax(350.0f), m_wowRateMin(0.15f), m_wowRateMax(2.0f), m_flutterRateMin(2.0f),
    m_flutterRateMax(9.0f), m_wowDepthMaxSamples(35.0f), m_flutterDepthMaxSamples(9.0f), m_ledValue(0.0f),
    m_sampleRate(48000.0f), m_delayL(nullptr), m_delayR(nullptr) {
    m_name = "Tape Delay";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

TapeDelayModule::~TapeDelayModule() {}

void TapeDelayModule::UpdateMix() {
    float mixKnob = GetParameterAsFloat(MIX);
    float x2 = 1.0f - mixKnob;
    float A = mixKnob * x2;
    float B = A * (1.0f + 1.4186f * A);
    float C = B + mixKnob;
    float D = B + x2;

    m_mixWet = C * C;
    m_mixDry = D * D;
}

float TapeDelayModule::GetDivisionMultiplier() const {
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
    float wowRate = m_wowRateMin + (m_wowRateMax - m_wowRateMin) * wowFlutter;
    float flutterRate = m_flutterRateMin + (m_flutterRateMax - m_flutterRateMin) * wowFlutter;
    float wowDepth = wowFlutter * wowFlutter * m_wowDepthMaxSamples;
    float flutterDepth = wowFlutter * m_flutterDepthMaxSamples;
    return m_tapeMod.GetTapeSpeed(wowRate, flutterRate, wowDepth, flutterDepth);
}

void TapeDelayModule::GetHeadMix(float baseSamples, DelayLine<float, TAPE_MAX_DELAY_SAMPLES> &delay, float &out) const {
    int mode = GetParameterAsBinnedValue(MODE) - 1;
    int headCfg = GetParameterAsBinnedValue(HEAD_CONFIG) - 1;

    float h1 = baseSamples * 0.5f;
    float h2 = baseSamples * 1.0f;
    float h3 = baseSamples * 1.5f;

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

    if (mode == 1) {
        if (headCfg == 0) {
            out = 0.62f * delay.Read(h1) + 0.38f * delay.Read(h2);
        } else if (headCfg == 1) {
            out = 0.62f * delay.Read(h2) + 0.38f * delay.Read(h3);
        } else {
            out = 0.45f * delay.Read(h1) + 0.35f * delay.Read(h2) + 0.20f * delay.Read(h3);
        }
        return;
    }

    if (headCfg == 0) {
        out = 0.75f * delay.Read(h1) + 0.25f * delay.Read(h2);
    } else if (headCfg == 1) {
        out = 0.2f * delay.Read(h1) + 0.6f * delay.Read(h2) + 0.2f * delay.Read(h3);
    } else {
        out = 0.25f * delay.Read(h2) + 0.75f * delay.Read(h3);
    }
}

float TapeDelayModule::ProcessChannel(float input, float speedMod, DelayLine<float, TAPE_MAX_DELAY_SAMPLES> &delay, Tone &tone,
                                      Svf &hp) {
    float division = GetDivisionMultiplier();
    float baseSamples = m_currentDelaySamples * division + speedMod;
    baseSamples = fmaxf(1.0f, fminf(baseSamples, static_cast<float>(TAPE_MAX_DELAY_SAMPLES - 2)));

    float wet = 0.0f;
    GetHeadMix(baseSamples, delay, wet);

    float age = GetParameterAsFloat(TAPE_AGE);
    float ageShaped = age * age;
    float lpFreq = m_ageLpMax - (m_ageLpMax - m_ageLpMin) * ageShaped;
    tone.SetFreq(lpFreq);

    float contour = GetParameterAsFloat(LOW_END_CONTOUR);
    float hpFreq = m_contourHpMin + (m_contourHpMax - m_contourHpMin) * contour;
    hp.SetFreq(hpFreq);

    float filteredWet = tone.Process(wet);
    hp.Process(filteredWet);
    filteredWet = hp.High();

    float repeats = GetParameterAsFloat(REPEATS);
    float feedback = 0.1f + repeats * 0.88f;

    float bias = GetParameterAsFloat(TAPE_BIAS);
    float drive = 1.0f + bias * 4.0f;
    float sat = tanhf((input + feedback * filteredWet) * drive) / tanhf(drive);

    delay.Write(sat);

    return filteredWet;
}

void TapeDelayModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    m_sampleRate = sample_rate;

    m_delayL = &s_tapeDelayLineL;
    m_delayR = &s_tapeDelayLineR;

    m_delayL->Init();
    m_delayR->Init();

    m_toneL.Init(sample_rate);
    m_toneR.Init(sample_rate);

    m_hpL.Init(sample_rate);
    m_hpR.Init(sample_rate);
    m_hpL.SetRes(0.1f);
    m_hpR.SetRes(0.1f);

    m_reverb.Init(sample_rate);
    m_reverb.SetFeedback(0.85f);
    m_reverb.SetLpFreq(9000.0f);

    m_tapeMod.Init(sample_rate);

    m_ledOsc.Init(sample_rate);
    m_ledOsc.SetWaveform(Oscillator::WAVE_SQUARE);
    m_ledOsc.SetAmp(1.0f);
    m_ledOsc.SetFreq(2.0f);

    UpdateMix();
}

void TapeDelayModule::ParameterChanged(int parameter_id) {
    if (parameter_id == MIX) {
        UpdateMix();
    }
}

void TapeDelayModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    float time = GetParameterAsFloat(TIME);
    m_currentDelaySamples = m_timeMinSamples + (m_timeMaxSamples - m_timeMinSamples) * time;

    float ledFreq = m_sampleRate / fmaxf(1.0f, m_currentDelaySamples);
    m_ledOsc.SetFreq(ledFreq * 0.5f);
    m_ledValue = m_ledOsc.Process();

    float speedMod = GetWowFlutterOffset();

    float wetL = ProcessChannel(m_audioLeft, speedMod, *m_delayL, m_toneL, m_hpL);
    float wetR = ProcessChannel(m_audioRight, speedMod, *m_delayR, m_toneR, m_hpR);
    m_lastWetL = wetL;
    m_lastWetR = wetR;

    float reverbMix = GetParameterAsFloat(REVERB_MIX);
    float reverbL = 0.0f;
    float reverbR = 0.0f;
    m_reverb.Process(wetL, wetR, &reverbL, &reverbR);

    float wetOutL = wetL * (1.0f - reverbMix) + reverbL * reverbMix;
    float wetOutR = wetR * (1.0f - reverbMix) + reverbR * reverbMix;

    m_audioLeft = wetOutL * m_mixWet + m_audioLeft * m_mixDry;
    m_audioRight = wetOutR * m_mixWet + m_audioRight * m_mixDry;
}

void TapeDelayModule::ProcessStereo(float inL, float inR) {
    BaseEffectModule::ProcessStereo(inL, inR);

    float time = GetParameterAsFloat(TIME);
    m_currentDelaySamples = m_timeMinSamples + (m_timeMaxSamples - m_timeMinSamples) * time;

    float ledFreq = m_sampleRate / fmaxf(1.0f, m_currentDelaySamples);
    m_ledOsc.SetFreq(ledFreq * 0.5f);
    m_ledValue = m_ledOsc.Process();

    float speedMod = GetWowFlutterOffset();

    float wetL = ProcessChannel(m_audioLeft, speedMod, *m_delayL, m_toneL, m_hpL);
    float wetR = ProcessChannel(m_audioRight, speedMod, *m_delayR, m_toneR, m_hpR);
    m_lastWetL = wetL;
    m_lastWetR = wetR;

    float reverbMix = GetParameterAsFloat(REVERB_MIX);
    float reverbL = 0.0f;
    float reverbR = 0.0f;
    m_reverb.Process(wetL, wetR, &reverbL, &reverbR);

    float wetOutL = wetL * (1.0f - reverbMix) + reverbL * reverbMix;
    float wetOutR = wetR * (1.0f - reverbMix) + reverbR * reverbMix;

    m_audioLeft = wetOutL * m_mixWet + m_audioLeft * m_mixDry;
    m_audioRight = wetOutR * m_mixWet + m_audioRight * m_mixDry;
}

void TapeDelayModule::SetTempo(uint32_t bpm) {
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
