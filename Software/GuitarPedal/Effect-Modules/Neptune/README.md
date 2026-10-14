# Neptune (stereo reverb and delay)

Neptune is a stereo reverb and delay: the CloudSeed reverb in three modes (Chorus, Medium Space, RubiKa) and a delay of up to 4 seconds with normal, octave and reverse
modes, with three ways of routing the two together.

## Origin and licence

Adapted from the **Neptune** pedal of the **FunBox** project by GuitarML: <https://github.com/GuitarML/Funbox> (`software/Neptune/neptune/neptune.cpp`), Copyright (c) 2024
Keith Bloemer, **MIT License** ([`LICENSE-FunBox-MIT.txt`](LICENSE-FunBox-MIT.txt)); like the rest of this repository it is MIT, so there is no licence change to opt into. The reverb is the
**CloudSeed** engine by Valdemar Erlingsson (<https://github.com/ValdemarOrn/CloudSeed>, MIT), in the Daisy port that is already in this repository (`dependencies/CloudSeed`, also used by
the CloudSeed and Jupiter modules). The octave delay line is DaisySP's `DelayLine` (MIT, by shensley) with an octave mode, and the **reverse delay line is from the Veno-Echo pedal by Adam Fulford**
(<https://github.com/AdamFulford/Veno-Echo>, MIT, Copyright 2021 Adam Fulford; its licence text is in the header of `delayline_reverse.h`).

## Files

| File | |
|---|---|
| `../neptune_module.{h,cpp}` | The effect module: parameters, the reverb modes, the routing, the dry/wet and reverb/delay mix |
| `delayline_oct.h` | The delay line with an octave mode |
| `delayline_reverse.h` | The reverse delay line |
| `../cloudseed_pool.cpp` | The engine's delay line memory (SDRAM), shared with the CloudSeed and Jupiter modules |

## Controls

| Control | Parameter | What it does (original control) |
|---|---|---|
| Knob 1 | Decay | Decay time of the reverb |
| Knob 2 | Mix | Dry/effect mix |
| Knob 3 | Dly Time | Delay time, 50 ms to 4 s. The delay is off at the bottom of the knob (its repeats die out) |
| Knob 4 | Mod | Amount of modulation of the reverb |
| Knob 5 | LowPass | Low-pass on the reverb (cubic taper, as the original's knob) |
| Knob 6 | Dly Fdbk | Delay feedback |
| Menu | Rev Mode | Chorus / Medium Space / RubiKa (CloudSeed's factory programs). Changing it mutes the output for 21 ms while the reverb is cleared and loaded |
| Menu | Routing | Send: the delay also feeds the reverb (and is heard) / Parallel: reverb and delay side by side / Series: the delay only goes into the reverb (only the reverb is heard) |
| Menu | Dly Mode | Normal / Octave / Reverse |
| Menu | Stereo In | Off: mono in (the left input feeds both sides, the original's MISO mode); on: true stereo |
| Menu | PreDelay | The original's alternate function of the Decay knob: reverb pre-delay |
| Menu | Mod Rate | The alternate function of the Mod knob: modulation rate |
| Menu | Rev/Delay | The alternate function of the Mix knob: the balance between the reverb and the delay |

## Changes from the original

* **Alternate mode.** The original reaches PreDelay, Mod Rate and the Rev/Delay balance by holding the left footswitch (the same knobs then control them); here they are menu items. MIDI CC 14-26.
  The defaults are mine (the pedal's switches have none): Decay 0.5, Mix 0.4, Dly Time 0.3, Mod 0.3, LowPass 0.8, Dly Fdbk 0.3, Medium Space, Parallel, Normal, mono in, PreDelay 0, Mod Rate 0.2, Rev/Delay 0.5.
* **Parameter updates are spread over audio blocks**, like Jupiter: a changed reverb control is flagged and one is pushed to the engine per 48 samples, so a quick knob movement cannot land as one heavy block.
* **The reverb presets** are this repository's CloudSeed presets with the original's output stage (a higher main output, and a higher early output in Medium Space), because the original's CloudSeed copy differs there.
* **A delay line per channel for the low-pass.** The original filters both channels' delay output through one filter (it is shared, and the two channels' samples go through it alternately).
* **The reverse delay is only read in reverse mode** (the original also computes it, with two `sinf`, for every sample in the other modes). The line is still written all the time, so switching to reverse
  works at once. The jump of the read pointer wraps around where the original read outside the buffer when the write pointer was at 0, and the delay starts at its setting instead of gliding up from 0
  (which made the reverse line divide by zero).
* **Unused code left out:** the second delay tap, the dual (normal plus reverse) delay and the per-channel stereo levels, which nothing in the pedal switches on.
* **The wet gain is applied twice**, as in the original (the balanced reverb and delay are multiplied by the wet gain and then again when added to the dry): at half Mix the effect is 3 dB quieter than the dry signal.
  This is kept for the sound; removing the second factor would make the wet signal louder.
* **Memory.** One reverb instance takes about 4.9 MB of the pool in `cloudseed_pool.cpp` (now 19.7 MB, enough for CloudSeed, Jupiter and Neptune together), and the delay lines 4.6 MB of SDRAM
  (two 4 s octave lines and two 8 s reverse lines). The delay lines' bookkeeping is in the fast RAM. The delay times are in samples, so the effect needs a sample rate of 48 kHz.
* **Level.** An output gain of 1.38 makes the effect about +2 dB louder than bypass at its defaults.
