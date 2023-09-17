#include <string.h>
#include "guitar_pedal_125b.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;
using namespace bkshepherd;

// Hardware Interface
GuitarPedal125B hardware;

// Hardware Related Variables
bool useDebugDisplay = false;
bool  effectOn = false;
float led1Brightness = 0.0f;
float led2Brightness = 0.0f;
bool midiEnabled = true;
bool relayBypassEnabled = true;
bool splitMonoInputToStereo = false; // Only available when not using true bypass

bool muteOn = false;
float muteOffTransitionTimeInSeconds = 0.02f;
int muteOffTransitionTimeInSamples;
int samplesTilMuteOff;

bool bypassOn = false;
float bypassToggleTransitionTimeInSeconds = 0.01f;
int bypassToggleTransitionTimeInSamples;
int samplesTilBypassToggle;

// Menu System Variables
daisy::UI ui;
FullScreenItemMenu mainMenu;
FullScreenItemMenu tremoloMenu;
FullScreenItemMenu globalSettingsMenu;
UiEventQueue       eventQueue;

const int                kNumMainMenuItems =  2;
AbstractMenu::ItemConfig mainMenuItems[kNumMainMenuItems];
const int                kNumTremoloMenuItems = 4;
AbstractMenu::ItemConfig tremoloMenuItems[kNumTremoloMenuItems];
const int                kNumGlobalSettingsMenuItems = 4;
AbstractMenu::ItemConfig globalSettingsMenuItems[kNumGlobalSettingsMenuItems];

// Tremolo menu items
const char* tremTypeListValues[]
    = {"Simple", "Harmonic"};
MappedStringListValue tremTypeListMappedValues(tremTypeListValues, 2, 0);

const char* tremWaveformListValues[]
    = {"Sine", "Triangle", "Saw", "Ramp", "Square"};
MappedStringListValue tremWaveformListMappedValues(tremWaveformListValues, 5, 0);
MappedStringListValue tremOscWaveformListMappedValues(tremWaveformListValues, 5, 0);

// Effect Related Variables
Tremolo    treml, tremr;
Oscillator freq_osc;
int  waveform;
float osc_freq;
Parameter osc_freq_knob;

bool isCrossFading = false;
bool isCrossFadingForward = true;   // True goes Source->Target, False goes Target->Source
CrossFade crossFaderLeft, crossFaderRight;
float crossFaderTransitionTimeInSeconds = 0.1f;
int crossFaderTransitionTimeInSamples;
int samplesTilCrossFadingComplete;

/** This is the type of display we use on the patch. This is provided here for better readability. */
using OledDisplayType = decltype(GuitarPedal125B::display);

// These will be called from the UI system. @see InitUi() in UiSystemDemo.cpp
void FlushCanvas(const daisy::UiCanvasDescriptor& canvasDescriptor)
{
    if(canvasDescriptor.id_ == 0)
    {
        OledDisplayType& display
            = *((OledDisplayType*)(canvasDescriptor.handle_));
        display.Update();
    }
}
void ClearCanvas(const daisy::UiCanvasDescriptor& canvasDescriptor)
{
    if(canvasDescriptor.id_ == 0)
    {
        OledDisplayType& display
            = *((OledDisplayType*)(canvasDescriptor.handle_));
        display.Fill(false);
    }
}

void InitUi()
{
    UI::SpecialControlIds specialControlIds;
    specialControlIds.okBttnId
        = hardware.ENCODER_1; // Encoder button is our okay button
    specialControlIds.menuEncoderId
        = hardware.ENCODER_1; // Encoder is used as the main menu navigation encoder

    // This is the canvas for the OLED display.
    UiCanvasDescriptor oledDisplayDescriptor;
    oledDisplayDescriptor.id_     = 0; // the unique ID
    oledDisplayDescriptor.handle_ = &hardware.display;   // a pointer to the display
    oledDisplayDescriptor.updateRateMs_      = 50; // 50ms == 20Hz
    oledDisplayDescriptor.screenSaverTimeOut = 0;  // display always on
    oledDisplayDescriptor.clearFunction_     = &ClearCanvas;
    oledDisplayDescriptor.flushFunction_     = &FlushCanvas;

    ui.Init(eventQueue,
            specialControlIds,
            {oledDisplayDescriptor},
            0);
}

