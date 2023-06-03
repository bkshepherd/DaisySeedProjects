#pragma once
#ifndef GUITAR_PEDAL_1590B_H
#define GUITAR_PEDAL_1590B_H /**< & */

#include "daisy_seed.h"

using namespace daisy;

namespace bkshepherd {

/**
   @brief Helpers and hardware definitions for a 1590B sized Guitar Pedal based on the Daisy Seed.
*/
class GuitarPedal1590B
{
  public:
    /** Switches */
    enum SwitchIndex
    {
        SWITCH_1,    /**< Footswitch */
        SWITCH_2,    /**< Footswitch */
        SWITCH_LAST, /**< Last enum item */
    };

    /** Knobs */
    enum KnobIndex
    {
        KNOB_1,    /**< & */
        KNOB_2,    /**< & */
        KNOB_3,    /**< & */
        KNOB_4,    /**< & */
        KNOB_LAST, /**< & */
    };

    /**  Status LEDs */
    enum LedIndex
    {
        LED_1,    /**< & */
        LED_2,    /**< & */
        LED_LAST, /**< & */
    };

    /** Constructor */
    GuitarPedal1590B() {}
    /** Destructor */
    ~GuitarPedal1590B() {}

    /** Initialize the pedal */
    void Init(bool boost = false);

    /**
       Wait before moving on.
       \param del Delay time in ms.
     */
    void DelayMs(size_t del);


    /** Starts the callback
    \param cb Interleaved callback function
    */
    void StartAudio(AudioHandle::InterleavingAudioCallback cb);

    /** Starts the callback
    \param cb multichannel callback function
    */
    void StartAudio(AudioHandle::AudioCallback cb);

    /**
       Switch callback functions
       \param cb New interleaved callback function.
    */
    void ChangeAudioCallback(AudioHandle::InterleavingAudioCallback cb);

    /**
       Switch callback functions
       \param cb New multichannel callback function.
    */
    void ChangeAudioCallback(AudioHandle::AudioCallback cb);

    /** Stops the audio if it is running. */
    void StopAudio();

    /** Updates the Audio Sample Rate, and reinitializes.
     ** Audio must be stopped for this to work.
     */
    void SetAudioSampleRate(SaiHandle::Config::SampleRate samplerate);

    /** Returns the audio sample rate in Hz as a floating point number.
     */
    float AudioSampleRate();

    /** Sets the number of samples processed per channel by the audio callback.
       \param size Audio block size
     */
    void SetAudioBlockSize(size_t size);

    /** Returns the number of samples per channel in a block of audio. */
    size_t AudioBlockSize();

    /** Returns the rate in Hz that the Audio callback is called */
    float AudioCallbackRate();

    /** Start analog to digital conversion. */
    void StartAdc();

    /** Stops Transfering data from the ADC */
    void StopAdc();

    /** Call at the same frequency as controls are read for stable readings.*/
    void ProcessAnalogControls();

    /** Process Analog and Digital Controls */
    inline void ProcessAllControls()
    {
        ProcessAnalogControls();
        ProcessDigitalControls();
    }

    /** Get value per knob.
    \param k Which knob to get
    \return Floating point knob position.
    */
    float GetKnobValue(KnobIndex k);

    /** Process digital controls */
    void ProcessDigitalControls();

    /** Toggle the Hardware Audio Bypass (if applicable) */
    void SetAudioBypass(bool enabled);

        /** Toggle the Hardware Audio Mute (if applicable) */
    void SetAudioMute(bool enabled);

    /** Turn all leds off */
    void ClearLeds();

    /**
       Set Led
       \param idx Led Index
       \param bright Brightness
     */
    void SetLed(LedIndex idx, float bright);

    /** Updates all the LEDs based on their values */
    void UpdateLeds();

    DaisySeed seed;    /**< & */
    AnalogControl knobs[KNOB_LAST]; /**< & */
    Switch        switches[SWITCH_LAST] /**< & */;
    Led           leds[LED_LAST]; /**< & */
    MidiUartHandler midi;
    GPIO audioBypassTrigger;
    bool audioBypass;
    GPIO audioMuteTrigger;
    bool audioMute;

  private:
    void SetHidUpdateRates();
    void InitSwitches();
    void InitLeds();
    void InitAnalogControls();
    void InitMidi();

    inline uint16_t* adc_ptr(const uint8_t chn) { return seed.adc.GetPtr(chn); }

};
} // namespace bkshepherd
#endif
