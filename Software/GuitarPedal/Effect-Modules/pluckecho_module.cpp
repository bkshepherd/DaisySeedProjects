#include "pluckecho_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;

DelayLine<float, MAX_DELAY_PLUCKECHO> DSY_SDRAM_BSS delay;

static const int s_paramCount = 4;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {
        name : "StringDecay",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : 14
    },
    {
        name : "DelayTime",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : 15
    },
    {
        name : "DelayFdbk",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 2,
        midiCCMapping : 16
    },
    {name : "Level",
     valueType : ParameterValueType::Float,
     defaultValue : {.float_value = 0.5f},
     knobMapping : 3,
     midiCCMapping : 17}};

// Default Constructor
PluckEchoModule::PluckEchoModule()
    : BaseEffectModule(), m_freqMin(500.0f), m_freqMax(20000.0f),
      // m_verbMin(0.5f),
      // m_verbMax(0.97f), // Preventing unity gain
      nn(0.0f), trig(1.0), m_cachedEffectMagnitudeValue(1.0f) {
    // Set the name of the effect
    m_name = "PluckEcho";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
PluckEchoModule::~PluckEchoModule() {
    // No Code Needed
}

void PluckEchoModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    synth.Init(sample_rate);

    delay.Init();
    delay.SetDelay(sample_rate * 0.8f);

    // verb.Init(sample_rate);
    // verb.SetFeedback(0.85f);
    // verb.SetLpFreq(2000.0f);
}

void PluckEchoModule::ParameterChanged(int parameter_id) {
    if (parameter_id == 0) {
        float decay = 0.5f + GetParameterAsFloat(0) * 0.5f;
        synth.SetDecay(decay);
    }
}

void PluckEchoModule::OnNoteOn(float notenumber, float velocity) {
    // Note Off can come in as Note On w/ 0 Velocity
    if (velocity == 0.f) {

    } else {
        nn = notenumber;
        trig = 1.0;
    }
}

void PluckEchoModule::OnNoteOff(float notenumber, float velocity) {
    // Currently note off does nothing
}

void PluckEchoModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in); // This doesn't matter since not using input audio

    float through_audioL = m_audioLeft;
    // float through_audioR = m_audioRight;

    float kval = GetParameterAsFloat(1);
    float deltime = (0.001f + (kval * kval) * 5.0f) * 48000; // samplerate=48000

    float delfb = GetParameterAsFloat(2);

    // Smooth delaytime, and set.
    fonepole(smooth_time, deltime, 0.0005f);
    delay.SetDelay(smooth_time);

    // Synthesize Plucks
    float sig = synth.Process(trig, nn);
    trig = 0.0;

    //		// Handle Delay
    float delsig = delay.Read();
    delay.Write(sig + (delsig * delfb));

    // Create Reverb Send
    float dry = sig + delsig;
    float send = dry * 0.6f;
    // float wetl, wetr;
    // verb.Process(send, send, &wetl, &wetr);

    // Output
    m_audioLeft = (dry)*GetParameterAsFloat(3) * 1.5 + through_audioL; // 50/50 dry wet mix and level adjust
    m_audioRight = (dry)*GetParameterAsFloat(3) * 1.5 + through_audioL;
}

void PluckEchoModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    // ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(inL, inR);

    float through_audioL = m_audioLeft;
    float through_audioR = m_audioRight;

    float kval = GetParameterAsFloat(1);
    float deltime = (0.001f + (kval * kval) * 5.0f) * 48000; // samplerate=48000

    float delfb = GetParameterAsFloat(2);

    // Smooth delaytime, and set.
    fonepole(smooth_time, deltime, 0.0005f);
    delay.SetDelay(smooth_time);

    // Synthesize Plucks
    float sig = synth.Process(trig, nn);
    trig = 0.0;

    //		// Handle Delay
    float delsig = delay.Read();
    delay.Write(sig + (delsig * delfb));

    // Create Reverb Send
    float dry = sig + delsig;
    float send = dry * 0.6f;
    // float wetl, wetr;
    // verb.Process(send, send, &wetl, &wetr);

    // Output
    m_audioLeft = (dry)*GetParameterAsFloat(3) * 1.5 + through_audioL; // 50/50 dry wet mix and level adjust
    m_audioRight = (dry)*GetParameterAsFloat(3) * 1.5 + through_audioR;
}

float PluckEchoModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