void InitUiPages()
{
    // ====================================================================
    // The main menu
    // ====================================================================

    mainMenuItems[0].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    mainMenuItems[0].text = "Tremolo";
    mainMenuItems[0].asOpenUiPageItem.pageToOpen = &tremoloMenu;

    mainMenuItems[1].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    mainMenuItems[1].text = "Settings";
    mainMenuItems[1].asOpenUiPageItem.pageToOpen = &globalSettingsMenu;

    mainMenu.Init(mainMenuItems, kNumMainMenuItems);

    // ====================================================================
    // The "Tremolo" menu
    // ====================================================================

    tremoloMenuItems[0].type = daisy::AbstractMenu::ItemType::valueItem;
    tremoloMenuItems[0].text = "Type";
    tremoloMenuItems[0].asMappedValueItem.valueToModify
        = &tremTypeListMappedValues;

    tremoloMenuItems[1].type = daisy::AbstractMenu::ItemType::valueItem;
    tremoloMenuItems[1].text = "Waveform";
    tremoloMenuItems[1].asMappedValueItem.valueToModify
        = &tremWaveformListMappedValues;

    tremoloMenuItems[2].type = daisy::AbstractMenu::ItemType::valueItem;
    tremoloMenuItems[2].text = "Osc Wave";
    tremoloMenuItems[2].asMappedValueItem.valueToModify
        = &tremOscWaveformListMappedValues;

    tremoloMenuItems[3].type = daisy::AbstractMenu::ItemType::closeMenuItem;
    tremoloMenuItems[3].text = "Back";

    tremoloMenu.Init(tremoloMenuItems, kNumTremoloMenuItems);

    // ====================================================================
    // The "Global Settings" menu
    // ====================================================================
    globalSettingsMenuItems[0].type = daisy::AbstractMenu::ItemType::checkboxItem;
    globalSettingsMenuItems[0].text = "True Bypass";
    globalSettingsMenuItems[0].asCheckboxItem.valueToModify = &relayBypassEnabled;

    globalSettingsMenuItems[1].type = daisy::AbstractMenu::ItemType::checkboxItem;
    globalSettingsMenuItems[1].text = "Split Mono";
    globalSettingsMenuItems[1].asCheckboxItem.valueToModify = &splitMonoInputToStereo;

    globalSettingsMenuItems[2].type = daisy::AbstractMenu::ItemType::checkboxItem;
    globalSettingsMenuItems[2].text = "Midi";
    globalSettingsMenuItems[2].asCheckboxItem.valueToModify = &midiEnabled;

    globalSettingsMenuItems[3].type = daisy::AbstractMenu::ItemType::closeMenuItem;
    globalSettingsMenuItems[3].text = "Back";

    globalSettingsMenu.Init(globalSettingsMenuItems, kNumGlobalSettingsMenuItems);
}

void GenerateUiEvents()
{
    if(hardware.encoders[0].RisingEdge())
        eventQueue.AddButtonPressed(hardware.ENCODER_1, 1);

    if(hardware.encoders[0].FallingEdge())
        eventQueue.AddButtonReleased(hardware.ENCODER_1);

    const auto increments = hardware.encoders[0].Increment();
    if(increments != 0)
        eventQueue.AddEncoderTurned(hardware.ENCODER_1, increments, 12);
}

// Calculate the number of samples for a specified amount of time in seconds.
int GetNumberOfSamplesForTime(float time)
{
    return (int)(hardware.AudioSampleRate() * time);
}

