#include "reverb_delay_module.h"
#include "../Util/audio_utilities.h"
#include <array>
#include <cmath>

using namespace bkshepherd;

static const char *s_waveBinNames[5] = {"Sine", "Triangle", "Saw", "Ramp",
                                        "Square"}; //, "Poly Tri", "Poly Saw", "Poly Sqr"};  // Horrible loud sound when switching to
                                                   // poly tri, not every time, TODO whats going on?
static const char *s_modParamNames[5] = {"None", "DelayTime", "DelayLevel", "ReverbLevel", "DelayPan"};
static const char *s_delayModes[3] = {"Normal", "Triplett", "Dotted 8th"};

static DelayLineRevOct<float, REVDELAY_MAX_DELAY> DSY_SDRAM_BSS s_revDelay_delayLineLeft;
static DelayLineRevOct<float, REVDELAY_MAX_DELAY> DSY_SDRAM_BSS s_revDelay_delayLineRight;
static DelayLineReverse<float, REVDELAY_MAX_DELAY_REV> DSY_SDRAM_BSS s_revDelay_delayLineRevLeft;
static DelayLineReverse<float, REVDELAY_MAX_DELAY_REV> DSY_SDRAM_BSS s_revDelay_delayLineRevRight;
static DelayLine<float, REVDELAY_MAX_DELAY_SPREAD> DSY_SDRAM_BSS s_revDelay_delayLineSpread;
static ReverbSc DSY_SDRAM_BSS reverbStereo_verbDelay; // as in the Reverb, SciFi and Tape Delay modules: the object holds a 388 KB buffer (the heap is small)

static const auto s_metaData = [] {
    std::array<ParameterMetaData, ReverbDelayModule::PARAM_COUNT> params{};

    params[ReverbDelayModule::DELAY_TIME] = {
        name : "Delay Time",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.241379f}, // 400 ms: 50 + 0.241379 * (1500 - 50)
        knobMapping : 0,
        midiCCMapping : 1,
        unit : " ms",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Lin(v, 50.0f, 1500.0f); }
    };

    params[ReverbDelayModule::D_FEEDBACK] = {
        name : "D Feedback",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.45f},
        knobMapping : 1,
        midiCCMapping : 22,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[ReverbDelayModule::DELAY_MIX] = {
        name : "Delay Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.45f},
        knobMapping : 2,
        midiCCMapping : 23,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[ReverbDelayModule::REVERB_TIME] = {
        name : "Reverb Time",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.45f},
        knobMapping : 3,
        midiCCMapping : 24
    };

    params[ReverbDelayModule::REVERB_DAMP] = {
        name : "Reverb Damp",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f},
        knobMapping : 4,
        midiCCMapping : 25,
        unit : " Hz",
        decimals : 0,
        displayFn : [](float v) { return 600.0f + (1.0f - v) * (1.0f - v) * (16000.0f - 600.0f); }
    };

    params[ReverbDelayModule::REVERB_MIX] = {
        name : "Reverb Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.45f},
        knobMapping : 5,
        midiCCMapping : 26,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[ReverbDelayModule::DELAY_MODE] = {
        name : "Delay Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_delayModes,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 27
    };

    params[ReverbDelayModule::SERIES_D_TO_R] = {
        name : "Series D>R",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 28
    };

    params[ReverbDelayModule::REVERSE] = {
        name : "Reverse",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 29
    };

    params[ReverbDelayModule::OCTAVE] = {
        name : "Octave",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 30
    };

    params[ReverbDelayModule::DELAY_LPF] = {
        name : "Delay LPF",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.95f},
        knobMapping : -1,
        midiCCMapping : 31,
        unit : " Hz",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Lin(v, 300.0f, 20000.0f); }
    };

    params[ReverbDelayModule::D_SPREAD] = {
        name : "D Spread",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.25f},
        knobMapping : -1,
        midiCCMapping : 39
    };

    params[ReverbDelayModule::DUAL_DELAY] = {
        name : "Dual Delay",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 33
    };

    params[ReverbDelayModule::MOD_AMT] = {
        name : "Mod Amt",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.15f},
        knobMapping : -1,
        midiCCMapping : 34,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[ReverbDelayModule::MOD_RATE] = {
        name : "Mod Rate",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.25f},
        knobMapping : -1,
        midiCCMapping : 35,
        unit : " Hz",
        decimals : 1,
        displayFn : [](float v) { return ParamDisplay::Lin(v, 0.0f, 3.0f); }
    };

    params[ReverbDelayModule::MOD_PARAM] = {
        name : "Mod Param",
        valueType : ParameterValueType::Binned,
        valueBinCount : 5,
        valueBinNames : s_modParamNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 36
    };

    params[ReverbDelayModule::MOD_WAVE] = {
        name : "Mod Wave",
        valueType : ParameterValueType::Binned,
        valueBinCount : 5,
        valueBinNames : s_waveBinNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 37
    };

    params[ReverbDelayModule::SYNC_MOD_F] = {
        name : "Sync Mod F",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 38
    };

    return params;
}();

