#include "daisysp.h"
#include "guitar_pedal_storage.h"
#include "loaded_effects.h"
#include <string.h>

#include "UI/guitar_pedal_ui.h"
#include "Util/audio_utilities.h"
#include "Util/heap_guard.h"

using namespace daisy;
using namespace daisysp;
using namespace bkshepherd;

// Uncomment the version you are trying to use, by default (and if nothing is
// uncommented), the 125B with 2 footswitch variant will be used

// #define VARIANT_125B
// #define VARIANT_1590B
// #define VARIANT_1590B_SMD
// #define VARIANT_TERRARIUM
// #define VARIANT_FUNBOX

// Store the hardware in SRAM to save DTCMRAM for the dynamic
// variables and buffers, this saves a large amount of DTCMRAM
#define TEXT_SECTION __attribute__((section(".text")))
#if defined(VARIANT_TERRARIUM)
#include "Hardware-Modules/guitar_pedal_terrarium.h"
constexpr bool has_alternate_footswitch = true;
GuitarPedalTerrarium TEXT_SECTION hardware;
#elif defined(VARIANT_1590B)
#include "Hardware-Modules/guitar_pedal_1590b.h"
constexpr bool has_alternate_footswitch = false;
GuitarPedal1590B TEXT_SECTION hardware;
#elif defined(VARIANT_1590B_SMD)
#include "Hardware-Modules/guitar_pedal_1590b-SMD.h"
constexpr bool has_alternate_footswitch = false;
GuitarPedal1590BSMD TEXT_SECTION hardware;
#elif defined(VARIANT_FUNBOX)
#include "Hardware-Modules/guitar_pedal_funbox.h"
constexpr bool has_alternate_footswitch = true;
GuitarPedalFunbox TEXT_SECTION hardware;
#else
#include "Hardware-Modules/guitar_pedal_125b.h"
constexpr bool has_alternate_footswitch = true;
GuitarPedal125B TEXT_SECTION hardware;
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

// Audio sample rate and block size (see main())
constexpr SaiHandle::Config::SampleRate kAudioSampleRate = SaiHandle::Config::SampleRate::SAI_48KHZ;
constexpr size_t kAudioBlockSize = 48;

// Hardware Related Variables

// Developer switch (compile time): replaces the whole display with a plain text page (footswitch taps and BPM) instead of the normal UI.
// The CPU and level readouts are not part of this; they are the saved "Show Debug" setting (settings.globalShowDebug).
constexpr bool useDebugTextScreen = false;

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

struct AutoSaveEffectState {
    uint32_t observedParameterChangeCount;
    uint32_t lastParameterChangeTime;
    bool pending;
};

AutoSaveEffectState *autoSaveEffectStates = nullptr;
uint32_t last_auto_save_check_time = 0;
constexpr uint32_t kAutoSaveDebounceMs = 5000;
constexpr uint32_t kAutoSaveCheckIntervalMs = 50;

struct PendingMidiBank {
    uint8_t msb = 0;
    uint8_t lsb = 0;
    bool active = false;
};

PendingMidiBank pendingMidiBank;

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
bool alternateHeldFor1SecondTriggered = false;

// Tempo
bool needToChangeTempo = false;
uint32_t globalTempoBPM = 0;

// Set when an alternate footswitch event may have changed the active
// effect's own parameters from inside itself (e.g. EffectChain's
// footswitch-toggle mode flipping a slot's "On" parameter). The menu's
// per-tick writeback loop would otherwise silently revert that change on the
// next UpdateUI(), so the main loop must refresh the UI's cached values
// first - see the needToChangeTempo handling below for the same pattern.
bool needToRefreshMenuParameterValues = false;

bool isCrossFading = false;
bool isCrossFadingForward = true; // True goes Source->Target, False goes Target->Source
CrossFade crossFaderLeft, crossFaderRight;
float crossFaderTransitionTimeInSeconds = 0.1f;
int crossFaderTransitionTimeInSamples;
int samplesTilCrossFadingComplete;