static void AudioCallback(AudioHandle::InputBuffer  in,
                     AudioHandle::OutputBuffer out,
                     size_t                    size)
{
    // Process Audio
    float inputLeft;
    float inputRight;

    // Handle Inputs
    hardware.ProcessAnalogControls();
    hardware.ProcessDigitalControls();

    GenerateUiEvents();

    // Handle knobs Tremolo
    float tremFreqMin = 1.0f;
    float tremFreqMax = hardware.knobs[0].Process() * 20.f; //0 - 20 Hz
    treml.SetDepth(hardware.knobs[1].Process());
    tremr.SetDepth(hardware.knobs[1].Value());
    float knob2Value = osc_freq_knob.Process();
    float freq_osc_min = 0.01f;
    freq_osc.SetFreq(freq_osc_min + (knob2Value * 3.0f)); //0 - 20 Hz
    float mod = freq_osc.Process();

    if (knob2Value < 0.01) {
        mod = 1.0f;
    }

    treml.SetFreq(tremFreqMin + tremFreqMax * mod); 
    tremr.SetFreq(tremFreqMin + tremFreqMax * mod);

    //If the First Footswitch button is pressed, toggle the effect enabled
    bool oldEffectOn = effectOn;
    effectOn ^= hardware.switches[0].RisingEdge();

    // Handle updating the Hardware Bypass & Muting signals
    if (relayBypassEnabled)
    {
        hardware.SetAudioBypass(bypassOn);
        hardware.SetAudioMute(muteOn);
    }
    else 
    {
        hardware.SetAudioBypass(false);
        hardware.SetAudioMute(false);
    }

    // Handle Effect State being Toggled.
    if (effectOn != oldEffectOn)
    {
        // Setup the crossfade
        isCrossFading = true;
        samplesTilCrossFadingComplete = crossFaderTransitionTimeInSamples;
        isCrossFadingForward = effectOn;

        // Start the timing sequence for the Hardware Mute and Relay Bypass.
        if (relayBypassEnabled)
        {
            // Immediately Mute the Output using the Hardware Mute.
            muteOn = true;

            // Set the timing for when the bypass relay should trigger and when to unmute.
            samplesTilMuteOff = muteOffTransitionTimeInSamples;
            samplesTilBypassToggle = bypassToggleTransitionTimeInSamples;
        }
    }

    for(size_t i = 0; i < size; i++)
    {
        if (isCrossFading)
        {
            float crossFadeFactor = (float)samplesTilCrossFadingComplete / (float)crossFaderTransitionTimeInSamples;

            if (isCrossFadingForward){
                crossFadeFactor = 1.0f - crossFadeFactor;
            }

            crossFaderLeft.SetPos(crossFadeFactor);
            crossFaderRight.SetPos(crossFadeFactor);

            samplesTilCrossFadingComplete -= 1;

            if (samplesTilCrossFadingComplete < 0)
            {
                isCrossFading = false;
            }
        }

        // Handle Timing for the Hardware Mute and Relay Bypass
        if (muteOn) {
            // Decrement the Sample Counts for the timing of the mute and bypass
            samplesTilMuteOff -= 1;
            samplesTilBypassToggle -= 1;

            // If mute time is up, turn it off.
            if (samplesTilMuteOff < 0) {
                muteOn = false;
            }

            // Toggle the bypass when it's time (needs to be timed to happen while things are muted, or you get an audio pop)
            if (samplesTilBypassToggle < 0) {
                bypassOn = !effectOn;
            }
        }

        // Handle Mono vs Stereo
        inputLeft = in[0][i];
        inputRight = in[1][i];

        // Split the Mono Input to Stereo (Only allowed if relay bypass non enabled)
        if (splitMonoInputToStereo && !relayBypassEnabled)
        {
            inputRight = inputLeft;
        }

        // Setup Master Crossfader. By default source & target is always the input signal
        float crossFadeSourceLeft = inputLeft;
        float crossFadeSourceRight = inputRight;
        float crossFadeTargetLeft = inputLeft;
        float crossFadeTargetRight = inputRight;
        float effectOutputLeft = inputLeft;
        float effectOutputRight = inputRight;

        // By default the leds are off
        led1Brightness = 0.0f;
        led2Brightness = 0.0f;
        
        // Only calculate the effect when it's needed
        if(effectOn || isCrossFading)
        {
            // Apply the Tremolo Effect and modulate the LED at the frequency of the Tremolo
            led1Brightness = 1.0f;
            led2Brightness = treml.Process(1.0f);
            effectOutputLeft = inputLeft * led2Brightness;
            effectOutputRight = tremr.Process(inputRight);
        }

        // Setup the crossfade target to be the effect
        crossFadeTargetLeft = effectOutputLeft;
        crossFadeTargetRight = effectOutputRight;

        out[0][i] = crossFaderLeft.Process(crossFadeSourceLeft, crossFadeTargetLeft);
        out[1][i] = crossFaderRight.Process(crossFadeSourceRight, crossFadeTargetRight);
    }

    // Handle LEDs
    hardware.SetLed((GuitarPedal125B::LedIndex)0, led1Brightness);
    hardware.SetLed((GuitarPedal125B::LedIndex)1, led2Brightness);
    hardware.UpdateLeds();
}

