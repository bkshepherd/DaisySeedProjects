#include <string.h>
#include "guitar_pedal_125b.h"
#include "modulated_tremolo_module.h"
#include "overdrive_module.h"
#include "autopan_module.h"
#include "chorus_module.h"
#include "daisysp.h"

// Peristant Storage Settings
#define SETTINGS_FILE_FORMAT_VERSION 1
#define SETTINGS_MAX_EFFECT_COUNT 8
#define SETTINGS_MAX_EFFECT_PARAM_COUNT 16

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

bool muteOn = false;
float muteOffTransitionTimeInSeconds = 0.02f;
int muteOffTransitionTimeInSamples;
int samplesTilMuteOff;

bool bypassOn = false;
float bypassToggleTransitionTimeInSeconds = 0.01f;
int bypassToggleTransitionTimeInSamples;
int samplesTilBypassToggle;

uint32_t lastTimeStampUS;
float secondsSinceStartup = 0.0f;

// Save System Variables
struct Settings
{
    int fileFormatVersion;
    int globalActiveEffectID;
    bool globalMidiEnabled;
    bool globalMidiThrough;
    int globalMidiChannel;
    bool globalRelayBypassEnabled;
    bool globalSplitMonoInputToStereo;
    uint8_t globalEffectsSettings[SETTINGS_MAX_EFFECT_COUNT * SETTINGS_MAX_EFFECT_PARAM_COUNT];     // Set aside a block of memory for individual effect params

    bool operator==(const Settings &rhs)
    {
        if (fileFormatVersion != rhs.fileFormatVersion
            || globalActiveEffectID != rhs.globalActiveEffectID
            || globalMidiEnabled != rhs.globalMidiEnabled
            || globalMidiThrough != rhs.globalMidiThrough
            || globalMidiChannel != rhs.globalMidiChannel
            || globalRelayBypassEnabled != rhs.globalRelayBypassEnabled
            || globalSplitMonoInputToStereo != rhs.globalSplitMonoInputToStereo)
        {
            return false;
        }

        for (int i = 0; i < SETTINGS_MAX_EFFECT_COUNT * SETTINGS_MAX_EFFECT_PARAM_COUNT; i++)
        {
            if (globalEffectsSettings[i] != rhs.globalEffectsSettings[i])
            {
                return false;
            }
        }

        return true;
    }
    
    bool operator!=(const Settings &rhs)
    {
        return !operator==(rhs);
    }
};

PersistentStorage<Settings> storage(hardware.seed.qspi);
uint32_t last_save_time;        // Time we last set it
bool needToSaveSettingsForActiveEffect = false;
bool displayingSaveSettingsNotification = false;
float secondsSinceLastActiveEffectSettingsSave = 0;

// Menu System Variables
daisy::UI ui;
FullScreenItemMenu mainMenu;
FullScreenItemMenu activeEffectSettingsMenu;
FullScreenItemMenu globalSettingsMenu;
UiEventQueue       eventQueue;

// Pot Monitoring Variables
bool knobValuesInitialized = false;
float knobValueChangeTolerance = 1.0f / 256.0f;
float knobValueIdleTimeInSeconds = 1.0f;
int knobValueIdleTimeInSamples;
bool knobValueCacheChanged[hardware.KNOB_LAST];
float knobValueCache[hardware.KNOB_LAST];
int knobValueSamplesTilIdle[hardware.KNOB_LAST];

const int                kNumMainMenuItems =  2;
AbstractMenu::ItemConfig mainMenuItems[kNumMainMenuItems];
const int                kNumGlobalSettingsMenuItems = 7;
AbstractMenu::ItemConfig globalSettingsMenuItems[kNumGlobalSettingsMenuItems];
int                      numActiveEffectSettingsItems = 0;
AbstractMenu::ItemConfig *activeEffectSettingsMenuItems = NULL;

// Effect Settings Value Items
const char** availableEffectNames = NULL;
MappedStringListValue *availableEffectListMappedValues = NULL;
MappedIntValue **activeEffectSettingValues = NULL;
MappedIntValue midiChannelSettingValue(1,16,1,1,5);

// Effect Related Variables
int availableEffectsCount = 0;
BaseEffectModule **availableEffects = NULL;
BaseEffectModule *activeEffect = NULL;

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

