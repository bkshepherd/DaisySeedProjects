#include "base_hardware_module.h"

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

// Display (Other display pins are default SPI pins)
#define DISPLAY_DC_PIN 9
#define DISPLAY_RESET_PIN 11

BaseHardwareModule::BaseHardwareModule()
{

}

BaseHardwareModule::~BaseHardwareModule()
{

}

void BaseHardwareModule::Init(bool boost)
{
    // Set Some numbers up for accessors.
    // Initialize the hardware.
    seed.Configure();
    seed.Init(boost);

    InitKnobs();
    InitSwitches();
    InitEncoders();
    InitLeds();
    InitMidi();

    // Init the HW Audio Bypass
    audioBypassTrigger.Init(daisy::seed::D1, GPIO::Mode::OUTPUT);
    SetAudioBypass(true);

    // Init the HW Audio Mute
    audioMuteTrigger.Init(daisy::seed::D12, GPIO::Mode::OUTPUT);
    SetAudioMute(false);

    // Configure the Display
    MyOledDisplay::Config disp_cfg;
    disp_cfg.driver_config.transport_config.pin_config.dc    = seed.GetPin(DISPLAY_DC_PIN);
    disp_cfg.driver_config.transport_config.pin_config.reset = seed.GetPin(DISPLAY_RESET_PIN);
    display.Init(disp_cfg);

    SetAudioBlockSize(48);
}

void BaseHardwareModule::DelayMs(size_t del)
{
    seed.DelayMs(del);
}

void BaseHardwareModule::SetHidUpdateRates()
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


void BaseHardwareModule::StartAudio(AudioHandle::InterleavingAudioCallback cb)
{
    seed.StartAudio(cb);
}

void BaseHardwareModule::StartAudio(AudioHandle::AudioCallback cb)
{
    seed.StartAudio(cb);
}

void BaseHardwareModule::ChangeAudioCallback(AudioHandle::InterleavingAudioCallback cb)
{
    seed.ChangeAudioCallback(cb);
}

void BaseHardwareModule::ChangeAudioCallback(AudioHandle::AudioCallback cb)
{
    seed.ChangeAudioCallback(cb);
}

void BaseHardwareModule::StopAudio()
{
    seed.StopAudio();
}

void BaseHardwareModule::SetAudioBlockSize(size_t size)
{
    seed.SetAudioBlockSize(size);
    SetHidUpdateRates();
}

size_t BaseHardwareModule::AudioBlockSize()
{
    return seed.AudioBlockSize();
}

void BaseHardwareModule::SetAudioSampleRate(SaiHandle::Config::SampleRate samplerate)
{
    seed.SetAudioSampleRate(samplerate);
    SetHidUpdateRates();
}

float BaseHardwareModule::AudioSampleRate()
{
    return seed.AudioSampleRate();
}

float BaseHardwareModule::AudioCallbackRate()
{
    return seed.AudioCallbackRate();
}

void BaseHardwareModule::StartAdc()
{
    seed.adc.Start();
}

void BaseHardwareModule::StopAdc()
{
    seed.adc.Stop();
}

void BaseHardwareModule::ProcessAnalogControls()
{
    for(size_t i = 0; i < KNOB_LAST; i++)
    {
        knobs[i].Process();
    }
}

void BaseHardwareModule::ProcessDigitalControls()
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

int BaseHardwareModule::GetNumberOfSamplesForTime(float time)
{
    return (int)(AudioSampleRate() * time);
}

int BaseHardwareModule::GetKnobCount()
{
    return KNOB_LAST;
}

float BaseHardwareModule::GetKnobValue(int knobID)
{
    size_t idx;
    idx = knobID < KNOB_LAST ? knobID : KNOB_1;
    return knobs[idx].Value();
}

int BaseHardwareModule::GetSwitchCount()
{
    return SWITCH_LAST;
}

int BaseHardwareModule::GetEncoderCount()
{
    return ENCODER_LAST;
}

int BaseHardwareModule::GetLedCount()
{
    return LED_LAST;
}

void BaseHardwareModule::SetLed(int ledID, float bright)
{
    size_t idx;
    idx = ledID < LED_LAST ? ledID : LED_1;
    leds[idx].Set(bright);
}

void BaseHardwareModule::UpdateLeds()
{
    for(size_t i = 0; i < LED_LAST; i++)
    {
        leds[i].Update();
    }
}

void BaseHardwareModule::SetAudioBypass(bool enabled)
{
    audioBypass = enabled;
    audioBypassTrigger.Write(!audioBypass);
}

void BaseHardwareModule::SetAudioMute(bool enabled)
{
    audioMute = enabled;
    audioMuteTrigger.Write(audioMute);
}

bool BaseHardwareModule::SupportsMidi()
{
    return true;
}

bool BaseHardwareModule::SupportsDisplay()
{
    return true;
}

bool BaseHardwareModule::SupportsTrueBypass()
{
    return true;
}

void BaseHardwareModule::InitKnobs()
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

void BaseHardwareModule::InitSwitches()
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

void BaseHardwareModule::InitEncoders()
{
    encoders[ENCODER_1].Init(seed.GetPin(ENCODER_1_A_PIN),
                            seed.GetPin(ENCODER_1_B_PIN),
                            seed.GetPin(ENCODER_1_BUTTON_PIN));

}

void BaseHardwareModule::InitLeds()
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

void BaseHardwareModule::InitMidi()
{
    MidiUartHandler::Config midi_config;
    midi_config.transport_config.rx = seed.GetPin(MIDI_RX_PIN);
    midi_config.transport_config.tx = seed.GetPin(MIDI_TX_PIN);
    midi.Init(midi_config);
}
