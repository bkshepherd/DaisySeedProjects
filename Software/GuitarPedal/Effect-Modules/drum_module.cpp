#include "drum_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;


static const char *s_voiceBinNames[7] = {"Bass","SynthBass", "Snare", "SynthSnare", "HiHat", "Kit", "Auto" };
static const char *s_dryThruBinNames[3] = {"None", "Mono", "Stereo"};

constexpr uint32_t minTempoDrum = 40;
constexpr uint32_t maxTempoDrum = 200;

/// Default "boom-chick" beat
const int beat1[16] = {1, -1, 2, -1, 1, -1, 2, -1, 1, -1, 2, -1, 1, -1, 4, -1};  // -1=rest, 0=bass, 1=synthbass, 2=snare, 3=synthsnare, 4=hihat



static const int s_paramCount = 7;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {name : "Level", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 0, midiCCMapping : 14},
    {
        name : "Voice",
        valueType : ParameterValueType::Binned,
        valueBinCount : 7,
        valueBinNames : s_voiceBinNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 1,
        midiCCMapping : 15
    },
    {name : "Tone", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 2, midiCCMapping : 16},
    {name : "Decay", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 3, midiCCMapping : 17},
    {name : "Timbre", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 4, midiCCMapping : 18},
    {
        name : "Tempo",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 100.f},
        knobMapping : 5,
        midiCCMapping : 19,
        minValue : minTempoDrum,
        maxValue : maxTempoDrum
    },
    {
        name : "DryThru",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_dryThruBinNames,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 20
    }
};

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

    instrument_ = 0;
    voice_ = 0;         // 0=bass; 1=synthbass; 2=snare; 3=synthsnare; 4=hihat

    float initial_freq = tempo_to_freq(GetParameterAsFloat(5));
    metro.Init(initial_freq, sample_rate);
    beat_count = 0;
 
    //auto_mode = false; // needed?

}

void DrumModule::ParameterChanged(int parameter_id) {

    if (parameter_id == 0) { // Level
       
    } else if (parameter_id == 1) {  // Voice
        instrument_ = GetParameterAsBinnedValue(1) - 1;
        if (instrument_ == 6) {
            auto_mode = true;
            beat_count = 0; // start beat at beginning
        } else {
            auto_mode = false;
        }

    }  else if (parameter_id == 2) {  // Tone
        instrument_ = GetParameterAsBinnedValue(1) - 1;
        if (instrument_ == 0) {     // snare
            bass.SetTone(GetParameterAsFloat(2));

        } else if (instrument_ == 1) { // bass
            synthbass.SetTone(GetParameterAsFloat(2));

        } else if (instrument_ == 2) { // hihat
            snare.SetTone(GetParameterAsFloat(2));

        } else if (instrument_ == 3) { //synth snare
            synthsnare.SetFmAmount(GetParameterAsFloat(2));  // Different function

        } else if (instrument_ == 4) { // synth bass
            hihat.SetTone(GetParameterAsFloat(2));

        }

    }  else if (parameter_id == 3) {  // Decay
        instrument_ = GetParameterAsBinnedValue(1) - 1;

        if (instrument_ == 0) {     // snare
            bass.SetDecay(GetParameterAsFloat(3));

        } else if (instrument_ == 1) { // bass
            synthbass.SetDecay(GetParameterAsFloat(3));

        } else if (instrument_ == 2) { // hihat
            snare.SetDecay(GetParameterAsFloat(3));

        } else if (instrument_ == 3) { //synth snare
            synthsnare.SetDecay(GetParameterAsFloat(3));

        } else if (instrument_ == 4) { // synth bass
            hihat.SetDecay(GetParameterAsFloat(3));

        }

    }  else if (parameter_id == 4) {  // Timbre
        instrument_ = GetParameterAsBinnedValue(1) - 1;

        if (instrument_ == 0) {     // snare
            bass.SetSelfFmAmount(GetParameterAsFloat(4)); // self fm amount

        } else if (instrument_ == 1) { // bass
            synthbass.SetDirtiness(GetParameterAsFloat(4)); // dirtiness

        } else if (instrument_ == 2) { // hihat
            snare.SetSnappy(GetParameterAsFloat(4));  // snappy

        } else if (instrument_ == 3) { //synth snare
            synthsnare.SetSnappy(GetParameterAsFloat(4)); // snappy

        } else if (instrument_ == 4) { // synth bass
            hihat.SetNoisiness(GetParameterAsFloat(4)); // noisiness

        }
    } else if (parameter_id == 5) {  // Tempo
        m_bpm = GetParameterAsFloat(5);
        float freq = tempo_to_freq(m_bpm);
        metro.SetFreq(freq);

    }
}

