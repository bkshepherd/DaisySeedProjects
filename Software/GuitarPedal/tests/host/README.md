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
| Tape Delay defaults | startup state (only `Init()` run) vs the state after touching Mix to its default | regression test for a past bug (see below); includes memory pre-filled with zero / garbage / NaN bytes and a one-step Mix change |
| Venus | sliced STFT vs one-shot `Fourier` (exact), module block vs per-sample | exact; the sliced pipeline matches the original `Fourier` delayed by a hop until the original's slot-reuse drift begins |
| Earth | arena size, octave, freeze, overdrive swell, finite output, block vs per-sample | block and per-sample paths identical; behaviour checks on the PC |
| Balnea | block vs per-sample (mono, stereo, 256-sample blocks, every option, taps, MIDI note), reverse line vs the original `delayline_reverse.h`, delay time, tap tempo, self-oscillation | block and per-sample identical; the reverse line is bit-exact to the original (kept in `tests/host/balnea/`) |
| Pluto | block vs per-sample for every effect and speed mode; loopers (playback at 1x, 2x and backwards equals the recording, pause, clear, frippertronics, footswitch target), effects, LED | block and per-sample identical; loops equal the recorded input to 1e-6 |
| Cassini | block vs per-sample; the sampler (record, pitch of every key, one-shot/looped, trims, backwards, banks, velocity, release), looper, filter, LED | block and per-sample identical; measured pitches within 3% of the key's |
| Mars | the GRU network vs the original's output on RTNeural (reference data in `tests/host/mars/`), block vs per-sample for each model/cabinet/delay mode, controls, delay time and second tap | exact maths agrees to 2e-5, the fast tanh/sigmoid to 2% of the peak |
| Mercury | the pico WaveNet vs the original's output (RTNeural-NAM, reference data in `tests/host/mercury/`) for all nine models, the short prewarm vs the original's 16384 samples, block vs per-sample, EQ bands, model change | network within 2e-5 of the original; the prewarm identical |
| CloudSeed | original (`BASE_REF`) vs new | bit-exact when the new module code is built with block size 1; at the shipped block size only finite and close in level (the block size shifts the reverb's feedback timing by a few samples) |

Note on the Tape Delay equivalence step: the original code has the Mix bug fixed in `test_tape_delay_defaults.cpp`, so `run_tests.sh` applies the same two-line fix to its copy of the
original before comparing. That keeps the step checking only that the refactor itself changed nothing; if the original ever changes so the patch no longer applies, the script stops with a clear message.

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

* Signal: peak-normalised test tones at -49, -37, -25 and -12 dBFS peak. **-37 dBFS peak is the reference level** the rule is applied to (what the
  pedal's on-screen input level readout shows for normal playing); change `kReferenceInput` in `levels/level_common.h` if your real input level differs.
* Two signals: pure sines (220 Hz and 1 kHz) and a guitar-like harmonic series (110 Hz and 330 Hz fundamentals). Amp models are checked on the
  guitar-like signal because their gain depends strongly on pitch.
* The pass check uses the **mean RMS gain over the two test frequencies** at the reference level, within the window and within a tolerance of the
  target. It also fails if the output reaches full scale at any measured level. The gain at other levels is printed for information: nonlinear
  effects have a fixed output ceiling, so their gain falls as the input gets louder.
* To add an effect: write `levels/test_level_<name>.cpp` (see `test_level_distortion.cpp`), then add a `run <name> <sources...>` line to the script.

Calibrated so far: Distortion (`normalizeVolume` factors in `distortion_module.cpp`), the NAM amp models (`outputGain` in
`Effect-Modules/Nam/model_data_nam_a2.h`), the Overdrive (Level range in `overdrive_module.cpp`) and the Compressor (default Ratio, Threshold,
Attack, Release and Level; its parameters are stored in real units and `Init()` applies them, which the test checks).

## Startup-state audit

`tests/host/run_defaults_audit.sh` checks that every effect applies the state derived from its parameter **defaults** when only `Init()` has run, as on the pedal. `ParameterChanged()` only fires
when a value changes, and first-boot storage equals the metadata defaults, so an effect that derives state from a parameter only inside `ParameterChanged()` never applies that default at startup
(the Compressor and Tape Delay had this; the menu then shows values that are not what is running).

For each of the 39 effect modules it runs a test signal through three instances, one process each, and compares the outputs: **startup** (`Init()` only), **touched** (`Init()`, then every parameter
moved away from its default and straight back, which fires `ParameterChanged` for each) and **poison** (startup over memory pre-filled with 0xFF bytes, to expose reads of uninitialised members). A second
startup run checks determinism (the harness makes `rand()` constant, so random-table effects compare fairly). `--bisect <effect_file_stem>` touches one parameter at a time to show which default is not applied.

Findings that are known and understood are listed in the `KNOWN` variable of the script (CloudSeed: `Init()` forces a line count of 3 that selecting a preset later restores; Modal Keys: an uninitialised DaisySP
voice, in an effect that is disabled in the default build). The script fails only on something new. Limits: effects that output nothing without their own input (Tuner, FM Keys, the other keyboard effects
without MIDI notes) are barely exercised, and an effect whose behaviour depends on state the harness does not reach (tempo, MIDI) is only tested on its audio path. It takes a few minutes the first time
(it builds the shared objects), and less afterwards.

`tests/host/compare_earth_engine.sh` (optional, needs a clone of the FunBox project, `FUNBOX_DIR`) compares the Dattorro engine of the FunBox Earth pedal with this repository's in impulse
responses; see `Effect-Modules/Earth/README.md` for what it found.


## Reference data from RTNeural (Mars, Mercury)

`tests/host/mars/gru_reference.bin` and `tests/host/mercury/wavenet_reference.bin` are the outputs of the original networks (RTNeural and RTNeural-NAM with Eigen, from a clone of
<https://github.com/GuitarML/Funbox>) for a fixed input; the tests compare the modules' own plain C++ networks to them. They only need regenerating if the model data changes: the generators
(`gru_reference_gen.cpp`, `wavenet_reference_gen.cpp` in the same directories) have the command line in their header comments.

## NAM against the reference implementation

`nam_reference/run_nam_reference.sh` compares the NAM effect with NeuralAmpModelerCore (needs a recent checkout, see `NAM_CORE` in the script; not part of `run_tests.sh`). For every file in `Effect-Modules/Nam/models/` it builds a reference `.nam` (the A2 Lite network in `nam_reference/a2_lite_template.json` with that model's weights), renders a guitar-like signal (plucked notes, noise, a loud burst; 4 s) through the reference, through `A2Player` alone and through `NamA2Module` (Gain and Level at unity, EQ and cab IR off, the model's `kOutputGain` divided out) and reports the signal/error ratio after the first 0.1 s. Last run: all five models 106 .. 133 dB (maximum difference below 2e-5) for both paths.
