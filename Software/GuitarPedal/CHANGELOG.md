# Changelog

## Breaking or reset-worthy
- **Saved settings are reset** on the first boot after an update (settings format 13, 16 and 18; the `funbox-` rename and the alphabetical effect order change the layout again).
- **The effect order is alphabetical** (the eight new distortion effects are in it): the menu order and the MIDI Program Change numbers change for most effects.
- **FunBox effects are renamed** `funbox-venus`, `funbox-jupiter` and so on, in `effects.config`, the presets and the tool commands.
- **NAM models are listed alphabetically** (the default is now Ampeg); saved presets store the model by position, so they may point at another model.
- **NAM models are about 3 dB louder** (output gains calibrated to +12 dB).
- **Tap tempo needs three taps** (it was a double-tap) and takes the tempo from the last two.
- **Reverb Delay delay time** is 50-1500 ms (was 50-4000 ms) with a default of 400 ms; a saved delay time plays shorter.
- **Delay:** Delay Time is 50-2000 ms (was 50-4000 ms) with a default of 400 ms (was 2025 ms), and Delay Mix defaults to 35% (was 50%).
- **Balnea delay time** is 100-2000 ms (was up to 4000 ms) and its tap window is 2 s (was 4 s).
- **Crusher defaults** are 8 bits, 8000 Hz and a 6000 Hz cutoff (were 32 bits, 48 kHz and 20 kHz, which left the sound unchanged), so the effect is audible when first selected. Its Level is now -24 to +6 dB (was a linear 0 to 1, default -6 dB), and the default is level-matched to +6 dB like the other distortions; a saved Level plays louder (about +2 dB at the old default, +11 dB at maximum).

