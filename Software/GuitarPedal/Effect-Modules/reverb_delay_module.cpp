#include "reverb_delay_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;

static const char* s_waveBinNames[5] = {"Sine", "Triangle", "Saw", "Ramp", "Square"}; //, "Poly Tri", "Poly Saw", "Poly Sqr"};  // Horrible loud sound when switching to poly tri, not every time, TODO whats going on?
static const char* s_modParamNames[5] = {"None", "DelayTime", "DelayLevel", "ReverbLevel", "DelayPan"};
static const char* s_delayModes[3] = {"Normal", "Triplett", "Dotted 8th"};

DelayLineRevOct<float, MAX_DELAY> DSY_SDRAM_BSS delayLineLeft;
DelayLineRevOct<float, MAX_DELAY> DSY_SDRAM_BSS delayLineRight;
DelayLineReverse<float, MAX_DELAY_REV> DSY_SDRAM_BSS delayLineRevLeft;
DelayLineReverse<float, MAX_DELAY_REV> DSY_SDRAM_BSS delayLineRevRight;
DelayLine<float, MAX_DELAY_SPREAD> DSY_SDRAM_BSS delayLineSpread;

static const int s_paramCount = 18;   // TODO: TEST STARTING WITH THE EXTREMES OF ALL PARAMETERS (high and low, this is where errors tend to occur)
static const ParameterMetaData s_metaData[s_paramCount] = {{name: "Delay Time", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 57, knobMapping: 0, midiCCMapping: 1},  //mod
                                                           {name: "D Feedback", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 57, knobMapping: 1, midiCCMapping: 22},
                                                           {name: "Delay Mix", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 57, knobMapping: 2, midiCCMapping: 23},
                                                           {name: "Reverb Time", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 57, knobMapping: 3, midiCCMapping: 24},
                                                           {name: "Reverb Damp", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 40, knobMapping: 4, midiCCMapping: 25},  //mod
                                                           {name: "Reverb Mix", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 57, knobMapping: 5, midiCCMapping: 26},
                                                           {name: "Delay Mode", valueType: ParameterValueType::Binned, valueBinCount: 3, valueBinNames: s_delayModes, defaultValue: 0, knobMapping: -1, midiCCMapping: 27},
                                                           {name: "Series D>R", valueType: ParameterValueType::Bool, valueBinCount: 0, defaultValue: 0, knobMapping: -1, midiCCMapping: 28},
                                                           {name: "Reverse", valueType: ParameterValueType::Bool, valueBinCount: 0, defaultValue: 0, knobMapping: -1, midiCCMapping: 29},
                                                           {name: "Octave", valueType: ParameterValueType::Bool, valueBinCount: 0, defaultValue: 0, knobMapping: -1, midiCCMapping: 30},
                                                           {name: "Delay LPF", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 120, knobMapping: -1, midiCCMapping: 31},  //mod
                                                  /*11*/   {name: "D Spread", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 30, knobMapping: -1, midiCCMapping: 32}, 
                                                           //{name: "Ping Pong", valueType: ParameterValueType::Bool, valueBinCount: 0, defaultValue: 0, knobMapping: -1, midiCCMapping: 33},
                                                  /*12*/   {name: "Dual Delay", valueType: ParameterValueType::Bool, valueBinCount: 0, defaultValue: 0, knobMapping: -1, midiCCMapping: 33},
                                                           {name: "Mod Amt", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 20, knobMapping: -1, midiCCMapping: 34}, 
                                                           {name: "Mod Rate", valueType: ParameterValueType::FloatMagnitude, valueBinCount: 0, defaultValue: 30, knobMapping: -1, midiCCMapping: 35}, 
                                                           {name: "Mod Param", valueType: ParameterValueType::Binned, valueBinCount: 5, valueBinNames: s_modParamNames, defaultValue: 0, knobMapping: -1, midiCCMapping: 36},
                                                           {name: "Mod Wave", valueType: ParameterValueType::Binned, valueBinCount: 5, valueBinNames: s_waveBinNames, defaultValue: 0, knobMapping: -1, midiCCMapping: 37},
                                                           {name: "Sync Mod F", valueType: ParameterValueType::Bool, valueBinCount: 0, defaultValue: 0, knobMapping: -1, midiCCMapping: 38}
                                                           };

                                                           

