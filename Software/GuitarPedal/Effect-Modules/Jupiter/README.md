# Jupiter (stereo reverb)

Jupiter is a stereo reverb on the CloudSeed engine, with controls for the decay, a low shelf and a high shelf after the reverb (frequency and gain), and the
dry/wet mix, plus three-way "lush", modulation and pre-delay settings.

## Origin and licence

Adapted from the **Jupiter** pedal of the **FunBox** project by GuitarML: <https://github.com/GuitarML/Funbox> (`software/Jupiter/jupiter/jupiter.cpp`),
Copyright (c) 2024 Keith Bloemer, **MIT License** ([`LICENSE-FunBox-MIT.txt`](LICENSE-FunBox-MIT.txt)). Like the rest of this repository it is MIT, so there is no
licence change to opt into. The reverb is the **CloudSeed** engine by Valdemar Erlingsson (<https://github.com/ValdemarOrn/CloudSeed>, MIT), in the Daisy port that is
already in this repository (`dependencies/CloudSeed`, used by the CloudSeed module); Jupiter only adds a different control mapping on top of it.

## Files

| File | |
|---|---|
| `../jupiter_module.{h,cpp}` | The effect module: parameters, the mapping to the engine's parameters, the dry/wet mix |
| `../cloudseed_pool.cpp` | The engine's delay line memory (SDRAM), shared with the CloudSeed module |

## Controls

| Control | Parameter | What it does (original control) |
|---|---|---|
| Knob 1 | Decay | Decay time of the reverb, about 0.3 s to 60 s with an exponential taper (about 2 s in the middle) |
| Knob 2 | Mix | Dry/wet mix (a cheap, mostly energy-constant crossfade) |
| Knob 3 | Low Gain | Low shelf cut: fully cut at 0, no cut at 1 |
| Knob 4 | Low Freq | Where the low cut begins, 20 Hz to 1 kHz (about 200 Hz in the middle). Only audible when Low Gain is below 1 |
| Knob 5 | High Gain | High shelf cut: strongly cut at 0, no cut at 1 |
| Knob 6 | High Freq | Where the high cut begins, 400 Hz to 20 kHz (about 4.3 kHz in the middle). Only audible when High Gain is below 1 |
| Menu | Lush | Low / Medium / High: how many reverb lines and how much late diffusion (low = 1 line without late diffusion, high = 2 lines with the most) |
| Menu | Modulation | Low / Medium / High: amount and rate of the delay line modulation |
| Menu | PreDelay | Off / 100 ms / 200 ms |
| Menu | Stereo In | Off = mono in (the left input feeds both sides, the original's MISO mode); on = true stereo |

## Changes from the original

* **Controls.** The original's footswitch preset, expression pedal, and per-knob MIDI control are replaced by this project's own (knob mapping, MIDI CC 14-23); the three-way
  and dip switches became menu items. The knobs are ordered Decay, Mix, then each shelf's gain and frequency (the original orders them Decay, Low Freq, High Freq, Mix, Low Gain, High Gain). The default settings are mine (the pedal's switches have none): Decay 0.5, Mix 0.4, Low Gain 0.7, Low/High Freq 0.5, High Gain 0.5 (a mild cut, so the frequency knobs are audible), Lush
  Medium, Modulation Medium, PreDelay Off, mono in.
* **Parameter updates are spread over audio blocks.** Some of the engine's parameters recompute filters and delay lengths. The original called them from the audio callback
  whenever a knob moved; here a changed control is only flagged and one flagged control is pushed to the engine per block (every 48 samples at most), so a quick knob
  movement cannot land as one heavy block. (The CloudSeed module throttles in a different way, by ignoring most calls.)
* **The engine** is this repository's CloudSeed code (engine block size 4), not the copy in the FunBox project (block size 1).
* **Level.** An output gain of 1.33 makes the effect about +2 dB louder than bypass at its defaults, as for the other effects.
* **Memory.** One reverb instance takes about 4.9 MB of the pool in `cloudseed_pool.cpp` (12.6 MB, enough for Jupiter and the CloudSeed module together).
