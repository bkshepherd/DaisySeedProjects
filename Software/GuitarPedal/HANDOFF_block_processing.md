# Hand-off: block processing and CPU optimisation work

Written so a fresh session or developer can continue without the original conversation. Everything below is on branch
`block-processing` (branched from `main` at 053079b).

## Goal

Reduce CPU load of the pedal's `AudioCallback` (48-sample blocks at 48 kHz, so one callback every 1 ms). Started from a question
about moving the per-sample `ProcessMono`/`ProcessStereo` interface to whole buffers.

## What was built

* **Block interface** on `BaseEffectModule`: `ProcessMonoBlock(in, outL, outR, n)` and
  `ProcessStereoBlock(inL, inR, outL, outR, n)`. The default implementations loop over the old per-sample methods, so an effect
  works unchanged until it overrides them. In-place processing is allowed (read inputs before writing outputs).
* **`AudioCallback` restructure** (`guitar_pedal.cpp`): the active effect runs over the whole block first, then a per-sample
  loop does mute/bypass timing and the master crossfade. Effect on/off is decided once per block.
* **Block overrides** for NAM, IR, CloudSeed, Reverb, Dattorro, SciFi and Tape Delay.
* **CPU readout** on the default effect screen (originally the `showCPU` constant; now the saved "Show Debug" setting, see `HANDOFF_level_calibration.md`).
* **`disableKnobs` flag** in `guitar_pedal.cpp` (default `false`; replaced by the saved "Lock Knobs" setting): when `true`, knobs are neither read nor applied, so parameters stay
  at defaults and CPU readings are steady. Flip to `true` when measuring.
* **Host tests** in `tests/host/` (see its README). Run `tests/host/run_tests.sh` after any change to these modules.

## Measurements (CPU readout, knobs enabled unless noted)

| Effect | Before | After | Change | Cause |
|---|---|---|---|---|
| Tape Delay | 63.8% (knobs off) | 39.5% | -24.3 | Filter `SetFreq` calls (sinf/cosf/sqrtf/powf, 8 per sample) hoisted out of the loop, plus parameter-derived values |
| CloudSeed | about 54% | 48% | -6 | Block size 4 inside the reverb (see below) |
| SciFi | 53.6% (knobs off) | 48% | -5.6 | Parameter reads, `SetFeedback`/`SetLpFreq`/`SetDrive` hoisted |
| NAM | 68.9% | 66.5% | -2.4 | Mostly the callback restructure; the zero-latency direct path was only -0.3 |
| IR | 22% | 19.8% | -2.2 | Direct path |
| Reverb | 14.6% | 12.9% | -1.7 | Hoisted parameters, float instead of double |
| Dattorro | 31% | 29.3% | -1.7 | Hoisted mix gain |

Callback overhead with the effect bypassed is only about 1.5%, so there is no hidden fixed cost; per-effect work is what matters.
The CloudSeed "before" figure was derived from relative readings, not measured directly.

## Findings worth remembering

1. **The gains come from per-sample setup work done once per block, not from blocks as such.** Biggest wins: coefficient
   recalculation (`Svf::SetFreq`, `Tone::SetFreq`, `ReverbSc::SetLpFreq`, `Overdrive::SetDrive`) and repeated `GetParameter*` calls inside
   per-sample code. Effects whose DSP is already block-based inside (NAM, IR) gained almost nothing from the interface itself.
2. **CloudSeed's block size is a cache-pressure sweet spot.** Sweep (relative to the original per-sample): 48 = +10, 16 = about -1,
   8 = about -5, 4 = best (48%), 2 = about 51%. Set in `dependencies/CloudSeed/ReverbController.h` (`bufferSize`). Larger blocks make
   the per-stage scratch buffers outgrow the 16 KB D-cache. The block size also delays the internal feedback by (block size - 1)
   samples, so the sound differs slightly from the original; level and decay match closely.