// Default Constructor
ReverbDelayModule::ReverbDelayModule() : BaseEffectModule(),
                                        m_timeMin(0.6f),
                                        m_timeMax(1.0f),
                                        m_lpFreqMin(600.0f),
                                        m_lpFreqMax(16000.0f),
                                        m_delaylpFreqMin(300.0f),
                                        m_delaylpFreqMax(20000.0f),
                                        m_delaySamplesMin(2400.0f),
                                        m_delaySamplesMax(192000.0f),
                                        m_delaySpreadMin(24.0f),
                                        m_delaySpreadMax(2400.0f),
                                        //m_delayPPMin(1200.0f),
                                        //m_delayPPMax(96000.0f),
                                        m_pdelRight_out(0.0),
                                        m_modOscFreqMin(0.0),
                                        m_modOscFreqMax(3.0),
                                        m_currentMod(1.0),
                                        m_LEDValue(1.0f)
{
    // Set the name of the effect
    m_name = "Verb Delay";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);

}

// Destructor
ReverbDelayModule::~ReverbDelayModule()
{
    // No Code Needed
}

void ReverbDelayModule::UpdateLEDRate()
{
    // Update the LED oscillator frequency based on the current timeParam
    float timeParam = GetParameterAsMagnitude(0);
    float delaySamples = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam;
    float delayFreq =  effect_samplerate / delaySamples;
    led_osc.SetFreq(delayFreq / 2.0);   
}

void ReverbDelayModule::CalculateDelayMix()
{
    // Handle Normal or Alternate Mode Mix Controls
    //    A computationally cheap mostly energy constant crossfade from SignalSmith Blog
    //    https://signalsmith-audio.co.uk/writing/2021/cheap-energy-crossfade/

    float delMixKnob = GetParameterAsMagnitude(2);
    float x2 = 1.0 - delMixKnob;
    float A = delMixKnob*x2;
    float B = A * (1.0 + 1.4186 * A);
    float C = B + delMixKnob;
    float D = B + x2;

    delayWetMix = C * C;
    delayDryMix = D * D;

}

void ReverbDelayModule::CalculateReverbMix()
{
    // Handle Normal or Alternate Mode Mix Controls
    //    A computationally cheap mostly energy constant crossfade from SignalSmith Blog
    //    https://signalsmith-audio.co.uk/writing/2021/cheap-energy-crossfade/

    float revMixKnob = GetParameterAsMagnitude(5);
    float x2 = 1.0 - revMixKnob;
    float A = revMixKnob*x2;
    float B = A * (1.0 + 1.4186 * A);
    float C = B + revMixKnob;
    float D = B + x2;

    reverbWetMix = C * C;
    reverbDryMix = D * D;

}

void ReverbDelayModule::Init(float sample_rate)
{
    BaseEffectModule::Init(sample_rate);

    delayLineLeft.Init();
    delayLineRevLeft.Init();
    delayLeft.del = &delayLineLeft;
    delayLeft.delreverse = &delayLineRevLeft;
    delayLeft.delayTarget = 24000; // in samples
    delayLeft.feedback = 0.0;
    delayLeft.active = true;     // Default to no delay
    delayLeft.toneOctLP.Init(sample_rate);
    delayLeft.toneOctLP.SetFreq(20000.0);

    delayLineRight.Init();
    delayLineRevRight.Init();
    delayRight.del = &delayLineRight;
    delayRight.delreverse = &delayLineRevRight;
    delayRight.delayTarget = 24000; // in samples
    delayRight.feedback = 0.0;
    delayRight.active = true;     // Default to no 
    delayRight.toneOctLP.Init(sample_rate);
    delayRight.toneOctLP.SetFreq(20000.0);

    delayLineSpread.Init();
    delaySpread.del = &delayLineSpread;
    delaySpread.delayTarget = 1500; // in samples
    delaySpread.active = true; 

    m_reverbStereo.Init(sample_rate);
    m_reverbStereo.SetFeedback(0.0);
 
    effect_samplerate = sample_rate;

    led_osc.Init(sample_rate);
    led_osc.SetWaveform(1);
    led_osc.SetFreq(2.0);

    modOsc.Init(sample_rate);
    modOsc.SetAmp(1.0);

    CalculateDelayMix(); 
    CalculateReverbMix();
}

