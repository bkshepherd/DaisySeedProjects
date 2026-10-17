# Changelog

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
