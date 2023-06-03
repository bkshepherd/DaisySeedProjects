#include "guitar_pedal_1590b.h"

using namespace daisy;
using namespace bkshepherd;

// Hardware related defines.
// Switches
#define SWITCH_1_PIN 6         // Foot Switch 1
#define SWITCH_2_PIN 5         // Foot Switch 2

// Knobs
#define KNOB_PIN_1 15
#define KNOB_PIN_2 16
#define KNOB_PIN_3 17
#define KNOB_PIN_4 18

#define LED_1_PIN 22
#define LED_2_PIN 23

void GuitarPedal1590B::Init(bool boost)
{
    // Initialize the seed hardware.
    seed.Configure();
    seed.Init(boost);

    // Initialize all the hardware accessories
    InitSwitches();
    InitLeds();
    InitAnalogControls();
    InitMidi();

    // Set Default Audio Configurations
    SetAudioBlockSize(4);
    SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    // Init the HW Audio Bypass
    audioBypassTrigger.Init(daisy::seed::D1, GPIO::Mode::OUTPUT);
    SetAudioBypass(true);

    // Init the HW Audio Mute
    audioMuteTrigger.Init(daisy::seed::D12, GPIO::Mode::OUTPUT);
    SetAudioMute(false);
}

void GuitarPedal1590B::DelayMs(size_t del)
{
    seed.DelayMs(del);
}

void GuitarPedal1590B::SetHidUpdateRates()
{
    for(size_t i = 0; i < KNOB_LAST; i++)
    {
        knobs[i].SetSampleRate(AudioCallbackRate());
    }
    for(size_t i = 0; i < LED_LAST; i++)
    {
        leds[i].SetSampleRate(AudioCallbackRate());
    }
}


void GuitarPedal1590B::StartAudio(AudioHandle::InterleavingAudioCallback cb)
{
    seed.StartAudio(cb);
}

void GuitarPedal1590B::StartAudio(AudioHandle::AudioCallback cb)
{
    seed.StartAudio(cb);
}

void GuitarPedal1590B::ChangeAudioCallback(AudioHandle::InterleavingAudioCallback cb)
{
    seed.ChangeAudioCallback(cb);
}

void GuitarPedal1590B::ChangeAudioCallback(AudioHandle::AudioCallback cb)
{
    seed.ChangeAudioCallback(cb);
}

void GuitarPedal1590B::StopAudio()
{
    seed.StopAudio();
}

void GuitarPedal1590B::SetAudioBlockSize(size_t size)
{
    seed.SetAudioBlockSize(size);
    SetHidUpdateRates();
}

size_t GuitarPedal1590B::AudioBlockSize()
{
    return seed.AudioBlockSize();
}

void GuitarPedal1590B::SetAudioSampleRate(SaiHandle::Config::SampleRate samplerate)
{
    seed.SetAudioSampleRate(samplerate);
    SetHidUpdateRates();
}

float GuitarPedal1590B::AudioSampleRate()
{
    return seed.AudioSampleRate();
}

float GuitarPedal1590B::AudioCallbackRate()
{
    return seed.AudioCallbackRate();
}

void GuitarPedal1590B::StartAdc()
{
    seed.adc.Start();
}

void GuitarPedal1590B::StopAdc()
{
    seed.adc.Stop();
}

void GuitarPedal1590B::ProcessAnalogControls()
{
    for(size_t i = 0; i < KNOB_LAST; i++)
    {
        knobs[i].Process();
    }
}

float GuitarPedal1590B::GetKnobValue(KnobIndex k)
{
    size_t idx;
    idx = k < KNOB_LAST ? k : KNOB_1;
    return knobs[idx].Value();
}

void GuitarPedal1590B::ProcessDigitalControls()
{
    for(size_t i = 0; i < SWITCH_LAST; i++)
    {
        switches[i].Debounce();
    }
}

void GuitarPedal1590B::SetAudioBypass(bool enabled)
{
    audioBypass = enabled;
    audioBypassTrigger.Write(!audioBypass);
}

void GuitarPedal1590B::SetAudioMute(bool enabled)
{
    audioMute = enabled;
    audioMuteTrigger.Write(audioMute);
}

void GuitarPedal1590B::ClearLeds()
{
    for(size_t i = 0; i < LED_LAST; i++)
    {
        SetLed(static_cast<LedIndex>(i), 0.0f);
    }
}

void GuitarPedal1590B::SetLed(LedIndex k, float bright)
{
    size_t idx;
    idx = k < LED_LAST ? k : LED_1;
    leds[idx].Set(bright);
}

void GuitarPedal1590B::UpdateLeds()
{
    for(size_t i = 0; i < LED_LAST; i++)
    {
        leds[i].Update();
    }
}

void GuitarPedal1590B::InitSwitches()
{
    uint8_t pin_numbers[SWITCH_LAST] = {
        SWITCH_1_PIN,
        SWITCH_2_PIN,
    };

    for(size_t i = 0; i < SWITCH_LAST; i++)
    {
        switches[i].Init(seed.GetPin(pin_numbers[i]));
    }
}

void GuitarPedal1590B::InitLeds()
{
    uint8_t pin_numbers[LED_LAST] = {
        LED_1_PIN,
        LED_2_PIN,
    };

    for(size_t i = 0; i < LED_LAST; i++)
    {
        leds[i].Init(seed.GetPin(pin_numbers[i]), false);
    }
}

void GuitarPedal1590B::InitAnalogControls()
{
    // Set order of ADCs based on CHANNEL NUMBER
    AdcChannelConfig cfg[KNOB_LAST];
    // Init with Single Pins
    cfg[KNOB_1].InitSingle(seed.GetPin(KNOB_PIN_1));
    cfg[KNOB_2].InitSingle(seed.GetPin(KNOB_PIN_2));
    cfg[KNOB_3].InitSingle(seed.GetPin(KNOB_PIN_3));
    cfg[KNOB_4].InitSingle(seed.GetPin(KNOB_PIN_4));
    
    seed.adc.Init(cfg, KNOB_LAST);
    // Make an array of pointers to the knob.
    for(int i = 0; i < KNOB_LAST; i++)
    {
        knobs[i].Init(seed.adc.GetPtr(i), AudioCallbackRate());
    }
}

void GuitarPedal1590B::InitMidi()
{
    MidiUartHandler::Config midi_config;
    midi.Init(midi_config);
}