void ReverbDelayModule::ParameterChanged(int parameter_id)
{
    if (parameter_id == 0) {  // Delay Time
        UpdateLEDRate();
    } else if (parameter_id == 2) {  // Delay Mix
        CalculateDelayMix();
    } else if (parameter_id == 5) {  // Reverb Mix
        CalculateReverbMix();
    } else if (parameter_id == 6) {  // Delay Mode
        int delay_mode_temp = (GetParameterAsBinnedValue(6) - 1);
        if (delay_mode_temp > 0) {
            delayLeft.secondTapOn = true; // triplett, dotted 8th
            delayRight.secondTapOn = true; // triplett, dotted 8th
            if (delay_mode_temp == 1) {
                delayLeft.del->set2ndTapFraction(0.6666667); // triplett
                delayRight.del->set2ndTapFraction(0.6666667); // triplett
            } else if (delay_mode_temp == 2) {
                delayLeft.del->set2ndTapFraction(0.75); // dotted eighth
                delayRight.del->set2ndTapFraction(0.75); // dotted eighth
            } 
        } else {
            delayLeft.secondTapOn = false;
            delayRight.secondTapOn = false;   
        }
    } else if (parameter_id = 10) {
        delayLeft.toneOctLP.SetFreq(m_delaylpFreqMin + (m_delaylpFreqMax - m_delaylpFreqMin) * GetParameterAsMagnitude(10));
        delayRight.toneOctLP.SetFreq(m_delaylpFreqMin + (m_delaylpFreqMax - m_delaylpFreqMin) * GetParameterAsMagnitude(10));
    }
}

