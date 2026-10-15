# Cassini (chromatic sampler)

Cassini is a chromatic sampler to play with a MIDI keyboard. Hold the footswitch to sample the input (up to 20 seconds, into one of three banks); every key plays the sample at its own pitch (middle C is
the unchanged pitch) through an envelope, a low/high pass filter and a reverb. A looper can record and loop what you play, at a variable speed (also backwards).

## Origin and licence

Adapted from the **Cassini** pedal of the **FunBox** project by GuitarML: <https://github.com/GuitarML/Funbox> (`software/Cassini/`), Copyright (c) 2024 Keith Bloemer, **MIT License**
([`LICENSE-FunBox-MIT.txt`](LICENSE-FunBox-MIT.txt)); like the rest of this repository it is MIT, so there is no licence change to opt into. The looper (`../../Util/var_speed_looper.h`) is shared
with Pluto. The reverb is DaisySP's `ReverbSc` (LGPL).

## Controls

| Control | Parameter | What it does (original control) |
|---|---|---|
| Knob 1 | Trim Start | Where the sample starts, as a part of its length |
| Knob 2 | Trim End | Where it ends; before the start, the sample plays backwards |
| Knob 3 | Level | The keys' volume (0.5 is 1.25x) |
| Knob 4 | Loop Speed | The looper's speed and direction: -2x (reverse) to +2x with 1x at noon |
| Knob 5 | Filter | Low pass to the left of the middle, high pass to the right |
| Knob 6 | Reverb | Decay time of the reverb on the keys |
| Menu | Speed Mode | Smooth or Stepped (-2x to 3x in steps) looper speed |
| Menu | Sample Mode | One-shot (a key plays the sample once) or Looped (it repeats while the key is down) |
| Menu | Bank | Which of the three samples is recorded and played |
| Menu | Footswitch | What the secondary footswitch does. **Sample**: hold to record the sample of the bank. **Looper**: tap to start recording the loop, tap again to loop, tap twice within a second to pause, hold for a second to clear |
| MIDI | | Notes play the sample (12 voices); CCs 14 to 19 set the knobs |

The LED shows the recording of the sample or the state of the looper (pulsing while recording, blinking while paused, lit while a loop plays).

## Memory

The three samples take 11.5 MB of SDRAM and the looper (a minute) another 11.5 MB. None of it is cleared at startup (only what has been recorded is played).

## Changes from the original

* The hardware code is gone; the three switches are menu parameters and the two footswitches share one (see Footswitch above), because the pedal framework has one secondary footswitch.
* Backwards playback of the sample ends when the position reaches 1 (the original compared an index below 1, which the position rarely hits exactly, and then stayed silent at the start for good).
* The looper starts playing at the start of the loop when the first recording is stopped (see Pluto).
* The sample banks and the looper are not cleared at startup.
