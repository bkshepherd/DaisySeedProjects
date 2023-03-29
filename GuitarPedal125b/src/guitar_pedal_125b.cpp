#include "guitar_pedal_125b.h"

using namespace daisy;
using namespace bkshepherd;

#ifndef SAMPLE_RATE
//#define SAMPLE_RATE DSY_AUDIO_SAMPLE_RATE
#define SAMPLE_RATE 48014.f
#endif

// Hardware related defines.
// Switches
#define SWITCH_1_PIN 6         // Foot Switch 1
#define SWITCH_2_PIN 5         // Foot Switch 2

// Knobs
#define KNOB_PIN_1 15
#define KNOB_PIN_2 16
#define KNOB_PIN_3 17
#define KNOB_PIN_4 18
#define KNOB_PIN_5 19
#define KNOB_PIN_6 20

// Encoders
#define ENCODER_1_BUTTON_PIN 4
#define ENCODER_1_A_PIN 3
#define ENCODER_1_B_PIN 2

// LEDS
#define LED_1_PIN 22
#define LED_2_PIN 23

// Midi
#define MIDI_RX_PIN 30
#define MIDI_TX_PIN 29

// Display
#define DISPLAY_CS_PIN 7
#define DISPLAY_SCK_PIN 8
#define DISPLAY_DC_PIN 9
#define DISPLAY_MOSI_PIN 10
#define DISPLAY_RESET_PIN 11

void GuitarPedal125B::Init(bool boost)
{
    // Set Some numbers up for accessors.
    // Initialize the hardware.
    seed.Configure();
    seed.Init(boost);
    InitSwitches();
    InitEncoders();
    InitLeds();
    InitAnalogControls();
    InitMidi();
    SetAudioBlockSize(48);

    // Init the HW Audio Bypass
    audioBypassTrigger.Init(daisy::seed::D1, GPIO::Mode::OUTPUT);
    SetAudioBypass(true);

    // Configure the Display
    MyOledDisplay::Config disp_cfg;
    disp_cfg.driver_config.transport_config.pin_config.dc    = seed.GetPin(DISPLAY_DC_PIN);
    disp_cfg.driver_config.transport_config.pin_config.reset = seed.GetPin(DISPLAY_RESET_PIN);
    display.Init(disp_cfg);
}

void GuitarPedal125B::DelayMs(size_t del)
{
    seed.DelayMs(del);
}

void GuitarPedal125B::SetHidUpdateRates()
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


void GuitarPedal125B::StartAudio(AudioHandle::InterleavingAudioCallback cb)
{
    seed.StartAudio(cb);
}

void GuitarPedal125B::StartAudio(AudioHandle::AudioCallback cb)
{
    seed.StartAudio(cb);
}

void GuitarPedal125B::ChangeAudioCallback(AudioHandle::InterleavingAudioCallback cb)
{
    seed.ChangeAudioCallback(cb);
}

void GuitarPedal125B::ChangeAudioCallback(AudioHandle::AudioCallback cb)
{
    seed.ChangeAudioCallback(cb);
}

void GuitarPedal125B::StopAudio()
{
    seed.StopAudio();
}

void GuitarPedal125B::SetAudioBlockSize(size_t size)
{
    seed.SetAudioBlockSize(size);
    SetHidUpdateRates();
}

size_t GuitarPedal125B::AudioBlockSize()
{
    return seed.AudioBlockSize();
}

void GuitarPedal125B::SetAudioSampleRate(SaiHandle::Config::SampleRate samplerate)
{
    seed.SetAudioSampleRate(samplerate);
    SetHidUpdateRates();
}

float GuitarPedal125B::AudioSampleRate()
{
    return seed.AudioSampleRate();
}

float GuitarPedal125B::AudioCallbackRate()
{
    return seed.AudioCallbackRate();
}

void GuitarPedal125B::StartAdc()
{
    seed.adc.Start();
}

void GuitarPedal125B::StopAdc()
{
    seed.adc.Stop();
}

void GuitarPedal125B::ProcessAnalogControls()
{
    for(size_t i = 0; i < KNOB_LAST; i++)
    {
        knobs[i].Process();
    }
}

float GuitarPedal125B::GetKnobValue(KnobIndex k)
{
    size_t idx;
    idx = k < KNOB_LAST ? k : KNOB_1;
    return knobs[idx].Value();
}

void GuitarPedal125B::ProcessDigitalControls()
{
    for(size_t i = 0; i < SWITCH_LAST; i++)
    {
        switches[i].Debounce();
    }

    for(size_t i = 0; i < ENCODER_LAST; i++)
    {
        encoders[i].Debounce();
    }
}

void GuitarPedal125B::SetAudioBypass(bool enabled)
{
    audioBypass = enabled;
    audioBypassTrigger.Write(!audioBypass);
}

void GuitarPedal125B::ClearLeds()
{
    for(size_t i = 0; i < LED_LAST; i++)
    {
        SetLed(static_cast<LedIndex>(i), 0.0f);
    }
}

void GuitarPedal125B::SetLed(LedIndex k, float bright)
{
    size_t idx;
    idx = k < LED_LAST ? k : LED_1;
    leds[idx].Set(bright);
    leds[idx].Update();
}

void GuitarPedal125B::InitSwitches()
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

void GuitarPedal125B::InitEncoders()
{
    encoders[ENCODER_1].Init(seed.GetPin(ENCODER_1_A_PIN),
                            seed.GetPin(ENCODER_1_B_PIN),
                            seed.GetPin(ENCODER_1_BUTTON_PIN));

}

void GuitarPedal125B::InitLeds()
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

void GuitarPedal125B::InitAnalogControls()
{
    // Set order of ADCs based on CHANNEL NUMBER
    AdcChannelConfig cfg[KNOB_LAST];
    // Init with Single Pins
    cfg[KNOB_1].InitSingle(seed.GetPin(KNOB_PIN_1));
    cfg[KNOB_2].InitSingle(seed.GetPin(KNOB_PIN_2));
    cfg[KNOB_3].InitSingle(seed.GetPin(KNOB_PIN_3));
    cfg[KNOB_4].InitSingle(seed.GetPin(KNOB_PIN_4));
    cfg[KNOB_5].InitSingle(seed.GetPin(KNOB_PIN_5));
    cfg[KNOB_6].InitSingle(seed.GetPin(KNOB_PIN_6));

    seed.adc.Init(cfg, KNOB_LAST);
    // Make an array of pointers to the knob.
    for(int i = 0; i < KNOB_LAST; i++)
    {
        knobs[i].Init(seed.adc.GetPtr(i), AudioCallbackRate());
    }
}

void GuitarPedal125B::InitMidi()
{
    MidiUartHandler::Config midi_config;
    midi_config.transport_config.rx = seed.GetPin(MIDI_RX_PIN);
    midi_config.transport_config.tx = seed.GetPin(MIDI_TX_PIN);
    midi.Init(midi_config);
}