void ReverbDelayModule::ProcessModulation()
{
    int modParam = (GetParameterAsBinnedValue(15) - 1);
    // Calculate Modulation
    modOsc.SetWaveform(GetParameterAsBinnedValue(16) - 1);
   
    if (GetParameterAsBool(17)) {  // If mod frequency synced to delay time, override mod rate setting
        float dividor;
        if (modParam == 2 || modParam == 3) {
            dividor = 2.0; 
        } else {
            dividor = 4.0; 
        }
        float freq = (effect_samplerate / delayLeft.delayTarget) / dividor;
        modOsc.SetFreq(freq);
    } else {
        modOsc.SetFreq(m_modOscFreqMin + (m_modOscFreqMax - m_modOscFreqMin) * GetParameterAsMagnitude(14));
    }
    
    // Ease the effect value into it's target to avoid clipping with square or sawtooth waves
    fonepole(m_currentMod, modOsc.Process(), .01f);
    float mod = m_currentMod;
    float mod_amount = GetParameterAsMagnitude(13);

     // {"None", "DelayTime", "DelayLevel", "ReverbLevel", "DelayPan"};
    if (modParam == 1) {         
        float timeParam = GetParameterAsMagnitude(0);
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

void ReverbDelayModule::ProcessMono(float in)
{
    BaseEffectModule::ProcessMono(in);

    // Calculate the effect
    float timeParam = GetParameterAsMagnitude(0);

    delayLeft.delayTarget = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam;
    delayRight.delayTarget = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam;

    delayLeft.feedback = GetParameterAsMagnitude(1);
    delayRight.feedback = GetParameterAsMagnitude(1);

    delayLeft.reverseMode = GetParameterAsBool(8);
    delayRight.reverseMode = GetParameterAsBool(8);

    delayLeft.del->setOctave(GetParameterAsBool(9)); 
    delayRight.del->setOctave(GetParameterAsBool(9));

    delayLeft.dual_delay = GetParameterAsBool(12); 
    delayRight.dual_delay = GetParameterAsBool(12);


    if (GetParameterAsMagnitude(12)) {       // If dual delay is turned on, spread controls the L/R panning of the two delays
        delayLeft.level = GetParameterAsMagnitude(11) + 1.0;
        delayRight.level = 1.0 - GetParameterAsMagnitude(11);

        delayLeft.level_reverse = 1.0 - GetParameterAsMagnitude(11); 
        delayRight.level_reverse = GetParameterAsMagnitude(11) + 1.0;

    } else {  // If dual delay is off reset the levels to normal, spread controls the amount of additional delay applied to the right channel
        delayLeft.level = 1.0; 
        delayRight.level = 1.0;
        delayLeft.level_reverse = 1.0; 
        delayRight.level_reverse = 1.0;
    }

    // Calculate Reverb Params
    reverb_level = 1.0;
    m_reverbStereo.SetFeedback(m_timeMin + GetParameterAsMagnitude(3) * (m_timeMax - m_timeMin));
    float invertedFreq = 1.0 - GetParameterAsMagnitude(4); // Invert the damping param so that knob left is less dampening, knob right is more dampening
    invertedFreq = invertedFreq * invertedFreq; // also square it for exponential taper (more control over lower frequencies)
    m_reverbStereo.SetLpFreq(m_lpFreqMin + invertedFreq * (m_lpFreqMax - m_lpFreqMin));

    // Modulation, this overwrites any previous parameter settings for the modulated param - TODO Better way to do this for less processing?
    ProcessModulation();

    float delLeft_out = delayLeft.Process(m_audioLeft);
    float delRight_out = delayRight.Process(m_audioRight);
    //float delRight_out = delLeft_out;

    // Calculate any delay spread 
    delaySpread.delayTarget = m_delaySpreadMin + (m_delaySpreadMax - m_delaySpreadMin) * GetParameterAsMagnitude(11);
    float delSpread_out = delaySpread.Process(delRight_out);  
    if (GetParameterRaw(11) > 0 && !GetParameterAsBool(12)) {
        delRight_out = delSpread_out; 
    }

    float delay_out_left = delLeft_out * delayWetMix + m_audioLeft * delayDryMix;
    float delay_out_right  = delRight_out * delayWetMix + m_audioRight * delayDryMix;

    // REVERB //////////////
    float sendl, sendr, wetl, wetr;  // Reverb Inputs/Outputs
    if (GetParameterAsBool(7))
    {
        sendl = delay_out_left;
        sendr = delay_out_right;
    } else {
        sendl = m_audioLeft;
        sendr = m_audioRight;
    }

    m_reverbStereo.Process(sendl, sendr, &wetl, &wetr);

    m_audioLeft = wetl * reverbWetMix * reverb_level / 2.0 + m_audioLeft * reverbDryMix;    // divide by 2 for volume correction
    m_audioRight = wetr * reverbWetMix * reverb_level / 2.0 + m_audioRight * reverbDryMix;

    if (!GetParameterAsBool(7)) { // If not series mode
        m_audioLeft = (m_audioLeft + delay_out_left) / 2.0;  // Dividing by 2 to compensate for double signal volume with both reverb and delay mix outputs
        m_audioRight = (m_audioRight + delay_out_right) / 2.0;
    }


}

void ReverbDelayModule::ProcessStereo(float inL, float inR)
{
    // Calculate the mono effect
    //ProcessMono(inL);  // Can't simply call the mono processing because of how the reverb is calculated

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(inL, inR);

    ProcessModulation();

    // Calculate the effect
    float timeParam = GetParameterAsMagnitude(0);

    delayLeft.delayTarget = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam;
    delayRight.delayTarget = m_delaySamplesMin + (m_delaySamplesMax - m_delaySamplesMin) * timeParam;

    delayLeft.feedback = GetParameterAsMagnitude(1);
    delayRight.feedback = GetParameterAsMagnitude(1);

    delayLeft.reverseMode = GetParameterAsBool(8);
    delayRight.reverseMode = GetParameterAsBool(8);

    delayLeft.del->setOctave(GetParameterAsBool(9));
    delayRight.del->setOctave(GetParameterAsBool(9));

    delayLeft.dual_delay = GetParameterAsBool(12); 
    delayRight.dual_delay = GetParameterAsBool(12);
    

    if (GetParameterAsMagnitude(12)) {       // If dual delay is turned on, spread controls the L/R panning of the two delays
        delayLeft.level = GetParameterAsMagnitude(11) + 1.0; 
        delayRight.level = 1.0 - GetParameterAsMagnitude(11);

        delayLeft.level_reverse = 1.0 - GetParameterAsMagnitude(11); 
        delayRight.level_reverse = GetParameterAsMagnitude(11) + 1.0;

    } else {  // If dual delay is off reset the levels to normal, spread controls the amount of additional delay applied to the right channel
        delayLeft.level = 1.0; 
        delayRight.level = 1.0;
        delayLeft.level_reverse = 1.0; 
        delayRight.level_reverse = 1.0;
    }


    // Calculate Reverb Params
    reverb_level = 1.0;
    m_reverbStereo.SetFeedback(m_timeMin + GetParameterAsMagnitude(3) * (m_timeMax - m_timeMin));
    float invertedFreq = 1.0 - GetParameterAsMagnitude(4); // Invert the damping param so that knob left is less dampening, knob right is more dampening
    invertedFreq = invertedFreq * invertedFreq; // also square it for exponential taper (more control over lower frequencies)
    m_reverbStereo.SetLpFreq(m_lpFreqMin + invertedFreq * (m_lpFreqMax - m_lpFreqMin));

    // Modulation, this overwrites any previous parameter settings for the modulated param
    ProcessModulation();

    float delLeft_out = delayLeft.Process(m_audioLeft);
    float delRight_out = delayRight.Process(m_audioRight);

    // Calculate any delay spread 
    delaySpread.delayTarget = m_delaySpreadMin + (m_delaySpreadMax - m_delaySpreadMin) * GetParameterAsMagnitude(11);
    float delSpread_out = delaySpread.Process(delRight_out);  
    if (GetParameterRaw(11) > 0 && !GetParameterAsBool(12)) { // If spread > 0 and dual delay isn't on
        delRight_out = delSpread_out; 
    }

    float delay_out_left = delLeft_out * delayWetMix + m_audioLeft * delayDryMix;
    float delay_out_right  = delRight_out * delayWetMix + m_audioRight * delayDryMix;

    /// REVERB
    float sendl, sendr, wetl, wetr;  // Reverb Inputs/Outputs
    if (GetParameterAsBool(7))
    {
        sendl = delay_out_left;
        sendr = delay_out_right;
    } else {
        sendl = m_audioLeft;
        sendr = m_audioRight;
    }

    m_reverbStereo.Process(sendl, sendr, &wetl, &wetr);

    m_audioLeft = wetl * reverbWetMix * reverb_level / 2.0 + m_audioLeft * reverbDryMix;
    m_audioRight = wetr * reverbWetMix * reverb_level / 2.0 + m_audioRight * reverbDryMix;

    if (!GetParameterAsBool(7)) { // If not series mode
        m_audioLeft = (m_audioLeft + delay_out_left) / 2.0;
        m_audioRight = (m_audioRight + delay_out_right) / 2.0;
    }
}

// Set the delay time from the tap tempo  TODO: Currently the tap tempo led isn't set to delay time on pedal boot up, how to do this?
void ReverbDelayModule::SetTempo(uint32_t bpm)
{
    float freq = tempo_to_freq(bpm);
    float delay_in_samples = effect_samplerate / freq; 

    if (delay_in_samples <= m_delaySamplesMin)
    {
        SetParameterRaw(0, 0);
    }
    else if (delay_in_samples >= m_delaySamplesMax)
    {
        SetParameterRaw(0, 127);
    }
    else 
    {
        // Get the parameter as close as we can to target tempo
        SetParameterRaw(0, ((delay_in_samples - m_delaySamplesMin) / (m_delaySamplesMax - m_delaySamplesMin)) * 127); // TODO This was "* 128", verify 
    }
    UpdateLEDRate();
}

float ReverbDelayModule::GetBrightnessForLED(int led_id)
{    
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    float osc_val = led_osc.Process();
    if (osc_val > 0.45) { 
        m_LEDValue = 1.0;
    } else { 
        m_LEDValue = 0.0;
    }

    if (led_id == 1)
    {
        return value * m_LEDValue;
    }

    return value;
}