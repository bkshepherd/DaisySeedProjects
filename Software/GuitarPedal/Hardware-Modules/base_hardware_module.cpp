#include "base_hardware_module.h"

using namespace daisy;
using namespace bkshepherd;

// Hardware related defines.
// Switches
static const int s_switchPins[] = {6, 5};

// Knobs
static const int s_knobPins[] = {15, 16, 17, 18, 19, 20};

// Encoders
static const int s_encoderPins[][3] = {{3, 2, 4}};

// LEDS
static const int s_ledPins[] = {22, 23};

// Midi
static const int s_midiPins[] = {30, 29};

// Display (Other display pins are default SPI pins)
static const int s_displayPins[] = {9, 11};

BaseHardwareModule::BaseHardwareModule() : m_knobCount(0),
                                            knobs(NULL),
                                            m_switchCount(0),
                                            switches(NULL),
                                            m_encoderCount(0),
                                            encoders(NULL),
                                            m_ledCount(0),
                                            leds(NULL),
                                            m_supportsMidi(false),
                                            midi(NULL)
{

}

BaseHardwareModule::~BaseHardwareModule()
{
    if (knobs != NULL)
    {
        delete [] knobs;
    }

    if (switches != NULL)
    {
        delete [] switches;
    }

    if (encoders != NULL)
    {
        delete [] encoders;
    }

    if (leds != NULL)
    {
        delete [] leds;
    }

    if (midi != NULL)
    {
        delete midi;
    }
}

void BaseHardwareModule::Init(bool boost)
{
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
    disp_cfg.driver_config.transport_config.pin_config.dc    = seed.GetPin(s_displayPins[0]);
    disp_cfg.driver_config.transport_config.pin_config.reset = seed.GetPin(s_displayPins[1]);
    display.Init(disp_cfg);

    SetAudioBlockSize(48);
}

void BaseHardwareModule::DelayMs(size_t del)
{
    seed.DelayMs(del);
}

void BaseHardwareModule::SetHidUpdateRates()
{
    if (knobs != NULL)
    {
        for(int i = 0; i < m_knobCount; i++)
        {
            knobs[i].SetSampleRate(AudioCallbackRate());
        }
    }

    if (leds != NULL)
    {
        for(int i = 0; i < m_ledCount; i++)
        {
            leds[i].SetSampleRate(AudioCallbackRate());
        }
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
    if (knobs != NULL)
    {
        for(int i = 0; i < m_knobCount; i++)
        {
            knobs[i].Process();
        }
    }
}

void BaseHardwareModule::ProcessDigitalControls()
{
    if (switches != NULL)
    {
        for(int i = 0; i < m_switchCount; i++)
        {
            switches[i].Debounce();
        }
    }

    if (encoders != NULL)
    {
        for(int i = 0; i < m_encoderCount; i++)
        {
            encoders[i].Debounce();
        }
    }
}

int BaseHardwareModule::GetNumberOfSamplesForTime(float time)
{
    return (int)(AudioSampleRate() * time);
}

int BaseHardwareModule::GetKnobCount()
{
    return m_knobCount;
}

float BaseHardwareModule::GetKnobValue(int knobID)
{
    if (knobs != NULL && knobID >= 0 && knobID < m_knobCount)
    {
        return knobs[knobID].Value();
    }

    return 0.0f;
}

int BaseHardwareModule::GetSwitchCount()
{
    return m_switchCount;
}

int BaseHardwareModule::GetEncoderCount()
{
    return m_encoderCount;
}

int BaseHardwareModule::GetLedCount()
{
    return m_ledCount;
}

void BaseHardwareModule::SetLed(int ledID, float bright)
{
    if (leds != NULL && ledID >= 0 && ledID < m_ledCount)
    {
        leds[ledID].Set(bright);
    }
}

void BaseHardwareModule::UpdateLeds()
{
    if (leds != NULL)
    {
        for(int i = 0; i < m_ledCount; i++)
        {
            leds[i].Update();
        }
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
    return m_supportsMidi;
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
    if (knobs != NULL)
    {
        delete [] knobs;
    }

    knobs = new AnalogControl[m_knobCount];

    // Set order of ADCs based on CHANNEL NUMBER
    AdcChannelConfig cfg[m_knobCount];

    // Init with Single Pins
    for (int i = 0; i < m_knobCount; i++)
    {
        cfg[i].InitSingle(seed.GetPin(s_knobPins[i]));
    }

    seed.adc.Init(cfg, m_knobCount);

    // Make an array of pointers to the knob.
    for(int i = 0; i < m_knobCount; i++)
    {
        knobs[i].Init(seed.adc.GetPtr(i), AudioCallbackRate());
    }
}

void BaseHardwareModule::InitSwitches()
{
    if (switches != NULL)
    {
        delete [] switches;
    }

    switches = new Switch[m_switchCount];

    for(int i = 0; i < m_switchCount; i++)
    {
        switches[i].Init(seed.GetPin(s_switchPins[i]));
    }
}

void BaseHardwareModule::InitEncoders()
{
    if (encoders != NULL)
    {
        delete [] encoders;
    }

    encoders = new Encoder[m_encoderCount];

    for (int i = 0; i < m_encoderCount; i++)
    {
        encoders[i].Init(seed.GetPin(s_encoderPins[i][0]),
                            seed.GetPin(s_encoderPins[i][1]),
                            seed.GetPin(s_encoderPins[i][2]));
    }
}

void BaseHardwareModule::InitLeds()
{
    if (leds != NULL)
    {
        delete [] leds;
    }

    leds = new Led[m_ledCount];

    for(int i = 0; i < m_ledCount; i++)
    {
        leds[i].Init(seed.GetPin(s_ledPins[i]), false);
    }
}

void BaseHardwareModule::InitMidi()
{
    if (midi != NULL)
    {
        delete midi;
    }

    if (m_supportsMidi)
    {
        midi = new MidiUartHandler();

        MidiUartHandler::Config midi_config;
        midi_config.transport_config.rx = seed.GetPin(s_midiPins[0]);
        midi_config.transport_config.tx = seed.GetPin(s_midiPins[1]);
        midi->Init(midi_config);
    }
}
