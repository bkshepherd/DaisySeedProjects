# EFFECTS


These are the currently documented effects that can be included in a firmware; which of them your pedal has depends on how you generated it. Their exact controls and knob assignments can change with the build, the effect parameter menu lists the controls available in your version.


- **Tremolo, Harm Trem, AutoPan, Chopper:** Amplitude or stereo-pan modulation; rate/tempo, depth, waveform, and related modulation controls.
- **Chorus, Phaser, Flanger:** Sweeping modulation effects; rate, depth, mix, feedback, and tone controls.
- **Multi Phaser:** Up to four phasers in one effect, each with its own rate, depth, feedback, number of stages, centre frequency, wave and phase, in series or in parallel, mono or stereo. See [MULTI_PHASER.md](MULTI_PHASER.md).
- **Overdrive, Distortion, Crusher:** Saturation and digital reduction; drive/gain, tone, output level, bit depth, or sample-rate reduction.
- **Booster:** A gain stage to put in front of an amp or a distortion, in four types (Clean, Treble, Mid, Warm); gain (0 to 24 dB), tone, level. See [BOOSTER.md](BOOSTER.md) for what the types do.
- **Preamp:** A light amp-like tone (a cheaper alternative to a NAM model): gain, bass, mid, treble, level, and three voicings (Clean, Crunch, Lead).
- **Screamer:** Tube Screamer-style overdrive with the typical mid hump and a clean low end; drive, tone, level, and three clipping modes.
- **Muff Fuzz:** Big Muff-style fuzz with a booster and two clipping stages; sustain, tone (dark to bright, scooped in the middle), volume, and three tone voicings.
- **Wavefolder:** Metallic, synth-like harmonics from a sine fold instead of clipping; fold, symmetry, tone, mix, level.
- **Exciter:** Adds the 2nd to 5th harmonic of the highs (a level for each) to a clean sound for sparkle and warmth; drive, focus, mix, level.
- **Bass Dist:** Distortion that keeps the low end clean (only the band above the crossover is distorted); drive, crossover, clip type (soft, hard, fuzz), lows, tone, mix, level.
- **Octa Fuzz:** Octave-up fuzz in the spirit of the Octavia; fuzz, octave blend, tone, level.
- **Tape Sat:** Soft tape or transformer saturation that compresses loud notes and adds warmth, with unity gain for small signals; drive, warmth, tone, mix, level, and two modes (Tape, Trafo).
- **Compressor, Noise Gate:** Dynamic range and noise control; level, ratio, threshold, attack, and release.
- **Reverb, CloudSeed:** Short room-like to expansive reverberation; mix, decay, pre-delay, size, and tone/damping.
- **Spring Reverb:** A physical model of a spring-reverb tank with 2 to 6 springs of different lengths: the bright, splashy "drip" of old amps; dwell (how hard the tank is driven), decay, tone, mix, drip (how much each echo sweeps), tank length (short, medium, long), the number of springs (more springs: a denser, less echo-like tail) and an Output of mono (the default) or stereo (every other spring on each side). See [SPRING_REVERB.md](SPRING_REVERB.md).
- **Earth:** Plate reverb (Dattorro) with an optional octave up / up and down in front of it, and a momentary footswitch action (freeze the reverb, swell an overdrive on it, or switch the octave on); pre-delay, mix, decay, modulation depth and speed, input filter, size. Adapted from the FunBox project; GPLv3, so it is only in builds that opt in to the Dattorro reverb.
- **Venus:** Spectral reverb with octave and fifth shimmer, detune, spectral freeze (hold the secondary footswitch), lo-fi and drift modes; decay, mix, damp, shimmer, shimmer tone, and detune. Adapted from the FunBox project.
- **Delay, Multi Delay, Tape Delay, Dual Echo ("Halo"), Granular Delay, Spectral Delay:** Echo and time-based textures; delay/tap times, feedback, mix, filtering, modulation, and grain or spectral controls.
- **Pitch, PolyOctave, SciFi:** Pitch shifting, octave generation, and processed pitch effects; interval, dry/wet balance, and effect-specific controls.
- **Graphic EQ, Parametric EQ:** Tone shaping; band gain and, for parametric bands, frequency and bandwidth.
- **NAM, IR:** Amp and cabinet impulse-response processing; model or IR selection, gain, level, and EQ.
- **Looper, Drum, Metronome, Tuner:** Performance and utility tools; loop levels/actions, rhythm or tempo controls, and tuning options.
- **Other FunBox effects (funbox-jupiter, -saturn, -neptune, -uranus, -balnea, -pluto, -cassini, -mars, -mercury):** Further effects adapted from the FunBox project, available when selected in the firmware; they show up in the effect list with the `funbox-` prefix, like Earth and Venus above.
- **Duck Compressor, Reverb Delay, Scope, MIDI Keys, String Keys, Modal Keys, FM Keys, Pluck Echo:** Further effects (a ducking compressor, a combined reverb and delay, an oscilloscope display, and synths and a plucked-echo effect played from a MIDI keyboard).
- **Test Tone:** A signal generator for calibrating levels together with In Gain, Out Gain and the level readout of Show Debug: a sine, square or triangle wave of a chosen frequency (20 Hz to 20 kHz), or white noise, at a chosen level (-60 to 0 dB; the tone's peak level, and for noise the same RMS as a sine of that level). It replaces the input while the effect is on.

