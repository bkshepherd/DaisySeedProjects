#include "drum_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;

static const char *s_voiceBinNames[6] = {"Analog", "Synth", "Mix1", "Mix2"};
static const char *s_midiVoiceBinNames[6] = {"AnalogBass", "SynthBass", "AnalogSnare", "SynthSnare", "HiHat", "Kit"};
static const char *s_beatBinNames[8] = {"4/4 Rock1", "4/4 Rock2", "4/4 Rock3", "4/4 Rock4",
                                        "3/4 Rock1", "3/4 Rock2", "6/8 Rock1", "6/8 Rock2"};
static const char *s_dryThruBinNames[3] = {"None", "Mono", "Stereo"};
static const char *s_modeBinNames[2] = {"Machine", "Midi"};

constexpr uint32_t minTempoDrum = 40;
constexpr uint32_t maxTempoDrum = 200;

// -1=rest, 0=bass, 1=synthbass, 2=snare, 3=synthsnare, 4=hihat
const int beats44[4][16] = {{1, -1, -1, -1, 2, -1, -1, -1, 1, -1, -1, -1, 2, -1, 4, -1}, // Default "boom-chick" beat
                            {1, -1, 4, -1, 2, -1, 4, -1, 1, -1, 4, -1, 2, -1, 4, -1},    // more hihat
                            {1, -1, 4, -1, 2, -1, 1, -1, 4, -1, 1, -1, 2, -1, 4, -1},
                            {1, -1, 4, -1, 2, -1, -1, 1, 1, -1, 1, -1, 2, -1, 4, -1}}; // Walk this Way beat

// 3/4 time and 6/8 time beats
const int beats34[4][12] = {{1, -1, 4, -1, 4, -1, 1, -1, 2, -1, 4, -1},  // 3/4 Rock1
                            {1, -1, 1, -1, 4, -1, 1, -1, 2, -1, 4, -1},  // 3/4 Rock2
                            {1, -1, 4, -1, 4, -1, 2, -1, 4, -1, 4, -1},  // 6/8 Rock1
                            {1, -1, 4, -1, 1, -1, 2, -1, 4, -1, 1, -1}}; // 6/8 Rock2

static const int s_paramCount = 10;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {name : "Level", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 0, midiCCMapping : 14},
    {
        name : "Voice",
        valueType : ParameterValueType::Binned,
        valueBinCount : 4,
        valueBinNames : s_voiceBinNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 1,
        midiCCMapping : 15
    },
    {
        name : "Beats",
        valueType : ParameterValueType::Binned,
        valueBinCount : 8,
        valueBinNames : s_beatBinNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 2,
        midiCCMapping : 16
    },
    {
        name : "Tempo",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 100.f},
        knobMapping : 3,
        midiCCMapping : 17,
        minValue : minTempoDrum,
        maxValue : maxTempoDrum
    },
    {
        name : "MidiVoice",
        valueType : ParameterValueType::Binned,
        valueBinCount : 6,
        valueBinNames : s_midiVoiceBinNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 4,
        midiCCMapping : 18
    },
    {
        name : "Timbre",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 5,
        midiCCMapping : 19
    },
    {name : "Tone", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : -1, midiCCMapping : 20},
    {
        name : "Decay",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 21
    },
    {
        name : "Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 2,
        valueBinNames : s_modeBinNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 22
    },
    {
        name : "DryThru",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_dryThruBinNames,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : 23
    }};

// Default Constructor
DrumModule::DrumModule() : BaseEffectModule(), m_cachedEffectMagnitudeValue(1.0f) {
    // Set the name of the effect
    m_name = "Drum";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
DrumModule::~DrumModule() {
    // No Code Needed
}

void DrumModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    snare.Init(sample_rate);
    bass.Init(sample_rate);
    hihat.Init(sample_rate);
    synthsnare.Init(sample_rate);
    synthbass.Init(sample_rate);

    // Custom initial settings
    hihat.SetDecay(.8f);

    instrument_ = 0;
    voice_ = 0; // 0=bass; 1=synthbass; 2=snare; 3=synthsnare; 4=hihat

    float initial_freq = tempo_to_freq(GetParameterAsFloat(3));
    metro.Init(initial_freq * 4.0,
               sample_rate); // Multiplying freq by 4, since beats are defined every sixteenth note, and we want BPM by quarter note
    beat_count = 0;
    led_tempo_count = 1;
}

