# Changelog

## 18 October 2026

### Changed behaviour
- **Tap tempo needs three taps.** The second footswitch's tap tempo (Delay, Tape Delay, Reverb Delay, Dual Echo, AutoPan, Chopper, Harmonic Tremolo, Modulated Tremolo, Drum and Metro) used to be a double-tap. It now takes three taps, each within 2 seconds of the one before, and sets the tempo from the interval between the last two; every further tap of the series adjusts it again. Balnea keeps its own tap tempo.
- **Reverb Delay delay time:** the range is 50-1500 ms (it was 50-4000 ms) and the default is 400 ms (it was 1828 ms). Saved settings store the knob position, so a saved delay time now plays shorter.
- **Balnea delay time:** the range is 100-2000 ms (it was 4000 ms at most), and its tap window is 2 seconds (it was 4).
- **Harmonic Tremolo LED:** the left LED now blinks once per LFO cycle, which is once per beat after a tempo tap, and is on for about 27% of the cycle like the delay effects. It used to dim and brighten with the LFO, scaled by the depth, so it barely moved at low depth.

### Fixed
- **Reverb Delay tempo LED blinked about 24 times too slowly.** The LED oscillator was advanced inside the LED function, which is called only twice per audio block, and its rate was only set after the Delay Time knob had been moved. It now advances with the audio and follows the delay time from start-up (one blink per two delay times, as in the Delay module).
- **The menu could undo a parameter an effect had just changed itself.** The menu writes its cached values back into the effect on every main-loop pass; a footswitch press (handled in the audio callback), a MIDI message or a tempo tap in that window was overwritten with the menu's older value. This is why the Crusher's Pitch Tracking and the Uranus Hold toggles worked only sometimes. The menu now takes the effect's value when the effect has changed it.
- **Reverb Delay out of memory:** it no longer stops at start-up with "OUT OF MEMORY". The module held its reverb engine inside the object (397 KB on a heap with about 147 KB free); the reverb is now a static in SDRAM, as in the Reverb, SciFi and Tape Delay modules. Reverb Delay can be used alone or in a chain, and `effects-memory.json` now has its heap figure, so the effect tool can warn about it.
- **Delay and Reverb Delay can be built together:** the two headers defined the same names (`MAX_DELAY_REV`, `MAX_DELAY_SPREAD`, `delay_spread`) and both sources defined global delay lines with the same names. Reverb Delay's names are now its own and its buffers are file-local. The conflict is removed from `effects.json` and the "everything" preset includes Reverb Delay. A host test checks that each of the two gives the output it gives alone.

### Performance
- **Reverb Delay block path:** the parameters are read once per block instead of once per sample. The output is identical to the per-sample path (host test, 13 configurations). Measured on the pedal: 35% / 40% CPU down to 26% / 33%.

### NAM
- **Level calibration in `update_models.sh`:** run by hand in a terminal it asks whether to calibrate the output gains (`--calibrate` and `--no-calibrate` skip the question; the Makefile and the test scripts use `--no-calibrate`). It measures every model on the host, sets `kOutputGain` in each model file so all models give the same level, and measures again to check.
- **Louder models:** the default target is +12 dB, and the five models are calibrated to it, so the NAM models are about 3 dB louder than with the old +9 dB calibration. The NAM level test expects +12 dB.
- **`nam_to_cpp_array.py`:** fixed a syntax error from an earlier edit; new model files start at an output gain of 1.0.

### Documentation
- **`SECOND_FOOTSWITCH_AND_LED.md`:** how every effect uses the second footswitch and the left LED, with a list of inconsistencies to look at later. The user manual describes the three-tap tempo.
- **CPU load on the pedal:** measured figures for NAM, the distortion types and several other effects, and what they mean for chains, are in `PERFORMANCE_AND_MEMORY.md` (section 9).

## 12–17 October 2026