void InitEffectUiPages()
{
    // ====================================================================
    // The Main Menu
    // ====================================================================

    mainMenuItems[0].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    mainMenuItems[0].text = activeEffect->GetName();
    mainMenuItems[0].asOpenUiPageItem.pageToOpen = &activeEffectSettingsMenu;

    mainMenuItems[1].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    mainMenuItems[1].text = "Settings";
    mainMenuItems[1].asOpenUiPageItem.pageToOpen = &globalSettingsMenu;

    mainMenu.Init(mainMenuItems, kNumMainMenuItems);

    // ====================================================================
    // The "Settings" menu for the Active Effect (depends on what effect is active)
    // ====================================================================

    // Clean up any dynamically allocated memory
    if (activeEffectSettingValues != NULL)
    {
        for(int i = 0; i < numActiveEffectSettingsItems; ++i)
            delete activeEffectSettingValues[i];

        delete [] activeEffectSettingValues;
        activeEffectSettingValues = NULL;
        numActiveEffectSettingsItems = 0;
    }

    if (activeEffectSettingsMenuItems != NULL)
    {
        delete [] activeEffectSettingsMenuItems;
    }

    numActiveEffectSettingsItems = activeEffect->GetParameterCount();
    activeEffectSettingValues = new MappedIntValue*[numActiveEffectSettingsItems];
    activeEffectSettingsMenuItems = new AbstractMenu::ItemConfig[numActiveEffectSettingsItems + 1];
    
    for (int i = 0; i < numActiveEffectSettingsItems; i++)
    {
        activeEffectSettingsMenuItems[i].type = daisy::AbstractMenu::ItemType::valueItem;
        activeEffectSettingsMenuItems[i].text = activeEffect->GetParameterName(i);
        activeEffectSettingValues[i] = new MappedIntValue(0, 127, activeEffect->GetParameter(i), 1, 5);
        activeEffectSettingsMenuItems[i].asMappedValueItem.valueToModify = activeEffectSettingValues[i];
    }

    activeEffectSettingsMenuItems[numActiveEffectSettingsItems].type = daisy::AbstractMenu::ItemType::closeMenuItem;
    activeEffectSettingsMenuItems[numActiveEffectSettingsItems].text = "Back";

    activeEffectSettingsMenu.Init(activeEffectSettingsMenuItems, numActiveEffectSettingsItems + 1);
}

