# Hand-off: default output level calibration

> **UPDATE: the reference level is now -37 dBFS peak, not -12.** The pedal's display now shows the averaged peak input (`I`) and output (`O`) in dB, and real playing
> reads around -37 dBFS. All gain figures, the "Before/After" table and the "-12 dBFS" statements below describe the first -12 dBFS calibration. Current state at -37 dBFS
> (test inputs -49/-37/-25/-12): Distortion (all types) and Overdrive about **+6 dB**, NAM models about **+9 dB** (NAM Level default 0.3, so `outputGain` is higher), Compressor
> **+3 dB** (its displayed Level default is +3 dB, the real gain is displayed minus 7.5 dB). Values: Distortion `normalizeVolume` 1.709/0.1836/0.1061/0.3531/0.0647/1.709, NAM
> `outputGain` 0.3369/0.3496/1.1737/0.2611/0.4339, Overdrive level range 0.001794..0.0644. `SETTINGS_FILE_FORMAT_VERSION` is 14. The author found the NAM feel "still not quite right".
> **Consequence:** saturating effects attenuate noticeably at a hot -12 dBFS input (their ceiling is lower). The display readout is an average of block peaks (about 50 ms), so it
> reads below the true peak of a strum; if -37 turns out to be low, adjust `kReferenceInput`.

Written so a fresh session or developer can continue without the original conversation. This work is on branch `level-calibration`,
which is based on `block-processing` (see `HANDOFF_block_processing.md` for that earlier work and for the general build and test notes).

## Goal

Harmonise how loud each effect is at its **default parameters**, so switching effects does not jump in volume.

**Rule of thumb (set by the project owner):** at default settings an effect should amplify by **0 dB to +6 dB** and must never attenuate.
Distortion-like effects (distortion, overdrive, compression, amp models) aim for **+3 dB**.

Nothing here has been verified by ear. The calibration is by measurement on the PC; the Compressor was also checked on the pedal for its
parameter display. Distortion, Overdrive, NAM and the Level defaults still need real-world listening.

## How the measurement works

`tests/host/run_level_tests.sh` (details in `tests/host/README.md`) compiles the real effect modules for the PC and plays test signals through
them at default settings, with a fresh module per measurement, using the stereo block path like the pedal does.

* **Signals:** pure sines (220 Hz, 1 kHz) and a *guitar-like* harmonic series (partial k at 1/k up to the 16th; fundamentals 110 Hz and 330 Hz).
  Peak-normalised at **-24, -12 and -6 dBFS peak**.
* **Reference level: -12 dBFS peak** (a typical strummed guitar into a line input). This is an assumption. If the real level at the pedal's input is
  hotter, nonlinear effects will read lower than calibrated (see Findings 1). Change `kReferenceInput` in `tests/host/levels/level_common.h`.
* **Pass check:** the mean RMS gain over the two test frequencies at the reference level must be inside 0..+6 dB and within a tolerance of the target
  (the tolerance is +/- 1 dB). It also fails if the output reaches full scale at any measured level. The mean is used because tone-shaping effects
  (amp models especially) differ by several dB between a low and a higher note.
* Which signal is enforced: Distortion on sines; NAM, Overdrive and Compressor on the guitar-like signal (the other is printed for information).

Run `tests/host/run_level_tests.sh` and `tests/host/run_tests.sh` (block-processing equivalence tests) after any change to these modules.
Needs `g++` (C++20) and `python3`.

## Results (RMS gain at the reference level)

| Effect | Before | After | What was changed |
|---|---|---|---|
| Distortion (Hard Clip, the default type) | +0.5 dB | +3.0 dB | Per-type `normalizeVolume` factors in `distortion_module.cpp`: 2.38, 1.11, 1.10, 0.87, 0.79, 2.38 for Hard / Soft / Fuzz / Tube / Multi / Diode. All six types now give +2.9..+3.1 dB (before: -1.0 .. +3.3 dB) |
| NAM amp models (IR cab Off) | -6.1 .. +4.4 dB across models | +3.0 dB each | `outputGain` per model in `Effect-Modules/Nam/model_data_nam_a2.h`: BE-100 0.60->1.70, JCM800 1.10->1.08, Ampeg 1.00->0.85, Mesa 0.50->1.06, 1959BJA 1.00->1.50. With the IR cab on, every model is +2.7..+5.4 dB |
| Overdrive | -1.9 dB (guitar-like) | +3.0 dB (+0.2 dB on sines) | Level range maximum 0.20 -> 0.36 so the default 0.5 maps to 0.185 |
| Compressor | -3.0 dB (as actually shipped) | +3.6 dB guitar-like, +2.1 dB sines | New real-unit parameters and defaults (below) |
| Crusher | not measured | not measured | Level default 1.0 -> 0.5 only (halves its default gain) |
| Dual Echo | not measured | not measured | Level default 0.4 -> 0.5 only (about +2 dB) |

