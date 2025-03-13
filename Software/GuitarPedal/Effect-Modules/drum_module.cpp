#include "drum_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;


static const char *s_modelBinNames[6] = {"Snare", "Bass", "HiHat",   "SynthSnare", "SynthBass", "Kit"};

static const int s_paramCount = 5;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {name : "Level", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 0, midiCCMapping : 14},
    {
        name : "Voice",
        valueType : ParameterValueType::Binned,
        valueBinCount : 6,
        valueBinNames : s_modelBinNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 1,
        midiCCMapping : 15
    },
    {name : "Tone", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 2, midiCCMapping : 16},
    {name : "Decay", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 3, midiCCMapping : 17},
    {name : "Timbre", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 4, midiCCMapping : 18}
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
    m_sustain = false;

}

void DrumModule::ParameterChanged(int parameter_id) {

    if (parameter_id == 0) { // Level
       
    } else if (parameter_id == 1) {  // Voice
        instrument_ = GetParameterAsBinnedValue(1) - 1;

    }  else if (parameter_id == 2) {  // Tone
        instrument_ = GetParameterAsBinnedValue(1) - 1;
        if (instrument_ == 0) {     // snare
            snare.SetTone(GetParameterAsFloat(2));

        } else if (instrument_ == 1) { // bass
            bass.SetTone(GetParameterAsFloat(2));

        } else if (instrument_ == 2) { // hihat
            hihat.SetTone(GetParameterAsFloat(2));

        } else if (instrument_ == 3) { //synth snare
            synthsnare.SetFmAmount(GetParameterAsFloat(2));  // Different function

        } else if (instrument_ == 4) { // synth bass
            synthbass.SetTone(GetParameterAsFloat(2));

        }

    }  else if (parameter_id == 3) {  // Decay
        instrument_ = GetParameterAsBinnedValue(1) - 1;

        if (instrument_ == 0) {     // snare
            snare.SetDecay(GetParameterAsFloat(3));

        } else if (instrument_ == 1) { // bass
            bass.SetDecay(GetParameterAsFloat(3));

        } else if (instrument_ == 2) { // hihat
            hihat.SetDecay(GetParameterAsFloat(3));

        } else if (instrument_ == 3) { //synth snare
            synthsnare.SetDecay(GetParameterAsFloat(3));

        } else if (instrument_ == 4) { // synth bass
            synthbass.SetDecay(GetParameterAsFloat(3));

        }

    }  else if (parameter_id == 4) {  // Timbre
        instrument_ = GetParameterAsBinnedValue(1) - 1;

        if (instrument_ == 0) {     // snare
            snare.SetSnappy(GetParameterAsFloat(4));  // snappy

        } else if (instrument_ == 1) { // bass
            bass.SetSelfFmAmount(GetParameterAsFloat(4)); // self fm amount

        } else if (instrument_ == 2) { // hihat
            hihat.SetNoisiness(GetParameterAsFloat(4)); // noisiness

        } else if (instrument_ == 3) { //synth snare
            synthsnare.SetSnappy(GetParameterAsFloat(4)); // snappy

        } else if (instrument_ == 4) { // synth bass
            synthbass.SetDirtiness(GetParameterAsFloat(4)); // dirtiness

        }
    } 
}

void DrumModule::OnNoteOn(float notenumber, float velocity) {
    // Note Off can come in as Note On w/ 0 Velocity
    if (velocity == 0.f) {
        
    } else {

        if (instrument_ == 0) {     // snare
            snare.SetAccent(velocity / 128.0);
            snare.SetFreq(mtof(notenumber));
            snare.Trig();

        } else if (instrument_ == 1) { // bass
            // Adjust the midi note number to be at a good frequency for bass drum at middle c
            float new_notenumber = notenumber - 12.0;
            new_notenumber = (new_notenumber < 12.0) ? 12.0 : new_notenumber;
            bass.SetFreq(mtof(new_notenumber));
            bass.SetAccent(velocity / 128.0);
            bass.Trig();

        } else if (instrument_ == 2) { // hihat
            // Adjust the midi note number to be at a good frequency for a hihat at middle c
            float new_notenumber = notenumber + 36.0;
            new_notenumber = (new_notenumber > 127.0) ? 127.0 : new_notenumber;
            hihat.SetFreq(mtof(new_notenumber));
            hihat.SetAccent(velocity / 128.0);
            hihat.Trig();

        } else if (instrument_ == 3) { //synth snare
            synthsnare.SetAccent(velocity / 128.0);
            synthsnare.SetFreq(mtof(notenumber));
            synthsnare.Trig();

        } else if (instrument_ == 4) { // synth bass
            // Adjust the midi note number to be at a good frequency for bass drum at middle c
            float new_notenumber = notenumber - 12.0;
            new_notenumber = (new_notenumber < 12.0) ? 12.0 : new_notenumber;
            synthbass.SetFreq(mtof(new_notenumber));
            synthbass.SetAccent(velocity / 128.0);
            synthbass.Trig();

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

void DrumModule::AlternateFootswitchPressed() { 
    m_sustain = !m_sustain;   // sustain didnt seem particularly usefull, but experiment before removing, maybe use for different hihat sounds
    
    snare.SetSustain(m_sustain); 
    bass.SetSustain(m_sustain);
    hihat.SetSustain(m_sustain);
    synthsnare.SetSustain(m_sustain);
    synthbass.SetSustain(m_sustain);

}

void DrumModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    // float through_audioL = m_audioLeft;  // Todo Add bool param option to pass audio through

    float sum = 0.f;
    if (instrument_ == 0) {
        sum = snare.Process(false); 

    } else if (instrument_ == 1) {
        sum = bass.Process(false); 

    } else if (instrument_ == 2) {
        sum = hihat.Process(false); 

    } else if (instrument_ == 3) {
        sum = synthsnare.Process(false); 

    } else if (instrument_ == 4) {
        sum = synthbass.Process(false); 

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

    m_audioLeft = sum * GetParameterAsFloat(0);
    m_audioRight = m_audioLeft;
}

void DrumModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);
}

float DrumModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        // Enable the LED when held
        return value * m_sustain;
    }

    return value;
}