void InitGlobalSettingsUIPages()
{
    // Get a handle to the persitance storage settings
    Settings &settings = storage.GetSettings();

    // ====================================================================
    // The "Global Settings" menu
    // ====================================================================
    if (availableEffectListMappedValues != NULL)
    {
        delete availableEffectListMappedValues;
    }

    if (availableEffectNames != NULL)
    {
        delete [] availableEffectNames;
    }

    availableEffectNames = new const char*[availableEffectsCount];
    int activeEffectIndex = -1;

    for (int i = 0; i < availableEffectsCount; i++)
    {
        availableEffectNames[i] = availableEffects[i]->GetName();

        if (availableEffects[i] == activeEffect)
        {
            activeEffectIndex = i;
        }
    }
    
    availableEffectListMappedValues = new MappedStringListValue(availableEffectNames, availableEffectsCount, activeEffectIndex);

    globalSettingsMenuItems[0].type = daisy::AbstractMenu::ItemType::valueItem;
    globalSettingsMenuItems[0].text = "Effect";
    globalSettingsMenuItems[0].asMappedValueItem.valueToModify = availableEffectListMappedValues;

    globalSettingsMenuItems[1].type = daisy::AbstractMenu::ItemType::checkboxItem;
    globalSettingsMenuItems[1].text = "True Bypass";
    globalSettingsMenuItems[1].asCheckboxItem.valueToModify = &settings.globalRelayBypassEnabled;

    globalSettingsMenuItems[2].type = daisy::AbstractMenu::ItemType::checkboxItem;
    globalSettingsMenuItems[2].text = "Split Mono";
    globalSettingsMenuItems[2].asCheckboxItem.valueToModify = &settings.globalSplitMonoInputToStereo;

    globalSettingsMenuItems[3].type = daisy::AbstractMenu::ItemType::checkboxItem;
    globalSettingsMenuItems[3].text = "Midi On";
    globalSettingsMenuItems[3].asCheckboxItem.valueToModify = &settings.globalMidiEnabled;

    globalSettingsMenuItems[4].type = daisy::AbstractMenu::ItemType::checkboxItem;
    globalSettingsMenuItems[4].text = "Midi Thru";
    globalSettingsMenuItems[4].asCheckboxItem.valueToModify = &settings.globalMidiThrough;

    globalSettingsMenuItems[5].type = daisy::AbstractMenu::ItemType::valueItem;
    globalSettingsMenuItems[5].text = "Midi Ch";
    midiChannelSettingValue.Set(settings.globalMidiChannel);
    globalSettingsMenuItems[5].asMappedValueItem.valueToModify = &midiChannelSettingValue;

    globalSettingsMenuItems[6].type = daisy::AbstractMenu::ItemType::closeMenuItem;
    globalSettingsMenuItems[6].text = "Back";

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

// Helpful Function for getting a parameter value for an effect from the Persistant Storage
uint8_t GetSettingsParameterValueForEffect(int effectID, int paramID)
{
    // Make sure the effect and param id are within valid ranges for the settings.
    if (effectID < 0 || effectID > SETTINGS_MAX_EFFECT_COUNT - 1 || paramID < 0 || paramID > SETTINGS_MAX_EFFECT_PARAM_COUNT - 1)
    {
        return 0;
    }

    // Get a handle to the persitance storage settings
    Settings &settings = storage.GetSettings();
    return settings.globalEffectsSettings[(effectID * SETTINGS_MAX_EFFECT_PARAM_COUNT) + paramID];
}

// Helpful Function for setting a parameter value for an effect from the Persistant Storage
void SetSettingsParameterValueForEffect(int effectID, int paramID, uint8_t paramValue)
{
    // Make sure the effect and param id are within valid ranges for the settings.
    if (effectID < 0 || effectID > SETTINGS_MAX_EFFECT_COUNT - 1 || paramID < 0 || paramID > SETTINGS_MAX_EFFECT_PARAM_COUNT - 1)
    {
        return;
    }

    // Get a handle to the persitance storage settings
    Settings &settings = storage.GetSettings();
    settings.globalEffectsSettings[(effectID * SETTINGS_MAX_EFFECT_PARAM_COUNT) + paramID] = paramValue;
}

void InitPersistantStorage()
{
    Settings defaultSettings;
    defaultSettings.fileFormatVersion = SETTINGS_FILE_FORMAT_VERSION;
    defaultSettings.globalActiveEffectID = 0;
    defaultSettings.globalMidiEnabled = true;
    defaultSettings.globalMidiChannel = 1;
    defaultSettings.globalMidiThrough = true;
    defaultSettings.globalRelayBypassEnabled = false;
    defaultSettings.globalSplitMonoInputToStereo = true;

    // All Effect Params in the settings sound be zero'd
    for (int i = 0; i < SETTINGS_MAX_EFFECT_COUNT * SETTINGS_MAX_EFFECT_PARAM_COUNT; i++)
    {
        defaultSettings.globalEffectsSettings[i] = 0;
    }

    // Override any defaults with effect specific default settings
    for (int effectID = 0; effectID < availableEffectsCount; effectID++)
    {
        int paramCount = availableEffects[effectID]->GetParameterCount();

        for (int paramID = 0;  paramID < paramCount; paramID++)
        {
            defaultSettings.globalEffectsSettings[(effectID * SETTINGS_MAX_EFFECT_PARAM_COUNT) + paramID] = availableEffects[effectID]->GetParameter(paramID);
        }
    } 
    
    storage.Init(defaultSettings);
}

void LoadEffectSettingsFromPersistantStorage()
{
    // Load Effect Parameters based on values from Persistant Storage
    for (int effectID = 0; effectID < availableEffectsCount; effectID++)
    {
        int paramCount = availableEffects[effectID]->GetParameterCount();

        for (int paramID = 0; paramID < paramCount; paramID++)
        {
            availableEffects[effectID]->SetParameter(paramID, GetSettingsParameterValueForEffect(effectID, paramID));
        }
    }
}

void SaveEffectSettingsToPersitantStorageForEffectID(int effectID)
{
    // Save Effect Parameters to Persistant Storage based on values from the specified active effect
    if (effectID >= 0 && effectID < availableEffectsCount)
    {
        int paramCount = availableEffects[effectID]->GetParameterCount();

        for (int paramID = 0; paramID < paramCount; paramID++)
        {
            SetSettingsParameterValueForEffect(effectID, paramID, availableEffects[effectID]->GetParameter(paramID));
        }
    }
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

    // Get a handle to the persitance storage settings
    Settings &settings = storage.GetSettings();
    
    // Process the Pots
    float knobValueRaw;

    for (int i = 0; i < hardware.KNOB_LAST; i++)
    {
        knobValueRaw = hardware.GetKnobValue((GuitarPedal125B::KnobIndex)i);

        if (!knobValuesInitialized)
        {
            // Initialize the knobs for the first time to whatever the current knob placements are
            knobValueCacheChanged[i] = false;
            knobValueSamplesTilIdle[i] = 0;
            knobValueCache[i] = knobValueRaw;
        }
        else
        {
            // If the knobs are initialized handle monitor them for changes.
            if (knobValueSamplesTilIdle[i] > 0)
            {
                knobValueSamplesTilIdle[i] -= size;

                if (knobValueSamplesTilIdle[i] <= 0)
                {
                    knobValueSamplesTilIdle[i] = 0;
                    knobValueCacheChanged[i] = false;
                }
            }

            bool knobValueChangedToleranceMet = false;

            if (knobValueRaw > (knobValueCache[i] + knobValueChangeTolerance) || knobValueRaw < (knobValueCache[i] - knobValueChangeTolerance))
            {
                knobValueChangedToleranceMet = true;
                knobValueCacheChanged[i] = true;
                knobValueSamplesTilIdle[i] = knobValueIdleTimeInSamples;
            }

            if (knobValueChangedToleranceMet || knobValueCacheChanged[i])
            {
                knobValueCache[i] = knobValueRaw;
            }
        
        }
    }

    //If both footswitches are down, save the parameters for this effect to persistant storage
    if (hardware.switches[1].TimeHeldMs() > 2000 && !displayingSaveSettingsNotification)
    {
        needToSaveSettingsForActiveEffect = true;
    }

    //If the First Footswitch button is pressed, toggle the effect enabled
    bool oldEffectOn = effectOn;
    effectOn ^= hardware.switches[0].RisingEdge();

    // Handle updating the Hardware Bypass & Muting signals
    if (settings.globalRelayBypassEnabled)
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
        if (settings.globalRelayBypassEnabled)
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
        if (settings.globalSplitMonoInputToStereo && !settings.globalRelayBypassEnabled)
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
        
        // Only calculate the active effect when it's needed
        if(activeEffect != NULL && (effectOn || isCrossFading))
        {
            // Apply the Active Effect
            activeEffect->ProcessStereo(inputLeft, inputRight);
            effectOutputLeft = activeEffect->GetAudioLeft();
            effectOutputRight = activeEffect->GetAudioRight();

            // Update state of the LEDs
            led1Brightness = 1.0f;
            led2Brightness = activeEffect->GetOutputLEDBrightness();
        }

        // Setup the crossfade target to be the effect
        crossFadeTargetLeft = effectOutputLeft;
        crossFadeTargetRight = effectOutputRight;

        out[0][i] = crossFaderLeft.Process(crossFadeSourceLeft, crossFadeTargetLeft);
        out[1][i] = crossFaderRight.Process(crossFadeSourceRight, crossFadeTargetRight);
    }

    // Override LEDs if we are saving the current settings
    if (displayingSaveSettingsNotification)
    {
        led1Brightness = 1.0f;
        led2Brightness = 1.0f;
    }

    // Handle LEDs
    hardware.SetLed((GuitarPedal125B::LedIndex)0, led1Brightness);
    hardware.SetLed((GuitarPedal125B::LedIndex)1, led2Brightness);
    hardware.UpdateLeds();
}