Level defaults: before this work Crusher was 1.0, Dual Echo 0.4 and Overdrive 0.3; every other effect with a Level control was already 0.5.

## Findings worth remembering

1. **Saturating effects lose gain as the input gets louder.** A clipper or amp model has a fixed output ceiling, so e.g. Distortion reads about +9 dB at
   -24 dBFS peak, +3 dB at -12 and about -2.6 dB at -6. NAM amp models drop about 6 dB per 6 dB of extra input. A single number is only meaningful at a
   stated input level, and "never attenuate" cannot hold for very hot playing without raising the ceiling (which would push the reference above +6 dB).
   The tables printed by the tests show all three levels.
2. **Amp models are pitch dependent.** The Ampeg (a bass amp) is about 8 dB louder on the 110 Hz note than the 330 Hz note; BE-100 differs by 6 dB. That is
   the character of the models; the calibration uses the mean of the two notes.
3. **Compressor startup bug (fixed).** `ParameterChanged` only fires when a stored value differs from the current one, and first-boot storage equals the
   metadata defaults, so on a fresh install the DaisySP compressor ran on its own built-in settings (about 2:1, -12 dB) while the menu showed 0.5 for
   everything. The old 0.5 defaults would have meant a 5 s attack and 5 s release. `CompressorModule::Init` now applies every parameter, and the test checks that
   the startup state equals the touched-to-default state.
4. **Stored settings and the format version.** The storage layout hash only covers each effect's parameter *count and types*, not ranges or meaning. Changing
   what a stored number means needs a bump of `SETTINGS_FILE_FORMAT_VERSION` (now **13**), which wipes all saved settings on first boot. The header comment says so.
5. **Auto makeup gain** in the DaisySP compressor is `|thresh - thresh/ratio| / 2` dB (+7.5 dB at the new defaults), which is why a compressor attenuates at hot input
   levels with these defaults: the gain reduction at -6 dBFS peak is larger than the makeup. A milder threshold (about -12 dB) would stop that.
6. NAM models share one process-wide model object with recurrent state; tests run each path in its own process or create-and-run one module at a time.

## What was added to the framework

* `ParameterMetaData` has three new trailing fields: `unit` (text appended to the value, e.g. `" dB"`, `":1"`), `decimals` and `showSign`. They are exposed through
  `GetParameterUnit/Decimals/ShowSign`, copied for effect-chain children in `effect_chain.cpp`, and used by the menu in `UI/guitar_pedal_ui.cpp` when it builds the
  float value. Other effects can adopt them by storing their parameters in real units (`minValue`/`maxValue` are ints) and setting these fields. Only the Compressor does so far.
* **Compressor parameters** are stored in real units: Level -12..+12 dB (default 0), Ratio 1..20 (default 4:1), Thresh -60..0 dB (default -20), Attack 1..100 ms (default 10, knob log),
  Release 10..1000 ms (default 200, knob log). The menu steps linearly across each range even for the log knobs.

## Things to know before building

* **SRAM is at 91.7%** (450,944 of 491,520 bytes, about 40 KB free). It was 84.9% at the start of the block-processing work; the block code, the Dattorro enable
  (a separate commit on `block-processing`, GPL-3.0) and the parameter-metadata fields account for most of the rise. See "Memory and code size" below.
* `Makefile` still has an **uncommitted local change** (`GCC_VERSION_MAJOR_REQUIRED := 13`), as on the previous branch. The author builds with arm-none-eabi-gcc 13.3.
* `libDaisy` is on the commit the repository records (v8.1.0-10). A fork with the uninitialised-`pwm_` LED fix exists (`spamatica/libDaisy`, untracked clone in
  `dependencies/libDaisy_spamatica/`); the project works around that bug in `BaseHardwareModule::InitLeds` (see `HANDOFF_block_processing.md`), so stock libDaisy is fine.
* **Show Debug and Lock Knobs are saved Global Settings checkboxes** (`settings.globalShowDebug` and `settings.globalLockKnobs`, both default off; `SETTINGS_FILE_FORMAT_VERSION` is 15).
  **Show Debug** draws the CPU percentage and the averaged input/output peak level in dB on the default effect screen. **Lock Knobs** ignores the knobs so parameters are changed from the
  display only (the knobs are still sampled, just never applied, so their cached state is valid when unlocked). The old text-page takeover that `useDebugDisplay` used to trigger is now behind
  the compile-time constant `useDebugTextScreen` (default false).
* Untracked `BlockProcessing_step*.diff` files are the author's own snapshots and can be ignored.

## Candidate next steps