// Trails (settings.globalTrails): an effect with a tail (GetMaxTrailSeconds() > 0) keeps being processed after it is turned off, with its
// input faded to silence, and its output is added to the dry signal. It stops when the tail has died out or the maximum time has passed.
float trailInputGain = 0.0f;    // 0..1: level of the signal going into the effect, ramped over the crossfade time
bool trailRunning = false;      // the effect is on, or off but still ringing
int trailSilentSamples = 0;     // how long the effect's output has been (nearly) silent while off
int trailSamplesSinceOff = 0;   // how long the effect has been off
constexpr float kTrailSilenceLevel = 1.0e-4f; // about -80 dB
constexpr float kTrailSilenceSeconds = 0.25f; // this long below the level means the tail is over
CpuLoadMeter cpuLoadMeter;

// Slowest audio callback since the main loop last read it, in system ticks (for the peak CPU readout: the meter above only gives a smoothed average, which hides
// the occasional block that takes longer than the time the block lasts, i.e. an overrun)
static volatile uint32_t slowestBlockTicks = 0;
static float ticksPerBlockInv = 0.0f;

// Averaged block peaks of the pedal's input and final output (linear), for the level readout on the display
static float avgInputPeak = 0.0f;
static float avgOutputPeak = 0.0f;
static constexpr float kLevelAvgCoeff = 0.02f; // per 1 ms block, about a 50 ms time constant

// Effect switch requested from the audio callback (interrupt context).
// Switching effects rebuilds the UI menus with heap new/delete, which is not
// safe from the interrupt (allocator reentrancy, and the main loop may be
// iterating those menu allocations), so the request is deferred to the main
// loop. -1 means no switch is pending.
volatile int pendingEffectIDFromAudioCallback = -1;

void SetActiveEffect(int effectID);

void processPots(size_t size)
{
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
}

