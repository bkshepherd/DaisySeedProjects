# Uranus (granular delay and synth)

Uranus cuts the input into small grains and plays them back transposed, at a chosen speed, in a stereo field: half a second of audio (with feedback) is the source for two grain
players. With a MIDI keyboard the same grains become a monophonic **granular synth** (an ADSR over the grains, transposed from the key relative to middle C), or the input is replaced by a
polyphonic two-operator **FM synth**.

## Origin and licence

Adapted from the **Uranus** pedal of the **FunBox** project by GuitarML: <https://github.com/GuitarML/Funbox> (`software/Uranus/uranus.cpp`), Copyright (c) 2024 Keith Bloemer, **MIT License**
([`LICENSE-FunBox-MIT.txt`](LICENSE-FunBox-MIT.txt)); like the rest of this repository it is MIT, so there is no licence change to opt into. The grain player is a modified version of DaisySP's
`GranularPlayer` (MIT) and was already in this repository (`Util/granularplayermod.*`, used by the Granular Delay module); the FM voices (`Util/fm_voices.h`, `Util/operator.*`) were already here too
(the FM Keys module), and were moved to their own header so that both modules can use them.

## Files

| File | |
|---|---|
| `../uranus_module.{h,cpp}` | The effect module: parameters, the buffer and the grain players, the synth modes, MIDI notes |
| `../../Util/granularplayermod.*` | The grain player (shared with Granular Delay) |
| `../../Util/fm_voices.h`, `../../Util/operator.*` | The FM voices and operators (shared with FM Keys) |

## Controls

| Control | Parameter | What it does (original control) |
|---|---|---|
| Knob 1 | Size | Grain size, 1 ms to 300 ms |
| Knob 2 | Mix | Dry/wet mix |
| Knob 3 | Pitch | Transposition of the grains, -13 to +13 semitones in steps of one (none in the middle) |
| Knob 4 | Feedback | Feedback of the buffer the grains are cut from |
| Knob 5 | Width | How far back in time (0 to 50 ms) grains can start. Low: a concise sound; high: the grains sample a wider range of the audio |
| Knob 6 | Speed | Playback speed of the grains, -2x to +2x (normal at 0.75; stopped in the middle; backwards below) |
| Menu | Grain Mode | Oct Up: the two grain players play the unchanged pitch and an octave up / Oct Down: unchanged and an octave down / Up+Down: an octave down and an octave up |
| Menu | Grain Env | The envelope of every grain: Cos (smooth) / SlowAtk (slow attack, fast decay) / FastAtk (fast attack) |
| Menu | Synth Mode | Off: audio in only / Granular: a MIDI key plays the grains through an ADSR, transposed from middle C (60) / FM: the input is replaced by a polyphonic FM synth (12 voices) |
| Menu | Hold | Stops writing new audio into the buffer: the grains keep playing what is there. The secondary footswitch toggles it, and LED 2 shows it |
| Menu | Spread | The original's alternate function of the Mix knob: the stereo spread of every grain |
| Menu | LFO Depth, LFO Rate | The alternate functions of the Pitch and Speed knobs: a pitch LFO (depth 0 to 1 semitone, rate 0 to 6 Hz) |
| Menu / MIDI | Car Level, Car Ratio, Mod Level, Mod Ratio, Mod A/D/S/R | FM synth controls (MIDI CC 20 to 27), ratios 0.5 to 16.5 in steps of 0.5 |
| Menu / MIDI | Env Attack, Decay, Sustain, Release | The granular synth's ADSR and the FM carrier's (MIDI CC 28 to 31) |

MIDI: the knobs are CC 14 to 19, the synth controls CC 20 to 31 (as on the pedal). Notes go to the active effect on the pedal's MIDI channel (Global Settings); a note on with velocity 0 is a note off.

## Changes from the original

* **Alternate mode** (hold the left footswitch for Spread, LFO Depth and LFO Rate) is replaced by menu items; the three-way switches became menu items. The defaults are mine: Size 0.3 (about 90 ms), Mix 0.5, Pitch 0 semitones,
  Feedback 0.4, Width 0.5 (25 ms), Speed 0.75 (normal), Spread 0.4, LFO off, Grain Mode Oct Up, Grain Env Cos, Synth Off, and the FM and envelope controls at the original's startup values.
* **The pitch LFO is symmetric.** The original runs a sine of amplitude 2 and subtracts 1, so the modulation went from three times the depth down to once the depth up; here it is plus and minus the depth.
* **Where the audio is.** The original keeps the half second of audio in ordinary RAM; here it is in the on-chip RAM_D2 (`.sram_d2_bss`), since the grain players read it at random places, which is much too slow in SDRAM.
  (The Granular Delay module keeps its buffer in SDRAM and notes that it can only run one grain player.)
* **Controls are read once per audio block** instead of for every sample.
* **Level.** An output gain of 1.75 makes the effect about +2 dB louder than bypass at its defaults.
* The FM synth's note handling, the 12 voices, the 0.05 gain on its output and the granular synth's gain of 3 are as in the original.

## Notes

* MIDI notes are handled in the main loop while the audio runs (as on the original), so a note can arrive between two samples of a block.
* **Tested on the pedal: the audio side only (it boots and runs as the only effect in the build). The MIDI input (notes in the granular and FM synth modes, the CCs, the MIDI channel setting) has NOT been tried on
  hardware yet, for lack of a MIDI connector: it is covered by the host tests only (`tests/host/test_uranus.cpp` calls `OnNoteOn`/`OnNoteOff` and `MidiCCValueNotification` directly). It needs testing with a keyboard.**