void DrumModule::ParameterChanged(int parameter_id) {

    if (parameter_id == 0) { // Level

    } else if (parameter_id == 2) {             // Beats
        if (GetParameterAsBinnedValue(2) < 5) { // Currently, first four beats are 4/4 time, second two are 3/4
            time_sig = 0;                       // 0=4/4; 1=3/4
            selected_beat = GetParameterAsBinnedValue(2) - 1;
        } else {
            time_sig = 1; // 0=4/4; 1=3/4
            selected_beat = GetParameterAsBinnedValue(2) - 5;
        }

    } else if (parameter_id == 3) { // Tempo
        m_bpm = GetParameterAsFloat(3);
        float freq = tempo_to_freq(m_bpm);
        metro.SetFreq(freq *
                      4.0); // Multiplying freq by 4, since beats are defined every sixteenth note, and we want BPM by quarter note

    } else if (parameter_id == 4) { // Midi Voice
        instrument_ = GetParameterAsBinnedValue(4) - 1;

    } else if (parameter_id == 5) { // Timbre
        instrument_ = GetParameterAsBinnedValue(4) - 1;

        if (instrument_ == 0) {
            bass.SetSelfFmAmount(GetParameterAsFloat(5)); // self fm amount

        } else if (instrument_ == 1) {
            synthbass.SetDirtiness(GetParameterAsFloat(5)); // dirtiness

        } else if (instrument_ == 2) {
            snare.SetSnappy(GetParameterAsFloat(5)); // snappy

        } else if (instrument_ == 3) {
            synthsnare.SetSnappy(GetParameterAsFloat(5)); // snappy

        } else if (instrument_ == 4) {
            hihat.SetNoisiness(GetParameterAsFloat(5)); // noisiness
        }

    } else if (parameter_id == 6) { // Tone
        instrument_ = GetParameterAsBinnedValue(4) - 1;
        if (instrument_ == 0) {
            bass.SetTone(GetParameterAsFloat(6));

        } else if (instrument_ == 1) {
            synthbass.SetTone(GetParameterAsFloat(6));

        } else if (instrument_ == 2) {
            snare.SetTone(GetParameterAsFloat(6));

        } else if (instrument_ == 3) {
            synthsnare.SetFmAmount(GetParameterAsFloat(6)); // Different function

        } else if (instrument_ == 4) {
            hihat.SetTone(GetParameterAsFloat(6));
        }

    } else if (parameter_id == 7) { // Decay
        instrument_ = GetParameterAsBinnedValue(4) - 1;

        if (instrument_ == 0) {
            bass.SetDecay(GetParameterAsFloat(7));

        } else if (instrument_ == 1) {
            synthbass.SetDecay(GetParameterAsFloat(7));

        } else if (instrument_ == 2) {
            snare.SetDecay(GetParameterAsFloat(7));

        } else if (instrument_ == 3) {
            synthsnare.SetDecay(GetParameterAsFloat(7));

        } else if (instrument_ == 4) {
            hihat.SetDecay(GetParameterAsFloat(7));
        }

    } else if (parameter_id == 8) { // Mode
        if (GetParameterAsBinnedValue(8) == 1) {
            auto_mode = true;
            beat_count = 0; // start beat at beginning
        } else {
            auto_mode = false;
        }
    }
}

void DrumModule::OnNoteOn(float notenumber, float velocity) {
    // Note Off can come in as Note On w/ 0 Velocity
    if (velocity == 0.f) {

    } else {

        if (instrument_ == 0) {
            // Adjust the midi note number to be at a good frequency for bass drum at middle c
            float new_notenumber = notenumber - 24.0;
            new_notenumber = (new_notenumber < 24.0) ? 24.0 : new_notenumber;
            bass.SetFreq(mtof(new_notenumber));
            bass.SetAccent(velocity / 128.0);
            bass.Trig();

        } else if (instrument_ == 1) {
            // Adjust the midi note number to be at a good frequency for bass drum at middle c
            float new_notenumber = notenumber - 12.0;
            new_notenumber = (new_notenumber < 12.0) ? 12.0 : new_notenumber;
            synthbass.SetFreq(mtof(new_notenumber));
            synthbass.SetAccent(velocity / 128.0);
            synthbass.Trig();

        } else if (instrument_ == 2) {
            snare.SetAccent(velocity / 128.0);
            snare.SetFreq(mtof(notenumber));
            snare.Trig();

        } else if (instrument_ == 3) {
            synthsnare.SetAccent(velocity / 128.0);
            synthsnare.SetFreq(mtof(notenumber));
            synthsnare.Trig();

        } else if (instrument_ == 4) {
            // Adjust the midi note number to be at a good frequency for a hihat at middle c
            float new_notenumber = notenumber + 36.0;
            new_notenumber = (new_notenumber > 127.0) ? 127.0 : new_notenumber;
            hihat.SetFreq(mtof(new_notenumber));
            hihat.SetAccent(velocity / 128.0);
            hihat.Trig();

        } else if (instrument_ == 5) { // kit
            if (notenumber == 60.0) {
                voice_ = 0;
                bass.SetAccent(velocity / 128.0);
                bass.Trig();

            } else if (notenumber == 62.0) {
                voice_ = 1;
                synthbass.SetAccent(velocity / 128.0);
                synthbass.Trig();

            } else if (notenumber == 64.0) {
                voice_ = 2;
                snare.SetAccent(velocity / 128.0);
                snare.Trig();

            } else if (notenumber == 65.0) {
                voice_ = 3;
                synthsnare.SetAccent(velocity / 128.0);
                synthsnare.Trig();

            } else if (notenumber == 67.0) {
                voice_ = 4;
                hihat.SetAccent(velocity / 128.0);
                hihat.Trig();
            }
        }
    }
}

