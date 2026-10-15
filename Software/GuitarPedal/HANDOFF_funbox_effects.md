# Hand-off: converting FunBox effects

Written so a fresh session or developer can continue converting effects from the FunBox project (<https://github.com/GuitarML/Funbox>, MIT, a clone
sits next to this repository as `../FunBox`, `software/<Effect>/`). All eleven product effects are converted now; this note records what was done, the recipe that worked and what is left to check on the pedal.

## Status

* **Venus** (spectral reverb): converted. `Effect-Modules/venus_module.{h,cpp}` and `Effect-Modules/Venus/` (see its README for the origin, licence and list of
  changes). Added to the `Makefile` and `loaded_effects.h`. It fits without disabling any other effect: SRAM 91.5% -> 93.1% (about 8 KB of code), SDRAM 73% -> 74%
  (3 x 147 KB of FFT buffers), DTCM 49%.
* **Earth** (plate reverb + octave): converted, GPLv3 (it is adapted from GPLv3 code and uses the GPL Dattorro engine), so it sits in the same opt-in block as the Dattorro reverb in the `Makefile`
  and `loaded_effects.h` and is registered next to it. `Effect-Modules/earth_module.{h,cpp}` and `Effect-Modules/Earth/` (README with the licence, origin, controls and changes). It reuses the
  repo's fixed Dattorro engine rather than a second copy (saving about 25 KB of SRAM); the engine's constructor gained an optional pre-delay length for it. SRAM 93.1% -> 94.9% (about 24 KB left), SDRAM
  74% -> 77% (2 MiB arena for the delay lines). Nothing had to be disabled.
* **Jupiter** (stereo reverb on the CloudSeed engine): converted. `Effect-Modules/jupiter_module.{h,cpp}` and `Effect-Modules/Jupiter/` (README with the controls and changes). MIT. It is only a different control
  mapping on the repository's existing CloudSeed engine; the CloudSeed module's SDRAM pool moved to `Effect-Modules/cloudseed_pool.cpp` and grew to 12.6 MB (one engine instance takes 4.9 MB; the CloudSeed module is currently
  not in the build). Found by the tests: this repository's RubiKa preset has the post-reverb shelves off, so Jupiter switches them on (and the cutoff off) itself. Control changes are pushed to the engine one per block.
  SRAM 47% -> 56%, SDRAM 15 MB. Not heard or run on the pedal.
* **Saturn** (spectral delay): converted. `Effect-Modules/saturn_module.{h,cpp}` and `Effect-Modules/Saturn/` (README with the controls, notes and the list of changes; the original has quirks that are documented there).
  Same recipe as Venus (sliced STFT scheduled by sample count + CMSIS FFT, now shared from `Util/STFT/sliced_fourier.h` and `Util/STFT/cmsis_fft.h`), N = 1024, two transforms for stereo. Its frames and work buffers are in
  the otherwise empty on-chip RAM_D2 (`__attribute__((section(".sram_d2_bss")))`, see `nam_a2_sections.lds`), which keeps DTCM unchanged (79%); the delay lines (2.9 MB) are in SDRAM. SRAM 56% -> 60%.
  Not heard or run on the pedal (check the average/peak readout and the sound, in particular that the 48-sample blocks keep up with the two transforms).