// Default Constructor
ReverbDelayModule::ReverbDelayModule()
    : BaseEffectModule(), m_timeMin(0.6f), m_timeMax(1.0f), m_lpFreqMin(600.0f), m_lpFreqMax(16000.0f), m_delaylpFreqMin(300.0f),
      m_delaylpFreqMax(20000.0f), m_delaySamplesMin(2400.0f), m_delaySamplesMax(72000.0f), m_delaySpreadMin(24.0f),
      m_delaySpreadMax(2400.0f),
      // m_delayPPMin(1200.0f),
      // m_delayPPMax(96000.0f),
      m_pdelRight_out(0.0), m_currentMod(1.0), m_modOscFreqMin(0.0), m_modOscFreqMax(3.0) {
    // Set the name of the effect
    m_name = "Verb Delay";
    m_reverbStereo = &reverbStereo_verbDelay;

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
ReverbDelayModule::~ReverbDelayModule() {
    // No Code Needed
}

void ReverbDelayModule::UpdateLEDRate() {
    // Update the LED oscillator frequency based on the current timeParam
    float timeParam = GetParameterAsFloat(DELAY_TIME);
    float delaySamples = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam;
    float delayFreq = effect_samplerate / delaySamples;
    m_ledPhaseInc = (delayFreq / 2.0f) / effect_samplerate;
}

void ReverbDelayModule::AdvanceLed(size_t samples) {
    m_ledPhase += samples * m_ledPhaseInc;
    if (m_ledPhase >= 1.0f) {
        m_ledPhase -= std::floor(m_ledPhase);
    }
}

void ReverbDelayModule::CalculateDelayMix() {
    // Handle Normal or Alternate Mode Mix Controls
    //    A computationally cheap mostly energy constant crossfade from SignalSmith Blog
    //    https://signalsmith-audio.co.uk/writing/2021/cheap-energy-crossfade/

    float delMixKnob = GetParameterAsFloat(DELAY_MIX);
    float x2 = 1.0 - delMixKnob;
    float A = delMixKnob * x2;
    float B = A * (1.0 + 1.4186 * A);
    float C = B + delMixKnob;
    float D = B + x2;

    delayWetMix = C * C;
    delayDryMix = D * D;
}

void ReverbDelayModule::CalculateReverbMix() {
    // Handle Normal or Alternate Mode Mix Controls
    //    A computationally cheap mostly energy constant crossfade from SignalSmith Blog
    //    https://signalsmith-audio.co.uk/writing/2021/cheap-energy-crossfade/

    float revMixKnob = GetParameterAsFloat(REVERB_MIX);
    float x2 = 1.0 - revMixKnob;
    float A = revMixKnob * x2;
    float B = A * (1.0 + 1.4186 * A);
    float C = B + revMixKnob;
    float D = B + x2;

    reverbWetMix = C * C;
    reverbDryMix = D * D;
}

void ReverbDelayModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    s_revDelay_delayLineLeft.Init();
    s_revDelay_delayLineRevLeft.Init();
    delayLeft.del = &s_revDelay_delayLineLeft;
    delayLeft.delreverse = &s_revDelay_delayLineRevLeft;
    delayLeft.delayTarget = 24000; // in samples
    delayLeft.currentDelay = 24000;
    delayLeft.feedback = 0.0;
    delayLeft.active = true; // Default to no delay
    delayLeft.toneOctLP.Init(sample_rate);
    delayLeft.toneOctLP.SetFreq(20000.0);

    s_revDelay_delayLineRight.Init();
    s_revDelay_delayLineRevRight.Init();
    delayRight.del = &s_revDelay_delayLineRight;
    delayRight.delreverse = &s_revDelay_delayLineRevRight;
    delayRight.delayTarget = 24000; // in samples
    delayRight.currentDelay = 24000;
    delayRight.feedback = 0.0;
    delayRight.active = true; // Default to no
    delayRight.toneOctLP.Init(sample_rate);
    delayRight.toneOctLP.SetFreq(20000.0);

    s_revDelay_delayLineSpread.Init();
    delaySpread.del = &s_revDelay_delayLineSpread;
    delaySpread.delayTarget = 1500; // in samples
    delaySpread.currentDelay = 1500;
    delaySpread.active = true;

    m_reverbStereo->Init(sample_rate);
    m_reverbStereo->SetFeedback(0.0);

    effect_samplerate = sample_rate;

    UpdateLEDRate(); // the tempo LED follows the delay time from the start, not only after the knob has been moved

    modOsc.Init(sample_rate);
    modOsc.SetAmp(1.0);

    CalculateDelayMix();
    CalculateReverbMix();
}