void DrumModule::OnNoteOff(float notenumber, float velocity) {
    // Nothing for now
}

void DrumModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    // For auto drum machine
    uint8_t tap = metro.Process(); //  1 is the tempo tap, 0 all other times

    // Blink the Tempo LED
    fonepole(tap_mag, 0.0, .001f);
    m_cachedEffectMagnitudeValue = tap_mag;

    // Trigger next beat
    if (tap == 1 && auto_mode) {

        int led_time_sig_tempo = (time_sig == 0) ? 3 : 2;

        if (led_tempo_count > led_time_sig_tempo) { // Flash LED every 4th beat if 4/4, every 3rd beat if 3/4 or 6/8
            tap_mag = tap;
            led_tempo_count = 0;
        }
        led_tempo_count += 1;

        float temp_instrument = 0;
        if (time_sig == 0) {
            temp_instrument = beats44[selected_beat][beat_count];
        } else {
            temp_instrument = beats34[selected_beat][beat_count];
        }

        // Select correct drum voice based on Voice setting
        // -1=rest, 0=bass, 1=synthbass, 2=snare, 3=synthsnare, 4=hihat
        if (GetParameterAsBinnedValue(1) == 1) { // Analog
            temp_instrument = (temp_instrument == 1) ? 0 : temp_instrument;

        } else if (GetParameterAsBinnedValue(1) == 2) { // Synth
            temp_instrument = (temp_instrument == 2) ? 3 : temp_instrument;

        } else if (GetParameterAsBinnedValue(1) == 3) { // Mix1 (Synth Bass, Analog Snare)
            // Do nothing, this is how ithe beats were originally programmed

        } else if (GetParameterAsBinnedValue(1) == 4) { // Mix2 (Analog Bass, Synth Snare)
            temp_instrument = (temp_instrument == 1) ? 0 : temp_instrument;
            temp_instrument = (temp_instrument == 2) ? 3 : temp_instrument;
        }

        // If the instrument is not a rest (-1 value), trigger drum note
        if (temp_instrument >= 0) {
            instrument_ = temp_instrument;
            OnNoteOn(60.0, 110.0); // always play auto drums as middle c at 110 velocity (for now)
        }

        // Increment beat count from 0 to 15 if 4/4 time, from 0 to 11 if 3/4 time
        beat_count += 1;
        if (time_sig == 0) {
            beat_count = beat_count > 15 ? 0 : beat_count;
        } else if (time_sig == 1) {
            beat_count = beat_count > 11 ? 0 : beat_count;
        }
    }

    float sum = 0.f;
    if (instrument_ == 0) {
        sum = bass.Process(false) * 6.0; // "* 6.0" is for volume adjustment, seems to be quieter than the other voices

    } else if (instrument_ == 1) {
        sum = synthbass.Process(false) * 0.8;

    } else if (instrument_ == 2) {
        sum = snare.Process(false) * 0.9;

    } else if (instrument_ == 3) {
        sum = synthsnare.Process(false) * 0.8;

    } else if (instrument_ == 4) {
        sum = hihat.Process(false);

        // Only processing one instrument at a time in Kit mode based on last key pressed, processing could not keep up otherwise
    } else if (instrument_ == 5) {
        if (voice_ == 0) {
            sum = bass.Process(false);
        } else if (voice_ == 1) {
            sum = synthbass.Process(false);
        } else if (voice_ == 2) {
            sum = snare.Process(false);
        } else if (voice_ == 3) {
            sum = synthsnare.Process(false);
        } else if (voice_ == 4) {
            sum = hihat.Process(false);
        }
    }

    float through_audioL = 0.0;
    if (GetParameterAsBinnedValue(9) > 1) {
        through_audioL = m_audioLeft;
    }

    m_audioLeft = sum * GetParameterAsFloat(0) * 0.5 + through_audioL; // "* 0.5" is just for volume reduction
    m_audioRight = m_audioLeft;
}

void DrumModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);

    float through_audioR = 0.0;
    if (GetParameterAsBinnedValue(9) == 3) {
        through_audioR = inR;
        m_audioRight = m_audioRight + through_audioR - inL;
    }
}

void DrumModule::SetTempo(uint32_t bpm) {
    m_bpm = std::clamp(bpm, minTempoDrum, maxTempoDrum);
    SetParameterAsFloat(3, m_bpm);
    float freq = tempo_to_freq(m_bpm);
    metro.SetFreq(freq * 4.0);
}

float DrumModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
