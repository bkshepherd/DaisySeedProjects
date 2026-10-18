# The second footswitch and the left LED, effect by effect

A review of how every effect uses the second (left) footswitch and the second (left) LED. It was made on 18 October 2026 by reading the source; apart from the Reverb Delay tempo LED (tested on the pedal after it was fixed) nothing here has been checked on hardware. It is meant as a starting point for later examination: the last section lists things that look inconsistent or unfinished.

## How it works

The code is in `guitar_pedal.cpp` (the footswitch handling and the LEDs) and `Effect-Modules/base_effect_module.h` (what an effect can override).

* **The right footswitch** switches the effect on and off. The **second footswitch** is only used on the variants that have one (`has_alternate_footswitch`).
* **The two LEDs.** LED 0 (right) shows whether the effect is on. LED 1 (left) is up to the effect: `GetBrightnessForLED(1)` is read once per audio block (LED 0 is read too, so the function is called twice per block). The default in the base class returns 1 while the effect is on and 0 otherwise, for both LEDs. Most effects start from that value and multiply it by something. **Because the function is called only twice per block, an effect must not advance any state inside it** (this was the cause of the slow Reverb Delay blink: its LED oscillator was stepped in the getter; it is now stepped with the audio like the Delay module's).
* **What an effect can override** for the second footswitch:
  * `AlternateFootswitchPressed()` and `AlternateFootswitchReleased()`: called on the press and on the release, **only while the effect is on**.
  * `AlternateFootswitchHeldFor1Second()`: called once when the switch has been held for one second, only while the effect is on.
  * `AlternateFootswitchForTempo()`: the default is true. Three taps (each within 2 seconds of the one before; every further tap of the series adjusts it again) then set the global tempo (`globalTempoBPM`) from the interval between the last two taps (the series logic is in `Util/tap_tempo.h`) and the main loop calls `SetTempo(bpm)` on the active effect. Effects that return false (loopers, momentary effects) get no tap tempo. An effect that returns true but does not override `SetTempo` simply ignores the taps.
* **Fixed behaviours** that an effect cannot change: both footswitches held for about 2 seconds saves the settings; the second footswitch held while turning the encoder changes effect.
* **Chains** (`effect_chain.cpp`): the press and release go to the primary slot, or toggle slots in the "toggle" mode; a 1-second hold switches the second knob bank when the chain maps knobs 7-12 (the display shows SHIFT).

## Effects that use the second footswitch for tap tempo

Three taps set the tempo (from the last two taps; see above). The first column says what the tempo sets.

| Effect | Tap tempo sets | Left LED | Also |
|---|---|---|---|
| Delay | delay time | blinks, one blink per two delay times | a 1-second hold toggles **Shift**: knobs 1-6 are remapped to the modulation parameters, the display shows SHIFT |
| Tape Delay | delay time (ignored in lo-fi mode) | blinks, one blink per two delay times | a 1-second hold toggles Shift, as in Delay |
| Reverb Delay | delay time | blinks, one blink per two delay times | |
| Dual Echo | time | blinks at the tempo | a hold of 1 second or more is a momentary **Hold**, which ends on release; the LED is steady while held |
| AutoPan | pan rate | follows the pan | |
| Chopper | chop rate | follows the chopping (the smoothed chopper output) | |
| Harmonic Tremolo | speed | blinks once per LFO cycle (once per beat after a tempo tap), around the LFO peak, whatever the depth | |
| Modulated Tremolo | rate | on (see "inconsistencies") | |
| Drum | BPM | the tap magnitude of the drum pattern (`tap_mag`) | |
| Metro | BPM | shows the beat | |
| Multi Delay | nothing: `SetTempo` is empty | on | |

## Effects where the second footswitch does something else

None of these use tap tempo: they all return false from `AlternateFootswitchForTempo()`.

| Effect | Footswitch | Left LED |
|---|---|---|
| Looper | press = record / play / overdub (`TrigRecord`), 1-second hold = clear | on while recording |
| Cassini | the Footswitch parameter picks the mode. *Sample*: hold to record. *Looper*: tap to record, tap again to loop, double-tap to pause, hold to clear | brightness pulses while recording, blinks while paused, steady when a loop exists |
| Pluto | like the Cassini looper mode, for its two loops; a second tap within a second pauses or resumes; a hold clears | as in Cassini |
| CloudSeed | press toggles "wet only": the dry input is muted and the Mix knob becomes the Dry volume knob | flashes while wet-only is on, otherwise on |
| Crusher | press toggles pitch tracking | on while tracking |
| Granular Delay | press toggles Hold | on while held |
| Uranus | press toggles Hold | on while held |
| Venus | momentary freeze while pressed | on while frozen (also when the Freeze parameter is on) |
| Earth | the FS Action parameter selects a momentary action: freeze the reverb, swell the overdrive, or octave on | on while pressed |
| Pitch Shifter | momentary or latching shift, depending on the Mode parameter (press shifts, release returns) | on (the effect has no LED function of its own) |
| Balnea | taps set the Time (the first two taps only start the count; the third and later set it from the interval). In Tap Seq mode each tap advances the stage. Holding for a time gives self-oscillation, which ends on release | blinks with the delay; brighter during self-oscillation |

## Effects that only use the left LED

| Effect | Left LED shows |
|---|---|
| NAM, IR | the output level (the absolute value of the last sample) |
| Chorus | the Wet parameter (a steady brightness, not a blink) |
| Flanger | the Mix parameter |
| Overdrive | the Drive parameter |
| Phaser | the sweep position (0.2 to 1.0) |
| Duck Compressor | the gain reduction (full at 12 dB) |
| Noise Gate | 1 while the gate is open, 0 while closed; it does not depend on whether the effect is on |

## Effects with nothing special

The second footswitch does nothing and the left LED is simply on while the effect is on: Compressor, Distortion, Reverb, Graphic EQ, Parametric EQ, Tuner, Scope, Test Tone, Mars, Mercury. Jupiter, Neptune, Saturn and Dattorro are the same, and they also switch tap tempo off explicitly. (Mars and Mercury leave tap tempo on, but without a `SetTempo` the taps have no effect.)

Several more effects have a left-LED function that, in practice, returns the same as the default (see the next section): Modulated Tremolo, SciFi, Spectral Delay, PolyOctave, Pluck Echo, String Keys, MIDI Keys, Modal Keys, FM Keys, Multi Delay and CloudSeed outside wet-only mode.

## Things that look inconsistent or unfinished (for later examination)

1. **A level meter that never moves.** Eleven modules multiply the left LED by `m_cachedEffectMagnitudeValue`, which only NAM, IR, Drum and Chopper ever update. In Modulated Tremolo, SciFi, Spectral Delay, PolyOctave, Pluck Echo, String Keys, MIDI Keys, Modal Keys, FM Keys, Multi Delay and CloudSeed the constructor sets it to 1.0 and nothing changes it, so the LED is just on. It looks like the intention was an output-level or activity LED. Either implement it (as NAM and IR do) or remove the dead variable.
2. **The tempo LED blinks once every two delay times, and is on for about 27% of that.** Delay, Tape Delay and Reverb Delay all do this (the oscillator runs at half the delay frequency). It was kept when the Reverb Delay blink was fixed. Whether one blink per delay time would be more natural is open. (Harmonic Tremolo, which used to dim and brighten with the LFO, now blinks once per LFO cycle, which is once per beat, and is on for the same 27%.)
3. **Taps that do nothing.** Tap tempo is on by default, so Multi Delay (empty `SetTempo`), Mars and Mercury accept the taps and ignore them. Tape Delay ignores it in lo-fi mode. Returning false from `AlternateFootswitchForTempo()` would make that explicit.
4. **The LED does not follow the on/off state in two effects.** Noise Gate returns its gate state, and Metro returns its beat position, without multiplying by the base value, so the LED can light while the effect is off. Every other effect uses the base value.
5. **The left LED of the Reverb function does nothing.** `ReverbModule::GetBrightnessForLED` returns the base value in both branches. Compressor and Distortion override the function and also just return the base value.
6. **Shift only in two effects.** Delay and Tape Delay toggle a Shift mode with a 1-second hold; other effects with many parameters (Reverb Delay, Multi Delay, Spectral Delay) have nothing like it. Chains have a shift bank.
7. **CloudSeed's flash is very slow.** In wet-only mode the LED function counts its own calls in a `static` counter and flashes every 10,000 calls (half a period). It is called once per audio block for LED 1, and the pedal runs 1,000 blocks per second, so the LED stays on for about 10 seconds and then off for about 10 seconds. It was probably written when the function was called per sample. A time-based flash (or counting samples, as the other effects do) would fix it.
8. **Callbacks only fire while the effect is on.** A press of the second footswitch while the effect is bypassed does nothing (not even a Hold toggle), which is intended but not obvious to a user.
9. **Pitch Shifter and Looper-type effects** use the press and the release as separate events; a release that arrives after the effect was switched off is not delivered, so a momentary action could in principle stay latched (not checked).
