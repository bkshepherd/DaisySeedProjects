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

// Delay
#define MAX_DELAY static_cast<size_t>(48000 * 4.f) // 4 second max delay
#define MAX_DELAY_REV static_cast<size_t>(48000 * 8.f) // 8 second max delay (needs to be double for reverse, since read/write pointers are going opposite directions in the buffer)

struct delay
{
    DelayLineRevOct<float, MAX_DELAY> *del;
    DelayLineReverse<float, MAX_DELAY_REV> *delreverse;
    float                        currentDelay;
    float                        delayTarget;
    float                        feedback;
    float                        active = false;
    //float                        maxInternalDelay;
    bool                         reverseMode = false;
    Tone                         toneOctLP;  // Low Pass filter
    
    float Process(float in)
    {
        //set delay times
        fonepole(currentDelay, delayTarget, .0002f);
        del->SetDelay(currentDelay);
        delreverse->SetDelay1(currentDelay);

        float del_read = del->Read();
        float read_reverse = delreverse->ReadRev(); // REVERSE

        float read = toneOctLP.Process(del_read);  // LP filter, tames harsh high frequencies on octave, has fading effect for normal/reverse
        //float read2 = delreverse->ReadFwd();
        if (active) {
            del->Write((feedback * read) + in);
            delreverse->Write((feedback * read) + in);
            //delreverse->Write((feedback * read2) + in);  // Writing the read from fwd/oct delay line allows for combining oct and rev for reverse octave!
        } else {
            del->Write((feedback * read)); // if not active, don't write any new sound to buffer
            delreverse->Write((feedback * read) + in);
            //delreverse->Write((feedback * read2));
        }

        if (reverseMode)
            return read_reverse;
        else  
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
    float m_delaySamplesMin;
    float m_delaySamplesMax;

    // Delays
    delay             delayLeft;
    delay             delayRight;

    // Mix params
    float delayWetMix;
    float delayDryMix;
    float reverbWetMix;
    float reverbDryMix;

    float effect_samplerate;

    // Oscillator for blinking tempo LED
    Oscillator led_osc;
    float m_LEDValue;
    

};
} // namespace bkshepherd
#endif
#endif
