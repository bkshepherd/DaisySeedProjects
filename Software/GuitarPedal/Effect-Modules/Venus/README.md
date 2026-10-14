# Venus (spectral reverb)

Venus is a spectral reverb: it works on the short-time Fourier transform (STFT) of the signal, which gives lush, string-like overtones, optional octave
and fifth shimmer, detune, a spectral freeze, two lo-fi modes and two "drift" modes that slowly modulate the controls. It is mono in, with the same
signal on both outputs.

## Origin and licence

This code is adapted from the **Venus** pedal of the **FunBox** project by GuitarML:
<https://github.com/GuitarML/Funbox> (`software/Venus/venus.cpp`), Copyright (c) 2024 Keith Bloemer, **MIT License**. The licence text is in
[`LICENSE-FunBox-MIT.txt`](LICENSE-FunBox-MIT.txt), like the rest of this repository it is MIT, so there is no licence change to opt into.

The original's acknowledgements apply here too:

* The STFT code adapts DaisySTFT (<https://github.com/amcerbu/DaisySTFT>), which uses `shy_fft` by Émilie Gillet of Mutable Instruments
  (<https://github.com/pichenettes/eurorack>). Both are already in this repository as `Util/STFT/` (used by the Spectral Delay).
* The reverb maths is heavily inspired by the Atlantis Reverb by Geraint Luff (<https://geraintluff.github.io/jsfx/#Atlantis%20Reverb>) and by the
  examples in his forum post <https://forum.cockos.com/showthread.php?t=225955>.

## Files

| File | |
|---|---|
| `../venus_module.{h,cpp}` | The effect module: parameters, the lo-fi and drift modes, the dry/wet mix |
| `venus_spectral.h` | The per-bin reverb (`SpectralReverb`), header only and free of hardware dependencies so the host tests can use it |
| `../../Util/STFT/cmsis_fft.h` | A real FFT with ShyFFT's interface and spectrum layout, computed by CMSIS-DSP's `arm_rfft_fast_f32` |
| `../../Util/STFT/sliced_fourier.h` | An STFT that spreads each frame's work over several audio blocks (see below) |

## Controls

| Control | Parameter | Original control |
|---|---|---|
| Knob 1 | Decay | Decay time of the reverb |
| Knob 2 | Mix | Dry/wet mix |
| Knob 3 | Damp | Reverb damping (a high-frequency roll-off) |
| Knob 4 | Shimmer | Octave shimmer amount |
| Knob 5 | Shim Tone | Adds octave plus fifth to the shimmer |
| Knob 6 | Detune | Below the middle detunes down, above detunes up (a small dead zone around the middle) |
| Menu | Shim Mode | Octave down / octave up / "up+down" (see below) |
| Menu | LoFi | Mild (9.6 kHz sample rate and a low-pass) / Off / Heavy (6.4 kHz), applied to the wet signal only |
| Menu | Drift | Slow / Off / Fast: oscillators slowly modulate Damp, Shimmer, Shim Tone and Detune |
| Menu | Freeze | Holds the reverb. Holding the secondary footswitch freezes it too, and LED 2 shows it |

## Changes from the original

* **Sample rate and block size.** The original runs at 32 kHz with 256-sample blocks. This pedal runs at 48 kHz with 48-sample blocks, so Decay, Damp, the
  energy that feeds the reverb, the lo-fi sample rates and the shimmer/detune rates are rescaled to keep the same time constants, cut-offs and levels in
  seconds and Hz.
* **The STFT work is spread over blocks.** `soundmath::Fourier` runs the forward FFT, the per-bin processing and the inverse FFT for a whole frame inside one
  call. For Venus's 4096-point transform that is several milliseconds, but a 48-sample block is 1 ms. `SlicedFourier` queues a finished frame and runs it as
  separate stages (the forward FFT, 16 chunks of bins, the inverse FFT), at the price of one extra hop (1024 samples) of latency. The stages are issued by sample count
  (27 per hop, 1.5 times what is needed), not one per block, so the schedule is the same for any block size. The parameters are latched once per frame.
  `tests/host/test_venus.cpp` checks that this gives exactly the same audio as running every stage immediately, at 48- and at 256-sample blocks.
* **Audio settings.** The original runs at 32 kHz with 256-sample blocks because its transforms run inside one block. Here the
  stages are spread over blocks and the FFT works in on-chip RAM (see `Util/STFT/cmsis_fft.h`), so the pedal's own 48 kHz / 48-sample settings work (measured on the pedal: peak
  block load 76%). `kAudioSampleRate` / `kAudioBlockSize` (top of `guitar_pedal.cpp`) can be changed to 32 kHz and 256 (the original's) or 48 kHz / 256, which also work;
  at 32 kHz nothing needs rescaling (`m_srRatio` is 1).
* **Cheaper random phase.** The original calls `rand()`, `cos()` and `sin()` for every bin of every frame; this uses a table of 1024 angles and a small
  random number generator.
* **Memory.** Only the input frames (9 x 16 KB = 147 KB) are in SDRAM. The two frequency-domain work buffers are shared by all frames (only one is processed at a
  time): 16 KB each, in the fast on-chip RAM instead of behind the cache.
* **Controls.** The original's footswitch handling, expression pedal and per-knob MIDI control are replaced by this project's own (knob mapping, MIDI CC
  14-23, the secondary footswitch, bypass in the main program). The six knobs map to the same parameters; the three-way switches became menu items.
* **Level.** A fixed output gain of 1.9 (+5.6 dB) makes the effect about +2 dB louder than bypass at its default settings, as for the other effects.

## Notes

* **"Oct Up+Dn" only goes up.** In the original code the up-octave branch is tested before the down-octave one with `else if`, so the third shimmer position
  never goes down. This port keeps that behaviour (see the comment in `venus_spectral.h`); fixing it would mean letting both branches run.
* Frames of the original `Fourier` drift by one sample per cycle (a slot starts writing one sample early). `SlicedFourier` keeps a drift-free cycle instead,
  so from the ninth frame on its frames sit one sample later; this is inaudible.
* Venus is the most memory-hungry effect (about 440 KB of SDRAM) and one of the more demanding on the CPU.
