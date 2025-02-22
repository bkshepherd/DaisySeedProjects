#include "granulardelay_module.h"

#include "../Util/audio_utilities.h"

using namespace bkshepherd;

// 1/2 second at 48kHz
#define MAX_SAMPLE_GRAN 24000


float DSY_SDRAM_BSS buffer_gran_delay[MAX_SAMPLE_GRAN];


static const char *s_grainEnvNames[3] = {"Cos", "SlowAtk", "FastAtk"};

static const int s_paramCount = 7;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {
        name : "Size(ms)",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 150.0f},
        knobMapping : 0,
        midiCCMapping : 14,
        minValue : 1,
        maxValue : 300
    },
    {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : 15
    },
    {
        name : "Pitch",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 2,
        midiCCMapping : 16,
        minValue : -12,
        maxValue : 12
    },
    {
        name : "Spread",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f},
        knobMapping : 3,
        midiCCMapping : 17
    },
    {  
        name : "GrainEnv",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_grainEnvNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 4,
        midiCCMapping : 18
    },
    {
        name : "Speed",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 5,
        midiCCMapping : 19,
        minValue : -2,
        maxValue : 2
    },

    {
        name : "Width",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 25.0f},
        knobMapping : -1,
        midiCCMapping : 20,
        minValue : 0,
        maxValue : 50
    }

};

// Default Constructor
GranularDelayModule::GranularDelayModule()
    : BaseEffectModule(), m_speed(0.5f), m_pitch(0.0f), m_grain_size(150.0f), m_width(0.5f) {
    // Set the name of the effect
    m_name = "GranDelay";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
GranularDelayModule::~GranularDelayModule() {
    // No Code Needed
}

void GranularDelayModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    // Init the looper
    m_looper.Init(buffer_gran_delay, MAX_SAMPLE_GRAN);
    m_looper.SetMode(static_cast<daisysp::Looper::Mode>(3)); // Frippertronics mode
    
    //m_looper.TrigRecord(); // Always run the frippertronics looper, so it acts like a delay
    //m_looper.SetMode(static_cast<daisysp::Looper::Mode>(1)); // One Shot Loop mode
    granular.Init(buffer_gran_delay, MAX_SAMPLE_GRAN, sample_rate, 0.0, 0.5);

    // Not sure if this is needed, should be defined by param initialization
    m_pitch = 0.0;

    m_hold = false;
    m_loop_recorded = false;
    first_count = 0;

    for(int i = 0; i < MAX_SAMPLE_GRAN; i++) { // hard coding sample length for now
        buffer_gran_delay[i] = 0.;
    }

    granular.setStereoSpread(0.4);

}


void GranularDelayModule::ParameterChanged(int parameter_id) {

    if (parameter_id == 0) {  // Size (handled in processmono)

    } else if (parameter_id == 1) {  // Mix (handled in processmono)
        

    } else if (parameter_id == 2) {
        // For now rounding to semitones, may change later (separate semitone and cent control?)
        int rounded_pitch = static_cast<int>(GetParameterAsFloat(2));
        m_pitch = rounded_pitch * 100;

    } else if (parameter_id == 3) {  // Stereo Spread
        granular.setStereoSpread(GetParameterAsFloat(3));

    } else if (parameter_id == 4) {   // Grain Env
        int option = GetParameterAsBinnedValue(4) - 1;

        granular.setEnvelopeMode(option);
   

    } else if (parameter_id == 5) {  // Speed (handled in processmono)


    } else if (parameter_id == 6) {  // Width (handled in processmono)


    }
}

void GranularDelayModule::AlternateFootswitchPressed() { 
    m_hold = !m_hold;
}


void GranularDelayModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    float input = in;

    float looperOutput = 0.0;

    

    if (!m_loop_recorded) {
        if (first_count == 0) {
            m_looper.TrigRecord(); 
        }
        if (first_count > 24001) {
            m_loop_recorded = true;
            m_looper.TrigRecord();
        } else {
            first_count += 1;
        }
    }

    // If not in Hold mode, process the next sample into looper
    if (!m_hold) {

        looperOutput = m_looper.Process(input);
        
    }
    
    /** Processes the granular player.
        \param speed playback speed. 1 is normal speed, 2 is double speed, 0.5 is half speed, etc. Negative values play the sample backwards.
        \param transposition transposition in cents. 100 cents is one semitone. Negative values transpose down, positive values transpose up.
        \param grain_size grain size in milliseconds. 1 is 1 millisecond, 1000 is 1 second. Does not accept negative values. Minimum value is 1.
        \param width width range in milliseconds. Will start each grain envelope at a random location within width range. 0 for no randomized location.
    */
    float gran_out_right = 0.0;
    float gran_out_left = 0.0;

    fonepole(m_current_grainsize, GetParameterAsFloat(0), .0001f); // decrease decimal to slow down transfer

    // Each GranularPlayerMod in the swarm starts at a different phase so the
    //  grains are offest from eachother to create a smoother sound. They also have
    //  different pitch settings. Play around with pitch variations to create 
    //  new textures.

    // Can only handle one?
    granular.Process(GetParameterAsFloat(5), m_pitch, m_current_grainsize, GetParameterAsFloat(6));
    gran_out_left += granular.getLeftOut();
    gran_out_right += granular.getRightOut();

    // Only one GranularPlayerMod works or really bad noise??

  
    m_audioLeft = gran_out_left * GetParameterAsFloat(1)  + input * (1.0 - GetParameterAsFloat(1));
    m_audioRight = gran_out_right * GetParameterAsFloat(1)  + input * (1.0 - GetParameterAsFloat(1));

}

void GranularDelayModule::ProcessStereo(float inL, float inR) {

    // Calculate the mono effect
    ProcessMono(inL);

}

float GranularDelayModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        // Enable the LED when held
        return value * m_hold;
    }

    return value;
}