// Typical Switch case for Message Type.
void HandleMidiMessage(MidiEvent m)
{
    switch(m.type)
    {
        case NoteOn:
        {
            NoteOnEvent p = m.AsNoteOn();
            char        buff[512];
            sprintf(buff,
                    "Note Received:\t%d\t%d\t%d\r\n",
                    m.channel,
                    m.data[0],
                    m.data[1]);
            //hareware.seed.usb_handle.TransmitInternal((uint8_t *)buff, strlen(buff));
            // This is to avoid Max/MSP Note outs for now..
            if(m.data[1] != 0)
            {
                p = m.AsNoteOn();
                //led2Brightness = ((float)p.velocity / 127.0f);
                //osc.SetFreq(mtof(p.note));
                //osc.SetAmp((p.velocity / 127.0f));
            }
        }
        break;
        case ControlChange:
        {
            ControlChangeEvent p = m.AsControlChange();
            switch(p.control_number)
            {
                case 1:
                    // CC 1 for cutoff.
                    //filt.SetFreq(mtof((float)p.value));
                    break;
                case 2:
                    // CC 2 for res.
                    //filt.SetRes(((float)p.value / 127.0f));
                    break;
                default: 
                    //led2Brightness = ((float)p.value / 127.0f);
                break;
            }
            break;
        }
        default: break;
    }
}

int main(void)
{
    hardware.Init();
    hardware.SetAudioBlockSize(4);

    float sample_rate = hardware.AudioSampleRate();

    // Set the number of samples to use for the crossfade based on the hardware sample rate
    muteOffTransitionTimeInSamples = GetNumberOfSamplesForTime(muteOffTransitionTimeInSeconds);
    bypassToggleTransitionTimeInSamples = GetNumberOfSamplesForTime(bypassToggleTransitionTimeInSeconds);
    crossFaderTransitionTimeInSamples = GetNumberOfSamplesForTime(crossFaderTransitionTimeInSeconds);

    InitUi();
    InitUiPages();
    ui.OpenPage(mainMenu);
    UI::SpecialControlIds ids;

    treml.Init(sample_rate);
    tremr.Init(sample_rate);
    osc_freq = 0.0f;
    freq_osc.Init(sample_rate);
    freq_osc.SetAmp(1.0f);
    freq_osc.SetFreq(osc_freq);
    osc_freq_knob.Init(hardware.knobs[2], 0.0, 1.0f, Parameter::Curve::EXPONENTIAL);
    
    // Setup the cross fader
    crossFaderLeft.Init();
    crossFaderRight.Init();
    crossFaderLeft.SetPos(0.0f);
    crossFaderRight.SetPos(0.0f);

    // start callback
    hardware.StartAdc();
    hardware.StartAudio(AudioCallback);
    hardware.midi.StartReceive();

    // Send Test Midi Message
    uint8_t midiData[3];
    midiData[0] = 0b10110000;
    midiData[1] = 0b00001010;
    midiData[2] = 0b01111111;
    hardware.midi.SendMessage(midiData, sizeof(uint8_t) * 3);

    // Setup Relay Bypass State
    if (relayBypassEnabled)
    {
        bypassOn = true;
    }

    // Setup Debug Logging
    //hardware.seed.StartLog();
    char strbuff[128];

    while(1)
    {
        // Handle Display
        if (useDebugDisplay)
        {
            // Debug Display hijacks the display to simply output text
            hardware.display.Fill(false);
            hardware.display.SetCursor(0, 0);
            hardware.display.WriteString("Debug:", Font_7x10, true);
            hardware.display.SetCursor(0, 15);
            sprintf(strbuff, "MuteOn: %d", muteOn);
            hardware.display.WriteString(strbuff, Font_7x10, true);
            hardware.display.SetCursor(0, 30);
            sprintf(strbuff, "BypassOn: %d", bypassOn);
            hardware.display.WriteString(strbuff, Font_7x10, true);
            hardware.display.SetCursor(0, 45);
            sprintf(strbuff, "CrossFader: %d", (int)(crossFaderLeft.GetPos(0) * 100.0f));
            hardware.display.WriteString(strbuff, Font_7x10, true);
            hardware.display.Update();
        }
        else
        {
            // Default behavior is to use the menu system.
            ui.Process();
        }

        // Handle Updaing Settings from Menus
        treml.SetWaveform(tremWaveformListMappedValues.GetIndex());
        tremr.SetWaveform(tremWaveformListMappedValues.GetIndex());
        freq_osc.SetWaveform(tremOscWaveformListMappedValues.GetIndex());

        // Handle MIDI Events
        if (midiEnabled)
        {
            hardware.midi.Listen();

            while(hardware.midi.HasEvents())
            {
                HandleMidiMessage(hardware.midi.PopEvent());
            }
        }
    }
}
