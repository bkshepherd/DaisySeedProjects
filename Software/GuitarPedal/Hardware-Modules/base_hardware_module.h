#pragma once
#ifndef BASE_HARDWARE_MODULE_H
#define BASE_HARDWARE_MODULE_H /**< & */

#include <vector>
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"

using namespace daisy;

/** Typedef the OledDisplay to make syntax cleaner below 
 *  This is a 4Wire SPI Transport controlling an 128x64 sized SSDD1306
 * 
 *  There are several other premade test 
*/
using MyOledDisplay = OledDisplay<SSD130x4WireSpi128x64Driver>;

namespace bkshepherd {

/**
   @brief Helpers and hardware definitions for a 125B sized Guitar Pedal based on the Daisy Seed.
*/
class BaseHardwareModule
{
  public:
    /** Constructor */
    BaseHardwareModule();
    /** Destructor */
    virtual ~BaseHardwareModule();

    /** Initialize the pedal */
    virtual void Init(bool boost = false);

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

    /** Process digital controls */
    void ProcessDigitalControls();

    /** Process Analog and Digital Controls */
    inline void ProcessAllControls()
    {
        ProcessAnalogControls();
        ProcessDigitalControls();
    }

    /** Get Numbers of Samples for a specified amount of time in seconds
    \param time Specified time in seconds.
    \return int number of samples at the current sample rate.
    */
    int GetNumberOfSamplesForTime(float time);

    /** Get number of knobs.
    \return Number of Knobs.
    */
    int GetKnobCount();

    /** Get value per knob.
    \param knobID Which knob to get
    \return Floating point knob position.
    */
    float GetKnobValue(int knobID);

    /** Get number of Switches.
    \return Number of Switches.
    */
    int GetSwitchCount();

    /** Get number of Encoders.
    \return Number of Encoders.
    */
    int GetEncoderCount();
    
    /** Get number of LEDs.
    \return Number of LEDs.
    */
    int GetLedCount();

    /**
       Set Led
       \param ledID Led Index
       \param bright Brightness
     */
    void SetLed(int ledID, float bright);

    /** Updates all the LEDs based on their values */
    void UpdateLeds();

        /** Toggle the Hardware Audio Bypass (if applicable) */
    void SetAudioBypass(bool enabled);

    /** Toggle the Hardware Audio Mute (if applicable) */
    void SetAudioMute(bool enabled);

    /** Checks to see if device hardware supports Stereo Audio*/
    bool SupportsStereo();

    /** Checks to see if device hardware supports MIDI*/
    bool SupportsMidi();

    /** Checks to see if device hardware supports the Display*/
    bool SupportsDisplay();

    /** Checks to see if device hardware supports True Bypass*/
    bool SupportsTrueBypass();

    DaisySeed seed;

    std::vector<AnalogControl> knobs;
    std::vector<Switch> switches;
    std::vector<Encoder> encoders;
    std::vector<Led> leds;

    
    MidiUartHandler midi;
    MyOledDisplay display;
    GPIO audioBypassTrigger;
    GPIO audioMuteTrigger;

  protected:
    bool m_supportsStereo;
    bool m_supportsMidi;
    bool m_supportsDisplay;
    bool m_supportsTrueBypass;
    bool m_audioBypass;
    bool m_audioMute;

    void SetHidUpdateRates();

    void InitKnobs(int count, Pin pins[]);
    void InitSwitches(int count, Pin pins[]);
    void InitEncoders(int count, Pin pins[][3]);
    void InitLeds(int count, Pin pins[]);
    void InitMidi(Pin rxPin, Pin txPin);
    void InitDisplay(Pin dcPin, Pin resetPin);
    void InitTrueBypass(Pin relayPin, Pin mutePin);

    inline uint16_t* adc_ptr(const uint8_t chn) { return seed.adc.GetPtr(chn); }
};
} // namespace bkshepherd
#endif
