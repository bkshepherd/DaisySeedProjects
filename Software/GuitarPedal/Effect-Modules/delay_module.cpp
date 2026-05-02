#include "delay_module.h"
#include "../Util/audio_utilities.h"
#include <array>

using namespace bkshepherd;

static const char *s_waveBinNames[6] = {"Sine", "Triangle", "Saw", "Ramp",
                                        "Square", "Tape"}; //, "Poly Tri", "Poly Saw", "Poly Sqr"};  // Horrible loud sound when switching to
                                                   // poly tri, not every time, TODO whats going on? (I suspect electro smith broke
                                                   // the poly tri osc, the same happens in the tremolo too)
static const char *s_modParamNames[4] = {"None", "DelayTime", "DelayLevel", "DelayPan"};
static const char *s_delayModes[3] = {"Normal", "Triplett", "Dotted 8th"};
static const char *s_delayTypes[6] = {"Forward", "Reverse", "Octave", "ReverseOct", "Dual", "DualOct"};

DelayLineRevOct<float, MAX_DELAY_NORM> DSY_SDRAM_BSS delayLineLeft;
DelayLineRevOct<float, MAX_DELAY_NORM> DSY_SDRAM_BSS delayLineRight;
DelayLineReverse<float, MAX_DELAY_REV> DSY_SDRAM_BSS delayLineRevLeft;
DelayLineReverse<float, MAX_DELAY_REV> DSY_SDRAM_BSS delayLineRevRight;
DelayLine<float, MAX_DELAY_SPREAD> DSY_SDRAM_BSS delayLineSpread;

static const auto s_metaData = [] {
    std::array<ParameterMetaData, DelayModule::PARAM_COUNT> params{};

    params[DelayModule::DELAY_TIME] = {
        name : "Delay Time",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : 14
    };

    params[DelayModule::D_FEEDBACK] = {
        name : "D Feedback",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : 15
    };

    params[DelayModule::DELAY_MIX] = {
        name : "Delay Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 2,
        midiCCMapping : 16
    };

    params[DelayModule::DELAY_MODE] = {
        name : "Delay Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_delayModes,
        defaultValue : {.uint_value = 0},
        knobMapping : 3,
        midiCCMapping : 17
    };

    params[DelayModule::DELAY_TYPE] = {
        name : "Delay Type",
        valueType : ParameterValueType::Binned,
        valueBinCount : 6,
        valueBinNames : s_delayTypes,
        defaultValue : {.uint_value = 0},
        knobMapping : 4,
        midiCCMapping : 18
    };

    params[DelayModule::DELAY_LPF] = {
        name : "Delay LPF",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 5,
        midiCCMapping : 19
    };

    params[DelayModule::D_SPREAD] = {
        name : "D Spread",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 20
    };

    params[DelayModule::MOD_AMT] = {
        name : "Mod Amt",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 21
    };

    params[DelayModule::MOD_RATE] = {
        name : "Mod Rate",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 22
    };

    params[DelayModule::MOD_PARAM] = {
        name : "Mod Param",
        valueType : ParameterValueType::Binned,
        valueBinCount : 4,
        valueBinNames : s_modParamNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 23
    };

    params[DelayModule::MOD_WAVE] = {
        name : "Mod Wave",
        valueType : ParameterValueType::Binned,
        valueBinCount : 6,
        valueBinNames : s_waveBinNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 24
    };

    params[DelayModule::SYNC_MOD_F] = {
        name : "Sync Mod F",
        valueType : ParameterValueType::Bool,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 25
    };

    return params;
}();

