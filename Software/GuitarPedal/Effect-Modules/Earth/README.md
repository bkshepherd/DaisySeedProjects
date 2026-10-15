# Earth (plate reverb and octave) - GPL-3.0-or-later

Earth is a Dattorro plate reverb with an optional octave effect in front of it and a momentary action on the secondary footswitch (freeze the reverb, swell an
overdrive on it, or switch the octave on). Mono in, stereo out.

**It is licensed under the GNU General Public License v3 (or later), not the MIT licence the rest of this repository uses**, because it is adapted from the Earth
pedal of the FunBox project, which is itself GPLv3, and links the GPLv3 Dattorro engine in `../Dattorro/`. The licence text is in
[`LICENSE-GPLv3.txt`](LICENSE-GPLv3.txt). Like the Dattorro reverb it is not compiled in by default: the lines for it sit in the same opt-in block as Dattorro in
the `Makefile` and `loaded_effects.h`. **A firmware binary built with it must be distributed under GPLv3, or not distributed at all.** Earth needs the Dattorro engine
(`Dattorro/Dattorro.cpp`), so enable them together.

## Origin and acknowledgements

* Adapted from **Earth** in the FunBox project by GuitarML, <https://github.com/GuitarML/Funbox> (`software/Earth/earth.cpp`), Copyright (c) 2024 Keith Bloemer,
  GPL-3.0-or-later. (FunBox as a whole is MIT; Earth was split out into its own GPL repository to keep the licence of the reused code.)
* The reverb is a modified version of the Dattorro implementation in **VCV Rack Plateau** by Valley Audio Soft / Dale Johnson
  (<https://github.com/ValleyAudio/ValleyRackFree/tree/main/src/Plateau>), GPL-3.0-or-later; see `../Dattorro/README.md` for the provenance of the engine used here.
* The octave effect is from **Terrarium-Poly-Octave** (<https://github.com/schult/terrarium-poly-octave>); it is the same code as this repository's PolyOctave module.

## Controls

| Control | Parameter | Original control |
|---|---|---|
| Knob 1 | PreDelay | Pre-delay, 0 to 700 ms |
| Knob 2 | Mix | Dry/wet mix |
| Knob 3 | Decay | Reverb decay |
| Knob 4 | Mod Depth | Intensity of the tank modulation |
| Knob 5 | Mod Speed | Rate of the tank modulation |
| Knob 6 | Filter | A low-pass on the reverb input: 0 is open, 1 is dark (the original's README says "high cut left, low cut right", but its code only does the low-pass) |
| Menu | Size | Small / Medium / Big (3-way switch 1) |
| Menu | Octave | None / Up / Up+Down (3-way switch 2) |
| Menu | FS Action | What the secondary footswitch does while held: Freeze / Overdrive / Octave (3-way switch 3) |
| Menu | In Diffuse | The input diffusion (the original's dip switch 1, which switched it *off*) |
| Menu | Oct Only | With an octave on, send only the octave to the reverb, not the dry signal (dip switch 2). "Up+Down" always includes the dry signal |
| Secondary footswitch | | Momentary action per FS Action; LED 2 lights while it is held |

## Changes from the original

* **Hardware.** The original's footswitch, LED, expression-pedal and MIDI handling are replaced by this project's (knob mapping, MIDI CC 14-24, the secondary footswitch hooks,
  bypass in the main program). The three-way and dip switches became menu items. The expression pedal is not supported.
* **Engine.** It uses this repository's vendored Dattorro engine (`../Dattorro/`), which already has fixes over the engine in the FunBox repository (its delay lines come from an
  arena instead of a fixed table of 13 buffers, its sample-rate handling is fixed). One change was needed for Earth: the engine's constructor takes the pre-delay buffer length
  (default unchanged at 200 ms), and Earth asks for 700 ms. The reverb's delay lines use about 1 MB of a 2 MB SDRAM arena.
* **Room sizes.** Earth's engine never wires up its maximum sample rate, so at 48 kHz its tank runs with 32 kHz time scales, about two thirds as long as designed (and the
  Size switch positions were chosen by ear against that). The fixed engine would sound about 1.5 times bigger at the same positions, so Size and the modulation depth are
  scaled by 32000 / sample rate. `tests/host/compare_earth_engine.sh` checks this against Earth's own engine on the PC: the engines agree exactly at 32 kHz, and with the scaling the
  tank's first echo at 48 kHz lands on the same sample as Earth's. The input diffusion network (not covered by Size) stays at the true 48 kHz scale, about 1.5 times longer than the
  original's, so the early reflections are a little smoother. The modulation speed was not scaled.
* **Pre-delay.** The knob covers 0 to 700 ms across its whole travel (in the original the top 30% of the knob was past the end of the buffer and did nothing).
* **Parameter smoothing** (pre-delay, modulation, decay, the freeze and the overdrive swell) still moves one step per sample, but the engine is updated once per audio block.
* **Octave.** The octave runs only while an octave mode is selected (the original ran it all the time and ignored the result). Its state belongs to the module.
* **Level.** An output gain of 1.4 (+2.9 dB) makes the effect about +2 dB louder than bypass at its default settings (it was about 1 dB quieter without it), as for the other effects.

## Notes

* The latency of the octave sound into the reverb is 6 samples (the octave works in chunks of 6).
* A loud sustained input with the octave on can drive the output past full scale: the octave up and down plus the dry signal add up, and the original has the same behaviour.
* Defaults differ from the pedal because the pedal's switches have no defaults: Octave is None, FS Action is Freeze, Size is Medium.