### Breaking or reset-worthy
- **Saved settings reset:** the settings format went to version 13, then 16, then 18, so each of those firmware updates resets saved settings on the first boot. The `funbox-` rename and the alphabetical order of `effects.json` change the layout once more.
- **Effect order and MIDI:** the effect order is now alphabetical, which changes the menu order and the MIDI Program Change number of almost every effect.
- **FunBox effect ids:** the FunBox effects are now `funbox-venus`, `funbox-jupiter` and so on, in `effects.config`, the presets and the tool commands.
- **NAM model order:** the NAM models are now listed alphabetically (Ampeg, BE-100, JCM800, 1959BJA, Mesa), and the default model is Ampeg instead of BE-100. Saved presets store the model by position, so they will point at a different model.

### New effects
- **FunBox ports:** Venus (spectral reverb), Earth (plate reverb with an octave, GPLv3 opt-in), Jupiter, Saturn, Neptune, Uranus, Balnea, Pluto, Cassini, Mars and Mercury. The newest five are marked "need testing on hardware".
- **Duck Compressor:** now a working side-chain compressor, with a Key Input parameter and a gain-reduction readout.
- **Test Tone:** a signal generator (sine, square, triangle, noise) for calibrating levels.

### Settings and display
- **New global settings:**
  - **Gain:** In Gain and Out Gain, in dB (−24…+24).
  - **Trails:** lets delay and reverb tails ring out after the effect is turned off.
  - **Lock Knobs:** on by default.
  - **Show Debug:** now a saved setting.
- **Display readouts:**
  - the CPU percentage, in small print;
  - averaged input and output peak levels;
  - the heap readout and an out-of-memory guard.
- **Display text and menus:**
  - Long texts fit the display (auto-fit font, cut with `~`, an 8x13 bold font), and there is a text lint.
  - Parameters show their units and decimals in the menu.
- **Fixes:**
  - Global Settings: Back no longer overwrites the Reboot text.
  - Balnea: the Time menu step is fixed, and tap tempo now takes three taps.
  - Venus: Oct Up+Dn now also shifts down.

### Performance and levels
- **Block processing:** the active effect now runs on whole blocks. This covers NAM (no added latency), IR, CloudSeed (blocks of 4), Reverb, Dattorro, Tape Delay and SciFi. Host tests check that the block path equals the per-sample path.
- **SRAM:** stopped GCC cloning `ProcessSamples`, which saves 6 KB of SRAM.
- **Level calibration:** default levels are calibrated against a −37 dBFS reference (distortion types, NAM models, Crusher, Dual Echo, Overdrive, Compressor), with host level tests.
- **Startup defaults:** several effects now apply their parameter defaults at startup (Compressor, Tape Delay, Spectral Delay, Drum, PluckEcho, CloudSeed), with a startup-state audit.
- **Fixes:** LEDs now work with GCC 13.

### Build tool (`effects_config.sh`)
- **Effect selection:**
  - Effects are chosen in `effects.config`, with presets in `configs/`.
  - There is an interactive full-screen selector, and it asks first before allowing a GPL effect.
  - `effects_config.sh` starts the tool.
- **Chains and memory:**
  - Chains can be made in the tool and are generated into the build.
  - It reports the memory budget of a selection, with a matrix of which effects fit together.
  - It generates the sources and defines the build needs, so only the chosen effects are compiled.
- **Boot results:** `observed` boot results are recorded in `effects-memory.json`.
- **Fixes:**
  - The CloudSeed pool size is now a define that rebuilds everything when it changes.
  - Jupiter plus Neptune now builds.

### NAM
- **Model files:** each model is a file of its own in `Effect-Modules/Nam/models/` (weights, name and output gain). `update_models.sh` lists them, and `nam_to_cpp_array.py` writes a model file. Adding or removing a model needs no other edit.
- **Reference check:** a new host test (`tests/host/nam_reference/`) compares the NAM effect with NeuralAmpModelerCore. All five models match to 106–133 dB signal/error.

### Documentation
- **New:** `GENERATE_FIRMWARE.md` covers the tool, the memory limits, the GPL opt-in and chains.
- **Updated manual:** `USER_MANUAL_125B.md` no longer mentions a default firmware, and it lists the missing settings (Lock Knobs, Show Debug, Trails, In Gain, Out Gain, Erase All).
- **Hand-off and performance notes:** block processing, level calibration, the FunBox conversions, performance and memory.