## New effects
- **FunBox ports:** Venus, Earth (GPLv3 opt-in), Jupiter, Saturn, Neptune, Uranus, Balnea, Pluto, Cassini, Mars and Mercury (the newest five need testing on hardware).
- **Duck Compressor:** a working side-chain compressor with a Key Input parameter and a gain-reduction readout.
- **Test Tone:** a signal generator (sine, square, triangle, noise) for calibrating levels.
- **Preamp:** a light amp-like tone (two gain stages, tone stack, Clean/Crunch/Lead), a cheaper alternative to a NAM model.
- **Screamer:** Tube Screamer-style overdrive (mid hump, clean low end) with Classic, Asymmetric and Hard modes.
- **Muff Fuzz:** Big Muff-style fuzz (booster and two clipping stages, scooped-mid tone control).
- **Wavefolder:** an anti-aliased sine wavefolder for metallic, synth-like harmonics.
- **Exciter:** a Chebyshev harmonic exciter (2nd to 5th harmonic, a level for each).
- **Bass Dist:** a distortion that keeps the lows clean (Linkwitz-Riley split, soft, hard or fuzz clip).
- **Octa Fuzz:** an Octavia-style octave-up fuzz.
- **Tape Sat:** tape and transformer saturation with warmth and high roll-off.
- **Spring Reverb:** a physical-modelling spring reverb with 2 to 6 springs of different lengths (4 by default; each costs a third of what it would at the full sample rate, because the springs run at 16 kHz): each echo is the upward sweep of a real spring (the lows arrive first, the highs last, up to the 4 kHz transition frequency), and the reverb is added to the dry signal at a modest level, with Dwell, Decay, Tone, Mix, Drip, a short, medium or long tank, a Springs count and a stereo or mono Output (mono, the default, sums all the springs on both channels).
- **Booster:** a gain stage in front of an amp or a distortion in four types: Clean, Treble (a Rangemaster-style treble booster: the bass is cut, the highs boosted, with germanium-like grit), Mid (a bell boost) and Warm (thick, tube-like, in the spirit of the Echoplex preamp boosters).
- **Chainsaw:** an HM-2-style heavy metal distortion, the "Swedish chainsaw": hard clipping into resonant Low (87 Hz) and High (960 Hz and 1.28 kHz) boosts.
- **Dist Bench:** a test bench with up to four distortion stages in series, each Off or one of Preamp, Screamer, Muff Fuzz, Wavefolder, Exciter, Bass Dist, Octa Fuzz, Tape Sat, Chainsaw, Overdrive and Distortion, with the same Drive, Tone, Char and Level controls for every type (50% is the effect's own default).

## Settings and display
- **Effect chains: Switches** (a parameter of every chain of two or more effects): the two footswitches switch two of its effects on and off (1+2, 1+3 or 2+3), each LED showing its effect; saved with the chain's presets.
- **New global settings:** In Gain and Out Gain (-24 to +24 dB), Trails, Lock Knobs (on by default); Show Debug is now saved.
- **Show Debug readouts:** CPU percentage, averaged input and output peak levels, heap use with an out-of-memory guard.
- **Menus:** long texts fit the display (auto-fit font, `~` cut, 8x13 bold font); parameters show units and decimals.
- **Harmonic Tremolo LED:** blinks once per LFO cycle (once per beat after a tempo tap) instead of dimming with the LFO.

## Fixed
- **Reverb Delay** no longer stops at start-up with "OUT OF MEMORY" (its reverb is now in SDRAM).
- **Reverb Delay and Delay** can be built together (the name clashes are removed).
- **Reverb Delay tempo LED** blinked about 24 times too slowly; it now follows the delay time.
- **CloudSeed wet-only LED** flashed far too slowly (about 10 s per phase); it now pulses on a sine curve with a 0.5 s period.
- **The menu could undo a change an effect had just made itself** (Crusher Pitch Tracking and Uranus Hold toggled only sometimes).
- **Global Settings:** Back no longer overwrites the Reboot text.
- **Balnea:** the Time menu step is fixed.
- **Delay:** switching it on (on its own, or its slot in a chain) brought back the repeats of what was played the last time it was on; it now starts from silence (not while the tail is still ringing with Trails). Selecting another effect now switches the old one off, so this also holds after switching effects (Tape Delay and Dual Echo too).
- **Distortion:** two Distortions (in an effect chain or the Dist Bench) no longer share their filters; a single Distortion sounds exactly as before.
- **Venus:** Oct Up+Dn now also shifts down.
- **LEDs** work with GCC 13.
- **Build:** Jupiter plus Neptune builds; a changed CloudSeed pool size rebuilds everything.

## Performance
- **Block processing** for NAM (no added latency), IR, CloudSeed, Reverb, Dattorro, Tape Delay, SciFi, Reverb Delay (35% / 40% down to 26% / 33% CPU), Distortion (Hard Clip 18% down to 12% CPU), Overdrive (about 4%), Crusher, Chopper, Modulated Tremolo and Harmonic Tremolo; the output is identical to the per-sample paths.
- **Distortion pre-filter:** its cutoff (which follows the input level) is now updated every 8 samples instead of every sample, a large saving; the level and the sound are unchanged for practical purposes.
- **6 KB of SRAM saved** (GCC no longer clones `ProcessSamples`).
- **Default levels** are calibrated against a -37 dBFS reference; several effects apply their defaults at start-up.

## NAM
- **NAM A2 runtime computes four frames at a time** (the layers are accumulated in tiles of four frames of the 48-sample block, so each weight loaded serves four frames; after the A2-Lite kernel of squatch-dsp): the same output (bit for bit without fast-math), the same latency, a few more history columns per layer (+552 B). Measured on the pedal: the NAM load went from 69% to 65%; `NAM_A2_TILE4=0` selects the previous 2-frame kernels.
- **One file per model** in `Effect-Modules/Nam/models/`; adding or removing a model needs no other edit.
- **`update_models.sh`** can calibrate all models to the same level (default +12 dB).
- **Reference test:** the NAM effect matches NeuralAmpModelerCore to 106-133 dB signal/error.

## Build tool (`effects_config.sh`)
- **Clear everything** (key `x`): deselects every effect, removes the chains and forbids the GPL effects again, to start a selection from scratch (asks first; nothing is written until saved, and an empty selection cannot be saved).
- **Effect selection** in an interactive full-screen tool (`effects.config`, presets in `configs/`); it asks before allowing a GPL effect.
- **Chains** can be made in the tool and are generated into the build.
- **Memory budget** report and a matrix of which effects fit together; boot results are recorded in `effects-memory.json`.
- **Only the chosen effects** are compiled.

## Documentation
- **`GENERATE_FIRMWARE.md`:** the tool, the memory limits, the GPL opt-in and chains.
- **`SECOND_FOOTSWITCH_AND_LED.md`:** how every effect uses the second footswitch and LED, and open points.
- **`PERFORMANCE_AND_MEMORY.md`** section 9: CPU load measured on the pedal and what it means for chains.
- **User manual:** no default firmware, all settings listed, three-tap tempo.