void processFootswitches(size_t size, Settings &settings)
{
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
            // to it, otherwise we just cycle through the effects.
            // The actual switch is deferred to the main loop (it isn't safe
            // from this interrupt); SetActiveEffect syncs the new effect's
            // enabled state with effectOn when the switch is applied.
            if (hardware.SupportsDisplay() && tunerModuleIndex >= 0) {
                // Start the quick switch to the tuner
                if (activeEffectID == tunerModuleIndex) {
                    // Set back the active effect before the quick switch
                    pendingEffectIDFromAudioCallback = prevActiveEffectID;

                    // Restore the effect state from when we quick switched, this is an
                    // inverse because the act of holding the switch caused the state to
                    // chnage due to the rising edge being detected
                    effectOn = !effectActiveBeforeQuickSwitch;
                } else {
                    // Store if effect is on or not when quick switching
                    effectActiveBeforeQuickSwitch = effectOn;

                    // Switch to tuner and force it to be enabled
                    pendingEffectIDFromAudioCallback = tunerModuleIndex;
                    effectOn = true;
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

                pendingEffectIDFromAudioCallback = newActiveEffectId;

                effectOn = false;

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
            needToRefreshMenuParameterValues = true;
        }

        bool switchReleased = hardware.switches[i].FallingEdge();
        if (switchReleased && i == hardware.GetPreferredSwitchIDForSpecialFunctionType(SpecialFunctionType::Alternate)) {
            alternateHeldFor1SecondTriggered = false;
        }
        if (effectOn && switchReleased && i == hardware.GetPreferredSwitchIDForSpecialFunctionType(SpecialFunctionType::Alternate)) {
            activeEffect->AlternateFootswitchReleased();
            needToRefreshMenuParameterValues = true;
        }

        bool switchHeld = hardware.switches[i].TimeHeldMs() >= 1000.f;
        if (effectOn && switchHeld && !alternateHeldFor1SecondTriggered &&
            i == hardware.GetPreferredSwitchIDForSpecialFunctionType(SpecialFunctionType::Alternate)) {
            alternateHeldFor1SecondTriggered = true;
            activeEffect->AlternateFootswitchHeldFor1Second();
            needToRefreshMenuParameterValues = true;
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

        // Persist the toggle so a power cycle restores the pedal in the same
        // on/off state it was left in. This is a plain memory write (like
        // SetActiveEffect's settings.globalActiveEffectID update), flushed to
        // flash by the periodic storage.Save() in the main loop - no separate
        // save gesture needed.
        settings.globalEffectOn = effectOn;

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
}

void updateLeds(float led1Brightness, float led2Brightness)
{
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

static float linearToDb(float linear) {
    return (linear > 1.0e-5f) ? 20.0f * log10f(linear) : -100.0f;
}

static void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    cpuLoadMeter.OnBlockStart();
    const uint32_t blockStartTick = System::GetTick();

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
    processPots(size);

    processFootswitches(size, settings);

    // Run the active effect over the whole block up front. The effect doesn't depend on the crossfade/mute state below, so the
    // per-sample loop only has to mix the dry and effect signals.
    // Whether the effect is needed is decided once per block; if a crossfade ends mid-block the effect simply runs a few extra samples.
    static constexpr size_t kMaxBlock = 256; // the largest audio block size supported (see kAudioBlockSize)
    static float dryLeft[kMaxBlock], dryRight[kMaxBlock], fxLeft[kMaxBlock], fxRight[kMaxBlock], fxInLeft[kMaxBlock], fxInRight[kMaxBlock];
    const bool splitMono = settings.globalSplitMonoInputToStereo && !settings.globalRelayBypassEnabled;

    // Trails need the audio to pass through the pedal (the True Bypass relay would route the dry signal around it and cut the tail).
    const float maxTrailSeconds = (activeEffect != nullptr) ? activeEffect->GetMaxTrailSeconds() : 0.0f;
    const bool trails = settings.globalTrails && !settings.globalRelayBypassEnabled && maxTrailSeconds > 0.0f;
    const float sampleRate = hardware.AudioSampleRate();
    if (trails) {
        if (effectOn || trailInputGain > 0.0f) {
            trailRunning = true;
            trailSilentSamples = 0;
            trailSamplesSinceOff = 0;
        }
    } else {
        trailRunning = false;
        trailInputGain = effectOn ? 1.0f : 0.0f;
    }
    const bool runEffect = activeEffect != nullptr && (trails ? trailRunning : (effectOn || isCrossFading));
    const float trailGainStep = 1.0f / (float)crossFaderTransitionTimeInSamples;
    float trailBlockPeak = 0.0f;

    for (size_t start = 0; start < size; start += kMaxBlock) {
        const size_t n = (size - start < kMaxBlock) ? (size - start) : kMaxBlock;

        for (size_t i = 0; i < n; i++) {
            dryLeft[i] = in[0][start + i];
            dryRight[i] = splitMono ? dryLeft[i] : in[1][start + i];
        }

        if (runEffect && trails) {
            // Input to the effect is the dry signal times the ramped gain, the dry signal around it times the rest: with the effect on this is
            // the plain effect output, with it off it is the dry signal plus whatever the effect still rings.
            const float gainTarget = effectOn ? 1.0f : 0.0f;
            for (size_t i = 0; i < n; i++) {
                if (trailInputGain < gainTarget) {
                    trailInputGain = fminf(gainTarget, trailInputGain + trailGainStep);
                } else if (trailInputGain > gainTarget) {
                    trailInputGain = fmaxf(gainTarget, trailInputGain - trailGainStep);
                }
                fxInLeft[i] = dryLeft[i] * trailInputGain;
                fxInRight[i] = dryRight[i] * trailInputGain;
                fxLeft[i] = dryLeft[i] * (1.0f - trailInputGain); // the dry part, the effect's output is added below
                fxRight[i] = dryRight[i] * (1.0f - trailInputGain);
            }
            static float wetLeft[kMaxBlock], wetRight[kMaxBlock];
            if (hardware.SupportsStereo()) {
                activeEffect->ProcessStereoBlock(fxInLeft, fxInRight, wetLeft, wetRight, n);
            } else {
                activeEffect->ProcessMonoBlock(fxInLeft, wetLeft, wetRight, n);
            }
            for (size_t i = 0; i < n; i++) {
                fxLeft[i] += wetLeft[i];
                fxRight[i] += wetRight[i];
                trailBlockPeak = fmaxf(trailBlockPeak, fmaxf(fabsf(wetLeft[i]), fabsf(wetRight[i])));
            }
        } else if (runEffect) {
            if (hardware.SupportsStereo()) {
                activeEffect->ProcessStereoBlock(dryLeft, dryRight, fxLeft, fxRight, n);
            } else {
                activeEffect->ProcessMonoBlock(dryLeft, fxLeft, fxRight, n);
            }
        } else {
            std::copy(dryLeft, dryLeft + n, fxLeft);
            std::copy(dryRight, dryRight + n, fxRight);
        }

        for (size_t i = 0; i < n; i++) {
            out[0][start + i] = fxLeft[i];
            out[1][start + i] = fxRight[i];
        }
    }

    if (trails && trailRunning && !effectOn && trailInputGain <= 0.0f) {
        // The effect is off and only ringing: stop it when the tail is over, or at the latest after its maximum trail time.
        trailSilentSamples = (trailBlockPeak < kTrailSilenceLevel) ? trailSilentSamples + (int)size : 0;
        trailSamplesSinceOff += (int)size;
        if (trailSilentSamples > (int)(kTrailSilenceSeconds * sampleRate) || trailSamplesSinceOff > (int)(maxTrailSeconds * sampleRate)) {
            trailRunning = false;
        }
    }
    if (activeEffect != nullptr) {
        // So that an effect that clears its buffers when it is turned on keeps the tail that is still ringing
        activeEffect->SetTrailing(trails && trailRunning && !effectOn);
    }

    if (runEffect) {
        // Update state of the LEDs
        led1Brightness = activeEffect->GetBrightnessForLED(0);
        led2Brightness = activeEffect->GetBrightnessForLED(1);
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

        // Dry input and effect output for this sample. The effect output was written to out[] by the block pass above.
        // (Dry input must be re-read from the input buffers, which are not aliased to out[] on the Daisy.)
        inputLeft = in[0][i];
        inputRight = (splitMono) ? inputLeft : in[1][i];

        float crossFadeSourceLeft = inputLeft;
        float crossFadeSourceRight = inputRight;
        float crossFadeTargetLeft = out[0][i];
        float crossFadeTargetRight = out[1][i];

        if (!trails) {
            out[0][i] = crossFaderLeft.Process(crossFadeSourceLeft, crossFadeTargetLeft);
            out[1][i] = crossFaderRight.Process(crossFadeSourceRight, crossFadeTargetRight);
        }
    }

    updateLeds(led1Brightness, led2Brightness);

    // Track the peak of this block's input and output (both channels) and average it over time
    float inPeak = 0.0f;
    float outPeak = 0.0f;
    for (size_t i = 0; i < size; i++) {
        inPeak = fmaxf(inPeak, fmaxf(fabsf(in[0][i]), fabsf(in[1][i])));
        outPeak = fmaxf(outPeak, fmaxf(fabsf(out[0][i]), fabsf(out[1][i])));
    }
    avgInputPeak += kLevelAvgCoeff * (inPeak - avgInputPeak);
    avgOutputPeak += kLevelAvgCoeff * (outPeak - avgOutputPeak);

    cpuLoadMeter.OnBlockEnd();
    const uint32_t blockTicks = System::GetTick() - blockStartTick;
    if (blockTicks > slowestBlockTicks) {
        slowestBlockTicks = blockTicks;
    }
}

void SetActiveEffect(int effectID) {
    if (effectID >= 0 && effectID < availableEffectsCount) {
        // Store the last used effect
        prevActiveEffectID = activeEffectID;

        // Update the ID cache
        activeEffectID = effectID;

        // An effect that is only ringing out is cut when another one is selected
        if (activeEffect != nullptr) {
            activeEffect->SetTrailing(false);
        }
        trailRunning = false;
        trailInputGain = effectOn ? 1.0f : 0.0f;

        // Update the Active Effect directly.
        activeEffect = availableEffects[effectID];

        // Keep the new effect's enabled state in sync with the global bypass
        // state (previously only the footswitch paths did this, leaving menu
        // and MIDI program-change switches with stale LED/enabled state)
        activeEffect->SetEnabled(effectOn);

        guitarPedalUI.UpdateActiveEffect(effectID);

        // Get a handle to the persitance storage settings
        Settings &settings = storage.GetSettings();

        // Update the persistant storage setting
        settings.globalActiveEffectID = effectID;

        last_effect_change_time = System::GetNow();
    }
}

void ResetAutoSaveTrackingForEffect(int effectID) {
    if (autoSaveEffectStates == nullptr || effectID < 0 || effectID >= availableEffectsCount) {
        return;
    }

    AutoSaveEffectState &autoSaveState = autoSaveEffectStates[effectID];
    autoSaveState.observedParameterChangeCount = availableEffects[effectID]->GetParameterChangeCount();
    autoSaveState.pending = false;
}

void SetActiveEffectAndPreset(int effectID, uint32_t presetID) {
    if (effectID < 0 || effectID >= availableEffectsCount) {
        return;
    }

    BaseEffectModule *effect = availableEffects[effectID];
    if (presetID < effect->GetPresetCount()) {
        effect->SetCurrentPreset(presetID);
        LoadPresetFromPersistentStorage(effectID, presetID);
        ResetAutoSaveTrackingForEffect(effectID);
    }

    SetActiveEffect(effectID);
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
        if (activeEffect != NULL) {
            NoteOnEvent p = m.AsNoteOn();
            activeEffect->OnNoteOn(p.note, p.velocity);
        }
        break;
    }
    case NoteOff: {
        if (activeEffect != NULL) {
            NoteOnEvent p = m.AsNoteOn();
            activeEffect->OnNoteOff(p.note, p.velocity);
        }
        break;
    }
    case ControlChange: {
        if (activeEffect != nullptr) {
            ControlChangeEvent p = m.AsControlChange();

            if (p.control_number == 0) {
                pendingMidiBank.msb = p.value;
                pendingMidiBank.lsb = 0;
                pendingMidiBank.active = true;
                break;
            }

            if (p.control_number == 32) {
                pendingMidiBank.lsb = p.value;
                pendingMidiBank.active = true;
                break;
            }

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

        const bool hasBank = pendingMidiBank.active;
        const uint16_t presetID = static_cast<uint16_t>(pendingMidiBank.msb) * 128U + pendingMidiBank.lsb;
        pendingMidiBank = {};

        if (p.program < 0 || p.program >= availableEffectsCount) {
            break;
        }

        if (hasBank) {
            SetActiveEffectAndPreset(p.program, presetID);
        } else {
            SetActiveEffect(p.program);
        }
        break;
    }
    default:
        break;
    }
}

// Three lines of text on the display, for the heap guard (see Util/heap_guard.h): the heap ran out, or an effect is estimated not to fit
static void drawHeapMessage(const char *line1, const char *line2, const char *line3) {
    if (!hardware.SupportsDisplay()) return;
    hardware.display.Fill(false);
    hardware.display.SetCursor(0, 0);
    hardware.display.WriteString(line1, Font_7x10, true);
    hardware.display.SetCursor(0, 16);
    hardware.display.WriteString(line2, Font_6x8, true);
    hardware.display.SetCursor(0, 28);
    hardware.display.WriteString(line3, Font_6x8, true);
    hardware.display.SetCursor(0, 46);
    hardware.display.WriteString("Select fewer effects", Font_6x8, true);
    hardware.display.SetCursor(0, 55);
    hardware.display.WriteString("(tools/effects.py)", Font_6x8, true);
    hardware.display.Update();
}

int main(void) {

    // Audio settings. Venus (the only effect in this build) is heavy: each of its 4096-point FFT stages has to fit inside one block, which a 1 ms block (48
    // samples) does not allow. 256 samples is 5.3 ms at 48 kHz. (The original FunBox pedal runs at 32 kHz with 256 samples; that also works and takes less CPU.)
    // The other effects were written and measured at 48 kHz with 48-sample blocks: set kAudioBlockSize back to 48 when they are enabled again.
    const size_t blockSize = kAudioBlockSize;
    const bool boost = true; // true enables cpu boost (480Mhz instead of 400Mhz)

    hardware.Init(blockSize, boost);
    hardware.SetAudioSampleRate(kAudioSampleRate);
    heapguard::SetHaltHandler(drawHeapMessage);

    const float sample_rate = hardware.AudioSampleRate();

    // Setup CPU logging of the audio callback
    cpuLoadMeter.Init(sample_rate, blockSize);
    ticksPerBlockInv = 1.0f / ((float)System::GetTickFreq() * (float)blockSize / sample_rate);

    // Set the number of samples to use for the crossfade based on the hardware sample rate
    muteOffTransitionTimeInSamples = hardware.GetNumberOfSamplesForTime(muteOffTransitionTimeInSeconds);
    bypassToggleTransitionTimeInSamples = hardware.GetNumberOfSamplesForTime(bypassToggleTransitionTimeInSeconds);
    crossFaderTransitionTimeInSamples = hardware.GetNumberOfSamplesForTime(crossFaderTransitionTimeInSeconds);

    // Init the Effects Modules
    const uint32_t *heapEstimates = nullptr;
    heapguard::SetStage("creating effects");
    load_effects(availableEffectsCount, availableEffects, heapEstimates);

    for (int i = 0; i < availableEffectsCount; i++) {
        // The heap has no other limit than the end of RAM_D2, where the heap guard stops with a message. An effect that is estimated to need more than is left is announced
        // first (the estimates are rough: it still tries, and the guard decides)
        heapguard::SetStage(availableEffects[i]->GetName());
        if (heapEstimates[i] > heapguard::Free() && hardware.SupportsDisplay()) {
            char line2[32], line3[32];
            snprintf(line2, sizeof line2, "%s", availableEffects[i]->GetName());
            snprintf(line3, sizeof line3, "need ~%uK, %uK left", static_cast<unsigned>(heapEstimates[i] / 1024), static_cast<unsigned>(heapguard::Free() / 1024));
            drawHeapMessage("HEAP WARNING", line2, line3);
            System::Delay(2500);
        }
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
    effectOn = settings.globalEffectOn;
    trailInputGain = effectOn ? 1.0f : 0.0f;
    activeEffect->SetEnabled(effectOn);

    // LoadEffectSettingsFromPersistantStorage() above loads every effect's own
    // saved preset 0, in effectList order. An effect that shares a
    // single-instance-only child with another effect (e.g. one
    // DattorroReverbModule referenced from two EffectChains, see README) ends
    // up with whichever of those effects happens to load last, which isn't
    // necessarily the one about to be active. Reloading just the active
    // effect's own preset 0 here makes it the final writer of anything it
    // touches, so it matches what the user actually saved for it. This is a
    // no-op for every parameter that isn't shared (SetParameterRaw/
    // SetParameterAsFloat only call ParameterChanged when a value actually
    // differs from what's already set).
    LoadPresetFromPersistentStorage(activeEffectID, 0);

    autoSaveEffectStates = new AutoSaveEffectState[availableEffectsCount];
    for (int i = 0; i < availableEffectsCount; ++i) {
        autoSaveEffectStates[i] = {availableEffects[i]->GetParameterChangeCount(), 0, false};
    }
    last_auto_save_check_time = System::GetNow();

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
        bypassOn = !effectOn;
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

    // Setup the cross fader. Match the restored effectOn state so the audio
    // path doesn't start silently bypassed/wet relative to what SetEnabled()
    // and the LEDs already reflect above.
    crossFaderLeft.Init();
    crossFaderRight.Init();
    crossFaderLeft.SetPos(effectOn ? 1.0f : 0.0f);
    crossFaderRight.SetPos(effectOn ? 1.0f : 0.0f);

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

        // Apply any effect switch requested by the audio callback. Doing it
        // here (instead of in the interrupt) keeps the UI menu heap
        // allocations off the audio interrupt. This must run before the menu
        // sync below so the menu reflects the switch instead of undoing it.
        if (pendingEffectIDFromAudioCallback >= 0) {
            int pendingID = pendingEffectIDFromAudioCallback;
            pendingEffectIDFromAudioCallback = -1;
            SetActiveEffect(pendingID);
        }

        // Handle Knob Changes
        if (!knobValuesInitialized && secondsSinceStartup > 1.0f) {
            // Let the initial readings of the knob values settle before trying to use them.
            knobValuesInitialized = true;
        }

        if (settings.globalLockKnobs) {
            // The knobs are still sampled (so their cached state is valid when they are unlocked) but never applied
            for (int i = 0; i < hardware.GetKnobCount(); i++) {
                knobValueCacheChanged[i] = false;
            }
        } else if (knobValuesInitialized) {
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

        // An alternate footswitch event may have changed the active effect's
        // own parameters (e.g. EffectChain's footswitch-toggle mode). Refresh
        // the menu's cached values from it before UpdateUI() runs below,
        // otherwise the menu's own writeback would silently revert the change.
        if (needToRefreshMenuParameterValues) {
            needToRefreshMenuParameterValues = false;
            guitarPedalUI.UpdateActiveEffectParameterValues();
        }

        // If alt footswitch held AND encoder turned, iterate to next/previous effect, also throttle the changes
        if (hardware.SupportsEncoder() && has_alternate_footswitch &&
            hardware.switches[hardware.GetPreferredSwitchIDForSpecialFunctionType(SpecialFunctionType::Alternate)].Pressed() &&
            System::GetNow() - last_effect_change_time >= 10) {
            const int encoderIncrement = hardware.encoders[0].Increment();
            if (encoderIncrement != 0) {
                int desiredIndex = activeEffectID + encoderIncrement;
                if (desiredIndex > availableEffectsCount - 1) {
                    desiredIndex = 0;
                } else if (desiredIndex < 0) {
                    desiredIndex = availableEffectsCount - 1;
                }
                SetActiveEffect(desiredIndex);
            }
        }

        if (hardware.SupportsDisplay()) {
            // Handle a Change in the Active Effect from the Menu System

            // Check which effect the Menu system thinks is active
            int menuEffectID = guitarPedalUI.GetSelectedEffectID();
            BaseEffectModule *selectedEffect = availableEffects[menuEffectID];

            // If the effect differs from the active effect, change the active effect
            if (activeEffect != selectedEffect) {
                SetActiveEffect(menuEffectID);
            }
        }

        // Set the latest cpu load to the effect
        activeEffect->SetShowDebug(settings.globalShowDebug);
        activeEffect->SetCPUUsage(cpuLoadMeter.GetAvgCpuLoad());

        // Peak load: the slowest block of the last two seconds (100% = the block took as long as it lasts, a bigger value is an overrun)
        static float peakLoad = 0.0f;
        static float secondsSincePeakReset = 0.0f;
        static float peakInWindow = 0.0f;
        secondsSincePeakReset += elapsedTimeInSeconds;
        const uint32_t ticks = slowestBlockTicks;
        slowestBlockTicks = 0;
        peakInWindow = std::max(peakInWindow, (float)ticks * ticksPerBlockInv);
        if (secondsSincePeakReset >= 2.0f) {
            secondsSincePeakReset = 0.0f;
            peakLoad = peakInWindow;
            peakInWindow = 0.0f;
        }
        activeEffect->SetCPUPeak(peakLoad);
        activeEffect->SetLevelsDb(linearToDb(avgInputPeak), linearToDb(avgOutputPeak));

        // Handle Display
        if (hardware.SupportsDisplay()) {
            if (useDebugTextScreen) {
                // Debug text screen hijacks the display to simply output text
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

        uint32_t currentTimeMs = System::GetNow();
        if (currentTimeMs - last_auto_save_check_time >= kAutoSaveCheckIntervalMs) {
            last_auto_save_check_time = currentTimeMs;

            for (int effectID = 0; effectID < availableEffectsCount; ++effectID) {
                AutoSaveEffectState &autoSaveState = autoSaveEffectStates[effectID];
                uint32_t changeCount = availableEffects[effectID]->GetParameterChangeCount();

                if (changeCount != autoSaveState.observedParameterChangeCount) {
                    autoSaveState.observedParameterChangeCount = changeCount;
                    autoSaveState.lastParameterChangeTime = currentTimeMs;
                    autoSaveState.pending = true;
                }

                if (!settings.globalAutoSave) {
                    autoSaveState.pending = false;
                } else if (autoSaveState.pending &&
                           currentTimeMs - autoSaveState.lastParameterChangeTime >= kAutoSaveDebounceMs) {
                    SaveEffectSettingsToPersitantStorageForEffectID(effectID, 0);
                    autoSaveState.pending = false;
                }
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
