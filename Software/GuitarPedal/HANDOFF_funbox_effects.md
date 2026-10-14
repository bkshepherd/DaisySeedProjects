# Hand-off: converting FunBox effects

Written so a fresh session or developer can continue converting effects from the FunBox project (<https://github.com/GuitarML/Funbox>, MIT, a clone
sits next to this repository as `../FunBox`, `software/<Effect>/`). Venus is done; this note records the recipe that worked and what is left.

## Status

* **Venus** (spectral reverb): converted. `Effect-Modules/venus_module.{h,cpp}` and `Effect-Modules/Venus/` (see its README for the origin, licence and list of
  changes). Added to the `Makefile` and `loaded_effects.h`. It fits without disabling any other effect: SRAM 91.5% -> 93.1% (about 8 KB of code), SDRAM 73% -> 74%
  (3 x 147 KB of FFT buffers), DTCM 49%.
* **Earth** (plate reverb + octave): converted, GPLv3 (it is adapted from GPLv3 code and uses the GPL Dattorro engine), so it sits in the same opt-in block as the Dattorro reverb in the `Makefile`
  and `loaded_effects.h` and is registered next to it. `Effect-Modules/earth_module.{h,cpp}` and `Effect-Modules/Earth/` (README with the licence, origin, controls and changes). It reuses the
  repo's fixed Dattorro engine rather than a second copy (saving about 25 KB of SRAM); the engine's constructor gained an optional pre-delay length for it. SRAM 93.1% -> 94.9% (about 24 KB left), SDRAM
  74% -> 77% (2 MiB arena for the delay lines). Nothing had to be disabled.
* Not checked on the pedal: the CPU load and, above all, whether the 48-sample blocks keep up (see "Real time" below), and the sound.

## Which FunBox effects are new here

| FunBox effect | What it is | Already in this repository? |
|---|---|---|
| **Balnea** | Digital delay and pitch shifter (CBA Thermae style) | New (there is a Delay, a Pitch Shifter and a Multi Delay, but not this combination) |
| **Cassini** | Chromatic sampler played from a MIDI keyboard | New |
| **Pluto** | Dual/stereo looper with variable-speed playback and effects | New (there is a Looper) |
| Saturn | Spectral delay | Spectral Delay (`spectral_delay_module`, from the same STFT code) |
| Earth | Dattorro plate reverb and octaves | Dattorro (GPL) - **converted**, see Status |
| Jupiter, Neptune | Stereo reverb / reverb + delay built on CloudSeed | CloudSeed |
| Mars, Mercury | Amp sims with NAM models and cabinet IRs | NAM and IR |
| Uranus | Granular delay and synth | Granular Delay |
| Venus | Spectral reverb | **Converted** |

## The conversion recipe

1. **Read the original** `venus.cpp`-style file. Separate the hardware code (footswitches, LEDs, knobs, expression pedal, MIDI, bypass, `DaisyPetal`) from the DSP.
   This project's framework already provides the hardware side: knobs map through `knobMapping` in the parameter metadata, MIDI through `midiCCMapping`, bypass
   is global, the secondary footswitch has `AlternateFootswitchPressed/Released/HeldFor1Second` hooks, and `GetBrightnessForLED(1)` drives the second LED. Three-way
   switches become binned parameters (menu items); dip switches become booleans.
2. **Write the module** (`<name>_module.{h,cpp}`, class derived from `BaseEffectModule`): parameter metadata, `Init(sample_rate)`, `ProcessMono/ProcessStereo`, and the block
   overrides `ProcessMonoBlock/ProcessStereoBlock` (share one `ProcessSamples(…, n)` routine; mark it `__attribute__((noclone))` so GCC does not emit a second 5 KB copy).
   Put state that depends on parameters in a function called from both `ParameterChanged` **and `Init()`**: `ParameterChanged` never fires for the defaults.
3. **Sample rate and block size.** The FunBox pedals often run at 32 kHz with 256-sample blocks (Venus does); this pedal runs at 48 kHz with 48-sample blocks. Rescale every
   constant that depends on the rate (decay times, cut-off fractions, lo-fi rates, per-frame amounts) so it sounds the same, and check anything done per block.
