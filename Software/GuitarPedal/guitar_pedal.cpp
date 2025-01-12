#include "Effect-Modules/modulated_tremolo_module.h"
#include "daisysp.h"
#include "guitar_pedal_storage.h"
#include <string.h>

#include "Effect-Modules/autopan_module.h"
#include "Effect-Modules/chopper_module.h"
#include "Effect-Modules/chorus_module.h"
#include "Effect-Modules/compressor_module.h"
#include "Effect-Modules/geq_module.h"
#include "Effect-Modules/looper_module.h"
#include "Effect-Modules/metro_module.h"
#include "Effect-Modules/multi_delay_module.h"
#include "Effect-Modules/noise_gate_module.h"
#include "Effect-Modules/overdrive_module.h"
#include "Effect-Modules/peq_module.h"
#include "Effect-Modules/pitch_shifter_module.h"
#include "Effect-Modules/reverb_module.h"
#include "Effect-Modules/tuner_module.h"

#include "UI/guitar_pedal_ui.h"
#include "Util/audio_utilities.h"

using namespace daisy;
using namespace daisysp;
using namespace bkshepherd;

// Uncomment the version you are trying to use, by default (and if nothing is
// uncommented), the 125B with 2 footswitch variant will be used

// #define VARIANT_125B
// #define VARIANT_1590B
// #define VARIANT_1590B_SMD
// #define VARIANT_TERRARIUM

#if defined(VARIANT_TERRARIUM)
#include "Hardware-Modules/guitar_pedal_terrarium.h"
constexpr bool has_alternate_footswitch = true;
GuitarPedalTerrarium hardware;
#elif defined(VARIANT_1590B)
#include "Hardware-Modules/guitar_pedal_1590b.h"
constexpr bool has_alternate_footswitch = false;
GuitarPedal1590B hardware;
#elif defined(VARIANT_1590B_SMD)
#include "Hardware-Modules/guitar_pedal_1590b-SMD.h"
constexpr bool has_alternate_footswitch = false;
GuitarPedal1590BSMD hardware;
#else
#include "Hardware-Modules/guitar_pedal_125b.h"
constexpr bool has_alternate_footswitch = true;
GuitarPedal125B hardware;
#endif

// Persistant Storage
PersistentStorage<Settings> storage(hardware.seed.qspi);

// Effect Related Variables
int availableEffectsCount = 0;
BaseEffectModule **availableEffects = nullptr;
int activeEffectID = 0;
int prevActiveEffectID = 0;
int tunerModuleIndex = -1;
BaseEffectModule *activeEffect = nullptr;

// UI Related Variables
GuitarPedalUI guitarPedalUI;

// Hardware Related Variables
bool useDebugDisplay = false;
bool effectOn = false;

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

bool needToSaveSettingsForActiveEffect = false;
uint32_t last_save_time; // Time we last set it

// Used to debounce quick switching to/from the tuner
bool ignoreBypassSwitchUntilNextActuation = false;
bool effectActiveBeforeQuickSwitch = false;

// Time we last changed effect
uint32_t last_effect_change_time;

// Pot Monitoring Variables
bool knobValuesInitialized = false;
float knobValueDeadZone = 0.05f; // Dead zone on both ends of the raw knob range
float knobValueChangeTolerance = 1.0f / 256.0f;
float knobValueIdleTimeInSeconds = 1.0f;
int knobValueIdleTimeInSamples;
bool *knobValueCacheChanged = nullptr;
float *knobValueCache = nullptr;
int *knobValueSamplesTilIdle = nullptr;

// Switch Monitoring Variables
float switchEnabledIdleTimeInSeconds = 2.0f;
int switchEnabledIdleTimeInSamples;
bool *switchEnabledCache = nullptr;
bool *switchDoubleEnabledCache = nullptr;
int *switchEnabledSamplesTilIdle = nullptr;

// Tempo
bool needToChangeTempo = false;
uint32_t globalTempoBPM = 0;

bool isCrossFading = false;
bool isCrossFadingForward = true; // True goes Source->Target, False goes Target->Source
CrossFade crossFaderLeft, crossFaderRight;
float crossFaderTransitionTimeInSeconds = 0.1f;
int crossFaderTransitionTimeInSamples;
int samplesTilCrossFadingComplete;

