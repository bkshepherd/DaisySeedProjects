#pragma once
#ifndef REVERBDELAY_MODULE_H
#define REVERBDELAY_MODULE_H

#include <stdint.h>
#include "daisysp.h"
#include "Delays/delayline_revoct.h"
#include "Delays/delayline_reverse.h"
#include "base_effect_module.h"
#ifdef __cplusplus

/** @file reverb_delay_module.h */

using namespace daisysp;

// Delay Max Definitions (Assumes 48kHz samplerate)
#define MAX_DELAY static_cast<size_t>(48000.0f * 8.f) // 4 second max delay // Increased the max to 8 seconds, got horrible pop noise when set to 4 seconds, increasing buffer size fixes it for some reason. TODO figure out why?
#define MAX_DELAY_REV static_cast<size_t>(48000.0f * 8.f) // 8 second max delay (needs to be double for reverse, since read/write pointers are going opposite directions in the buffer)
#define MAX_DELAY_SPREAD static_cast<size_t>(4800.0f) //  50 ms for Spread effect 

// This is the core delay struct, which actually includes two delays, 
// one for forwared/octave, and one for reverse. This is required
// because the reverse delayline needs to be double the size of the
// normal delayline to have the same range of the Time control. Both
// delays are processed, with the main delay feeding into the reverse
// delay to allow for Reverse Octave. A lowpass filter is included in
// the feedback loop, which can tame the harsh high frequencies of the
// octave delay, or create a "fading into the distance" effect for the
// forward and reverse delays. A "level" param is included for modulation
// of the output volume, for stereo panning.
struct delay
{
    DelayLineRevOct<float, MAX_DELAY> *del;
    DelayLineReverse<float, MAX_DELAY_REV> *delreverse;
    float                        currentDelay;
    float                        delayTarget;
    float                        feedback = 0.0;
    float                        active = false;
    bool                         reverseMode = false;
    Tone                         toneOctLP;  // Low Pass 
    float                        level = 1.0;      // Level multiplier of output, added for stereo modulation
    float                        level_reverse = 1.0;      // Level multiplier of output, added for stereo modulation
    bool                         dual_delay = false;
    bool                         secondTapOn = false;
    
    float Process(float in)
    {
        //set delay times
        fonepole(currentDelay, delayTarget, .0002f);
        del->SetDelay(currentDelay);
        delreverse->SetDelay1(currentDelay);

        float del_read = del->Read();

        float read_reverse = delreverse->ReadRev(); // REVERSE

        float read = toneOctLP.Process(del_read);  // LP filter, tames harsh high frequencies on octave, has fading effect for normal/reverse

        float secondTap = 0.0;
        if (secondTapOn) {
            secondTap = del->ReadSecondTap();
        }
        //float read2 = delreverse->ReadFwd();
        if (active) {
            del->Write((feedback * read) + in);
            delreverse->Write((feedback * read) + in);  // Writing the read from fwd/oct delay line allows for combining oct and rev for reverse octave!
            //delreverse->Write((feedback * read2) + in); 
        } else {
            del->Write(feedback * read); // if not active, don't write any new sound to buffer
            delreverse->Write(feedback * read);
            //delreverse->Write((feedback * read2));
        }

        // TODO Figure out how to do dotted eighth with reverse

        if (dual_delay) {
            return read_reverse * level_reverse * 0.5 + (read + secondTap) * level * 0.5; // Half the volume to keep total level consistent
        } else if (reverseMode) {
            return read_reverse * level_reverse;
        } else {
            return (read + secondTap) * level;
        }
    }
};

// For stereo spread setting (delay the right channel signal from 0 to 50ms)      
//    A short, zero feedback (one repeat) delay for stereo spread

struct delay_spread
{
    DelayLine<float, MAX_DELAY_SPREAD> *del;
    float                        currentDelay;
    float                        delayTarget;
    float                        active = false;
    
    float Process(float in)
    {
        //set delay times
        fonepole(currentDelay, delayTarget, .0002f);
        del->SetDelay(currentDelay);

        float read = del->Read();
        if (active) {
            del->Write(in);
        } 

        return read;
    }
};


namespace bkshepherd
{

class ReverbDelayModule : public BaseEffectModule
{
  public:
    ReverbDelayModule();
    ~ReverbDelayModule();

    void Init(float sample_rate) override;
    void UpdateLEDRate();
    void CalculateDelayMix();
    void CalculateReverbMix();
    void ParameterChanged(int parameter_id) override;
    void ProcessModulation();
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void SetTempo(uint32_t bpm) override;
    float GetBrightnessForLED(int led_id) override;
 

  private:

    ReverbSc m_reverbStereo;
    float m_timeMin;
    float m_timeMax;
    float m_lpFreqMin;
    float m_lpFreqMax;
    float m_delaylpFreqMin;
    float m_delaylpFreqMax;
    float m_delaySamplesMin;
    float m_delaySamplesMax;
    float m_delaySpreadMin;
    float m_delaySpreadMax;
    float m_pdelRight_out;
    float m_currentMod;

    Oscillator modOsc;
    float m_modOscFreqMin;
    float m_modOscFreqMax;


    // Delays
    delay             delayLeft;
    delay             delayRight;
    delay_spread       delaySpread;

    // Mix params
    float delayWetMix = 0.5;
    float delayDryMix = 0.5;
    float reverbWetMix = 0.5;
    float reverbDryMix = 0.5;

    float reverb_level = 1.0;

    float effect_samplerate;

    // Oscillator for blinking tempo LED
    Oscillator led_osc;
    float m_LEDValue;
    

};
} // namespace bkshepherd
#endif
#endif
