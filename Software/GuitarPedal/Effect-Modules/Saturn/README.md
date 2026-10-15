# Saturn (spectral delay)

Saturn is a spectral delay. A short-time Fourier transform (STFT) splits the signal into frequency bins and every bin has its own delay line with feedback, so the delay time
can grow with frequency, follow a sine across the frequencies, or be random for every bin. Plus a pre-delay (with optional feedback), a low-pass / high-pass that works on the
bins, slow "drift" of the delay time and feedback, a mono mode that doubles the number of bins, true stereo with two transforms, and a random per-bin level.

## Origin and licence

Adapted from the **Saturn** pedal of the **FunBox** project by GuitarML: <https://github.com/GuitarML/Funbox> (`software/Saturn/saturn.cpp`), Copyright (c) 2024 Keith Bloemer,
**MIT License** ([`LICENSE-FunBox-MIT.txt`](LICENSE-FunBox-MIT.txt)); like the rest of this repository it is MIT, so there is no licence change to opt into. The STFT code
adapts DaisySTFT (<https://github.com/amcerbu/DaisySTFT>), which uses `shy_fft` by Émilie Gillet of Mutable Instruments; both are in `Util/STFT/` (also used by Venus and the Spectral Delay).
This repository already has a simpler "Spectral Delay" module derived from an early version of this effect (256-point transform, 120 bins, no pre-delay, filter, drift or stereo);
Saturn is the full effect.

## Files

| File | |
|---|---|
| `../saturn_module.{h,cpp}` | The effect module: parameters, the delay lines and the per-bin processing |
| `../../Util/STFT/sliced_fourier.h` | An STFT that spreads each frame's work over several audio blocks (shared with Venus) |
| `../../Util/STFT/cmsis_fft.h` | The real FFT, computed by CMSIS-DSP (shared with Venus) |

## Controls

| Control | Parameter | What it does (original control) |
|---|---|---|
| Knob 1 | PreDelay | 50 ms to 1 s of delay before the spectral delay |
| Knob 2 | Mix | Dry/wet mix |
| Knob 3 | Filter | Low-pass to the left of the middle (only the lower bins), high-pass to the right, nothing in the middle |
| Knob 4 | Time | 0 to 4 seconds of delay spread over the bins; what exactly depends on Time Mode |
| Knob 5 | Feedback | Amount of feedback in the delay lines, depending on Fdbk Mode |
| Knob 6 | DriftRate | Speed of the drift oscillator, 0 to 1 Hz |
| Menu | Time Mode | Linear: the delay grows with frequency (Time sets the slope) / Sine: a sine wave of delays across the bins / Random: a random delay for every bin |
| Menu | Fdbk Mode | Same: the same feedback for every bin / Linear: less feedback towards high frequencies / Random |
| Menu | Drift | Delay: slowly modulates the delay time and the feedback / Off / +PreDelay: the pre-delay too (this warps the pitch) |
| Menu | Mono | Off: stereo (two transforms, 175 bins each). On: one transform with 350 bins, the same signal on both outputs |
| Menu | Stereo In | Off: mono in (the left input feeds both sides); on: true stereo. Ignored in mono mode |
| Menu | St Spread | A random level for every bin (see the notes) |
| Menu | PreFdbk | Feedback of 0.75 on the pre-delay |

Only the lower 175 bins (up to 8.2 kHz, 47 Hz each) are delayed; the rest is dropped, as in the original (which noted that more did not keep up). `kLinesPerSet` in `saturn_module.h` sets the number; 128 (6 kHz) was tried first, before the delay lines' bookkeeping was moved on chip (see below).

## Changes from the original

* **Sample rate and block size.** The original runs at 32 kHz with 256-sample blocks, and needed them. The transform is 1024 points with frames 256 samples apart; the delay lines are advanced once per frame
  (187.5 times a second at 48 kHz) and the delay times are scaled by the frame rate so that Time still means 0 to 4 seconds. The pre-delay is 50 ms to 1 s at any sample rate (the original's
  2400 .. 96000 samples would be 0.05 .. 2 seconds at 48 kHz and 0.075 .. 3 seconds at 32 kHz; the README said 1 second).
* **The work is spread over blocks and the FFT is CMSIS-DSP's**, like Venus (see its README): `SlicedFourier` runs a frame as separate stages scheduled by sample count, so it works with the
  pedal's own 48-sample blocks, at the price of one extra hop (256 samples, 5 ms) of latency. The frames and work buffers are in the on-chip RAM (`.sram_d2_bss`). The delay lines' samples are in SDRAM (2.1 MB, plus 0.8 MB for the two pre-delay lines)
  but their bookkeeping (position, delay, feedback) is on chip: `SdramDelay` is DaisySP's `DelayLine` (same output, bit for bit) split that way. DaisySP's keeps everything in one object, and
  in SDRAM each update then took about ten slow accesses on top of the three needed; on the pedal that made 128 line updates take a quarter of an audio block.
* **Controls.** The original's footswitch preset, expression pedal, and per-knob MIDI control are replaced by this project's own (knob mapping, MIDI CC 14-26); the three-way and dip switches
  became menu items (the dip switch "Mono (off) / Stereo (on)" is the Mono item, which is on when the original's switch is off). The defaults are mine: PreDelay 0, Mix 0.4, Filter 0.5 (off),
  Time 0.5, Feedback 0.3, DriftRate 0.1, Time Mode Linear, Fdbk Mode Same, Drift Off, stereo, mono in, no spread, no pre-delay feedback.
* **Bins.** The same 175 as the original. The delay and feedback slopes across the frequencies are `kReferenceBins` (175) per Hz, so a setting means the same thing if `kLinesPerSet` is changed. The Filter knob sweeps
  the bins that are delayed, as in the original (where mono mode, with twice the bins, reaches twice the frequency).
* **Delays start at their settings.** The original's delay lines start at zero delay and glide to the target by 0.02% of the distance per frame, which takes about 26 seconds for the whole
  distance (the glide is kept for later changes: turning Time makes the delays slide slowly). Here they start at the settings of the controls.
* **A delay of 0 is one frame.** The delay lines are read before they are written, so a delay of 0 would read the oldest content (4 seconds ago); it is clamped to one frame (the first bin
  in Linear mode, and everything at Time 0, would otherwise do that).
* **Mono mode.** The original reads the right delay lines at index -1 for one bin in mono mode (an out-of-bounds read); here the lower 175 bins use the left lines and bins 175 to 349 the right
  lines. The delay lines are cleared (a few per audio block) when switching between mono and stereo, because bins move between lines and would otherwise ring at the wrong frequencies (the original
  has a note about this).
* **Random settings** come from a small deterministic random number generator (the original calls `rand()` and draws new numbers whenever Time or Feedback is turned in the random modes).
* **Level.** An output gain of 1.75 makes the effect about +2 dB louder than bypass at its defaults.

## Notes

* **The STFT itself has a gain of 0.75** (Hann windows with four overlaps, normalised as in the original), and the original adds a constant gain of 3 on the wet signal: a single sine passes with a gain
  of 2.25. `tests/host/test_saturn.cpp` checks this and the Linear delay time.
* **Stereo Spread.** The original multiplies both channels' bins by (1 - random), so the bins are not panned in the stereo field as the README says, they just get random levels (the same on both sides).
  This is kept; making the right side use the random value itself would pan them.
* Drift moves the delay times, so with Drift on the block and per-sample paths agree only to within a drift step (the host test allows 2%).