// Typical Switch case for Message Type.
void HandleMidiMessage(MidiEvent m)
{
    /* Debug info
    uint8_t midiData[3];
    midiData[0] = 0b10000000 | ((uint8_t) m.type << 4) | ((uint8_t) m.channel);
    midiData[1] = m.data[0];
    midiData[2] = m.data[1];
    char strbuff[256];
    hardware.display.Fill(false);
    hardware.display.SetCursor(0, 0);
    hardware.display.WriteString("Midi:", Font_7x10, true);
    hardware.display.SetCursor(0, 15);
    sprintf(strbuff, "data[0]: %d", midiData[0]);
    hardware.display.WriteString(strbuff, Font_7x10, true);
    hardware.display.SetCursor(0, 30);
    sprintf(strbuff, "data[1]: %d", midiData[1]);
    hardware.display.WriteString(strbuff, Font_7x10, true);
    hardware.display.SetCursor(0, 45);
    sprintf(strbuff, "data[2]: %d", midiData[2]);
    hardware.display.WriteString(strbuff, Font_7x10, true);
    hardware.display.Update();
    */

    // Get a handle to the persitance storage settings
    Settings &settings = storage.GetSettings();

    int channel = 0;

    // Make sure the settings midi channel is within the proper range 
    // and convert the channel to be zero indexed instead of 1 like the setting.
    if (settings.globalMidiChannel >= 1 && settings.globalMidiChannel <= 16)
    {
        channel = settings.globalMidiChannel - 1;
    }

    // Pass the midi message through to midi out if so desired (only handles non system event types)
    if (settings.globalMidiThrough && m.type < SystemCommon)
    {
        // Re-pack the Midi Message
        uint8_t midiData[3];
        
        midiData[0] = 0b10000000 | ((uint8_t) m.type << 4) | ((uint8_t) m.channel);
        midiData[1] = m.data[0];
        midiData[2] = m.data[1];

        int bytesToSend = 3;

        if (m.type == ChannelPressure || m.type == ProgramChange)
        {
            bytesToSend = 2;
        }

        hardware.midi.SendMessage(midiData, sizeof(uint8_t) * bytesToSend);
    }

    // Only listen to messages for the devices set channel.
    if (m.channel != channel)
    {
        return;
    }

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
            }
        }
        break;
        case ControlChange:
        {   
            if (activeEffect != NULL)
            {
                ControlChangeEvent p = m.AsControlChange();
                int effectParamID =  activeEffect->GetMappedParameterIDForMidiCC(p.control_number);

                if (effectParamID != -1)
                {
                    activeEffect->SetParameter(effectParamID, p.value);
                    activeEffectSettingValues[effectParamID]->Set(activeEffect->GetParameter(effectParamID));
                }
            }
            break;
        }
        case ProgramChange:
        {
            ProgramChangeEvent p = m.AsProgramChange();

            if (p.program >= 0 && p.program < availableEffectsCount)
            {
                availableEffectListMappedValues->SetIndex(p.program);
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

    // Set initial time stamp
    lastTimeStampUS = System::GetUs();

    // Set the number of samples to use for the crossfade based on the hardware sample rate
    muteOffTransitionTimeInSamples = GetNumberOfSamplesForTime(muteOffTransitionTimeInSeconds);
    bypassToggleTransitionTimeInSamples = GetNumberOfSamplesForTime(bypassToggleTransitionTimeInSeconds);
    crossFaderTransitionTimeInSamples = GetNumberOfSamplesForTime(crossFaderTransitionTimeInSeconds);

    // Init the Effects Modules
    availableEffectsCount = 4;
    availableEffects = new BaseEffectModule*[availableEffectsCount];

    availableEffects[0] = new ModulatedTremoloModule();
    availableEffects[1] = new OverdriveModule();
    availableEffects[2] = new AutoPanModule();
    availableEffects[3] = new ChorusModule();

    for (int i = 0; i < availableEffectsCount; i++)
    {
        availableEffects[i]->Init(sample_rate);
    }

    // Initalize Persistance Storage
    InitPersistantStorage();

    Settings &settings = storage.GetSettings();

    // If the stored data is not the current version do a factory reset
    if (settings.fileFormatVersion != SETTINGS_FILE_FORMAT_VERSION)
    {
        storage.RestoreDefaults();
    }
    
    // Load all the effect specific settings
    LoadEffectSettingsFromPersistantStorage();
    
    // Set the active effect
    activeEffect = availableEffects[settings.globalActiveEffectID];

    // Init the Menu UI System
    InitUi();
    InitEffectUiPages();
    InitGlobalSettingsUIPages();
    ui.OpenPage(mainMenu);
    UI::SpecialControlIds ids;
    
    // Init the Knob Monitoring System
    knobValueIdleTimeInSamples = GetNumberOfSamplesForTime(knobValueIdleTimeInSeconds);

    // Setup the cross fader
    crossFaderLeft.Init();
    crossFaderRight.Init();
    crossFaderLeft.SetPos(0.0f);
    crossFaderRight.SetPos(0.0f);

    // start callback
    hardware.StartAdc();
    hardware.StartAudio(AudioCallback);
    hardware.midi.StartReceive();

    // Setup Relay Bypass State
    if (settings.globalRelayBypassEnabled)
    {
        bypassOn = true;
    }

    // Setup Debug Logging
    //hardware.seed.StartLog();

    while(1)
    {
        // Handle Clock Time
        uint32_t currentTimeStampUS = System::GetUs();
        uint32_t elapsedTimeStampUS = currentTimeStampUS - lastTimeStampUS;
        lastTimeStampUS = currentTimeStampUS;
        secondsSinceStartup = secondsSinceStartup + (elapsedTimeStampUS / 1000000.0f);

        // Handle Knob Changes
        if (!knobValuesInitialized && secondsSinceStartup > 1.0f)
        {
            // Let the initial readings of the knob values settle before trying to use them.
            knobValuesInitialized = true;
        }

        bool isKnobValueChanging = false;

        for (int i = 0; i < hardware.KNOB_LAST; i++)
        {
            if (knobValueCacheChanged[i])
            {
                int parameterID = activeEffect->GetMappedParameterIDForKnob(i);

                if (parameterID != -1)
                {
                    activeEffect->SetParameterAsMagnitude(parameterID, knobValueCache[i]);
                    activeEffectSettingValues[parameterID]->Set(activeEffect->GetParameter(parameterID));
                    isKnobValueChanging = true;

                    // Change the main menu to be the name of the value the Knob is changing
                    mainMenuItems[0].text = activeEffect->GetParameterName(parameterID);
                }
                
            }
        }
        
        // If no knobs are moving make sure the main menu is set to the Effect Name.
        if (!isKnobValueChanging)
        {
            mainMenuItems[0].text = activeEffect->GetName();
        }

        // Update all Active Effect Settings
        for (int i = 0; i < numActiveEffectSettingsItems; i++)
        {
            activeEffect->SetParameter(i, activeEffectSettingValues[i]->Get());
        }

        // Handle a Change in the Active Effect
        BaseEffectModule *selectedEffect = availableEffects[availableEffectListMappedValues->GetIndex()];

        if (activeEffect != selectedEffect)
        {
            // Update the active effect and reset the Effects Menu and Settings for that Effect
            activeEffect = selectedEffect;
            InitEffectUiPages();

            // Update the persistant storage setting
            settings.globalActiveEffectID = availableEffectListMappedValues->GetIndex();
        }

        // Handle Displaying the Saving notication
        if (displayingSaveSettingsNotification)
        {
            secondsSinceLastActiveEffectSettingsSave += (elapsedTimeStampUS / 1000000.0f);

            // Change the main menu text to say saved
            mainMenuItems[0].text = "Saved.";

            if (secondsSinceLastActiveEffectSettingsSave > 2.0f)
            {
                displayingSaveSettingsNotification = false;
            }
        }
        
        // Handle Display
        if (useDebugDisplay)
        {
            // Debug Display hijacks the display to simply output text
            char strbuff[128];
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
            sprintf(strbuff, "LED: %d", (int)(activeEffect->GetOutputLEDBrightness() * 100.0f));
            hardware.display.WriteString(strbuff, Font_7x10, true);
            hardware.display.Update();
        }
        else
        {
            // Default behavior is to use the menu system.
            ui.Process();
        }

        // Handle MIDI Events
        settings.globalMidiChannel = midiChannelSettingValue.Get();

        if (settings.globalMidiEnabled)
        {
            hardware.midi.Listen();

            while(hardware.midi.HasEvents())
            {
                HandleMidiMessage(hardware.midi.PopEvent());
            }
        }

        // Throttle persitant storage saves to once every 2 seconds:
        if (System::GetNow() - last_save_time >= 2000)
        {
            if (needToSaveSettingsForActiveEffect)
            {
                SaveEffectSettingsToPersitantStorageForEffectID(availableEffectListMappedValues->GetIndex());
                displayingSaveSettingsNotification = true;
                secondsSinceLastActiveEffectSettingsSave = 0.0f;
            }

            storage.Save();
            last_save_time = System::GetNow();
            needToSaveSettingsForActiveEffect = false;
        }
    }
}
