# Handover: the distortion family

Eight new effects (Preamp, Screamer, Muff Fuzz, Wavefolder, Exciter, Bass Dist, Octa Fuzz, Tape Sat), added in October 2026 as lighter alternatives to the existing Distortion and to a NAM model. This note is for whoever continues the work. What was measured on the pedal and what was only measured on the PC is marked.

## State

* **Code:** one `.h`/`.cpp` pair per effect in `Effect-Modules/`, a shared base class `Effect-Modules/mono_effect_module.h` and the DSP pieces in `Util/dist_dsp.h`. They are in `effects.json` and `effects-memory.json`.
* **Preset:** `configs/distortions.config` selects all of them plus Distortion, Overdrive and Crusher (11 effects, about 250-265 KB of code, i.e. a little over half of the code space, an estimated 18 KB of heap, no SDRAM). It built without errors; the firmware has been flashed and run for the CPU figures below, but nothing else about the sound has been judged by ear in this project.
* **Measured on the pedal (Show Debug, average CPU):** every new effect 4-5%. After block processing, Distortion Hard Clip went from 18% to 12% and Overdrive is about 4%.
* **Menu order and MIDI:** the effect list is alphabetical (`effects.json`), so the eight new ones moved the Program Change number of every effect after them and reset saved settings once more.

## The effects

All parameters are stored as 0..1 and shown through `displayFn`. Level defaults to 0 dB everywhere. All `midiCCMapping` are -1 (no CC assigned yet). Knobs 0-5 are mapped in the order of the first column.

| Effect (`id`, class) | Parameters (default) | Signal chain |
|---|---|---|
| **Preamp** (`preamp`, `PreampModule`) | Gain 4.0, Bass, Mid, Treble (0 dB), Voicing (Crunch; Clean, Crunch, Lead), Level | High-pass, asymmetric soft clip (stage 1), coupling high-pass, low-pass; stage 2 for Crunch and Lead; low shelf, peaking and high shelf tone stack (the mid frequency depends on the voicing); output low-pass |
| **Screamer** (`screamer`, `ScreamerModule`) | Drive 6.6, Tone 2.6 kHz, Level, Mode (Classic, Asymmetric, Hard) | `x + clip(g * highpass720(x))`: only a high-passed copy is clipped and the clean signal is added, as in the circuit; op-amp low-pass; tone low-pass |
| **Muff Fuzz** (`muff_fuzz`, `MuffFuzzModule`) | Sustain 6.0, Tone 50%, Volume, Mode (Classic, Fat, Bright: the tone corner 1.5 kHz, 800 Hz, 3 kHz) | Soft-clip booster, two more soft-clip stages (each band-limited), tone as a low-pass/high-pass crossfade |
| **Wavefolder** (`wavefolder`, `WavefolderModule`) | Fold 30x, Symmetry 0%, Tone 7 kHz, Mix 100%, Level | `sin(a (x + bias))` with first-order antiderivative anti-aliasing (two `cos` per sample), DC blocker, tone |
| **Exciter** (`exciter`, `ExciterModule`) | Drive 28 dB, 2nd 50%, 3rd 30%, (4th, 5th 0% in the menu only), Focus 1.1 kHz, Mix 50%, Level | The part above Focus, driven and clamped to +-1, through Chebyshev T2..T5 with a level each; DC blocker; added to the input |
| **Bass Dist** (`bass_dist`, `BassDistModule`) | Drive 5.0, Crossover 128 Hz, Clip (Soft; Soft, Hard, Fuzz), Lows 0 dB, Tone 5.2 kHz, Level, (Mix 100% in the menu only) | Fourth-order Linkwitz-Riley split; only the upper band is clipped; bands recombined |
| **Octa Fuzz** (`octave_fuzz`, `OctaveFuzzModule`) | Fuzz 6.0, Octave 50%, Tone 3.2 kHz, Level | Gain, full-wave rectified copy blended in (an octave up), hard clip with ADAA, tone |
| **Tape Sat** (`tape_sat`, `TapeSatModule`) | Drive 29 dB, Warmth 4.8 dB, Tone 8.6 kHz, Mix 100%, Level, Mode (Tape, Trafo) | Low shelf up, soft clip with unity small-signal gain, low shelf down (so the lows saturate more), tone; Tape has a small bias, Trafo none and more warmth |

## How they are built