4. **Real time.** Work that was fine once per 256-sample block can be too much for a 1 ms block. Anything that does a large FFT or a long loop in one go needs to be split
   across blocks (Venus's `SlicedFourier` is an example, and the spectral work is exact to the one-shot version). Cheap tricks that helped: tables instead of `rand()/sin()/cos()` per element.
5. **Memory.** Large buffers go in SDRAM (`DSY_SDRAM_BSS`, not zeroed at startup: clear them in `Init()`); small tables can stay in DTCM. Use unique names or an anonymous
   namespace for file-scope variables: the Spectral Delay already defines globals called `in`, `out`, `middle`, `hann`, `N`.
6. **Licence.** FunBox is MIT, like this repository, so adapting it is fine: keep the copyright notice (copy `LICENSE.txt` into the effect's directory, as in `Effect-Modules/Venus/`),
   credit upstream authors in a README and in the source header, and list the changes. (Check each effect's README: some use other people's code, e.g. CloudSeed, Dattorro.)
7. **Tests** (all on the PC, see `tests/host/README.md`): a test that compares the new pipeline with the original where possible (`test_venus.cpp`), the default-level test
   (`levels/test_level_<name>.cpp`, rule: 0 to +6 dB at default settings, never attenuating), and `tests/host/run_defaults_audit.sh <stem>` (finds defaults that are not applied at startup
   and uninitialised reads; picks up new effects automatically). Add the new test steps to `run_tests.sh` / `run_level_tests.sh`.
8. **Register it**: `CPP_SOURCES` in the `Makefile` and the include plus `new <Name>Module()` in `loaded_effects.h`; check the memory report. If it does not fit, disable effects in
   those two files (comment out the Makefile line, the include and the instance) starting with the biggest ones. Adding or reordering effects changes the settings layout, which resets
   saved settings on the next boot.

## Venus: things to know

* **Real time (the main thing to verify on the pedal).** Each 4096-point FFT runs in a single block (two per frame, one block each) and is the heaviest single step. On the PC it is
  well under a block; on the Cortex-M7 it should be a few hundred microseconds, but this has not been measured. If blocks are dropped (clicks), lower `kOrder` in `venus_spectral.h` to 11
  (2048 points; this also needs `kChunks` <= 8, a compile-time check enforces it). The CPU readout shows the average, not these peaks.
* **Level.** A fixed output gain of 1.9 (+5.6 dB) in `venus_module.cpp` gives about +2 dB at the default settings (without it Venus is 3.6 dB quieter than bypass). A full-scale input
  could clip at the output.
* **Latency.** About 107 ms from input to the start of the reverb output (the 4096-sample window plus one hop for the sliced processing). The dry signal is not delayed.
* "Oct Up+Dn" only goes up (a quirk kept from the original; see `Effect-Modules/Venus/README.md`).
* Mono in (left input), the same signal on both outputs, as in the original.

## Earth: things to know

* **Licence.** GPLv3, unlike the rest. Treat it like the Dattorro reverb: it is in the opt-in block, and a binary built with it must be distributed under GPLv3 (or not at all). Do not send it upstream in the MIT parts.
* **Engine differences found.** FunBox Earth's own Dattorro engine never wires up its maximum sample rate (at 48 kHz its tank behaves as if at 32 kHz, about 2/3 as large), and it hands out its delay buffers from a fixed
  table of 13 that it overruns (it crashes on a PC). The repo's engine has both fixed, so the Size switch and the modulation depth are scaled by 32000 / sample rate to keep Earth's room sizes.
  `tests/host/compare_earth_engine.sh` reproduces the evidence (needs the FunBox clone). The input diffusion stays 1.5 times longer than the original's; the modulation speed was not scaled (not verified).
* **Level.** An output gain of 1.4 gives about +2 dB at the defaults. The octave up and down with a loud input can exceed full scale (same as the original).
* **Defaults** are mine (the pedal's switches have none): Octave None, FS Action Freeze, Size Medium, Mix 0.4, Decay 0.8.
* Not yet heard or run on the pedal, including the CPU load and the footswitch actions.

## Venus tuning (second pass)

Venus gave sound but dropped blocks constantly at 48 kHz / 48 samples: each 4096-point FFT stage has to fit inside one 1 ms block. Changes made (not yet checked on the pedal):

* **All other effects are disabled** (Makefile and `loaded_effects.h`; the lines are only commented out) because having them all enabled locked the firmware up (probably out of memory; not investigated).
  The build is now SRAM 38%, DTCM 54%, SDRAM 144 KB.
* **Block size** (tried 32 kHz / 256 like the original, then 48 kHz / 256; the shipped setting is now 48 kHz / 48, see the CMSIS note below): `kAudioSampleRate` and `kAudioBlockSize` at the top of `guitar_pedal.cpp` (`SetAudioSampleRate` after `hardware.Init`). Set the block size back to 48 when the
  other effects come back (they were written and measured at those settings). `kMaxBlock` in the callback is 256 now.
* **Stage schedule by sample count** (27 stages per 1024-sample hop), so Venus no longer assumes a block size; drift and parameter updates use sample counters too.
* **Work buffers shrunk and moved on chip:** `middle`/`out` are one frame each (16 KB) in DTCM instead of nine frames each in SDRAM. SDRAM use fell from 441 KB to 147 KB.
* **CMSIS-DSP FFT** instead of ShyFFT (`Effect-Modules/Venus/cmsis_fft.h`; the Makefile adds the CMSIS transform sources and, via `ARM_DSP_CONFIG_TABLES`, only the tables the 4096-point real FFT needs, +42 KB SRAM).
  Same spectrum layout, so the spectral code is unchanged; differs from ShyFFT by rounding only (about 1e-6 relative). `tests/host/test_cmsis_fft.cpp` checks the real library, and the host `stub_cmsis/arm_math.h` has a stand-in for the other tests. On its own it did not change the CPU load.
* **The real fix: run the FFT passes in on-chip RAM, not SDRAM.** Both FFT libraries work in place, and the frames are in SDRAM; one frame's transform took ~2 ms and overran 48-sample blocks (Show Debug read
  33% average / 225% peak). `CmsisRealFft` now copies the frame in, transforms in a DTCM workspace and copies it out (+16 KB DTCM, now 79%). **Result on the pedal: peak 76%, no dropouts at 48 kHz / 48-sample blocks.**
  Lesson: the average CPU readout hid this; the peak readout (`average/peak%` in Show Debug, slowest block of the last 2 s) is what shows overruns. DTCM will be tight when NAM etc. return.
* The host test also runs at 256-sample blocks and fails if any frame needed the "late finish" fallback (`SlicedFourier::lateFinishes()`).
* Still to check on the pedal: that 8 ms blocks keep up (the CPU readout in Show Debug is an average; the worst block is not shown) and the sound against the original.

## Next steps

1. Flash and try Venus: check for dropouts, the sound against the original pedal, and the CPU readout; adjust `kOrder`/`kChunks` or the output gain if needed.
2. Convert Balnea, Pluto and Cassini the same way, one at a time (read each one's README first for the controls and what it needs from the hardware, e.g. Cassini needs a MIDI keyboard).
3. Decide whether to fix the "Oct Up+Dn" behaviour and whether to add a Level parameter to Venus.