// Default Constructor
DelayModule::DelayModule()
    : BaseEffectModule(), m_delaylpFreqMin(300.0f), m_delaylpFreqMax(20000.0f), m_delaySamplesMin(2400.0f),
      m_delaySamplesMax(192000.0f), m_delaySpreadMin(24.0f), m_delaySpreadMax(2400.0f), m_pdelRight_out(0.0), m_currentMod(1.0),
      m_modOscFreqMin(0.0), m_modOscFreqMax(3.0), m_LEDValue(1.0f) {
    // Set the name of the effect
    m_name = "Delay";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
DelayModule::~DelayModule() {
    // No Code Needed
}

void DelayModule::UpdateLEDRate() {
    // Update the LED oscillator frequency based on the current timeParam
    float timeParam = GetParameterAsFloat(DELAY_TIME);
    float delaySamples = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam;
    float delayFreq = effect_samplerate / delaySamples;
    led_osc.SetFreq(delayFreq / 2.0);
}

void DelayModule::CalculateDelayMix() {
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

void DelayModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    delayLineLeft.Init();
    delayLineRevLeft.Init();
    delayLeft.del = &delayLineLeft;
    delayLeft.delreverse = &delayLineRevLeft;
    delayLeft.delayTarget = 24000; // in samples
    delayLeft.feedback = 0.0;
    delayLeft.active = true; // Default to no delay
    delayLeft.toneOctLP.Init(sample_rate);
    delayLeft.toneOctLP.SetFreq(20000.0);

    delayLineRight.Init();
    delayLineRevRight.Init();
    delayRight.del = &delayLineRight;
    delayRight.delreverse = &delayLineRevRight;
    delayRight.delayTarget = 24000; // in samples
    delayRight.feedback = 0.0;
    delayRight.active = true; // Default to no
    delayRight.toneOctLP.Init(sample_rate);
    delayRight.toneOctLP.SetFreq(20000.0);

    delayLineSpread.Init();
    delaySpread.del = &delayLineSpread;
    delaySpread.delayTarget = 1500; // in samples
    delaySpread.active = true;

    effect_samplerate = sample_rate;

    led_osc.Init(sample_rate);
    led_osc.SetWaveform(1);
    led_osc.SetFreq(2.0);

    modOsc.Init(sample_rate);
    modOsc.SetAmp(1.0);

    modTape.Init(sample_rate);

    CalculateDelayMix();
}

void DelayModule::ParameterChanged(int parameter_id) {
    if (parameter_id == DELAY_TIME) { // Delay Time
        UpdateLEDRate();
    } else if (parameter_id == DELAY_MIX) { // Delay Mix
        CalculateDelayMix();
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

void DelayModule::ProcessModulation() {
    int modParam = (GetParameterAsBinnedValue(MOD_PARAM) - 1);
    // Calculate Modulation

    int waveForm = GetParameterAsBinnedValue(MOD_WAVE) - 1;
    float wowDepth = 2.0f;
    float flutterDepth = 2.0f;
    if (waveForm == 5) { // If using tape modulation
        float freq = GetParameterAsFloat(MOD_RATE);
        float wowRate = 0.2f + 2.0f * freq;
        float flutterRate = 2.0f + 5.0f * freq;
        m_currentMod = modTape.GetTapeSpeed(wowRate, flutterRate, wowDepth, flutterDepth);
    } else {
        modOsc.SetWaveform(waveForm);

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
    }

    float mod = m_currentMod;
    float mod_amount = GetParameterAsFloat(MOD_AMT);

    // {"None", "DelayTime", "DelayLevel", "Level", "DelayPan"};
    if (modParam == 1) {
        float delayTarget;
        float timeParam = GetParameterAsFloat(DELAY_TIME);
        const float D_min = 1.0f; // minimum allowable delay time: 1 sample

        if (waveForm == 5) {
            // Tape flutter mode with dynamic min
            const float M = wowDepth + 0.2f * flutterDepth; // Max amplitude of tape modulation.
            const float depth = 500.0f;

            float baseMin = D_min + M * mod_amount * depth;
            float baseMax = m_delaySamplesMax;

            float base = baseMin + (baseMax - baseMin) * timeParam;

            delayTarget = base + mod * mod_amount * depth;
        } else {
            delayTarget = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam + mod * mod_amount * 500;
        }
        if (delayTarget < D_min) {
            delayTarget = D_min;
        }
        if (delayTarget > MAX_DELAY_NORM - 2) {
            delayTarget = MAX_DELAY_NORM - 2;
        }

        delayLeft.delayTarget = delayTarget;
        delayRight.delayTarget = delayTarget;
    } else if (modParam == 2) {
        float mod_level = mod * mod_amount + (1.0 - mod_amount);
        delayLeft.level = mod_level;
        delayRight.level = mod_level;
        delayLeft.level_reverse = mod_level;
        delayRight.level_reverse = mod_level;

    } else if (modParam == 3) {
        _level = mod * mod_amount + (1.0 - mod_amount);

    } else if (modParam == 4) {
        float mod_level = mod * mod_amount + (1.0 - mod_amount);
        delayLeft.level = mod_level;
        delayRight.level = 1.0 - mod_level;
        delayLeft.level_reverse = mod_level;
        delayRight.level_reverse = 1.0 - mod_level;
    }
}

void DelayModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    m_LEDValue = led_osc.Process(); // update the tempo LED

    // Calculate the effect
    int delayType = GetParameterAsBinnedValue(DELAY_TYPE) - 1;

    float timeParam = GetParameterAsFloat(DELAY_TIME);

    delayLeft.delayTarget = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam;
    delayRight.delayTarget = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam;

    delayLeft.feedback = GetParameterAsFloat(D_FEEDBACK);
    delayRight.feedback = GetParameterAsFloat(D_FEEDBACK);
    if (delayType == 1 || delayType == 3) {
        delayLeft.reverseMode = true;
        delayRight.reverseMode = true;
    } else {
        delayLeft.reverseMode = false;
        delayRight.reverseMode = false;
    }
    if (delayType == 2 || delayType == 3 || delayType == 5) {
        delayLeft.del->setOctave(true);
        delayRight.del->setOctave(true);
    } else {
        delayLeft.del->setOctave(false);
        delayRight.del->setOctave(false);
    }
    if (delayType == 4 || delayType == 5) {
        delayLeft.dual_delay = true;
        delayRight.dual_delay = true;
    } else {
        delayLeft.dual_delay = false;
        delayRight.dual_delay = false;
    }

    if (delayType == 4 || delayType == 5) { // If dual delay is turned on, spread controls the L/R panning of the two delays
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

    // Modulation, this overwrites any previous parameter settings for the modulated param - TODO Better way to do this for less
    // processing?
    ProcessModulation();

    float delLeft_out = delayLeft.Process(m_audioLeft);
    float delRight_out = delayRight.Process(m_audioRight);
    // float delRight_out = delLeft_out;

    // Calculate any delay spread
    delaySpread.delayTarget = m_delaySpreadMin + (m_delaySpreadMax - m_delaySpreadMin) * GetParameterAsFloat(D_SPREAD);
    float delSpread_out = delaySpread.Process(delRight_out);
    if (GetParameterRaw(D_SPREAD) > 0 && delayType != 4 && delayType != 5) {
        delRight_out = delSpread_out;
    }

    m_audioLeft = delLeft_out * delayWetMix + m_audioLeft * delayDryMix;
    m_audioRight = delRight_out * delayWetMix + m_audioRight * delayDryMix;
}

void DelayModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    // ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(inL, inR);

    m_LEDValue = led_osc.Process(); // update the tempo LED

    // Calculate the effect
    int delayType = GetParameterAsBinnedValue(DELAY_TYPE) - 1;

    float timeParam = GetParameterAsFloat(DELAY_TIME);

    delayLeft.delayTarget = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam;
    delayRight.delayTarget = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam;

    delayLeft.feedback = GetParameterAsFloat(D_FEEDBACK);
    delayRight.feedback = GetParameterAsFloat(D_FEEDBACK);
    if (delayType == 1 || delayType == 3) {
        delayLeft.reverseMode = true;
        delayRight.reverseMode = true;
    } else {
        delayLeft.reverseMode = false;
        delayRight.reverseMode = false;
    }
    if (delayType == 2 || delayType == 3 || delayType == 5) {
        delayLeft.del->setOctave(true);
        delayRight.del->setOctave(true);
    } else {
        delayLeft.del->setOctave(false);
        delayRight.del->setOctave(false);
    }
    if (delayType == 4 || delayType == 5) {
        delayLeft.dual_delay = true;
        delayRight.dual_delay = true;
    } else {
        delayLeft.dual_delay = false;
        delayRight.dual_delay = false;
    }

    if (delayType == 4 || delayType == 5) { // If dual delay is turned on, spread controls the L/R panning of the two delays
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

    // Modulation, this overwrites any previous parameter settings for the modulated param - TODO Better way to do this for less
    // processing?
    ProcessModulation();

    float delLeft_out = delayLeft.Process(m_audioLeft);
    float delRight_out = delayRight.Process(m_audioRight);
    // float delRight_out = delLeft_out;

    // Calculate any delay spread
    delaySpread.delayTarget = m_delaySpreadMin + (m_delaySpreadMax - m_delaySpreadMin) * GetParameterAsFloat(D_SPREAD);
    float delSpread_out = delaySpread.Process(delRight_out);
    if (GetParameterRaw(D_SPREAD) > 0 && delayType != 4 && delayType != 5) {
        delRight_out = delSpread_out;
    }

    m_audioLeft = delLeft_out * delayWetMix + m_audioLeft * delayDryMix;
    m_audioRight = delRight_out * delayWetMix + m_audioRight * delayDryMix;
}

// Set the delay time from the tap tempo  TODO: Currently the tap tempo led isn't set to delay time on pedal boot up, how to do this?
void DelayModule::SetTempo(uint32_t bpm) {
    float freq = tempo_to_freq(bpm);
    float delay_in_samples = effect_samplerate / freq;

    if (delay_in_samples <= m_delaySamplesMin) {
        SetParameterAsMagnitude(0, 0.0f);
    } else if (delay_in_samples >= m_delaySamplesMax) {
        SetParameterAsMagnitude(0, 1.0f);
    } else {
        // Get the parameter as close as we can to target tempo
        float magnitude =
            static_cast<float>(delay_in_samples - m_delaySamplesMin) / static_cast<float>(m_delaySamplesMax - m_delaySamplesMin);
        SetParameterAsMagnitude(0, magnitude);
    }
    UpdateLEDRate();
}

float DelayModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    float ledValue = 0.0;
    if (m_LEDValue > 0.45) {
        ledValue = 1.0;
    } else {
        ledValue = 0.0;
    }

    if (led_id == 1) {
        return value * ledValue;
    }

    return value;
}

int DelayModule::GetMappedParameterIDForKnob(int knob_id) const {
    // If shift mode is active, remap knobs 0-5 to alternate parameters
    if (m_shiftModeActive) {
        switch (knob_id) {
        case 0:
            return MOD_AMT; // Knob 0 -> Mod Amount
        case 1:
            return MOD_RATE; // Knob 1 -> Mod Rate
        case 2:
            return MOD_WAVE; // Knob 2 -> Mod Waveform
        case 3:
            return MOD_PARAM; // Knob 3 -> Mod Parameter
        case 4:
            return SYNC_MOD_F; // Knob 4 -> Sync Mod Frequency (bool)
        case 5:
            return D_SPREAD; // Knob 5 -> Delay Spread
        default:
            return -1;
        }
    }

    // Normal mode: use default mapping from metadata
    return BaseEffectModule::GetMappedParameterIDForKnob(knob_id);
}

void DelayModule::AlternateFootswitchHeldFor1Second() {
    // If held for 1 second, toggle shift mode (uses same knobs as mod controls for
    // alternate parameter control)
    m_shiftModeActive = !m_shiftModeActive;
}

void DelayModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                         bool isEditing) {
    // Draw the base UI
    BaseEffectModule::DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn, isEditing);

    // If shift mode is active, display a "SHIFT" indicator on the UI
    if (m_shiftModeActive) {
        display.SetCursor(boundsToDrawIn.GetRight() - 40, 2);
        display.WriteString("SHIFT", Font_6x8, true);
    }
}