* **`MonoEffectModule`** implements `ProcessMono`, `ProcessStereo` and both block entry points on top of one virtual `Process(in, out, n)` (n >= 1, in-place safe). The left input is processed and both outputs get the result. The result does not depend on how the samples are split into calls (tested bit-exact).
* **Parameters:** `ParameterChanged` only sets a dirty flag; `Process` recomputes coefficients at the start of the next call (so the audio thread never reads half-written coefficients). Gain-like values are smoothed per sample (about 10 ms).
* **`Settle()`** (end of every `Init`) runs 0.15 s of silence through the effect so that DC blockers and biases start at rest. Without it the Exciter and the biased effects thump when they start.
* **Anti-aliasing:** no oversampling. `SoftClipAdaa` and `HardClipAdaa` in `Util/dist_dsp.h` use first-order antiderivative anti-aliasing (one division per sample). This is why they cost 4-5% where the old Distortion costs 12-23%.
* **Level matching:** each effect has a `kMakeup` constant (an array for effects with modes) in its `.cpp`, calibrated so the mean RMS gain at the project's reference level (-37 dBFS peak sine) is +6 dB (distortion types) or +3 dB (Exciter, Tape Sat), the rule in `tests/host/README.md`. The Preamp's Lead is the exception: it is matched to Crunch at -12 dBFS peak, where both saturate (at -37 dBFS Lead is already saturated and Crunch nearly clean, so the rule made Lead about 10 dB quieter on a guitar); its gain at the reference level is about +16 dB. Current values: preamp = 0.7736, 0.0856, 0.0797, screamer = 0.0829, 0.0885, 0.1148, muff_fuzz = 0.0420, 0.0420, 0.0420, wavefolder = 0.0454, exciter = 0.7413, bass_dist = 0.1304, 0.1941, 0.1432, octave_fuzz = 0.0356, tape_sat = 1.4859, 1.4825.
* **Top of the Drive knob:** `distdsp::topBoost` multiplies the drive gain from the default knob position up to the end of the knob (1 at and below the default, so the calibrated levels do not change). It is used where the knob at maximum would otherwise not reach full saturation (35 dB of gain compression) before full scale: Preamp Clean (x39 at Gain 10, `Voicing::topBoost`), Screamer (x5; its 6.6 default is on the boosted part, x1.4, and its `kMakeup` is calibrated there), Wavefolder (x13 from the middle of the knob, shown in the Fold value; its 30x default is on the boosted part, and its `kMakeup` is calibrated there) and Bass Dist (x10). With it, the maximum reaches 35 dB at about -12 dBFS peak in every mode (the Screamer measured at 1 kHz; at 200 Hz its Hard mode stays below, since only the part above 720 Hz is clipped and the clean signal is added). Side effect: at full Drive the Bass Dist clips some of a 50 Hz tone (raise the Crossover for clean lows). Not used for the Exciter, whose dry signal is added unclipped.
* **Filters:** `OnePole`, RBJ `Biquad` (low/high-pass, peaking, shelves) in `Util/dist_dsp.h`.

## Tests and how to use them

* `tests/host/run_tests.sh`, the step "Distortion family": `tests/host/test_distortion_family.cpp` checks, for every effect, that the block path equals the per-sample path (bit-exact), that the output stays finite and bounded for random parameters changed while playing, that silence in gives silence out, and 20-odd behaviour checks (tone stack ranges, the mid hump, tone sweep, harmonics, clean lows, octave, compression).
* `tests/host/run_level_tests.sh` ("distortion_family"): the default level of every effect and mode against the window (`levels/test_level_distortion_family.cpp`). **To recalibrate** an effect, run it, read the "reference level" line and multiply its `kMakeup` by `10^((target - measured)/20)`.
* `python3 tools/effects.py lint` checks that names fit the menu font (Tape Sat's mode is "Trafo" for that reason).
* Build: `python3 tools/effects.py preset distortions && make -j4`; flash as usual (`make program-dfu`).

## Open points

* **By ear:** only the CPU was checked on the pedal. Worth listening to: the Wavefolder's aliasing at high Fold, the Octa Fuzz tracking on low strings, the Muff Fuzz modes (they differ only in the tone corner, since the clipping saturates at any Sustain at the reference level), whether the Tape Sat default Drive (29 dB) is audible enough at the reference level.
* **MIDI CC:** none of the parameters has a CC mapping. Pick numbers that do not collide with the other effects (`midiCCMapping` in each `.cpp`; CC0 and CC32 are reserved).
* **Possible additions:** a switch for oversampling on the Wavefolder, a stereo variant (the family is mono in, same out on both channels), more voicings in the Preamp, a gate in the Muff Fuzz.
* **Level at other inputs:** the levels are matched only at the reference level (-37 dBFS peak). With a much hotter input the saturating effects no longer match each other or the dry level; that follows from the rule, not from a fault.

## Related work done at the same time

Committed separately on `effect-polish`: block processing for Distortion, Overdrive, Crusher, Chopper, Modulated Tremolo and Harmonic Tremolo, with the Distortion pre-filter change (updated every 8 samples; not bit-exact against the old code, the level is unchanged; Distortion Hard Clip 18% to 12% CPU on the pedal, Overdrive about 4%), the CloudSeed wet-only LED change, and the release-note rewrite of `CHANGELOG.md`. `effects.config` and `generated/*` are not committed: they hold the user's current selection (the `distortions` preset was applied for the build).

## Known problems in other code (not fixed)

* Two host-test steps fail to link: "CloudSeed: original (main) vs new" and "SciFi: original (main) vs new". They build the old module from `BASE_REF` (default `main`), which already contains the new code; use an older commit as `BASE_REF`.
* `FrequencyDetectorQ` (used by the Crusher) keeps its detector in static variables shared by every user, so two effects with pitch detection in one chain would share it; host tests that compare two Crusher instances have to run them in separate processes.
* The DaisySP `Oscillator` does not initialise `last_out_` in `Init`, so two instances can differ in their first samples (the host tests build modules in zeroed memory).
