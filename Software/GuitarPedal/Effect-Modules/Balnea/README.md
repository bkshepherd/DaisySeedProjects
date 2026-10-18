# Balnea (delay and pitch shifter)

Balnea is a digital delay and pitch shifter inspired by the CBA Thermae pedal. The delay lines are *read at a different speed than they are written*, which transposes the repeats; a
sequencer steps through up to three speeds, one per delay time (the base speed, the first interval, both intervals together), so the repeats climb or fall in pitch as they
recirculate. Add tap tempo, a reverse mode, tape wow and flutter, a self-oscillating hold and a small MIDI synth to play along with.

## Origin and licence

Adapted from the **Balnea** pedal of the **FunBox** project by GuitarML: <https://github.com/GuitarML/Funbox> (`software/Balnea/`), Copyright (c) 2024 Keith Bloemer, **MIT License**
([`LICENSE-FunBox-MIT.txt`](LICENSE-FunBox-MIT.txt)); like the rest of this repository it is MIT, so there is no licence change to opt into. The reverse delay line (`DelayLineReverse`) is by
Adam Fulford, Copyright 2021, MIT (the licence text is in the header of `tests/host/balnea/delayline_reverse_original.h`, the original file kept for a host test).

## Files

| File | |
|---|---|
| `../balnea_module.{h,cpp}` | The effect module: parameters, the delay lines, the sequencer, tap tempo |
| `balnea_dsp.h` | The tape wow/flutter modulator, the reverse delay line and the MIDI synth voices; free of hardware dependencies |

## Controls

| Control | Parameter | What it does (original control) |
|---|---|---|
| Knob 1 | Mix | Dry/wet, an energy-constant crossfade |
| Knob 2 | Tone | The low pass filter of the repeats, 100 Hz to 14 kHz |
| Knob 3 | Regen | Feedback |
| Knob 4 | Glide | How slowly the pitch slides from one interval to the next (none below 5%) |
| Knob 5 | Interval 1 | The first interval, 11 steps (the middle is no change) |
| Knob 6 | Interval 2 | The second interval, 11 steps; in the middle the third stage is skipped |
| Menu | Time | The delay time, 100 ms to 2 s. Tap tempo sets it: three taps, from the interval between the last two |
| Menu | Mod Rate, Mod Depth | The original's alternate functions of the interval knobs: the speed and the depth of the tape wow and flutter (depth 0 is off) |
| Menu | Base Stage, Int 1 Stage, Int 2 Stage | The original's three switches: how long the sequencer stays at each speed, as 1, 3/4 or 1/2 of the delay time |
| Menu | Scale | The intervals of the interval knobs: **Octaves** (octaves, fifths and fourths), **Dissonant** (sevenths, sixths, seconds) or **Chromatic** (semitones, -5 to +5) (the original's dip switches 3 and 4) |
| Menu | Reverse | Plays the repeats backwards (the original's dip switch 2) |
| Menu | Tap Sequencer | Every tap steps the sequencer to its next stage instead of tapping the tempo |
| Secondary footswitch | | Tap twice to set the delay time (the time between the two releases); keep tapping to change it. Hold for 300 ms: the feedback goes above 1 and the repeats build up (self-oscillation) until you let go. LED 2 blinks once per delay time (bright while self-oscillating) |
| MIDI notes | | A sawtooth synth (12 voices) mixed into the input, 1300 Hz low pass |

The stereo/mono dip switch of the original is the pedal's own mono or stereo input setting: with one input both delay lines get the same signal (and the wow differs left and right).

## Changes from the original

* The hardware code is gone: knobs, switches and dip switches are parameters, with the same MIDI CC numbers for the knobs (14 to 19); the Interval and Mod controls that shared knobs through the alternate mode are separate menu parameters.
* The delay time is a parameter ("Time") that tap tempo writes, so it is saved with the preset.
* The delay lines (16384 samples each) and the reverse lines (32768) are in SDRAM; the delay line classes are the repository's own, which keep their bookkeeping on chip. They are cleared in `Init()`.
* The wow and flutter noise is advanced once per 8 samples (it is very slow) instead of twice per sample; the time step is scaled to match.
* The reverse mode applies to both channels (the original set it on the left delay line only, because of a typing mistake).
* The reverse line has no modulo divisions, and no longer reads outside its buffer when the write position has just wrapped at the start of a crossfade (checked against the original in `tests/host/test_balnea.cpp`).
* The start-up state is deterministic (the original relied on zero-initialised globals).
* An output gain brings the default level to about +3 dB (see `tests/host/run_level_tests.sh`).
* The expression pedal control of Tone is not used.