* **Neptune** (reverb + delay with normal, octave and reverse modes): converted. `Effect-Modules/neptune_module.{h,cpp}` and `Effect-Modules/Neptune/` (README with the controls and the changes; the delay line headers
  carry Veno-Echo's MIT licence text). Same approach as Jupiter on the CloudSeed engine; the pool in `cloudseed_pool.cpp` is now 19.7 MB (three engine instances). The original's "alternate mode" (hold FS1) controls
  became menu items. Delay samples (4.6 MB) are in SDRAM with their bookkeeping on chip (lesson from Saturn). Bugs found in the original and fixed are listed in its README (divide by zero in the reverse line at start,
  an out-of-bounds read, one shared low-pass for both channels). SRAM 62%, SDRAM 44%. Not heard or run on the pedal (check the average/peak readout, in particular in reverse mode and with the reverb mode switch).
* **Uranus** (granular delay and synth, MIDI): converted. `Effect-Modules/uranus_module.{h,cpp}` and `Effect-Modules/Uranus/` (README with the controls, the MIDI notes and the changes). Reuses `Util/granularplayermod.*` (Granular
  Delay) and the FM voices, which moved from `fm_keys_module.h` to `Util/fm_voices.h`. The half second of audio is in RAM_D2 (84% now: NAM's 77 KB would no longer fit next to it); DTCM 82%. MIDI: the framework already routes notes (`OnNoteOn/OnNoteOff`) and
  CCs (`midiCCMapping`) to the active effect; Uranus uses both. Runs on the pedal (as the only effect in the build) but **the MIDI input is untested on hardware: no MIDI connector was available. Test the notes (granular and FM synth modes), the CCs and the MIDI channel with a keyboard.**
* **Balnea** (delay and pitch shifter, MIDI synth): converted. `Effect-Modules/balnea_module.{h,cpp}` and `Effect-Modules/Balnea/` (README with the controls and changes; `balnea_dsp.h` holds the tape wow/flutter modulator, the reverse delay line
  by Adam Fulford (MIT) and the MIDI synth voices). The delay lines (2 x 16384 samples) and the reverse lines (2 x 32768) are in SDRAM (about 384 KB). The three switches, the dip switches and the alternate mode became menu parameters; tap tempo is the
  secondary footswitch (tap on release, hold 300 ms for the self-oscillation) and writes a `Time` parameter. The reverse line is checked against the original (bit for bit) in `tests/host/test_balnea.cpp`; the original's out-of-range read at a crossfade start and the reverse mode that only reached the left channel are fixed (README).
  Not heard or run on the pedal; the MIDI synth is untested without a keyboard.
* **Pluto** (dual looper, variable speed, stability/filter/reverb): converted. `Effect-Modules/pluto_module.{h,cpp}`, `Effect-Modules/Pluto/`, and the looper in `Util/var_speed_looper.h` (shared with Cassini). The original ran at 96 kHz with 23 MB per looper;
  here 48 kHz, a minute (11.5 MB) per looper, 23 MB of SDRAM for both, with linear interpolation in the playback. One footswitch (the pedal has one secondary footswitch) for the loopers: a `Footswitch` parameter chooses A + B, A or B.
  The original read one stale sample at the start of every loop (it relied on a cleared buffer); fixed. Not heard or run on the pedal.
* **Cassini** (chromatic sampler for a MIDI keyboard): converted. `Effect-Modules/cassini_module.{h,cpp}` and `Effect-Modules/Cassini/`. Three 20 s sample banks (11.5 MB) and the looper (11.5 MB) in SDRAM, none cleared at startup. The secondary footswitch records the sample (hold)
  or runs the looper (tap), chosen by a `Footswitch` parameter. The original's reverse playback stalled at the start of the sample (found by the tests); fixed. Needs a MIDI keyboard; untested on hardware.
* **Mars** (amp sim with three GRU models, three cabinets, delay with dotted eighth and triplet): converted. `Effect-Modules/mars_module.{h,cpp}` and `Effect-Modules/Mars/` (README; `mars_dsp.h`, `mars_models.h`, `mars_ir_data.h`). RTNeural and Eigen are not in this repository, so the 9-unit GRU is
  a few lines of plain C++ with a rational `tanh` (the exact version agrees with RTNeural to 4e-6 on reference data generated with RTNeural, `tests/host/mars/`; the fast one to 2e-4); the cabinet uses the repository's CMSIS FIR (`ImpulseResponse`). Level calibrated per model and cabinet to +3 dB.
  Mono. Not heard or run on the pedal: **check the CPU (the GRU is the heavy part: about 250 multiply-adds and 27 `tanh`/logistic per sample).**
* **Mercury** (amp sim with nine NAM "pico" WaveNet models and a four band EQ): converted. `Effect-Modules/mercury_module.{h,cpp}` and `Effect-Modules/Mercury/` (README; `wavenet_pico.h` is a plain C++ rewrite of the RTNeural-NAM network, BSD-3-Clause, the licence text is in the directory). The output equals the original network's for all nine
  models to 4e-6 (`tests/host/mercury/`), the prewarm after a model change is shortened from 16384 samples to the receptive field (identical result, checked), and the model change runs on the main loop with the output muted, like the NAM module. 33 KB of DTCM for the network's history. Level calibrated per model to +3 dB.
  Not heard or run on the pedal: **check the CPU (about 500 multiply-adds and 40 `tanh` per sample for the 20 layers).**
* Not checked on the pedal: the CPU load and, above all, whether the 48-sample blocks keep up (see "Real time" below), and the sound.

## Which FunBox effects are new here

| FunBox effect | What it is | Already in this repository? |
|---|---|---|
| **Balnea** | Digital delay and pitch shifter (CBA Thermae style) | **Converted** (Balnea module) |
| **Cassini** | Chromatic sampler played from a MIDI keyboard | **Converted** (Cassini module) |
| **Pluto** | Dual/stereo looper with variable-speed playback and effects | **Converted** (Pluto module; the Looper is a simpler single/stereo looper) |
| Saturn | Spectral delay | **Converted** (Saturn module, the full effect; the repository's older Spectral Delay is a reduced version of it) |
| Earth | Dattorro plate reverb and octaves | Dattorro (GPL) - **converted**, see Status |
| Jupiter | Stereo reverb built on CloudSeed | CloudSeed engine - **converted** (Jupiter module) |
| Neptune | Reverb + delay built on CloudSeed | **Converted** (Neptune module) |
| Mars, Mercury | Amp sims with small neural models (GRU / NAM pico) and cabinet IRs | **Converted** (Mars and Mercury modules; the NAM A2 module runs the official A2 Nano models and is a separate thing) |
| Uranus | Granular delay and synth (with MIDI) | **Converted** (Uranus module; Granular Delay is the same grain player without the feedback, grain modes and synth modes) |
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

## Current build: Uranus and the five newest effects are enabled

Having all five of the first converted effects (Venus, Jupiter, Saturn, Neptune, Uranus) enabled together did **not boot** on the pedal (some memory ran out: with everything, DTCM was 82% and RAM_D2 84% static; my guess is the DTCM heap, where the effects'
`new` allocations go, but that was not confirmed). The Makefile and `loaded_effects.h` therefore enabled **only Uranus** (Venus, Jupiter, Saturn and Neptune are commented out, and `cloudseed_pool.cpp` with them since only Jupiter and Neptune need it).

The five effects converted after that (Balnea, Pluto, Cassini, Mars, Mercury) are **enabled next to Uranus**, because they are light where the lockup is suspected to come from (build report with GCC 13):
SRAM (code) 55%, **DTCM 52% (68.6 KB; Mercury's 33 KB is the new part)**, RAM_D2 36.6% (unchanged: Uranus only), SDRAM 71% (47.9 MB, of which Pluto 23 MB, Cassini 23 MB, Mars 0.7 MB, Balnea 0.4 MB), and **about 15 KB of heap in all**
(`tests/host/run_heap_report.sh`: Balnea 3.5, Pluto 0.5, Cassini 1.2, Mars 9.0, Mercury 0.3 KB). That is an expectation from the numbers, **not tested on the pedal: flash it and check that it boots.** If it does not, disable effects one at a time starting with the two 23 MB ones.
The effects that need a lot of SDRAM do not fit together: Pluto + Cassini take 47 MB of the 64 MB, which leaves room for Venus, Saturn, Balnea and Mars but **not for Jupiter or Neptune**, because the CloudSeed pool they share is 19.7 MB (sized for three engines; the Looper module would add another 23 MB).
To bring the CloudSeed effects back, disable Pluto or Cassini (or shrink the pool in `cloudseed_pool.cpp` to one engine, 4.9 MB, for Jupiter alone). Bring Venus, Jupiter, Saturn and Neptune back one or two at a time
(uncomment the Makefile line and the include and instance in `loaded_effects.h`), checking the memory report and that it boots. Venus, Jupiter and Saturn were each run on the pedal in earlier builds (with the older sets of effects); Neptune and the five newest have not been run on the pedal at all yet.

Building here needs `make GCC_VERSION_MAJOR_REQUIRED=13` (the installed compiler is GCC 13.3.1; the Makefile's own required version is 10).

See also `PERFORMANCE_AND_MEMORY.md`: the optimisations and what they gave, per-effect memory needs, the heap explanation of the lockup, the GCC 15 experience and a first draft of a matrix of effects that can be enabled together.

## Venus tuning (second pass)

Venus gave sound but dropped blocks constantly at 48 kHz / 48 samples: each 4096-point FFT stage has to fit inside one 1 ms block. Changes made (not yet checked on the pedal):

* **All other effects are disabled** (Makefile and `loaded_effects.h`; the lines are only commented out) because having them all enabled locked the firmware up (probably out of memory; not investigated).
  The build is now SRAM 38%, DTCM 54%, SDRAM 144 KB.
* **Block size** (tried 32 kHz / 256 like the original, then 48 kHz / 256; the shipped setting is now 48 kHz / 48, see the CMSIS note below): `kAudioSampleRate` and `kAudioBlockSize` at the top of `guitar_pedal.cpp` (`SetAudioSampleRate` after `hardware.Init`). Set the block size back to 48 when the
  other effects come back (they were written and measured at those settings). `kMaxBlock` in the callback is 256 now.
* **Stage schedule by sample count** (27 stages per 1024-sample hop), so Venus no longer assumes a block size; drift and parameter updates use sample counters too.
* **Work buffers shrunk and moved on chip:** `middle`/`out` are one frame each (16 KB) in DTCM instead of nine frames each in SDRAM. SDRAM use fell from 441 KB to 147 KB.
* **CMSIS-DSP FFT** instead of ShyFFT (`Util/STFT/cmsis_fft.h` (shared with Saturn); the Makefile adds the CMSIS transform sources and, via `ARM_DSP_CONFIG_TABLES`, only the tables the 4096-point real FFT needs, +42 KB SRAM).
  Same spectrum layout, so the spectral code is unchanged; differs from ShyFFT by rounding only (about 1e-6 relative). `tests/host/test_cmsis_fft.cpp` checks the real library, and the host `stub_cmsis/arm_math.h` has a stand-in for the other tests. On its own it did not change the CPU load.
* **The real fix: run the FFT passes in on-chip RAM, not SDRAM.** Both FFT libraries work in place, and the frames are in SDRAM; one frame's transform took ~2 ms and overran 48-sample blocks (Show Debug read
  33% average / 225% peak). `CmsisRealFft` now copies the frame in, transforms in a DTCM workspace and copies it out (+16 KB DTCM, now 79%). **Result on the pedal: peak 76%, no dropouts at 48 kHz / 48-sample blocks.**
  Lesson: the average CPU readout hid this; the peak readout (`average/peak%` in Show Debug, slowest block of the last 2 s) is what shows overruns. DTCM will be tight when NAM etc. return.
* The host test also runs at 256-sample blocks and fails if any frame needed the "late finish" fallback (`SlicedFourier::lateFinishes()`).
* Still to check on the pedal: that 8 ms blocks keep up (the CPU readout in Show Debug is an average; the worst block is not shown) and the sound against the original.

## Next steps

1. Flash and check that the firmware boots with this set (see "Current build"). If it does, try each new effect on the pedal: the CPU readout in Show Debug (average **and** peak) for Mars and Mercury in particular; the sound against the originals; the footswitch handling of Balnea (tap tempo, hold), Pluto and Cassini (record, pause, clear).
2. Test the MIDI notes of Balnea (synth) and Cassini (sampler) and of Uranus with a keyboard.
3. If Mars or Mercury overrun the 48-sample blocks: the GRU's `FastMath` is already used; the next steps would be a block-wise matrix product, or the original's 96-sample or larger blocks for these two. If Pluto's or Cassini's 23 MB of SDRAM clears at startup are noticeable: they are not cleared at all (only what is recorded is played).
4. Decide whether to fix the "Oct Up+Dn" behaviour and whether to add a Level parameter to Venus.
5. The Experiments directory of FunBox (ChorusVerb, DenoiseFFT, FmSynth, Granular, MidiKeys, NoiseFilterFFT, SamplerTest, SpectralReverb) was not converted: they are experiments, not pedals, and several overlap what is here (FM Keys, Granular Delay, MIDI Keys).