void SetActiveEffect(int effectID);

static void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    // Process Audio
    float inputLeft;
    float inputRight;

    // Default LEDs are off
    float led1Brightness = 0.0f;
    float led2Brightness = 0.0f;

    // Handle Inputs
    hardware.ProcessAnalogControls();
    hardware.ProcessDigitalControls();
    guitarPedalUI.GenerateUIEvents();

    // Get a handle to the persitance storage settings
    Settings &settings = storage.GetSettings();

    // Process the Pots
    float knobValueRaw;

    for (int i = 0; i < hardware.GetKnobCount(); i++) {
        knobValueRaw = hardware.GetKnobValue(i);

        // Knobs don't perfectly return values in the 0.0f - 1.0f range
        // so we will add some deadzone to either end of the knob and remap values into
        // a full 0.0f - 1.0f range.
        if (knobValueRaw < knobValueDeadZone) {
            knobValueRaw = 0.0f;
        } else if (knobValueRaw > (1.0f - knobValueDeadZone)) {
            knobValueRaw = 1.0f;
        } else {
            knobValueRaw = (knobValueRaw - knobValueDeadZone) / (1.0f - (2.0f * knobValueDeadZone));
        }

        if (!knobValuesInitialized) {
            // Initialize the knobs for the first time to whatever the current knob placements are
            knobValueCacheChanged[i] = false;
            knobValueSamplesTilIdle[i] = 0;
            knobValueCache[i] = knobValueRaw;
        } else {
            // If the knobs are initialized handle monitor them for changes.
            if (knobValueSamplesTilIdle[i] > 0) {
                knobValueSamplesTilIdle[i] -= size;

                if (knobValueSamplesTilIdle[i] <= 0) {
                    knobValueSamplesTilIdle[i] = 0;
                    knobValueCacheChanged[i] = false;
                }
            }

            bool knobValueChangedToleranceMet = false;

            if (knobValueRaw > (knobValueCache[i] + knobValueChangeTolerance) ||
                knobValueRaw < (knobValueCache[i] - knobValueChangeTolerance)) {
                knobValueChangedToleranceMet = true;
                knobValueCacheChanged[i] = true;
                knobValueSamplesTilIdle[i] = knobValueIdleTimeInSamples;
            }

            if (knobValueChangedToleranceMet || knobValueCacheChanged[i]) {
                knobValueCache[i] = knobValueRaw;
            }
        }
    }

    // Store the previous value of the effect bypass so that we can determine if
    // we need to perform a toggle at the end of processing the switches
    bool oldEffectOn = effectOn;

    // Process potential footswitch actions before the main switch processing loop
    if (has_alternate_footswitch) {
        // Handle the scenario where have 2 footswitches

        // If both footswitches are down, save the parameters for this effect to
        // persistant storage If there is only one footswitch, it will do
        // parameter saving here when held instead of tuner quick switching later
        if (hardware.switches[hardware.GetPreferredSwitchIDForSpecialFunctionType(SpecialFunctionType::Bypass)].TimeHeldMs() > 2000 &&
            hardware.switches[hardware.GetPreferredSwitchIDForSpecialFunctionType(SpecialFunctionType::Alternate)].TimeHeldMs() >
                2000 &&
            !guitarPedalUI.IsShowingSavingSettingsScreen() && !ignoreBypassSwitchUntilNextActuation) {

            needToSaveSettingsForActiveEffect = true;
            ignoreBypassSwitchUntilNextActuation = true;
        }

        // If bypass is held for 2 seconds and alternate footswitch is not
        // pressed (not trying to save) then perform an action
        if (hardware.switches[hardware.GetPreferredSwitchIDForSpecialFunctionType(SpecialFunctionType::Bypass)].TimeHeldMs() > 2000 &&
            !hardware.switches[hardware.GetPreferredSwitchIDForSpecialFunctionType(SpecialFunctionType::Alternate)].Pressed() &&
            !ignoreBypassSwitchUntilNextActuation) {

            // If we have a screen and there is a tuner module, we quick switch
            // to it, otherwise we just cycle through the effects
            if (hardware.SupportsDisplay() && tunerModuleIndex > 0) {
                // Start the quick switch to the tuner
                if (activeEffectID == tunerModuleIndex) {
                    // Set back the active effect before the quick switch
                    SetActiveEffect(prevActiveEffectID);

                    // Restore the effect state from when we quick switched, this is an
                    // inverse because the act of holding the switch caused the state to
                    // chnage due to the rising edge being detected
                    effectOn = !effectActiveBeforeQuickSwitch;
                    activeEffect->SetEnabled(effectOn);
                } else {
                    // Store if effect is on or not when quick switching
                    effectActiveBeforeQuickSwitch = effectOn;

                    // Switch to tuner and force it to be enabled
                    SetActiveEffect(tunerModuleIndex);
                    effectOn = true;
                    activeEffect->SetEnabled(effectOn);
                }
                ignoreBypassSwitchUntilNextActuation = true;
            } else {
                // Cycle to the next effect
                int newActiveEffectId = activeEffectID + 1;

                // Skip over the tuner if there is no screen
                if (newActiveEffectId == tunerModuleIndex) {
                    newActiveEffectId++;
                }

                if (newActiveEffectId > availableEffectsCount - 1) {
                    newActiveEffectId = 0;
                }

                SetActiveEffect(newActiveEffectId);

                effectOn = false;
                activeEffect->SetEnabled(effectOn);

                ignoreBypassSwitchUntilNextActuation = true;
            }
        }

        // Disable quick switching until the footswitch is released to prevent infinite switching
        // also prevents saving from toggling quick switch.
        if (ignoreBypassSwitchUntilNextActuation &&
            !hardware.switches[hardware.GetPreferredSwitchIDForSpecialFunctionType(SpecialFunctionType::Bypass)].Pressed()) {
            ignoreBypassSwitchUntilNextActuation = false;
        }
    } else {
        // Handle the scenario where we only have 1 footswitch
        if (hardware.switches[hardware.GetPreferredSwitchIDForSpecialFunctionType(SpecialFunctionType::Bypass)].TimeHeldMs() > 2000 &&
            !guitarPedalUI.IsShowingSavingSettingsScreen()) {
            needToSaveSettingsForActiveEffect = true;
        }
    }

    // Process the switches
    for (int i = 0; i < hardware.GetSwitchCount(); i++) {
        bool switchPressed = hardware.switches[i].RisingEdge();

        // If this is the bypass switch, check for a bypass transition already
        // in progress (isCrossFading), and toggle the effect if the switch is
        // pressed
        if (!ignoreBypassSwitchUntilNextActuation && !isCrossFading &&
            i == hardware.GetPreferredSwitchIDForSpecialFunctionType(SpecialFunctionType::Bypass) && switchPressed) {
            effectOn = !effectOn;
        }

        if (effectOn && switchPressed && i == hardware.GetPreferredSwitchIDForSpecialFunctionType(SpecialFunctionType::Alternate)) {
            activeEffect->AlternateFootswitchPressed();
        }

        bool switchReleased = hardware.switches[i].FallingEdge();
        if (effectOn && switchReleased && i == hardware.GetPreferredSwitchIDForSpecialFunctionType(SpecialFunctionType::Alternate)) {
            activeEffect->AlternateFootswitchReleased();
        }

        bool switchHeld = hardware.switches[i].TimeHeldMs() >= 1000.f;
        if (effectOn && switchHeld && i == hardware.GetPreferredSwitchIDForSpecialFunctionType(SpecialFunctionType::Alternate)) {
            activeEffect->AlternateFootswitchHeldFor1Second();
        }

        if (switchEnabledCache[i] == true) {
            switchEnabledSamplesTilIdle[i] -= size;

            if (switchEnabledSamplesTilIdle[i] <= 0) {
                switchEnabledCache[i] = false;

                if (switchDoubleEnabledCache[i] != true) {
                    // We can safely know this was only a single tap here.
                }

                switchDoubleEnabledCache[i] = false;
            }
        }

        if (switchPressed) {
            // Note that switch is pressed and reset the IdleTimer for detecting double presses
            switchEnabledCache[i] = switchPressed;

            if (switchEnabledSamplesTilIdle[i] > 0) {
                switchDoubleEnabledCache[i] = true;

                // Register as Tap Tempo if Switch ID matched preferred mapping for TapTempo
                if (activeEffect->AlternateFootswitchForTempo() &&
                    i == hardware.GetPreferredSwitchIDForSpecialFunctionType(SpecialFunctionType::Alternate)) {
                    needToChangeTempo = true;
                    float timeBetweenPresses =
                        hardware.GetTimeForNumberOfSamples(switchEnabledIdleTimeInSamples - switchEnabledSamplesTilIdle[i]);
                    globalTempoBPM = s_to_tempo(timeBetweenPresses);
                }
            }

            switchEnabledSamplesTilIdle[i] = switchEnabledIdleTimeInSamples;
        }
    }

    // Handle updating the Hardware Bypass & Muting signals
    if (hardware.SupportsTrueBypass() && settings.globalRelayBypassEnabled) {
        hardware.SetAudioBypass(bypassOn);
        hardware.SetAudioMute(muteOn);
    } else {
        hardware.SetAudioBypass(false);
        hardware.SetAudioMute(false);
    }

    // Handle Effect State being Toggled.
    if (effectOn != oldEffectOn) {
        // Set the stats on the effect
        if (activeEffect != nullptr) {
            activeEffect->SetEnabled(effectOn);
        }

        // Setup the crossfade
        isCrossFading = true;
        samplesTilCrossFadingComplete = crossFaderTransitionTimeInSamples;
        isCrossFadingForward = effectOn;

        // Start the timing sequence for the Hardware Mute and Relay Bypass.
        if (hardware.SupportsTrueBypass() && settings.globalRelayBypassEnabled) {
            // Immediately Mute the Output using the Hardware Mute.
            muteOn = true;

            // Set the timing for when the bypass relay should trigger and when to unmute.
            samplesTilMuteOff = muteOffTransitionTimeInSamples;
            samplesTilBypassToggle = bypassToggleTransitionTimeInSamples;
        }
    }

    for (size_t i = 0; i < size; i++) {
        if (isCrossFading) {
            float crossFadeFactor = (float)samplesTilCrossFadingComplete / (float)crossFaderTransitionTimeInSamples;

            if (isCrossFadingForward) {
                crossFadeFactor = 1.0f - crossFadeFactor;
            }

            crossFaderLeft.SetPos(crossFadeFactor);
            crossFaderRight.SetPos(crossFadeFactor);

            samplesTilCrossFadingComplete -= 1;

            if (samplesTilCrossFadingComplete < 0) {
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
        if (settings.globalSplitMonoInputToStereo && !settings.globalRelayBypassEnabled) {
            inputRight = inputLeft;
        }

        // Setup Master Crossfader. By default source & target is always the input signal
        float crossFadeSourceLeft = inputLeft;
        float crossFadeSourceRight = inputRight;
        float crossFadeTargetLeft = inputLeft;
        float crossFadeTargetRight = inputRight;
        float effectOutputLeft = inputLeft;
        float effectOutputRight = inputRight;

        // Only calculate the active effect when it's needed
        if (activeEffect != nullptr && (effectOn || isCrossFading)) {
            // Apply the Active Effect
            if (hardware.SupportsStereo()) {
                activeEffect->ProcessStereo(inputLeft, inputRight);
            } else {
                activeEffect->ProcessMono(inputLeft);
            }

            effectOutputLeft = activeEffect->GetAudioLeft();
            effectOutputRight = activeEffect->GetAudioRight();

            // Update state of the LEDs
            led1Brightness = activeEffect->GetBrightnessForLED(0);
            led2Brightness = activeEffect->GetBrightnessForLED(1);
        }

        // Setup the crossfade target to be the effect
        crossFadeTargetLeft = effectOutputLeft;
        crossFadeTargetRight = effectOutputRight;

        out[0][i] = crossFaderLeft.Process(crossFadeSourceLeft, crossFadeTargetLeft);
        out[1][i] = crossFaderRight.Process(crossFadeSourceRight, crossFadeTargetRight);
    }

    // Override LEDs if we are saving the current settings
    if (guitarPedalUI.IsShowingSavingSettingsScreen()) {
        led1Brightness = 1.0f;
        led2Brightness = 1.0f;
    }

    // Handle LEDs
    hardware.SetLed(0, led1Brightness);
    hardware.SetLed(1, led2Brightness);
    hardware.UpdateLeds();
}

void SetActiveEffect(int effectID) {
    if (effectID >= 0 && effectID < availableEffectsCount) {
        // Store the last used effect
        prevActiveEffectID = activeEffectID;

        // Update the ID cache
        activeEffectID = effectID;

        // Update the Active Effect directly.
        activeEffect = availableEffects[effectID];

        guitarPedalUI.UpdateActiveEffect(effectID);

        // Get a handle to the persitance storage settings
        Settings &settings = storage.GetSettings();

        // Update the persistant storage setting
        settings.globalActiveEffectID = effectID;

        last_effect_change_time = System::GetNow();
    }
}

// Typical Switch case for Message Type.
void HandleMidiMessage(MidiEvent m) {
    if (!hardware.SupportsMidi()) {
        return;
    }

    // Get a handle to the persitance storage settings
    Settings &settings = storage.GetSettings();

    int channel = 0;

    // Make sure the settings midi channel is within the proper range
    // and convert the channel to be zero indexed instead of 1 like the setting.
    if (settings.globalMidiChannel >= 1 && settings.globalMidiChannel <= 16) {
        channel = settings.globalMidiChannel - 1;
    }

    // Pass the midi message through to midi out if so desired (only handles non system event types)
    if (settings.globalMidiThrough && m.type < SystemCommon) {
        // Re-pack the Midi Message
        uint8_t midiData[3];

        midiData[0] = 0b10000000 | ((uint8_t)m.type << 4) | ((uint8_t)m.channel);
        midiData[1] = m.data[0];
        midiData[2] = m.data[1];

        int bytesToSend = 3;

        if (m.type == ChannelPressure || m.type == ProgramChange) {
            bytesToSend = 2;
        }

        hardware.midi.SendMessage(midiData, sizeof(uint8_t) * bytesToSend);
    }

    // Only listen to messages for the devices set channel.
    if (m.channel != channel) {
        return;
    }

    switch (m.type) {
    case NoteOn: {
        NoteOnEvent p = m.AsNoteOn();
        char buff[512];
        sprintf(buff, "Note Received:\t%d\t%d\t%d\r\n", m.channel, m.data[0], m.data[1]);
        // hareware.seed.usb_handle.TransmitInternal((uint8_t *)buff, strlen(buff));
        //  This is to avoid Max/MSP Note outs for now..
        if (m.data[1] != 0) {
            p = m.AsNoteOn();
        }
    } break;
    case ControlChange: {
        if (activeEffect != nullptr) {
            ControlChangeEvent p = m.AsControlChange();

            // Notify the activeEffect to handle this midi cc / value
            activeEffect->MidiCCValueNotification(p.control_number, p.value);

            // Notify the UI to update if this CC message was mapped to an EffectParameter
            int effectParamID = activeEffect->GetMappedParameterIDForMidiCC(p.control_number);

            if (effectParamID != -1) {
                guitarPedalUI.UpdateActiveEffectParameterValue(effectParamID, true);
            }
        }
        break;
    }
    case ProgramChange: {
        ProgramChangeEvent p = m.AsProgramChange();

        if (p.program >= 0 && p.program < availableEffectsCount) {
            SetActiveEffect(p.program);
        }
        break;
    }
    default:
        break;
    }
}

int main(void) {
    hardware.Init();
    hardware.SetAudioBlockSize(4);

    float sample_rate = hardware.AudioSampleRate();

    // Set the number of samples to use for the crossfade based on the hardware sample rate
    muteOffTransitionTimeInSamples = hardware.GetNumberOfSamplesForTime(muteOffTransitionTimeInSeconds);
    bypassToggleTransitionTimeInSamples = hardware.GetNumberOfSamplesForTime(bypassToggleTransitionTimeInSeconds);
    crossFaderTransitionTimeInSamples = hardware.GetNumberOfSamplesForTime(crossFaderTransitionTimeInSeconds);

    // Init the Effects Modules
    availableEffectsCount = 15;
    availableEffects = new BaseEffectModule *[availableEffectsCount];
    availableEffects[0] = new ModulatedTremoloModule();
    availableEffects[1] = new OverdriveModule();
    availableEffects[2] = new AutoPanModule();
    availableEffects[3] = new ChorusModule();
    availableEffects[4] = new ChopperModule();
    availableEffects[5] = new ReverbModule();
    availableEffects[6] = new MultiDelayModule();
    availableEffects[7] = new MetroModule();
    availableEffects[8] = new TunerModule();
    availableEffects[9] = new PitchShifterModule();
    availableEffects[10] = new CompressorModule();
    availableEffects[11] = new LooperModule();
    availableEffects[12] = new GraphicEQModule();
    availableEffects[13] = new ParametricEQModule();
    availableEffects[14] = new NoiseGateModule();

    for (int i = 0; i < availableEffectsCount; i++) {
        availableEffects[i]->Init(sample_rate);

        if (std::string(availableEffects[i]->GetName()) == std::string("Tuner")) {
            // Store the index for the tuner module so that we can quickswitch
            // to/from it
            tunerModuleIndex = i;
        }
    }

    // Initalize Persistance Storage
    InitPersistantStorage();

    Settings &settings = storage.GetSettings();

    // Load all the effect specific settings
    LoadEffectSettingsFromPersistantStorage();

    // Set the active effect
    activeEffect = availableEffects[settings.globalActiveEffectID];
    activeEffectID = settings.globalActiveEffectID;
    activeEffect->SetEnabled(effectOn);

    // Init the Menu UI System
    if (hardware.SupportsDisplay()) {
        guitarPedalUI.Init();
    }

    // Set up midi if supported.
    if (hardware.SupportsMidi()) {
        hardware.midi.StartReceive();
    }

    // Setup Relay Bypass State
    if (hardware.SupportsTrueBypass() && settings.globalRelayBypassEnabled) {
        bypassOn = true;
    }

    // Init the Knob Monitoring System
    knobValueCacheChanged = new bool[hardware.GetKnobCount()];
    knobValueCache = new float[hardware.GetKnobCount()];
    knobValueSamplesTilIdle = new int[hardware.GetKnobCount()];
    knobValueIdleTimeInSamples = hardware.GetNumberOfSamplesForTime(knobValueIdleTimeInSeconds);

    // Init the Switch Monitoring System
    switchEnabledCache = new bool[hardware.GetSwitchCount()];
    switchDoubleEnabledCache = new bool[hardware.GetSwitchCount()];
    switchEnabledSamplesTilIdle = new int[hardware.GetSwitchCount()];
    switchEnabledIdleTimeInSamples = hardware.GetNumberOfSamplesForTime(switchEnabledIdleTimeInSeconds);

    for (int i = 0; i < hardware.GetSwitchCount(); i++) {
        switchEnabledCache[i] = false;
        switchDoubleEnabledCache[i] = false;
        switchEnabledSamplesTilIdle[i] = 0;
    }

    // Setup the cross fader
    crossFaderLeft.Init();
    crossFaderRight.Init();
    crossFaderLeft.SetPos(0.0f);
    crossFaderRight.SetPos(0.0f);

    // start callback
    hardware.StartAdc();
    hardware.StartAudio(AudioCallback);

    // Set initial time stamp
    lastTimeStampUS = System::GetUs();

    // Setup Debug Logging
    // hardware.seed.StartLog();

    while (1) {
        // Handle Clock Time
        uint32_t currentTimeStampUS = System::GetUs();
        uint32_t elapsedTimeStampUS = currentTimeStampUS - lastTimeStampUS;
        lastTimeStampUS = currentTimeStampUS;
        float elapsedTimeInSeconds = (elapsedTimeStampUS / 1000000.0f);
        secondsSinceStartup = secondsSinceStartup + elapsedTimeInSeconds;

        // Handle Knob Changes
        if (!knobValuesInitialized && secondsSinceStartup > 1.0f) {
            // Let the initial readings of the knob values settle before trying to use them.
            knobValuesInitialized = true;
        }

        if (knobValuesInitialized) {
            for (int i = 0; i < hardware.GetKnobCount(); i++) {
                if (knobValueCacheChanged[i]) {
                    int parameterID = activeEffect->GetMappedParameterIDForKnob(i);

                    if (parameterID != -1) {
                        // Set the new value on the effect parameter directly
                        activeEffect->SetParameterAsMagnitude(parameterID, knobValueCache[i]);

                        // Update the effect parameter on the menu system to reflect the change
                        guitarPedalUI.UpdateActiveEffectParameterValue(parameterID, true);
                    }
                }
            }
        }

        // Handle Global Tempo Changes
        if (needToChangeTempo) {
            activeEffect->SetTempo(globalTempoBPM);
            needToChangeTempo = false;

            // Update the effect parameters on the menu system to reflect any changes
            guitarPedalUI.UpdateActiveEffectParameterValues();
        }

        // If alt footswitch held AND encoder turned, iterate to next/previous effect, also throttle the changes
        const int encoderIncrement = hardware.encoders[0].Increment();
        if (has_alternate_footswitch &&
            hardware.switches[hardware.GetPreferredSwitchIDForSpecialFunctionType(SpecialFunctionType::Alternate)].Pressed() &&
            encoderIncrement != 0 && System::GetNow() - last_effect_change_time >= 10) {
            int desiredIndex = activeEffectID - encoderIncrement;
            if (desiredIndex > availableEffectsCount - 1) {
                desiredIndex = 0;
            } else if (desiredIndex < 0) {
                desiredIndex = availableEffectsCount - 1;
            }
            SetActiveEffect(desiredIndex);
        }

        if (hardware.SupportsDisplay()) {
            // Handle a Change in the Active Effect from the Menu System

            // Check which effect the Menu system thinks is active
            int menuEffectID = guitarPedalUI.GetActiveEffectIDFromSettingsMenu();
            BaseEffectModule *selectedEffect = availableEffects[menuEffectID];

            // If the effect differs from the active effect, change the active effect
            if (activeEffect != selectedEffect) {
                SetActiveEffect(menuEffectID);
            }
        }

        // Handle Display
        if (hardware.SupportsDisplay()) {
            if (useDebugDisplay) {
                // Debug Display hijacks the display to simply output text
                char strbuff[128];
                hardware.display.Fill(false);
                hardware.display.SetCursor(0, 0);
                hardware.display.WriteString("Debug:", Font_7x10, true);
                hardware.display.SetCursor(0, 15);
                sprintf(strbuff, "tap: %d", switchEnabledCache[1]);
                hardware.display.WriteString(strbuff, Font_7x10, true);
                hardware.display.SetCursor(0, 30);
                sprintf(strbuff, "dtap: %d", switchDoubleEnabledCache[1]);
                hardware.display.WriteString(strbuff, Font_7x10, true);
                hardware.display.SetCursor(0, 45);
                sprintf(strbuff, "BPM %ld", globalTempoBPM);
                hardware.display.WriteString(strbuff, Font_7x10, true);
                hardware.display.Update();
            } else {
                // Handle UI Updates for the UI System
                guitarPedalUI.UpdateUI(elapsedTimeInSeconds);
            }
        }

        // Handle MIDI Events
        if (hardware.SupportsMidi() && settings.globalMidiEnabled) {
            hardware.midi.Listen();

            while (hardware.midi.HasEvents()) {
                HandleMidiMessage(hardware.midi.PopEvent());
            }
        }

        // Throttle persitant storage saves to once every 2 seconds:
        if (System::GetNow() - last_save_time >= 2000) {
            if (needToSaveSettingsForActiveEffect) {
                uint16_t tempPreset = activeEffect->GetCurrentPreset();
                SaveEffectSettingsToPersitantStorageForEffectID(activeEffectID, tempPreset);
                guitarPedalUI.ShowSavingSettingsScreen();
            }
            storage.Save();
            last_save_time = System::GetNow();
            needToSaveSettingsForActiveEffect = false;
        }
    }
}