void DrumModule::OnNoteOn(float notenumber, float velocity) {
    // Note Off can come in as Note On w/ 0 Velocity
    if (velocity == 0.f) {
        
    } else {

        if (instrument_ == 0) {     // snare
            // Adjust the midi note number to be at a good frequency for bass drum at middle c
            float new_notenumber = notenumber - 12.0;
            new_notenumber = (new_notenumber < 12.0) ? 12.0 : new_notenumber;
            bass.SetFreq(mtof(new_notenumber));
            bass.SetAccent(velocity / 128.0);
            bass.Trig();

        } else if (instrument_ == 1) { // bass
            // Adjust the midi note number to be at a good frequency for bass drum at middle c
            float new_notenumber = notenumber - 12.0;
            new_notenumber = (new_notenumber < 12.0) ? 12.0 : new_notenumber;
            synthbass.SetFreq(mtof(new_notenumber));
            synthbass.SetAccent(velocity / 128.0);
            synthbass.Trig();

        } else if (instrument_ == 2) { // hihat
            snare.SetAccent(velocity / 128.0);
            snare.SetFreq(mtof(notenumber));
            snare.Trig();

        } else if (instrument_ == 3) { //synth snare
            synthsnare.SetAccent(velocity / 128.0);
            synthsnare.SetFreq(mtof(notenumber));
            synthsnare.Trig();

        } else if (instrument_ == 4) { // synth bass
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
                //snare.SetFreq(mtof(notenumber));
                snare.Trig();
            } else if (notenumber == 65.0) {
                voice_ = 3;
                synthsnare.SetAccent(velocity / 128.0);
                //synthsnare.SetFreq(mtof(notenumber));
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
    uint8_t tap = metro.Process();  //  1 is the tempo tap, 0 all other times

    // Blink the Tempo LED
    fonepole(tap_mag, 0.0, .001f);
    m_cachedEffectMagnitudeValue = tap_mag;

    if (tap == 1 && auto_mode) {
        tap_mag = tap;
        float temp_instrument = beat1[beat_count];
        if (temp_instrument >= 0) {
            instrument_ = temp_instrument;
            OnNoteOn(60.0, 110.0); // always play auto drums as middle c at 110 velocity (for now)
        }

        // Increment beat count from 0 to 15
        beat_count += 1;
        beat_count = beat_count > 15 ? 0 : beat_count;
    }

    float sum = 0.f;
    if (instrument_ == 0) {
        sum = bass.Process(false); 

    } else if (instrument_ == 1) {
        sum = synthbass.Process(false); 

    } else if (instrument_ == 2) {
        sum = snare.Process(false); 

    } else if (instrument_ == 3) {
        sum = synthsnare.Process(false); 

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
    if (GetParameterAsBinnedValue(6) > 1) {
        through_audioL = m_audioLeft; 
    }

    m_audioLeft = sum * GetParameterAsFloat(0) + through_audioL;
    m_audioRight = m_audioLeft;
}

void DrumModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);

    // TODO If the other new stuff works try this out next for stereo dry through
    //float through_audioR = 0.0; 
    //if (GetParameterAsBinnedValue(6) == 3;) {
    //    through_audioR = inR;
    //    m_audioRight = m_audioRight + through_audioR - inL; // DOES THIS MAKE SENSE? subtracting out left through
    //}


    // TODO Figure out how to send stereo through and still use the ProcessMono function
    
}

void DrumModule::SetTempo(uint32_t bpm) { 
    m_bpm = std::clamp(bpm, minTempoDrum, maxTempoDrum); 
    SetParameterAsFloat(5, m_bpm);
    float freq = tempo_to_freq(m_bpm);  // TODO Is this needed or will it go to the ParameterChanged function?
    metro.SetFreq(freq);                // TODO Is this needed or will it go to the ParameterChanged function?
}


float DrumModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);
    
    if (led_id == 1) {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