void ReverbDelayModule::ParameterChanged(int parameter_id) {
    if (parameter_id == DELAY_TIME) { // Delay Time
        UpdateLEDRate();
    } else if (parameter_id == DELAY_MIX) { // Delay Mix
        CalculateDelayMix();
    } else if (parameter_id == REVERB_MIX) { // Reverb Mix
        CalculateReverbMix();
    } else if (parameter_id == DELAY_MODE) { // Delay Mode
        int delay_mode_temp = (GetParameterAsBinnedValue(DELAY_MODE) - 1);
        if (delay_mode_temp > 0) {
            delayLeft.secondTapOn = true;  // triplett, dotted 8th
            delayRight.secondTapOn = true; // triplett, dotted 8th
            if (delay_mode_temp == 1) {
                delayLeft.del->set2ndTapFraction(0.6666667);  // triplett
                delayRight.del->set2ndTapFraction(0.6666667); // triplett
            } else if (delay_mode_temp == 2) {
                delayLeft.del->set2ndTapFraction(0.75);  // dotted eighth
                delayRight.del->set2ndTapFraction(0.75); // dotted eighth
            }
        } else {
            delayLeft.secondTapOn = false;
            delayRight.secondTapOn = false;
        }
    } else if (parameter_id == DELAY_LPF) {
        delayLeft.toneOctLP.SetFreq(m_delaylpFreqMin + (m_delaylpFreqMax - m_delaylpFreqMin) * GetParameterAsFloat(DELAY_LPF));
        delayRight.toneOctLP.SetFreq(m_delaylpFreqMin + (m_delaylpFreqMax - m_delaylpFreqMin) * GetParameterAsFloat(DELAY_LPF));
    }
}

void ReverbDelayModule::ProcessModulation() {
    int modParam = (GetParameterAsBinnedValue(MOD_PARAM) - 1);
    // Calculate Modulation
    modOsc.SetWaveform(GetParameterAsBinnedValue(MOD_WAVE) - 1);

    if (GetParameterAsBool(SYNC_MOD_F)) { // If mod frequency synced to delay time, override mod rate setting
        float dividor;
        if (modParam == 2 || modParam == 3) {
            dividor = 2.0;
        } else {
            dividor = 4.0;
        }
        float freq = (effect_samplerate / delayLeft.delayTarget) / dividor;
        modOsc.SetFreq(freq);
    } else {
        modOsc.SetFreq(m_modOscFreqMin + (m_modOscFreqMax - m_modOscFreqMin) * GetParameterAsFloat(MOD_RATE));
    }

    // Ease the effect value into it's target to avoid clipping with square or sawtooth waves
    fonepole(m_currentMod, modOsc.Process(), .01f);
    float mod = m_currentMod;
    float mod_amount = GetParameterAsFloat(MOD_AMT);

    // {"None", "DelayTime", "DelayLevel", "ReverbLevel", "DelayPan"};
    if (modParam == 1) {
        float timeParam = GetParameterAsFloat(DELAY_TIME);
        delayLeft.delayTarget = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam + mod * mod_amount * 500;
        delayRight.delayTarget = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam + mod * mod_amount * 500;

    } else if (modParam == 2) {
        float mod_level = mod * mod_amount + (1.0 - mod_amount);
        delayLeft.level = mod_level;
        delayRight.level = mod_level;
        delayLeft.level_reverse = mod_level;
        delayRight.level_reverse = mod_level;

    } else if (modParam == 3) {
        reverb_level = mod * mod_amount + (1.0 - mod_amount);

    } else if (modParam == 4) {
        float mod_level = mod * mod_amount + (1.0 - mod_amount);
        delayLeft.level = mod_level;
        delayRight.level = 1.0 - mod_level;
        delayLeft.level_reverse = mod_level;
        delayRight.level_reverse = 1.0 - mod_level;
    }
}

void ReverbDelayModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);
    AdvanceLed(1);

    // Calculate the effect
    float timeParam = GetParameterAsFloat(DELAY_TIME);

    delayLeft.delayTarget = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam;
    delayRight.delayTarget = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam;

    delayLeft.feedback = GetParameterAsFloat(D_FEEDBACK);
    delayRight.feedback = GetParameterAsFloat(D_FEEDBACK);

    delayLeft.reverseMode = GetParameterAsBool(REVERSE);
    delayRight.reverseMode = GetParameterAsBool(REVERSE);

    delayLeft.del->setOctave(GetParameterAsBool(OCTAVE));
    delayRight.del->setOctave(GetParameterAsBool(OCTAVE));

    delayLeft.dual_delay = GetParameterAsBool(DUAL_DELAY);
    delayRight.dual_delay = GetParameterAsBool(DUAL_DELAY);

    if (GetParameterAsBool(DUAL_DELAY)) { // If dual delay is turned on, spread controls the L/R panning of the two delays
        delayLeft.level = GetParameterAsFloat(D_SPREAD) + 1.0;
        delayRight.level = 1.0 - GetParameterAsFloat(D_SPREAD);

        delayLeft.level_reverse = 1.0 - GetParameterAsFloat(D_SPREAD);
        delayRight.level_reverse = GetParameterAsFloat(D_SPREAD) + 1.0;

    } else { // If dual delay is off reset the levels to normal, spread controls the amount of additional delay applied to the right
             // channel
        delayLeft.level = 1.0;
        delayRight.level = 1.0;
        delayLeft.level_reverse = 1.0;
        delayRight.level_reverse = 1.0;
    }

    // Calculate Reverb Params
    reverb_level = 1.0;
    m_reverbStereo->SetFeedback(m_timeMin + GetParameterAsFloat(REVERB_TIME) * (m_timeMax - m_timeMin));
    float invertedFreq =
        1.0 - GetParameterAsFloat(REVERB_DAMP); // Invert the damping param so that knob left is less dampening, knob right is more dampening
    invertedFreq = invertedFreq * invertedFreq; // also square it for exponential taper (more control over lower frequencies)
    m_reverbStereo->SetLpFreq(m_lpFreqMin + invertedFreq * (m_lpFreqMax - m_lpFreqMin));

    // Modulation, this overwrites any previous parameter settings for the modulated param - TODO Better way to do this for less
    // processing?
    ProcessModulation();

    float delLeft_out = delayLeft.Process(m_audioLeft);
    float delRight_out = delayRight.Process(m_audioRight);
    // float delRight_out = delLeft_out;

    // Calculate any delay spread
    delaySpread.delayTarget = m_delaySpreadMin + (m_delaySpreadMax - m_delaySpreadMin) * GetParameterAsFloat(D_SPREAD);
    float delSpread_out = delaySpread.Process(delRight_out);
    if (GetParameterAsFloat(D_SPREAD) > 0.0f && !GetParameterAsBool(DUAL_DELAY)) {
        delRight_out = delSpread_out;
    }

    float delay_out_left = delLeft_out * delayWetMix + m_audioLeft * delayDryMix;
    float delay_out_right = delRight_out * delayWetMix + m_audioRight * delayDryMix;

    // REVERB //////////////
    float sendl, sendr, wetl, wetr; // Reverb Inputs/Outputs
    if (GetParameterAsBool(SERIES_D_TO_R)) {
        sendl = delay_out_left;
        sendr = delay_out_right;
    } else {
        sendl = m_audioLeft;
        sendr = m_audioRight;
    }

    m_reverbStereo->Process(sendl, sendr, &wetl, &wetr);

    m_audioLeft = wetl * reverbWetMix * reverb_level / 2.0 + m_audioLeft * reverbDryMix; // divide by 2 for volume correction
    m_audioRight = wetr * reverbWetMix * reverb_level / 2.0 + m_audioRight * reverbDryMix;

    if (!GetParameterAsBool(SERIES_D_TO_R)) { // If not series mode
        m_audioLeft = (m_audioLeft + delay_out_left) /
                      2.0; // Dividing by 2 to compensate for double signal volume with both reverb and delay mix outputs
        m_audioRight = (m_audioRight + delay_out_right) / 2.0;
    }
}

void ReverbDelayModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    // ProcessMono(inL);  // Can't simply call the mono processing because of how the reverb is calculated

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(inL, inR);
    AdvanceLed(1);

    // Calculate the effect
    float timeParam = GetParameterAsFloat(DELAY_TIME);

    delayLeft.delayTarget = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam;
    delayRight.delayTarget = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam;

    delayLeft.feedback = GetParameterAsFloat(D_FEEDBACK);
    delayRight.feedback = GetParameterAsFloat(D_FEEDBACK);

    delayLeft.reverseMode = GetParameterAsBool(REVERSE);
    delayRight.reverseMode = GetParameterAsBool(REVERSE);

    delayLeft.del->setOctave(GetParameterAsBool(OCTAVE));
    delayRight.del->setOctave(GetParameterAsBool(OCTAVE));

    delayLeft.dual_delay = GetParameterAsBool(DUAL_DELAY);
    delayRight.dual_delay = GetParameterAsBool(DUAL_DELAY);

    if (GetParameterAsBool(DUAL_DELAY)) { // If dual delay is turned on, spread controls the L/R panning of the two delays
        delayLeft.level = GetParameterAsFloat(D_SPREAD) + 1.0;
        delayRight.level = 1.0 - GetParameterAsFloat(D_SPREAD);

        delayLeft.level_reverse = 1.0 - GetParameterAsFloat(D_SPREAD);
        delayRight.level_reverse = GetParameterAsFloat(D_SPREAD) + 1.0;

    } else { // If dual delay is off reset the levels to normal, spread controls the amount of additional delay applied to the right
             // channel
        delayLeft.level = 1.0;
        delayRight.level = 1.0;
        delayLeft.level_reverse = 1.0;
        delayRight.level_reverse = 1.0;
    }

    // Calculate Reverb Params
    reverb_level = 1.0;
    m_reverbStereo->SetFeedback(m_timeMin + GetParameterAsFloat(REVERB_TIME) * (m_timeMax - m_timeMin));
    float invertedFreq =
        1.0 - GetParameterAsFloat(REVERB_DAMP); // Invert the damping param so that knob left is less dampening, knob right is more dampening
    invertedFreq = invertedFreq * invertedFreq; // also square it for exponential taper (more control over lower frequencies)
    m_reverbStereo->SetLpFreq(m_lpFreqMin + invertedFreq * (m_lpFreqMax - m_lpFreqMin));

    // Modulation, this overwrites any previous parameter settings for the modulated param
    ProcessModulation();

    float delLeft_out = delayLeft.Process(m_audioLeft);
    float delRight_out = delayRight.Process(m_audioRight);

    // Calculate any delay spread
    delaySpread.delayTarget = m_delaySpreadMin + (m_delaySpreadMax - m_delaySpreadMin) * GetParameterAsFloat(D_SPREAD);
    float delSpread_out = delaySpread.Process(delRight_out);
    if (GetParameterAsFloat(D_SPREAD) > 0.0f && !GetParameterAsBool(DUAL_DELAY)) { // If spread > 0 and dual delay isn't on
        delRight_out = delSpread_out;
    }

    float delay_out_left = delLeft_out * delayWetMix + m_audioLeft * delayDryMix;
    float delay_out_right = delRight_out * delayWetMix + m_audioRight * delayDryMix;

    /// REVERB
    float sendl, sendr, wetl, wetr; // Reverb Inputs/Outputs
    if (GetParameterAsBool(SERIES_D_TO_R)) {
        sendl = delay_out_left;
        sendr = delay_out_right;
    } else {
        sendl = m_audioLeft;
        sendr = m_audioRight;
    }

    m_reverbStereo->Process(sendl, sendr, &wetl, &wetr);

    m_audioLeft = wetl * reverbWetMix * reverb_level / 2.0 + m_audioLeft * reverbDryMix;
    m_audioRight = wetr * reverbWetMix * reverb_level / 2.0 + m_audioRight * reverbDryMix;

    if (!GetParameterAsBool(SERIES_D_TO_R)) { // If not series mode
        m_audioLeft = (m_audioLeft + delay_out_left) / 2.0;
        m_audioRight = (m_audioRight + delay_out_right) / 2.0;
    }
}

