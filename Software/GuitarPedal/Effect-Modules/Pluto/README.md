# Pluto (dual looper)

Pluto is a wacky dual (or stereo) looper for endless experimentation. Each of the two loopers records up to a minute, plays it at a variable speed (smooth or stepped, forwards or backwards, up to 3x), and
they share one live effect that the two Mod controls shape: random variations in the playback speed, a low/high pass filter, or a stereo reverb. Loops are recorded at normal speed, so turning
the Speed control changes their pitch and tempo; at other speeds you hear the aliasing (and, here, the interpolation between samples) of reading a recording at a different rate.

## Origin and licence

Adapted from the **Pluto** pedal of the **FunBox** project by GuitarML: <https://github.com/GuitarML/Funbox> (`software/Pluto/`), Copyright (c) 2024 Keith Bloemer, **MIT License**
([`LICENSE-FunBox-MIT.txt`](LICENSE-FunBox-MIT.txt)); like the rest of this repository it is MIT, so there is no licence change to opt into. The looper (`../../Util/var_speed_looper.h`) is the
`varSpeedLooper` of that project, a modified version of DaisySP's Looper (MIT, Electrosmith); it is shared with Cassini. The reverb is DaisySP's `ReverbSc` (LGPL, already used by the Reverb module).

## Files

| File | |
|---|---|
| `../pluto_module.{h,cpp}` | The effect module: parameters, the two loopers, the speed, the effects, footswitch handling and the LED |
| `../../Util/var_speed_looper.h` | The multimode looper with variable speed (shared with Cassini) |

## Controls

| Control | Parameter | What it does (original control) |
|---|---|---|
| Knob 1 | Level A | Looper A's volume (0.5 is unity) |
| Knob 2 | Mod A | Looper A's effect control: Stability (speed variations), Filter (low pass to the left, high pass to the right) or Reverb (decay time) |
| Knob 3 | Level B | Looper B's volume |
| Knob 4 | Speed A | Looper A's speed and direction: -2x (reverse) to +2x with 1x at noon; the stepped mode has fixed steps from -2x to 3x |
| Knob 5 | Mod B | Looper B's effect control (for the Reverb: the damping) |
| Knob 6 | Speed B | Looper B's speed and direction |
| Menu / CC 21 | Speed Mode | Smooth (changes glide like a tape reel) or Stepped (the original's third position did nothing and is left out) |
| Menu / CC 22 | Effect | Stability, Filter or Reverb (stereo: A and B are the left and right send) |
| Menu / CC 23 | Loop Mode | Normal (overdub for as long as it records), One-time (records one pass and stops) or Fripp (frippertronics: records endlessly, every pass decays, like a tape delay) |
| Menu | Footswitch | Which looper(s) the secondary footswitch controls: A + B, A, or B |
| Secondary footswitch | | Tap: start recording, tap again: stop and loop (and overdub with the next tap). A second tap within a second pauses or resumes playback. Hold for a second: clear. The LED pulses while recording, blinks while paused and is lit while a loop plays |

With one input (mono) both loopers record it and both loops play on both channels; with two (stereo) looper A is the left channel and looper B the right.

## Changes from the original

* The hardware code is gone; the switches are menu parameters (and MIDI CC 21 to 23), knobs and CCs 14 to 19 as before.
* The original ran at 96 kHz (twice the loop memory, 23 MB for each looper); the pedal runs at 48 kHz, so each looper holds a minute in 11.5 MB of SDRAM (23 MB for both). The reverb is the ordinary `ReverbSc` at 48 kHz, so the modified `reverbsc96` is not needed. The playback now interpolates linearly between samples to make up for the lower sample rate, and the smoothing of the speed control is scaled to the same time.
* There is one footswitch for the loops (the original had one for each looper, or one for both in the stereo mode): it controls A + B, A or B, as chosen in the menu.
* The first recording starts playing at the start of the loop when it is stopped (the original read one sample past its end first, which relied on the buffer having been cleared); the loop memory is not cleared at startup, which would take a noticeable time with 23 MB (only what has been recorded is ever played).
* Looper B's high pass filter used looper A's filter in the original (a mix-up of the names); it has its own here.
