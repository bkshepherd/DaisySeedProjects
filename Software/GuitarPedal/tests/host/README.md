# Host-side equivalence tests

These tests run on a PC (no Daisy hardware, no ARM toolchain) and check that the **block-processing path** of an effect
module produces the same audio as its **per-sample path**, or as the original implementation in git.

They exist because block-processing changes cannot be checked by ear when you are away from the pedal.

```bash
tests/host/run_tests.sh                 # needs g++ (C++20) and python3; compares against the 'main' branch
BASE_REF=053079b tests/host/run_tests.sh   # compare against another ref
```

`BASE_REF` must point at a commit that still has the **original per-sample code** (default: `main`). Binaries and dumps go to
`/tmp/gp_host_tests` (override with `BUILD_DIR`). Each step prints OK or MISMATCH and the script exits non-zero on any failure.

## What is checked

| Test | Compares | Expected |
|---|---|---|
| Reverb | block vs per-sample, mono / stereo / in-place | within 1e-6 (the block path uses float where the old code used double for `1.0 - mix`) |
| Dattorro | block vs per-sample, mono / stereo / in-place | bit-exact. Includes a negative control (shifted parameter automation) proving the harness detects differences |
| IR | direct block path vs per-sample path | block output equals per-sample output delayed by exactly 48 samples; odd chunk sizes; fallback for oversized chunks |
| NAM | direct block path vs per-sample path | block output equals per-sample output delayed by 48 samples, for EQ and IR-cab on/off |
| SciFi | original (`BASE_REF`) vs new | bit-exact, 4 parameter sets with automation |
| Tape Delay | original (`BASE_REF`) vs new | bit-exact, all modes (fixed / multi / single / lo-fi), head configs, tap divisions, mono and stereo input |
| CloudSeed | original (`BASE_REF`) vs new | bit-exact when the new module code is built with block size 1; at the shipped block size only finite and close in level (the block size shifts the reverb's feedback timing by a few samples) |

## How it works

The real module `.cpp` files are compiled for the host against stubs:

* `stub/daisy_seed.h` - minimal libDaisy replacement (inert display types, `DSY_SDRAM_BSS` etc. defined empty).
* `stub_cmsis/arm_math.h` - a plain streaming FIR standing in for CMSIS-DSP, used by the IR and NAM tests. It only exercises
  the module's own staging logic, not CMSIS itself.

Notes for adding a test for another module:

* Compare against the original from git (`git show $BASE_REF:...`) rather than against the new per-sample path, so a mistake that
  affects both paths cannot hide.
* Change parameters only at block boundaries, identically in both paths, as the pedal's main loop effectively does.
* Make sure the wet path is actually active (check the tail after the input stops), otherwise a zero difference proves nothing.
* The NAM model is a process-wide static with recurrent state, so each run needs its own process.

## Default-level tests

`tests/host/run_level_tests.sh` measures how loud each effect is at its **default parameters** relative to its input, using the real
module code. Rule of thumb for the project: between **0 dB and +6 dB** at default settings (never attenuating), with distortion-like effects
(distortion, compression, amp models) aiming for **+3 dB**.

* Signal: peak-normalised test tones at -24, -12 and -6 dBFS peak. **-12 dBFS peak is the reference level** the rule is applied to (a typical
  strummed guitar into a line input); change `kReferenceInput` in `levels/level_common.h` if your real input level differs.
* Two signals: pure sines (220 Hz and 1 kHz) and a guitar-like harmonic series (110 Hz and 330 Hz fundamentals). Amp models are checked on the
  guitar-like signal because their gain depends strongly on pitch.
* The pass check uses the **mean RMS gain over the two test frequencies** at the reference level, within the window and within a tolerance of the
  target. It also fails if the output reaches full scale at any measured level. The gain at other levels is printed for information: nonlinear
  effects have a fixed output ceiling, so their gain falls as the input gets louder.
* To add an effect: write `levels/test_level_<name>.cpp` (see `test_level_distortion.cpp`), then add a `run <name> <sources...>` line to the script.

Calibrated so far: Distortion (`normalizeVolume` factors in `distortion_module.cpp`), the NAM amp models (`outputGain` in
`Effect-Modules/Nam/model_data_nam_a2.h`), the Overdrive (Level range in `overdrive_module.cpp`) and the Compressor (default Ratio, Threshold,
Attack, Release and Level; its parameters are stored in real units and `Init()` applies them, which the test checks).