3. **NAM's 48 is structural to its generated runtime** (`Effect-Modules/Nam/nam_a2_runtime.h`, `kBlockSize`) but history rings are
   independent of block size, so other even sizes are possible. Not tried: the layer-by-layer loops probably favour large blocks.
   The staged per-sample path adds one block (1 ms) of latency; the block path runs the model directly with no added latency.
4. **IR direct path** also removes that block of latency (same idea as NAM).
5. **Double-precision constants** (`1.0`, `0.8`, `0.6` in expressions) are still used in SciFi's overdrive/mix maths, to keep results
   bit-identical. Converting to float is a possible further 1-3 points but needs a tolerance in the test.

## Things to know before building

* **SRAM is at about 92%** (about 37 KB free) because code lives in SRAM. Each change added a little. The Dattorro enable is the
  largest single item if space gets tight.
* **Dattorro is enabled** by a separate commit ("Enable Dattorro reverb"). Its code is **GPL-3.0-or-later**: a firmware binary with it
  enabled must be distributed under GPLv3 (or not distributed). Revert that commit before sending anything upstream.
* **Compiler: GCC 13 works, but needed a fix.** With arm-none-eabi-gcc 13 the LEDs stopped working because libDaisy's `Led::Init()` never
  initialises its software-PWM phase (`pwm_`), so a stack-allocated `Led` started with leftover stack garbage (huge or NaN keeps the LED
  permanently off). `BaseHardwareModule::InitLeds` now zeroes the object first (confirmed on hardware). This is an upstream libDaisy bug.
  A GCC 13 warning scan of the project found no undefined-behaviour-class warnings; the only "may be used uninitialized" warnings are in
  Dattorro's `AllpassFilter` and the third-party `q` pitch detector (tuner), which are the first places to look if something else
  misbehaves on a newer compiler. Knobs, switches and encoders use the same stack-then-copy pattern but initialise what they use.
* `Makefile` still has an **uncommitted local change**: `GCC_VERSION_MAJOR_REQUIRED := 13` (the author's toolchain is
  arm-none-eabi-gcc 13.3). It was a local change before this work started and was intentionally not committed.
* Untracked `BlockProcessing_step*.diff` files in this directory are the author's own snapshots; ignore or delete.
* The CPU readout is shown through the saved "Show Debug" setting (off by default); it draws on every effect that uses the default screen.

## Candidate next steps (by measured CPU, knobs-disabled baselines)

| Effect | CPU | Notes |
|---|---|---|
| Spectral Delay | 52.0% | One sample at a time into an STFT (`Util/STFT/`); not yet read. `ParameterChanged` rebuilds per-bin tables with sin/rand on every knob move |
| PolyOctave | 33.8% | Already works in 6-sample chunks; SciFi shares the octave code |
| Dual Echo | 24.8% | Not read |
| Delay | 21.2% | 20 parameter reads in the process bodies; likely helper functions hide setup work (as in Tape Delay) |
| MultiDelay | 21.1% | Not read |
| Crusher | 20.4% | Not read |
| Others | 10-16% | Chorus, Flanger, Pitch about 10%; of that only about 1.5% is callback overhead |

A rough static scan (expensive calls inside `ProcessMono`/`ProcessStereo` bodies) flagged Autopan (sinf/cosf per sample), Looper
(3x `SetFreq`), Reverb Delay (42 parameter reads, `SetFeedback`/`SetLpFreq`). It misses helper functions, so read each effect fully.

Method that worked each time: read the effect -> share the per-sample code in one routine that takes a length (`ProcessMono` calls it
with 1, block overrides with n), hoisting everything that depends only on parameters while keeping expression types identical ->
host test against the original from git -> measure on the pedal.

## Not verified

* Nothing here was verified by ear; only on the PC (bit-exact or within rounding, except CloudSeed's slightly different timing) and by
  CPU readings on the pedal. CloudSeed at block size 4 is the one place the sound differs, so it is the first thing to listen to.
* The `AudioCallback` restructure (crossfade, mute and relay timing, LEDs) is not covered by host tests.
