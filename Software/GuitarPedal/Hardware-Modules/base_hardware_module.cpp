#include "base_hardware_module.h"

using namespace daisy;
using namespace bkshepherd;

BaseHardwareModule::BaseHardwareModule() : m_supportsMidi(false),
                                           m_supportsDisplay(false),
                                           m_supportsTrueBypass(false)
{

}

BaseHardwareModule::~BaseHardwareModule()
{

}

void BaseHardwareModule::Init(bool boost)
{
    // Initialize the hardware.
    seed.Configure();
    seed.Init(boost);

    SetAudioBlockSize(48);
}

void BaseHardwareModule::DelayMs(size_t del)
{
    seed.DelayMs(del);
}

void BaseHardwareModule::SetHidUpdateRates()
{
    if (!knobs.empty())
    {
        for(int i = 0; i < GetKnobCount(); i++)
        {
            knobs[i].SetSampleRate(AudioCallbackRate());
        }
    }

    if (!leds.empty())
    {
        for(int i = 0; i < GetLedCount(); i++)
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
    if (!knobs.empty())
    {
        for(int i = 0; i < GetKnobCount(); i++)
        {
            knobs[i].Process();
        }
    }
}

void BaseHardwareModule::ProcessDigitalControls()
{
    if (!switches.empty())
    {
        for(int i = 0; i < GetSwitchCount(); i++)
        {
            switches[i].Debounce();
        }
    }

    if (!encoders.empty())
    {
        for(int i = 0; i < GetEncoderCount(); i++)
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
    return knobs.size();
}

float BaseHardwareModule::GetKnobValue(int knobID)
{
    if (!knobs.empty() && knobID >= 0 && knobID < GetKnobCount())
    {
        return knobs[knobID].Value();
    }

    return 0.0f;
}

int BaseHardwareModule::GetSwitchCount()
{
    return switches.size();
}

int BaseHardwareModule::GetEncoderCount()
{
    return encoders.size();
}

int BaseHardwareModule::GetLedCount()
{
    return leds.size();
}

void BaseHardwareModule::SetLed(int ledID, float bright)
{
    if (!leds.empty() && ledID >= 0 && ledID < GetLedCount())
    {
        leds[ledID].Set(bright);
    }
}

void BaseHardwareModule::UpdateLeds()
{
    if (!leds.empty())
    {
        for(int i = 0; i < GetLedCount(); i++)
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
    return m_supportsDisplay;
}

bool BaseHardwareModule::SupportsTrueBypass()
{
    return m_supportsTrueBypass;
}

void BaseHardwareModule::InitKnobs(int count, Pin pins[])
{
    // Set order of ADCs based on CHANNEL NUMBER
    AdcChannelConfig cfg[count];

    // Init with Single Pins
    for (int i = 0; i < count; i++)
    {
        cfg[i].InitSingle(pins[i]);
    }

    seed.adc.Init(cfg, count);

    // Setup the Knobs
    for(int i = 0; i < count; i++)
    {
        AnalogControl myKnob;
        myKnob.Init(seed.adc.GetPtr(i), AudioCallbackRate());
        knobs.push_back(myKnob);
    }
}

void BaseHardwareModule::InitSwitches(int count, Pin pins[])
{
    for(int i = 0; i < count; i++)
    {
        Switch mySwitch;
        mySwitch.Init(pins[i]);
        switches.push_back(mySwitch);
    }
}

void BaseHardwareModule::InitEncoders(int count, Pin pins[][3])
{
    for (int i = 0; i < count; i++)
    {
        Encoder myEncoder;
        myEncoder.Init(pins[i][0],
                       pins[i][1],
                       pins[i][2]);
        encoders.push_back(myEncoder);
    }
}

void BaseHardwareModule::InitLeds(int count, Pin pins[])
{
    for(int i = 0; i < count; i++)
    {
        Led newLed;
        newLed.Init(pins[i], false, AudioCallbackRate());
        leds.push_back(newLed);
    }
}

void BaseHardwareModule::InitMidi(Pin rxPin, Pin txPin)
{   
    MidiUartHandler::Config midi_config;
    midi_config.transport_config.rx = rxPin;
    midi_config.transport_config.tx = txPin;
    midi.Init(midi_config);

    m_supportsMidi = true;
}

void BaseHardwareModule::InitDisplay(Pin dcPin, Pin resetPin)
{
    // Configure the Display
    MyOledDisplay::Config disp_cfg;
    disp_cfg.driver_config.transport_config.pin_config.dc    = dcPin;
    disp_cfg.driver_config.transport_config.pin_config.reset = resetPin;
    display.Init(disp_cfg);

    m_supportsDisplay = true;   
}

void BaseHardwareModule::InitTrueBypass(Pin relayPin, Pin mutePin)
{
    // Init the HW Audio Bypass
    audioBypassTrigger.Init(relayPin, GPIO::Mode::OUTPUT);
    SetAudioBypass(true);

    // Init the HW Audio Mute
    audioMuteTrigger.Init(mutePin, GPIO::Mode::OUTPUT);
    SetAudioMute(false);
    
    m_supportsTrueBypass = true;
}