1. **Measure and calibrate Crusher and Dual Echo.** Their defaults were only moved to 0.5; the actual gain at default has not been measured. Add `tests/host/levels/test_level_<name>.cpp`
   (copy `test_level_overdrive.cpp`) and a `run <name> ...` line in `run_level_tests.sh`.
2. **Other effects that have a Level-like control but were not measured:** Drum, IR (Level range 0..2, input scaled 0.5), SciFi (Level 0..1), Pluck Echo, Looper (Input Level, Loop Level), the keyboard
   effects (disabled in the default build), PEQ/GEQ gains.
3. **Effects with no Level parameter** (loudness depends on their mix settings): Autopan, Chopper, Chorus, Delay, Flanger, GEQ, Granular Delay, Harmonic Tremolo, Metro, Modulated Tremolo, MultiDelay,
   Noise Gate, Phaser, Pitch Shifter, PolyOctave, Reverb, Reverb Delay, Dattorro, Spectral Delay, Tape Delay, CloudSeed (Dry Volume only). Worth measuring at default mix to see whether any attenuates.
4. **Listen.** Distortion, Overdrive, NAM and the Level defaults are calibrated only by measurement. The NAM gains differ a lot from the old hand-tuned ones (BE-100 and Mesa were set low by ear),
   which suggests real playing may be hotter than the -12 dBFS reference, so check that first and adjust `kReferenceInput` if needed.
5. Optionally adopt real-unit display for other effects (delay times in ms, filter frequencies in Hz) using the new `unit`/`decimals` fields.

## Memory and code size

Measured on the linked firmware (`arm-none-eabi-size -A` and `arm-none-eabi-nm -S` on `build/guitarpedal.elf`):

| Memory | Used | Contents |
|---|---|---|
| **SRAM** (AXI, 480 KB) | 451 KB, 91.7% | About 448 KB is **program code and read-only data** (`.text`); only about 3 KB is data. The app boots from SRAM (`BOOT_APP`, SRAM linker script) |
| **SDRAM** (64 MB) | 49.2 MB, 73% | File-scope buffers: Looper 2 x 11.5 MB, eight delay lines of 1.5 MB each (Delay, Reverb Delay, MultiDelay, Tape Delay), CloudSeed pool 7 MB, Dattorro arena 1 MB, smaller ones |
| **RAM_D2** (256 KB) | NAM history 77 KB, audio DMA buffers, the heap | `new` uses newlib's heap, which starts after `.sram_d2_bss`. The 32 effect objects total about 54 KB (Chorus 19 KB, NAM 9 KB, IR 9 KB, Granular 6 KB, Flanger 4 KB). The heap limit was not verified at runtime |
| **DTCM** (128 KB) | 56 KB static | NAM weights and hot state, storage, UI |

**SRAM is a code-size problem.** Creating effects dynamically on switch would not help it (the code is always resident). It would only help SDRAM (a shared arena reset on
effect change could bring the peak down to about the largest single effect, roughly 23 MB for the Looper) and a little of the heap. That is a sizeable refactor: storage, presets, the layout
hash and the menus need every effect's metadata and parameter values at boot, the audio callback must be kept away from an effect being (re)initialised, effect chains share children, and the
Looper would lose its recording on a switch. Not done; worth it only if SDRAM becomes the limit.

**Ideas for reducing SRAM, in rough order of effort:**
1. **Stop GCC cloning functions.** `-Ofast` makes interprocedural-constant-propagation clones (`[clone .constprop.0]`) of functions called with a constant argument. SciFi's and Tape Delay's
   `ProcessSamples` (called with `n = 1` by `ProcessMono`) were cloned; marking them `__attribute__((noclone))` saved 6,016 bytes (done, commit `51b6069`). Remaining clones >= 400 bytes are
   CloudSeed's `ReverbChannel::Process` (3.2 KB) and `AllpassDiffuser::Process` (0.9 KB), third-party code left alone. List them with
   `arm-none-eabi-nm -S -C build/guitarpedal.elf | grep clone`.
2. **Compile cold code with `-Os`** (UI, hardware modules, settings), keeping `-Ofast` for the DSP. Not tried.
3. **Drop what is not used:** the five NAM models are 37 KB of read-only weights (7.5 KB each), Dattorro about 25 KB, CloudSeed about 24 KB of code.
4. **Execute from QSPI flash** (`BOOT_QSPI`) or put the read-only model weights in QSPI. `nam_a2_runtime.h` notes that QSPI-placed data only works under `BOOT_QSPI`, because the
   bootloader loads just the SRAM image under `BOOT_SRAM`. This changes how the firmware is flashed and has a speed cost; not investigated.

Largest single items in SRAM (code and read-only data): the `bkshepherd::` effect and UI code about 114 KB in total, libDaisy about 38 KB, NAM weights 37 KB, CloudSeed 24 KB, Dattorro 25 KB.