// Block versions. Same signal flow and the same arithmetic as ProcessStereo (so the output is bit-identical), but everything that only
// depends on the parameters is read and calculated once per block: the per-sample versions read about twenty parameters, rebuild the
// delay targets and levels and reconfigure the reverb for every sample. What really varies per sample (the modulation, the smoothed delay
// times, the delay lines, the reverb) stays in the loop. The modulation oscillator is not run when no parameter is modulated.
void ReverbDelayModule::ProcessBlockInternal(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    AdvanceLed(n);

    // Parameters, once per block
    const float timeParam = GetParameterAsFloat(DELAY_TIME);
    const float baseTarget = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam;
    const float feedback = GetParameterAsFloat(D_FEEDBACK);
    const bool reverseMode = GetParameterAsBool(REVERSE);
    const bool octave = GetParameterAsBool(OCTAVE);
    const bool dualDelay = GetParameterAsBool(DUAL_DELAY);
    const float spread = GetParameterAsFloat(D_SPREAD);
    const bool seriesDToR = GetParameterAsBool(SERIES_D_TO_R);
    const int modParam = (GetParameterAsBinnedValue(MOD_PARAM) - 1);
    const float mod_amount = GetParameterAsFloat(MOD_AMT);

    delayLeft.feedback = feedback;
    delayRight.feedback = feedback;
    delayLeft.reverseMode = reverseMode;
    delayRight.reverseMode = reverseMode;
    delayLeft.del->setOctave(octave);
    delayRight.del->setOctave(octave);
    delayLeft.dual_delay = dualDelay;
    delayRight.dual_delay = dualDelay;

    // Levels and targets as ProcessStereo sets them before the modulation; only the modulated ones are written again in the loop
    if (dualDelay) { // spread controls the L/R panning of the two delays
        delayLeft.level = spread + 1.0;
        delayRight.level = 1.0 - spread;
        delayLeft.level_reverse = 1.0 - spread;
        delayRight.level_reverse = spread + 1.0;
    } else {
        delayLeft.level = 1.0;
        delayRight.level = 1.0;
        delayLeft.level_reverse = 1.0;
        delayRight.level_reverse = 1.0;
    }
    delayLeft.delayTarget = baseTarget;
    delayRight.delayTarget = baseTarget;
    reverb_level = 1.0;

    // Reverb
    m_reverbStereo->SetFeedback(m_timeMin + GetParameterAsFloat(REVERB_TIME) * (m_timeMax - m_timeMin));
    float invertedFreq = 1.0 - GetParameterAsFloat(REVERB_DAMP);
    invertedFreq = invertedFreq * invertedFreq;
    m_reverbStereo->SetLpFreq(m_lpFreqMin + invertedFreq * (m_lpFreqMax - m_lpFreqMin));

    // Modulation oscillator (the sync frequency uses the unmodulated delay time, as in ProcessModulation)
    if (modParam != 0) {
        modOsc.SetWaveform(GetParameterAsBinnedValue(MOD_WAVE) - 1);
        if (GetParameterAsBool(SYNC_MOD_F)) {
            const float dividor = (modParam == 2 || modParam == 3) ? 2.0f : 4.0f;
            modOsc.SetFreq((effect_samplerate / baseTarget) / dividor);
        } else {
            modOsc.SetFreq(m_modOscFreqMin + (m_modOscFreqMax - m_modOscFreqMin) * GetParameterAsFloat(MOD_RATE));
        }
    }

    const float spreadTarget = m_delaySpreadMin + (m_delaySpreadMax - m_delaySpreadMin) * spread;
    const bool useSpread = spread > 0.0f && !dualDelay;

    for (size_t i = 0; i < n; i++) {
        // Read both inputs before writing so in-place processing is safe
        const float l = inL[i];
        const float r = inR[i];

        if (modParam != 0) {
            fonepole(m_currentMod, modOsc.Process(), .01f);
            const float mod = m_currentMod;
            if (modParam == 1) {
                delayLeft.delayTarget = baseTarget + mod * mod_amount * 500;
                delayRight.delayTarget = baseTarget + mod * mod_amount * 500;
            } else if (modParam == 2) {
                float mod_level = mod * mod_amount + (1.0 - mod_amount);
                delayLeft.level = mod_level;
                delayRight.level = mod_level;
                delayLeft.level_reverse = mod_level;
                delayRight.level_reverse = mod_level;
            } else if (modParam == 3) {
                reverb_level = mod * mod_amount + (1.0 - mod_amount);
            } else if (modParam == 4) {
                float mod_level = mod * mod_amount + (1.0 - mod_amount);
                delayLeft.level = mod_level;
                delayRight.level = 1.0 - mod_level;
                delayLeft.level_reverse = mod_level;
                delayRight.level_reverse = 1.0 - mod_level;
            }
        }

        float delLeft_out = delayLeft.Process(l);
        float delRight_out = delayRight.Process(r);

        delaySpread.delayTarget = spreadTarget;
        float delSpread_out = delaySpread.Process(delRight_out);
        if (useSpread) {
            delRight_out = delSpread_out;
        }

        float delay_out_left = delLeft_out * delayWetMix + l * delayDryMix;
        float delay_out_right = delRight_out * delayWetMix + r * delayDryMix;

        float sendl, sendr, wetl, wetr;
        if (seriesDToR) {
            sendl = delay_out_left;
            sendr = delay_out_right;
        } else {
            sendl = l;
            sendr = r;
        }

        m_reverbStereo->Process(sendl, sendr, &wetl, &wetr);

        float left = wetl * reverbWetMix * reverb_level / 2.0 + l * reverbDryMix;
        float right = wetr * reverbWetMix * reverb_level / 2.0 + r * reverbDryMix;

        if (!seriesDToR) {
            left = (left + delay_out_left) / 2.0;
            right = (right + delay_out_right) / 2.0;
        }

        outL[i] = left;
        outR[i] = right;
    }
}

void ReverbDelayModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    ProcessBlockInternal(in, in, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void ReverbDelayModule::ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    ProcessBlockInternal(inL, inR, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

// Set the delay time from the tap tempo  TODO: Currently the tap tempo led isn't set to delay time on pedal boot up, how to do this?
void ReverbDelayModule::SetTempo(uint32_t bpm) {
    float freq = tempo_to_freq(bpm);
    float delay_in_samples = effect_samplerate / freq;

    if (delay_in_samples <= m_delaySamplesMin) {
        SetParameterAsMagnitude(0, 0.0f);
    } else if (delay_in_samples >= m_delaySamplesMax) {
        SetParameterAsMagnitude(0, 1.0f);
    } else {
        float magnitude =
            static_cast<float>(delay_in_samples - m_delaySamplesMin) / static_cast<float>(m_delaySamplesMax - m_delaySamplesMin);
        SetParameterAsMagnitude(0, magnitude);
    }
    UpdateLEDRate();
}

float ReverbDelayModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    // On for the part of the triangle wave that is above 0.45: the first and the last 13.75% of the cycle
    const float ledValue = (m_ledPhase < 0.1375f || m_ledPhase > 0.8625f) ? 1.0f : 0.0f;

    if (led_id == 1) {
        return value * ledValue;
    }

    return value;
}
