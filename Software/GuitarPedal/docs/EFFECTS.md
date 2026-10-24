# Effects

These are the effects that can be included in a firmware; which of them your pedal has depends on how you generated it (see [GENERATE_FIRMWARE.md](GENERATE_FIRMWARE.md)). The effects are named as they appear in the pedal's effect list. Their exact controls and knob assignments can change with the build: the effect parameter menu lists the controls available in your version.

Each effect below has a short description, where it comes from when that is known, and a list of its parameters. "Knob N" is the knob the parameter is on by default; "menu only" parameters are reached through the parameter menu. Ranges and defaults are the values the display shows.

- [Gain, Distortion and Saturation](#gain-distortion-and-saturation): Booster, Overdrive, Screamer, Distortion, Chainsaw, Muff Fuzz, Octa Fuzz, Bass Dist, Preamp, Wavefolder, Crusher, Exciter, Tape Sat, Dist Bench
- [Modulation](#modulation): Tremolo, Harm Trem, AutoPan, Chopper, Chorus, Phaser, Multi Phaser, Flanger
- [Delay](#delay): Delay, Multi Delay, Tape Delay, Dual Echo, GranDelay, SpctDelay, PluckEcho
- [Reverb](#reverb): Reverb, Spring Reverb, CloudSeed, Dattorro, Verb Delay
- [Pitch](#pitch): Pitch, PolyOctave, SciFi
- [Dynamics](#dynamics): Comp, DuckComp, Noise Gate
- [Equalizers](#equalizers): GraphicEQ, ParaEQ
- [Amp and Cabinet](#amp-and-cabinet): NAM, IR
- [Synths and Rhythm](#synths-and-rhythm): MidiKeys, StringKeys, ModalKeys, FmKeys, Drum
- [Utilities](#utilities): Looper, Metronome, Tuner, Scope, TestTone
- [Funbox](#funbox): Venus, Earth, Jupiter, Saturn, Neptune, Uranus, Balnea, Pluto, Cassini, Mars, Mercury

## Gain, Distortion and Saturation

Boosts, overdrives, distortions, fuzzes, preamps and other ways of adding gain and harmonics. Most of them are single-input mono effects with the same set of basic controls (a drive, a tone control, an output Level), and the newer ones are level-matched so that switching between them at default settings keeps about the same loudness.

### Booster

A booster makes your guitar louder before it reaches an amp or a distortion, so that it pushes them harder. It is less an effect you hear on its own than a way of changing how the next thing in the chain reacts: in front of an overdriven amp the amp distorts more, in front of a clean amp it just gets louder. The four types boost different parts of the sound:

- **Clean**: everything equally, almost no coloring. Pushes an amp or pedal without changing its character.
- **Treble**: the highs, with the bass cut. Thin, bright and cutting, with a little grit when pushed (the Rangemaster-style treble booster). Makes a smooth amp bite and sing; a lead tone that cuts through.
- **Mid**: a bump around one frequency, lows and highs left alone. Pushed-forward, "honky" mids; makes a guitar jump out of a mix or tightens a muddy distortion.
- **Warm**: the mids, with more lows and fewer highs. Thick, rounded and slightly saturated (in the spirit of the Echoplex preamp boosters). Fattens a thin single-coil and adds body and a little compression.

Original to this project; the types are modelled on classic booster circuits. Clean and Mid stay clean (they only limit softly near full scale), Treble and Warm clip asymmetrically when pushed, which adds even harmonics (the germanium and tube sound): subtle at the default, gritty with a hot guitar or a high Gain.

- **Gain** (0 to 24 dB, default 12 dB; Knob 1): how much the type boosts the part of the sound it is built for (everything for Clean, the highs for Treble, the centre frequency for Mid, the mids for Warm). At 0 dB Clean and Mid do nothing at all, while Treble still cuts the bass and Warm still lifts the lows and rolls off the highs.
- **Tone** (0 to 100%, default 50%; Knob 2): does something different for each type. Clean: tilts lows against highs (0% darker, 50% flat, 100% brighter, about +/-4 dB). Treble: the corner below which the bass is cut, 200 Hz at 0%, 800 Hz at 50%, 3.2 kHz at 100%. Mid: the frequency of the bump, 400 Hz at 0%, 1.1 kHz at 50%, 3.2 kHz at 100%. Warm: the high cut, 2 kHz (dark) at 0%, 4.9 kHz at 50%, 12 kHz (open) at 100%.
- **Level** (-24 to +6 dB, default 0 dB; Knob 3): output level after the boost.
- **Type** (Clean, Treble, Mid, Warm; default Clean; Knob 4): the boost type.

Tip: Gain is the boost where the type works, not the loudness of the whole guitar. At the default 12 dB Clean is about 12 dB louder, Warm about 13 dB, Mid about 4 dB and Treble about 3 dB, so adjust Gain or Level by ear when you change the type. Ideas: Clean at 8 to 12 dB in front of a Screamer or Chainsaw in an effect chain; Treble with Tone at 50 to 70% in front of an overdrive for a tight, cutting lead; Warm at 6 to 10 dB in front of a clean amp to thicken it; Mid at 1 to 1.5 kHz with Gain 6 to 12 dB in front of the Preamp's Crunch voicing.

### Overdrive

A simple overdrive: a soft, even clipping with one Drive control and an output Level. It is the original, light overdrive of the pedal and uses the overdrive from the DaisySP library. It is a good basic crunch for low to medium drive settings, and it is cheap on processing.

- **Drive** (0.00 to 1.00, default 0.45; Knob 2): how hard the signal is driven into the clipping. The setting is shown as a plain 0 to 1 number.
- **Level** (shown as about -55 to -24 dB, default about -30 dB; Knob 1): output level. The menu shows the raw output gain of the effect in dB, which is why the numbers are negative; just set it by ear.

### Screamer

A Tube Screamer-style overdrive: a mid hump with tight, uncluttered lows. Only the part of the signal above about 720 Hz is clipped, and the clean signal is added back, so the low end stays clear and the character is "mid push" rather than fuzz. It is the classic pedal to put in front of an amp to tighten it and push it into saturation, with Drive low and Level up.

The Tube Screamer is an overdrive from Ibanez (1970s) with a prominent mid hump, used by countless blues and rock players. This is a new implementation, original to this project.

- **Drive** (0 to 10, default 6.6; Knob 1): the amount of gain in the clipped path. The top of the knob adds much more gain so that 10 saturates fully with a hot guitar.
- **Tone** (500 Hz to 8 kHz, default 2.6 kHz; Knob 2): a high cut. Lower is darker.
- **Level** (-24 to +6 dB, default 0 dB; Knob 3): output level.
- **Mode** (Classic, Asymmetric, Hard; default Classic; Knob 4): Classic is a symmetric soft clip. Asymmetric adds even harmonics for a warmer, more open sound. Hard is a harder, more compressed clip.

### Distortion

A general-purpose distortion with six clipping types, a tilt tone control and an optional 16x oversampling that keeps the high harmonics from turning into harsh aliasing. It covers everything from sharp, buzzy hard clipping to smooth tube-like saturation and a very saturated multi-stage sound. At the default Hard Clip type it is a classic tight, aggressive distortion.

- **Level** (-100 to 0 dB, default -6 dB; Knob 1): output level. The effect's types are level-matched, so switching type keeps about the same loudness.
- **Gain** (0 to 26 dB, default 20.4 dB; Knob 2): the amount of drive into the clipper.
- **Tone** (0.00 to 1.00, default 0.50; Knob 3): a tilt control: 0 is all bass (dark), 1 is all treble (bright).
- **Dist Type** (Hard Clip, Soft Clip, Fuzz, Tube, Multi Stage, Diode Clip; default Hard Clip; Knob 4): Hard Clip and Diode Clip cut the peaks off sharply. Soft Clip and Tube round them. Fuzz adds a buzzy, compressed edge. Multi Stage is three clipping stages in a row, the most saturated.
- **Intensity** (0.00 to 1.00, default 0.50; Knob 5): how hard the clipping is. On the hard and diode types a higher value clips at a lower level (more distortion); on Fuzz and Tube it increases the amount of saturation. It has no effect on Soft Clip.
- **Oversamp** (Off or On, default On; Knob 6): 16x oversampling for a smoother top end at high Gain. Switch it off to save processing power.

### Chainsaw

A heavy metal distortion in the style of the Boss HM-2: hard clipping followed by big, resonant boosts at 87 Hz (Low) and in the upper mids (High, two peaks at 960 Hz and 1.28 kHz). This is the "Swedish chainsaw" sound of early Swedish death metal, played with all controls turned up. It is fully saturated from very quiet playing and it is never clean, even at Dist 0. It is a gated, scooped, buzzing sound, so it works best with palm-muted riffs; the gate-like dead band on note tails is part of the character.

The Boss HM-2 Heavy Metal pedal (1983) is the original. The frequencies here are the ones published for its tone circuit; the exact bandwidths and boost range are estimates. Original to this project.

- **Dist** (0 to 10, default 8.0; Knob 1): the amount of distortion.
- **Low** (-15 to +15 dB, default +10 dB; Knob 2): the peak at 87 Hz.
- **High** (-15 to +15 dB, default +10 dB; Knob 3): the two overlapping peaks at 960 Hz and 1.28 kHz. At +15 dB the upper mids rise by about 23 dB.
- **Level** (-24 to +6 dB, default 0 dB; Knob 4): output level.

Tip: the classic chainsaw is Dist and both EQ controls fully up.

### Muff Fuzz

A Big Muff-style fuzz: a booster and two clipping stages, with a tone control that scoops the mids. It is a thick, sustaining, saturated sound that stays saturated at any Sustain setting; Sustain changes how much the notes compress and sing, not whether it distorts. The tone control crossfades between a bass-heavy and a treble-heavy sound around a crossover frequency set by the Mode.

The Electro-Harmonix Big Muff Pi (late 1960s) is the classic fuzz of this kind, in recordings from Hendrix-era psychedelia to 1990s alternative rock. Original to this project.

- **Sustain** (0 to 10, default 6.0; Knob 1): the amount of gain in the clipping stages.
- **Tone** (0 to 100%, default 50%; Knob 2): 0% is all bass, 100% is all treble, and the middle scoops the mids.
- **Volume** (-24 to +6 dB, default 0 dB; Knob 3): output level.
- **Mode** (Classic, Fat, Bright; default Classic; Knob 4): moves the centre of the tone control: Classic 1.5 kHz, Fat 800 Hz (thicker), Bright 3 kHz (more cutting).

Tip: for a fuzz that keeps a clean low end and clear attack, use it as a stage in Dist Bench (below) with Mix around 60 to 70%.

### Octa Fuzz

An octave-up fuzz in the style of the Octavia: a full-wave rectified copy of your signal (which sounds an octave up) is blended with the original and driven into a hard clip. It is best for single notes higher up the neck, where it gives the classic raspy, ring-modulated Hendrix-style octave sound. On low strings and chords the octave tracks poorly and gets harsh, which is typical for this circuit.

The Octavia was a fuzz with an octave effect built for Jimi Hendrix in the late 1960s. Original to this project.

- **Fuzz** (0 to 10, default 6.0; Knob 1): the amount of gain into the clipper.
- **Octave** (0 to 100%, default 50%; Knob 2): how much octave-up is blended in. At 0% it is a plain fuzz.
- **Tone** (800 Hz to 8 kHz, default 3.2 kHz; Knob 3): a high cut.
- **Level** (-24 to +6 dB, default 0 dB; Knob 4): output level.

### Bass Dist

A distortion that keeps the lows clean: the signal is split at a crossover frequency, and only the band above the crossover is distorted, then recombined with the untouched low band. It is made for bass guitar, where an ordinary distortion loses the low end, but it also tightens a guitar fuzz or distortion. The Mix control blends the distorted band against the clean upper band.

Original to this project.

- **Drive** (0 to 10, default 5.0; Knob 1): the amount of gain into the clipper. At full Drive some of a 50 Hz tone can be clipped; raise the Crossover for clean lows.
- **Crossover** (60 to 400 Hz, default 128 Hz; Knob 2): the split frequency. Above it the sound is distorted.
- **Clip** (Soft, Hard, Fuzz; default Soft; Knob 3): Soft is smooth, Hard is edgier, Fuzz is an asymmetric, fuzzier clip.
- **Lows** (-24 to +6 dB, default 0 dB; Knob 4): the level of the clean low band.
- **Tone** (1.5 kHz to 12 kHz, default 5.2 kHz; Knob 5): a high cut on the distorted band.
- **Level** (-24 to +6 dB, default 0 dB; Knob 6): output level.
- **Mix** (0 to 100%, default 100%; menu only): the balance of the distorted band against the clean upper band. The clean low band is not affected by Mix.

### Preamp

A light amp-like preamp: one or two tube-style gain stages, a three-band tone stack and a speaker-like high cut, in three voicings. It is a cheaper alternative to a NAM amp model when you want an amp-like crunch without the processing cost. Put it after a pedal such as a Screamer or Booster to push it into saturation.

Original to this project.

- **Gain** (0 to 10, default 4.0; Knob 1): the amount of gain. The top of the knob gets much more gain so that the maximum saturates fully with a hot guitar signal (strongest in Clean).
- **Bass** (-12 to +12 dB, default 0 dB; Knob 2): a low shelf at 120 Hz.
- **Mid** (-10 to +10 dB, default 0 dB; Knob 3): a mid bump or dip. Its frequency depends on the voicing (Clean 500 Hz, Crunch 700 Hz, Lead 900 Hz).
- **Treble** (-12 to +12 dB, default 0 dB; Knob 4): a high shelf at 3.2 kHz.
- **Voicing** (Clean, Crunch, Lead; default Crunch; Knob 5): Clean is a single gain stage, a warm clean boost that breaks up at high Gain. Crunch has two stages for a classic rhythm crunch. Lead has the most gain and the tightest top end.
- **Level** (-24 to +6 dB, default 0 dB; Knob 6): output level. Clean and Crunch are matched to about the same loudness; Lead is matched to Crunch when played hard.

Tip: at a normal playing level Crunch is still fairly clean at medium Gain and comes alive above it; Lead is saturated from about the middle of the knob.

### Wavefolder

A wavefolder: instead of flattening peaks like a clipper, it folds them back on themselves, which gives metallic, bell-like, synth-like harmonics that change a lot with playing dynamics. It is an unusual distortion, best for single notes and experimental sounds; high Fold settings are intentionally extreme. Anti-aliasing keeps it reasonably clean in the top end.

Wavefolding comes from modular synthesizers (the Buchla-style folder). Original to this project.

- **Fold** (0.25x to 130x, default 30x; Knob 1): how many times the signal folds over. Low values are a gentle warm saturation, high values are very metallic. The top of the knob folds much more.
- **Symmetry** (-100% to +100%, default 0%; Knob 2): at 0% the folding is symmetric; moving it away from 0% makes it uneven, which adds even harmonics.
- **Tone** (1 kHz to 16 kHz, default 7 kHz; Knob 3): a high cut after the folder.
- **Mix** (0 to 100%, default 100%; Knob 4): blend of the clean signal and the folded signal.
- **Level** (-24 to +6 dB, default 0 dB; Knob 5): output level.

### Crusher

A bit crusher and sample-rate reducer: it lowers the bit depth (fewer quantization steps, so a grainy digital distortion) and lowers the sample rate (which makes aliasing, metallic or "video game" tones), with a low-pass filter to tame the harshness. It can also track the pitch of what you play so that the sample-rate reduction follows the note and gives a pitched, ring-like tone.

- **Level** (-24 to +6 dB, default 0 dB; Knob 1): output level.
- **Bits** (1 to 32, default 8; Knob 2): bit depth. Lower is more digital distortion; at 32 the bit depth is practically unchanged.
- **Rate** (100 Hz to 48 kHz, default 8000 Hz; Knob 3): the sample rate to reduce to. Lower is more aliased and metallic. In pitch-tracking mode this knob instead picks a multiplier of the detected pitch: unison, fifth, +1 octave, +1 octave and a fifth, +2 octaves, +2 octaves and a fifth, +3 octaves, +4 octaves.
- **Pitch Tracking** (Off or On, default Off; menu only): when on, the sample rate follows the detected pitch of the input.
- **Jitter** (0 to 100%, default 0%; Knob 4): random variation of the sample-rate hold interval, which adds instability and grit.
- **Cutoff** (500 Hz to 20 kHz, default 6000 Hz; Knob 5): a low-pass after the crusher to tame the high aliasing.
- **Mix** (0 to 100%, default 100%; Knob 6): blend of the clean signal and the crushed signal.
- **Footswitch 2**: toggles Pitch Tracking. The second LED is lit while tracking is on. When you switch tracking off, the Rate knob keeps its previous manual value until you move the knob.

### Exciter

An exciter adds harmonics to the highs for presence and air without sounding like obvious distortion. It drives the part of the signal above the Focus frequency into a harmonic generator that creates the 2nd to 5th harmonics, with a level for each, and adds them to the original signal. The 2nd harmonic sounds warm, the 3rd harder and more forward. It brightens a dull sound, or adds sparkle to a clean guitar; use a low Mix.

Original to this project.

- **Drive** (0 to 40 dB, default 28 dB; Knob 1): how hard the highs are driven into the harmonic generator.
- **2nd** (0 to 100%, default 50%; Knob 2): level of the 2nd harmonic (an octave up, warm).
- **3rd** (0 to 100%, default 30%; Knob 3): level of the 3rd harmonic (harder and more forward).
- **4th** (0 to 100%, default 0%; menu only): level of the 4th harmonic.
- **5th** (0 to 100%, default 0%; menu only): level of the 5th harmonic.
- **Focus** (500 Hz to 8 kHz, default 1.1 kHz; Knob 4): the frequency above which the harmonics are made. A lower Focus adds them to more of the signal, down into the mids.
- **Mix** (0 to 100%, default 50%; Knob 5): how much of the generated harmonics are added to the dry signal.
- **Level** (-24 to +6 dB, default 0 dB; Knob 6): output level.

### Tape Sat

Tape and transformer saturation: soft, musical compression with a warm low end and a gentle high roll-off. For small signals it has unity gain, so it adds saturation without raising the level. The Warmth control boosts the lows before the saturation and takes the boost out again after it, so the lows saturate more than the rest of the sound. It smooths out a harsh fuzz or gives a clean sound a bit of glue and weight.

Original to this project.

- **Drive** (0 to 36 dB, default 29 dB; Knob 1): how hard the signal is driven into the saturation.
- **Warmth** (0 to 12 dB, default 4.8 dB; Knob 2): how much more the lows saturate. Trafo mode applies 1.5 times more warmth than Tape.
- **Tone** (2 kHz to 16 kHz, default 8.6 kHz; Knob 3): the high roll-off.
- **Mix** (0 to 100%, default 100%; Knob 4): blend of the clean signal and the saturated signal.
- **Level** (-24 to +6 dB, default 0 dB; Knob 5): output level.
- **Mode** (Tape, Trafo; default Tape; Knob 6): Tape has a small asymmetry (some even harmonics) and the low boost at 90 Hz. Trafo (transformer) has none, a lower low boost at 55 Hz and more warmth.

Tip: put it last in a chain, with Drive a little below default and Warmth up, to take the edge off a harsh fuzz.

### Dist Bench

Dist Bench chains up to four distortion effects one after another inside a single effect. Use it to compare distortion types, to stack them (a boost into an amp, a fuzz into tape saturation) and to find out how hard each one should be driven. The signal runs through the stages in order 1, 2, 3, 4; a stage set to Off passes the signal through unchanged. Original to this project.

Each stage is Off or one of eleven types: Preamp, Screamer, Muff Fuzz, Wavefolder, Exciter, Bass Dist, Octa Fuzz, Tape Sat, Overdrive, Distortion or Chainsaw (all described above). Every type gets the same four controls (Drive, Tone, Char, Level), so you can swap the type of a stage and keep your settings. Changing a stage's type while playing crossfades over a tenth of a second, so there is no click.

- **1 Type to 4 Type** (Off, Preamp, Screamer, Muff Fuzz, Wavefolder, Exciter, Bass Dist, Octa Fuzz, Tape Sat, Overdrive, Distortion, Chainsaw; default stage 1 Screamer, stages 2 to 4 Off; menu only).
- **1 Drive to 4 Drive** (0 to 100%, default 50%; Knobs 1 to 4, stage 1 on Knob 1 and so on): how hard the stage distorts.
- **1 Tone to 4 Tone** (0 to 100%, default 50%; menu only): the brightness of the stage, usually a high cut.
- **1 Char to 4 Char** (0 to 100%, default 50%; menu only): the effect's most characteristic second control; for effects with modes it switches between them.
- **1 Level to 4 Level** (-24 to +6 dB, default 0 dB; menu only): the stage's output level. It is also the way to drive the next stage harder or softer.
- **Out Level** (-24 to +6 dB, default 0 dB; Knob 5): level of the whole effect.
- **Mix** (0 to 100%, default 100%; Knob 6): blend of the clean signal and the distorted signal. Lower values keep the attack and lows clear under a heavy fuzz.

50% always means the effect's own default: a stage at 50% sounds exactly like the effect on its own with its default settings. Below 50% the control moves towards the effect's minimum and above 50% towards its maximum, so the two halves of the knob can cover different amounts (on the Screamer, Drive 0 to 50% covers 0 to 6.6, 50 to 100% covers 6.6 to 10). The menu shows the percentage, not the effect's own value. For modes, 50% is the default mode and the others are stepped through below and above it. All other parameters of an effect stay at the effect's default inside the bench (the Preamp's Bass and Mid, the Exciter's 4th and 5th harmonic and Mix, the Tape Sat's Mode, the Distortion's Intensity); for those, use the effect on its own.

What the three controls mean for each type (0% / 50% / 100% where given):

| Type | Drive | Tone | Char |
|---|---|---|---|
| Preamp | Gain 0 / 4.0 / 10 | Treble -12 / 0 / +12 dB | Voicing: Clean below 25%, Crunch 25 to 74%, Lead 75% and up |
| Screamer | Drive 0 / 6.6 / 10 | high cut 500 Hz / 2.6 kHz / 8 kHz | Mode: Classic up to 62%, Asymmetric 63 to 87%, Hard 88% and up |
| Muff Fuzz | Sustain 0 / 6.0 / 10 | Tone 0% (bass) / 50% / 100% (treble) | Mode: Classic, Fat from 63%, Bright from 88% |
| Wavefolder | Fold 0.25x / 30x / 130x | high cut 1 kHz / 7 kHz / 16 kHz | Symmetry -100% / 0% / +100% |
| Exciter | Drive 0 / 28 / 40 dB | Focus 500 Hz / 1.1 kHz / 8 kHz | 2nd against 3rd: 2nd 100% and 3rd 0%, / 50% and 30% / 2nd 0% and 3rd 60% |
| Bass Dist | Drive 0 / 5.0 / 10 | high cut 1.5 kHz / 5.2 kHz / 12 kHz | Clip: Soft up to 62%, Hard 63 to 87%, Fuzz 88% and up |
| Octa Fuzz | Fuzz 0 / 6.0 / 10 | high cut 800 Hz / 3.2 kHz / 8 kHz | Octave 0% / 50% / 100% |
| Tape Sat | Drive 0 / 28.8 / 36 dB | high cut 2 kHz / 8.6 kHz / 16 kHz | Warmth 0 / 4.8 / 12 dB |
| Overdrive | Drive min / default / max | no effect | no effect |
| Distortion | Gain 0 / 20.4 / 26 dB | Tone: bass to treble tilt | Type: Hard Clip up to 54%, then Soft Clip (55%), Fuzz (65%), Tube (75%), Multi Stage (85%), Diode Clip (95%) |
| Chainsaw | Dist 0 / 8.0 / 10 | High -15 / +10 / +15 dB | Low -15 / +10 / +15 dB |

The Distortion runs without its 16x oversampling in the bench, so at high Gain its top end is a little grainier than on its own.

Levels: each effect on its own makes the signal about 6 dB louder at default settings. In the bench every active stage is lowered by 6 dB and the bench adds 6 dB at the end, so one stage at its defaults is exactly as loud as the effect on its own, and four stages are not 24 dB louder than one. With every stage Off, the bench is a clean 6 dB boost. If a chain gets too loud or too quiet, adjust the stage Levels first and Out Level last.

Tip: to compare types, leave stages 2 to 4 Off and step through 1 Type with everything at 50%. For a boost into an amp, use stage 1 Screamer with Drive 20 to 30% and Level +3 to +6 dB, then stage 2 Preamp at 50%. For a fuzz with a clean low end, Muff Fuzz with Mix around 60 to 70%.

## Modulation

Effects that move the level, the pan position or the phase and delay of the sound with a low-frequency oscillator (LFO): tremolos, auto-pan, chopper, chorus, phasers and flanger.

### Tremolo

A tremolo that varies the volume with a choice of eight LFO shapes. A second, slow oscillator can in turn vary the tremolo speed, so the pulse drifts or breathes instead of staying at a fixed rate. Turn Osc Freq to its lowest setting to switch the speed modulation off and get a plain tremolo. Use Sine for a smooth vintage pulse, Square for a hard on/off stutter, Saw or Ramp for a one-way swell or a choppy decay.

An extension of the simple tremolo of the original firmware this project is forked from.

- **Freq** (1.0 to 21.0 Hz, default 11.0 Hz; Knob 1): tremolo speed.
- **Depth** (0 to 100%, default 50%; Knob 2): how far the volume dips.
- **Osc Freq** (0.01 to 1.01 Hz, default 0.11 Hz; Knob 3): speed of the second oscillator that modulates the tremolo speed. At the lowest setting the modulation is off.
- **Wave** (Sine, Triangle, Saw, Ramp, Square, Poly Tri, Poly Saw, Poly Sqr; default Sine; Knob 4): shape of the tremolo. The "Poly" shapes are the band-limited versions, with less aliasing at fast speeds.
- **Osc Wave** (same eight shapes, default Sine; Knob 5): shape of the speed-modulating oscillator.
- **Footswitch 2**: tap tempo, sets Freq (limited to 1 to 20 Hz).

### Harm Trem

A harmonic tremolo: the signal is split into a low and a high band, and the two bands are modulated in opposite directions, so while the lows get louder the highs get quieter and the other way round. The result is a softer, more liquid, slightly swirling pulse than a plain volume tremolo, and the overall level stays more even. The voicing includes a gentle EQ in the style of the classic circuit.

Modelled on the harmonic tremolo (called "vibrato" by Fender) of early 1960s Fender "brownface" amps.

- **Speed** (0.2 to 16.2 Hz, default 4.2 Hz; Knob 1): speed of the LFO.
- **Depth** (0 to 125%, default 62%; Knob 2): how far the two bands move against each other. Above 100% the effect gets stronger still. The level is compensated, so more depth does not make it louder.
- **Footswitch 2**: tap tempo, sets Speed (limited to 0.2 to 16 Hz). The left LED blinks once per LFO cycle, so after a tap it shows the beat.

### AutoPan

Moves the sound back and forth between the left and right outputs with an LFO. The Stereo option decides what is panned: with Stereo off, the (mono) input is panned between the two outputs; with Stereo on, a stereo input keeps its stereo image and the centre of the image is moved from side to side. Set the Osc Freq to its lowest setting to hold the sound in the centre. The pedal display shows a small L to R slider with the pan position.

An extension of the simple auto-pan of the original firmware this project is forked from.

- **Wet** (0 to 100%, default 100%; Knob 1): amount of panned signal against the dry one.
- **Osc Freq** (0.01 to 4.01 Hz, default 0.41 Hz; Knob 2): pan speed. At the lowest setting the pan stops in the centre.
- **Osc Wave** (Sine, Triangle, Saw, Ramp, Square, Poly Tri, Poly Saw, Poly Sqr; default Sine; Knob 3): shape of the pan movement. Sine and Triangle glide, Square jumps from one side to the other (the movement is smoothed a little).
- **Stereo** (Off or On, default Off; Knob 4): Off pans the mono input; On treats the input as a stereo pair (see above).
- **Footswitch 2**: tap tempo. The pan cycle takes four beats (so a tapped 120 BPM gives 0.5 Hz). The left LED follows the pan position.

### Chopper

A rhythmic gate: the sound is switched on and off in one of 14 rhythm patterns, locked to a tempo. Use it for stutter and "trance gate" effects. Pattern 1 is a steady chop in sixteenth steps, others use every second step, eighth or quarter notes, one hit per beat, grouped pairs and triples, and syncopated groups of eighths, sixteenths and quarters. The display shows the pattern as a row of blocks. The effect is best with sustained chords or a bit of distortion in front of it.

A wave slicer created and contributed by KnightHill (the Chopper of the original firmware this project is forked from).

- **Wet** (0 to 100%, default 50%; Knob 1): amount of chopped signal against the dry one. Use 100% for a full gate.
- **Tempo** (30 to 240 BPM, default 135 BPM; Knob 2): speed of the pattern.
- **Duty** (10 to 90%, default 25%; Knob 3): how long each step stays on. Low values give short staccato bites, high values give a longer, more legato sound.
- **Pattern** (1 to 14, default 1; Knob 4): the rhythm pattern.
- **Footswitch 2**: tap tempo, sets Tempo. The left LED follows the chopping.

### Chorus

A stereo chorus: the signal is mixed with copies that are delayed by a few milliseconds, with the delay moved up and down by an LFO, which thickens the sound and adds a gentle shimmer. The Delay sets the base delay, LFO Depth how far it moves, and the Feedback feeds the output back to make the sound more intense and "swirly". The output is stereo: the left and right copies are modulated differently, so the chorus widens a mono guitar.

Uses the chorus from the DaisySP library (Electrosmith, MIT).

- **Wet** (0 to 100%, default 65%; Knob 1): amount of chorus signal against the dry one.
- **Delay** (0.1 to 8.1 ms, default 4.1 ms; Knob 2): base delay time. Short values give a thinner, more flanger-like sound.
- **LFO Freq** (1.0 to 20.0 Hz, default 2.2 Hz; Knob 3): modulation speed, finer control at the slow end.
- **LFO Depth** (0 to 93%, default 30%; Knob 4): how far the delay moves.
- **Feedback** (0 to 100%, default 25%; Knob 5): amount of the output fed back into the effect.
- The left LED brightness follows the Wet setting.

### Phaser

A classic mono phaser: the signal goes through a chain of allpass filters that shift its phase, and is mixed with the original so that notches in the spectrum sweep up and down with an LFO (between about 300 Hz and 1.5 kHz). Feedback makes the notches sharper and the sound more vocal. The Mix control is equal-power, so the level stays even as you blend. For several phasers at once, or more stages, see the Multi Phaser.

Output is mono (a stereo input is summed).

- **Mix** (0 to 100%, default 50%; Knob 1): dry against phased signal.
- **Rate** (0.10 to 8.00 Hz, default 0.22 Hz; Knob 2): sweep speed, with a slow-favouring curve for fine control of slow sweeps.
- **Depth** (0 to 98%, default 98%; Knob 3): how wide the sweep is.
- **Feedback** (0 to 45%, default 11%; Knob 4): resonance of the notches.
- The left LED brightness follows the sweep.

### Multi Phaser

A phaser with up to four phasers in one effect, each with its own settings. A phaser passes the signal through a chain of allpass filters, which change its phase but not its level, and adds the result to the original: where the two cancel there is a notch, and an LFO sweeps the notches up and down. Every two stages make one notch. The Multi Phaser runs several phasers at once, each with its own speed, depth, centre and number of stages, so the notches sweep against each other in a way a single phaser cannot do.

Original to this project.

The first three controls are for the whole effect, the rest are repeated for each of the four phasers (P1 to P4). Only P1 and P2 are on by default; P3 and P4 are set up to sound different from them when you switch them on.

- **Mix** (0 to 100%, default 100%; Knob 1): amount of phased signal against the dry one. At 100% the notches are deepest, as in the classic units; lower it for a shallower sweep.
- **Routing** (Series or Parallel, default Series; menu only): in Series each phaser phases the output of the one before it, so the sweep gets deeper and more complex with every phaser. In Parallel all phasers process the dry signal and their outputs are averaged: the sound is lighter and each phaser stays separate. Changing the routing while playing fades the effect out and in again (about 100 ms) so it does not click.
- **Output** (Mono or Stereo, default Mono; menu only): Mono sums to mono and sends the same signal to both outputs. In Stereo the right channel is the same phasers a quarter of a cycle later, a wide moving sweep.
- **P1 On, P2 On, P3 On, P4 On** (Off or On; default P1 and P2 On, P3 and P4 Off; menu only): switch a phaser in or out, with a short fade.
- **P1 Rate** (0.05 to 10 Hz, exponential, default 0.54 Hz; Knob 2): speed of the phaser's LFO.
- **P1 Depth** (0 to 100%, default 80%; Knob 3): how far the notches sweep from the Center, up to two and a half octaves each way.
- **P1 Feedback** (-85% to +85%, default +26%; Knob 4): feeds the end of the chain back to its input. Positive values make the notches narrow and the peaks resonant (a whistling, "jet" sound); negative values give a hollower, more nasal sound. The level is corrected so that more feedback does not get louder.
- **P2 Rate** (0.05 to 10 Hz, default 0.32 Hz; Knob 5) and **P2 Depth** (0 to 100%, default 70%; Knob 6): as P1.
- **P1 Stages, P2 Stages, P3 Stages, P4 Stages** (2, 4, 6, 8, 10 or 12; default 4, 6, 8, 2; menu only): number of allpass stages. Two give one notch, twelve give six; more stages sound deeper and more liquid. Changing it while playing fades the phaser out and in again (about 100 ms).
- **P1 Center to P4 Center** (100 Hz to 4 kHz, exponential; default 632 Hz, 914 Hz, 438 Hz, 1320 Hz; menu only): the frequency the sweep is centred on (where the notch sits with Depth at 0).
- **P1 Wave to P4 Wave** (Sine, Triangle, Random; default Sine, Triangle, Sine, Random; menu only): LFO shape. Random picks a new position every cycle and glides to it, so the sweep wanders.
- **P1 Phase to P4 Phase** (0 to 360 degrees; default 0, 90, 180, 270; menu only): where in its cycle the LFO starts, against the other phasers. With the same rate two phasers keep that distance.
- **P2 Feedback** (default 0%), **P3 and P4 Rate, Depth, Feedback** (as P1; menu only): P3 defaults to 0.92 Hz, 60%, +17%; P4 to 0.19 Hz, 90%, 0%.

Level: each phaser makes up the loss of level that phasing causes, so the default settings keep a guitar signal at about the level of the input, and more phasers in series get somewhat louder (four are about 5 dB louder). In Parallel they average, so the level does not grow. With several phasers in series and high feedback the output can clip; turn the Mix or Feedback down, or the level before the effect.

Tip: Classic phaser: turn P2 off, 4 stages, Rate 0.5 Hz, Depth 80%, Feedback +30%. Jet sweep: one phaser with 12 stages, Feedback +70%, Rate 0.15 Hz, Depth 100%, Center 800 Hz. Shimmer: four phasers in Parallel with Random waves at different rates. Stereo swirl: Output Stereo with two phasers in Series.

### Flanger

A flanger: a very short delay (fractions of a millisecond to a few milliseconds) is mixed with the dry signal, and an LFO sweeps the delay so that a comb of notches moves through the sound, the classic "jet plane" swoosh. Feedback makes it more metallic and resonant. The Manual control sets where in the delay range the sweep is centred, and Depth how far it moves around that. The Mix is equal-power, so the level stays even. The output is mono (a stereo input is summed).

Uses the flanger from the DaisySP library (Electrosmith, MIT).

- **Mix** (0 to 100%, default 65%; Knob 1): dry against flanged signal.
- **Manual** (0.1 to 7.0 ms, default 3.6 ms; Knob 2): the centre delay of the sweep. Lower values give a brighter, higher-pitched sweep.
- **Rate** (0.05 to 5.00 Hz, default 0.36 Hz; Knob 3): sweep speed, with finer control at the slow end.
- **Depth** (0 to 93%, default 30%; Knob 4): how far the delay moves.
- **Feedback** (0 to 97%, default 24%; Knob 5): amount of the output fed back into the delay. High values ring and whistle.
- The left LED brightness follows the Mix setting.

## Delay

Echoes and time-based textures, from clean repeats and tape echoes to granular and spectral effects. With Trails on, the repeats keep sounding when the effect is switched off.

### Delay

A versatile stereo delay with six delay types (forward, reverse, octave, combinations, and a dual-voice mode), a tone filter on the repeats, stereo spread and a modulation section that can add vibrato, tremolo or auto-pan to the echoes. Delay times go from 50 ms up to 2 seconds. Tap tempo and a rhythmic mode (triplet or dotted eighth) make it easy to play in time. Switching it on starts from silence, with no leftover repeats from last time (except when Trails is ringing out).


- **Delay Time** (50 to 2000 ms, default 400 ms; Knob 1): time between repeats.
- **D Feedback** (0 to 100%, default 50%; Knob 2): number of repeats.
- **Delay Mix** (0 to 100%, default 35%; Knob 3): balance between dry and echoes.
- **Delay Mode** (Normal, Triplett, Dotted 8th, default Normal; Knob 4): adds a second echo at two thirds (triplet) or three quarters (dotted eighth) of the delay time.
- **Delay Type** (Forward, Reverse, Octave, ReverseOct, Dual, DualOct, default Forward; Knob 5): Forward is a normal delay. Reverse plays the echoes backwards. Octave plays them an octave up. ReverseOct combines both. Dual runs a forward and a reverse delay together; DualOct adds the octave.
- **Delay LPF** (300 to 20000 Hz, default about 10000 Hz; Knob 6): low-pass filter on the repeats. Lower values give darker, softer repeats that sit behind the playing.
- **D Spread** (0.00 to 1.00, default 0.50; menu only, or Knob 6 in Shift): in Forward, Reverse and Octave types it delays the right channel by 0.5 to 50 ms for a wider stereo image. In the Dual types it pans the forward and reverse delays apart.
- **Mod Amt** (0 to 100%, default 50%; menu only, or Knob 1 in Shift): depth of the modulation.
- **Mod Rate** (0 to 3 Hz, default 1.5 Hz; menu only, or Knob 2 in Shift): speed of the modulation.
- **Mod Param** (None, DelayTime, DelayLevel, DelayPan, default None; menu only, or Knob 4 in Shift): what the modulation controls: the delay time (chorus or vibrato on the repeats), the level of the echoes (tremolo), or their pan (auto-pan).
- **Mod Wave** (Sine, Triangle, Saw, Ramp, Square, Tape, default Sine; menu only, or Knob 3 in Shift): shape of the modulation. Tape gives the slow and fast speed variation of a tape machine (wow and flutter).
- **Sync Mod F** (off or on, default off; menu only, or Knob 5 in Shift): ties the modulation speed to the delay time instead of Mod Rate.
- **Footswitch 2**: tap tempo (tap three times) sets the delay time. The left LED blinks once every two delay times. Holding for about one second toggles Shift: knobs 1 to 6 then control Mod Amt, Mod Rate, Mod Wave, Mod Param, Sync Mod F and D Spread, and the display shows SHIFT.

### Multi Delay

A delay with separate left and right delay times (up to 4 seconds each), feedback, and four extra taps with pitch shifting, for rhythmic, shimmering and stereo echo patterns. In Follower mode the extra taps sit at 2 and 4 times each channel's delay time. In Time mode each tap has its own delay time, so you can build any pattern. Defaults are all at zero, so it is silent until you set a delay time and the Wet amount.

Original to this project.

- **Wet %** (0 to 100%, default 0%; Knob 1): balance between dry and the delayed signal.
- **Delay L ms** (0 to 4000 ms, default 0 ms; Knob 2): delay time of the left channel.
- **Delay R ms** (0 to 4000 ms, default 0 ms; Knob 3): delay time of the right channel.
- **Feedback** (0 to 100%, default 0%; Knob 4): number of repeats of the main delays.
- **Tap Mode** (Follower, Time, default Follower; Knob 5): Follower sets the four taps to 2x and 4x of the left and right delay times (taps 1 and 2 for the left channel, 3 and 4 for the right). Time lets you set each tap time separately with Delay Tap 1 to 4.
- **Delay Tap 1** to **Delay Tap 4** (0 to 4000 ms, default 0 ms; menu only): delay time of each extra tap, used in Time mode.
- **Shift Tap 1** to **Shift Tap 4** (-12 to +12 semitones, default 0; menu only): pitch shift of each extra tap. At the moment only taps 1 and 3 are pitch shifted; the shift on taps 2 and 4 has no effect.
- **Footswitch 2**: tap tempo has no effect on this delay; the left LED is simply on while the effect is on.

### Tape Delay

A delay that sounds like a tape echo machine: repeats that get darker and dirtier each time, with wow and flutter, tape wear and an optional spring-style reverb in the repeats. Instead of one playback head, it models a machine with three heads, so you can pick one head, a pair of heads for a rhythmic double echo, a single-head tape speed, or switch to a short, wobbly LoFi mode for a chorus-like effect. The repeats are saturated in the loop and the feedback can reach slight self-oscillation at high Repeats.

Original to this project, modelled on classic tape echo machines.

- **Time** (50 to 4000 ms, default about 1040 ms; Knob 1): delay time. Moving it glides the pitch of the repeats like a tape motor speeding up or slowing down. In Fixed mode this is the time of the middle head.
- **Mix** (0 to 100%, default 40%; Knob 2): balance between dry and echoes.
- **Repeats** (0 to 98%, default 15%; Knob 3): number of repeats. High values go toward self-oscillation.
- **Mode** (Fixed, Multi, Single, LoFi, default Fixed; Knob 4): Fixed plays one head, chosen with Head Config. Multi plays two heads at once, chosen with Head Config (1+2, 2+3, 1+3). Single plays one head and uses Head Config to set tape speed, and Tap Div to set rhythmic divisions. LoFi turns it into a very short (0.5 to 10 ms) delay with strong tape coloration and no repeats, for a wobbly chorus or vibrato-like sound.
- **WowFlutter** (0.00 to 1.00, default 0.30; Knob 5): amount of tape speed variation. At the top of the range the tape becomes unstable, like a broken cassette. In LoFi mode it controls the wobble depth.
- **Tape Age** (18000 Hz down to 1200 Hz, default about 15900 Hz; Knob 6): high cut of the repeats. Higher values are darker, as with worn tape.
- **Tap Div** (Quarter, Dotted8, Triplet, default Quarter; menu only, or Knob 1 in Shift): rhythmic division of the delay time, in Single mode only.
- **Tape Bias** (0.00 to 1.00, default 0.00; menu only, or Knob 2 in Shift): drive of the tape saturation in the loop. Higher values compress and distort the repeats.
- **Glitches** (0 to 100%, default 20%; menu only, or Knob 3 in Shift): random tape imperfections: dropouts, crinkles and splices. The amount sets how often and how strong.
- **Low End** (20 to 350 Hz, default about 150 Hz; menu only, or Knob 4 in Shift): high-pass filter on the repeats. Higher values keep the repeats thinner and out of the way of the low strings.
- **Reverb** (0 to 100%, default 0%; menu only, or Knob 5 in Shift): amount of reverb added to the echoes.
- **Head Config** (Head1, Head2, Head3, default Head2; menu only, or Knob 6 in Shift): in Fixed mode, which head is heard (at 0.5x, 1x or 1.5x of the Time). In Multi mode, which pair of heads (1+2, 2+3, 1+3). In Single mode, the tape speed: Head1 is faster (0.75x time), Head2 normal, Head3 slower (1.35x time).
- **Footswitch 2**: tap tempo (tap three times) sets the delay time (not in LoFi mode). The left LED blinks once every two delay times. Holding for about one second toggles Shift: knobs 1 to 6 then control Tap Div, Tape Bias, Glitches, Low End, Reverb and Head Config, and the display shows SHIFT.

### Dual Echo

A two-voice modulated echo, called "Halo" in some places (such as the README). It runs two echoes at the same time, one at the set time and one at a dotted eighth of it, with modulation only on the repeats, saturation and compression in the feedback loop, and a momentary infinite hold. With the Halo Rhythm (the default) it has the shimmering, drifting dual-echo sound often used for ambient lead playing. The Rhythm option selects between that and simpler echo types.

Inspired by the Andy Timmons "Halo" dual echo (the Rhythm names and the knob order follow that unit's panel); the implementation is original to this project.

- **Time** (5 to 1500 ms, default about 440 ms; Knob 1): echo time. The scale is not linear: it has more resolution at short times.
- **Level** (0 to 100%, default 50%; Knob 2): volume of the echoes. They are added to the dry signal, which stays the same.
- **Feedback** (0 to 100%, default 35%; Knob 3): number of repeats.
- **Rate** (0.1 to 10 Hz, default about 0.4 Hz; Knob 4): speed of the modulation on the repeats.
- **Depth** (0 to 40 cents, default about 7 ct; Knob 5): amount of the modulation. This gives the repeats a chorused drift.
- **Tone** (1000 to 8750 Hz, default about 4600 Hz; Knob 6): high cut on the repeats. Lower values are darker.
- **Rhythm** (Quarter, Dotted8, Halo, BBD, Tape, default Halo; menu only): Quarter and Dotted8 run both echoes at the set time or at three quarters of it. Halo runs one echo at the time and one at a dotted eighth, with the repeats drifting differently on each side for a wide stereo image. BBD gives a darker, analog bucket-brigade delay sound with triangle modulation and a softer high end. Tape gives a tape-style echo with two heads and wow and flutter.
- **Saturate** (0 to 100%, default 30%; menu only): saturation and compression in the repeats. Higher values also darken them.
- **Low Cut** (10 to 300 Hz, default about 55 Hz; menu only): high-pass filter in the loop, to keep the repeats from muddying the low end.
- **Spread** (0 to 100%, default 50%; menu only): stereo width of the echoes.
- **Cascade** (0 to 100%, default 50%; menu only): in Halo only, feeds the quarter echo into the dotted-eighth line, like two pedals in series, for denser rhythmic patterns.
- **Footswitch 2**: tap tempo (tap three times) sets the time. The left LED blinks at the tempo. Holding the footswitch for about one second gives a momentary Hold: the repeats are frozen into a near-infinite loop and the input is muted, until you release the switch. The display shows HOLD and the LED is steady while held.

### GranDelay

A granular delay: the input is recorded into a short buffer and played back as many small overlapping "grains", which can be shifted in pitch, spread in stereo and played at different speeds, even backwards. It makes shimmery, smeared and glitchy textures that are not like a normal echo. Footswitch 2 freezes the buffer, so you can play over a sustained texture.

Based on the granular player of the DaisySP library.

- **Size(ms)** (1 to 300 ms, default 150 ms; Knob 1): length of each grain. Short grains sound rough and buzzy; long grains sound smoother and more like an echo.
- **Mix** (0 to 100%, default 50%; Knob 2): balance between dry and granular signal.
- **Pitch** (-12 to +12 semitones in whole steps, default 0; Knob 3): pitch shift of the grains.
- **Spread** (0 to 100%, default 40%; Knob 4): stereo spread of the grains.
- **GrainEnv** (Cos, SlowAtk, FastAtk, default Cos; Knob 5): shape of each grain's volume envelope. Cos is smooth and soft, SlowAtk fades each grain in slowly, FastAtk starts each grain sharply.
- **Speed** (-2.00 to 2.00x, default 0.50x; Knob 6): playback speed of the buffer. 1x is normal, 0.5x is half speed and negative values play it backwards.
- **Width** (0 to 50 ms, default 25 ms; menu only): random offset applied to the start of each grain. 0 gives a regular pattern; higher values smear and scatter the grains.
- **Footswitch 2**: toggles Hold. While on, the buffer stops recording and the current sound is kept; the left LED is on. Press again to resume.

### SpctDelay

Spectral delay: the sound is split into many frequency bands and each band gets its own delay time and feedback, so that different parts of the notes come back at different times. It gives sweeping, ghostly echoes that smear a chord into a rising, falling or random cloud. The Time Mode and Feedback Mode decide how the delay time and feedback are distributed across the spectrum.

It has no counterpart in a conventional pedal. The analysis is short, so the repeats have a slightly smeared character.

- **Mix** (0 to 100%, default 50%; Knob 1): balance between dry and spectral echoes.
- **Time** (0.0 to 4.0 s, default 2.0 s; Knob 2): the delay time scale. The delay each band gets depends on Time Mode: up to this time at most.
- **FDBK** (0 to 100%, default 50%; Knob 3): the feedback scale; how each band's feedback is set depends on FDBK Mode.
- **TimeMode** (Random, Sine, LinearUp, LinearDn, Const, default Random; Knob 4): how delay times are distributed across the frequency bands. Random gives each band a random time. Sine makes the time vary smoothly up and down across the spectrum. LinearUp and LinearDn make the time rise or fall steadily from the lows to the highs, giving a sweeping effect. Const gives every band the same time, like an ordinary delay.
- **FDBK Mode** (Random, Sine, LinearUp, LinearDn, Const, default Random; Knob 5): the same choices for feedback: random per band, a sine pattern, rising or falling with frequency, or the same for all bands.
- **Tone** (0 to 3750 Hz, default 0 Hz; Knob 6): removes everything below this frequency from the delayed signal.

### PluckEcho

A plucked-string synth with an echo, played from a MIDI keyboard. Each MIDI note triggers a plucked string sound that feeds an echo with feedback. Your guitar signal passes through unchanged, so you can play along with it. It makes harp-like or kalimba-like echoing plucks.

Original to this project (a small synth built on the DaisySP plucked string model).

- **StringDecay** (0.00 to 1.00, default 0.50; Knob 1): how long each string rings. Higher values sustain longer.
- **DelayTime** (1 ms to 5 s, default about 1250 ms; Knob 2): echo time. The knob is not linear: it has finer control at short times.
- **DelayFdbk** (0 to 100%, default 50%; Knob 3): number of repeats of the plucks.
- **Level** (0 to 100%, default 50%; Knob 4): volume of the plucks and their echoes, added to the guitar signal.
- **MIDI notes**: each note on message plays a pluck at that pitch; note off messages do nothing.

## Reverb

Reverbs that add space to the sound. With Trails on, the tail of each of them rings out when the effect is switched off.

### Reverb

A straightforward stereo reverb, from small room to long wash, with a single decay control and a damping filter. It is simple to dial in: set the Mix, then shape the length with Time and the brightness with Damp. The Mix is a crossfade, so turning it up also turns the dry signal down.

Based on the ReverbSc reverb of the DaisySP library (a feedback delay network reverb in the style of Sean Costello's design for Csound).

- **Time** (0.00 to 1.00, default 0.45; Knob 1): reverb decay. Low values give a short room, high values a long tail that lasts many seconds.
- **Damp** (16000 Hz down to 600 Hz, default about 8100 Hz; Knob 2): low-pass filter in the reverb. Turning it up lowers the cutoff, so the tail gets darker and softer; at the low end the tail is bright.
- **Mix** (0 to 100%, default 45%; Knob 3): balance between dry and reverb.

### Spring Reverb

The reverb of old guitar amps: a signal sent down one or more metal springs and picked up at the other end. It is thin and bright, with a characteristic "drip" or "boing": every echo is a short upward sweep, because the highs travel slower along a spring and arrive last. It is not a room or a hall and does not sound like one. The input is mono (left and right are summed, like the driver of a real tank).

The effect models this physically, with 2 to 6 springs of different lengths. Real tanks have two or three springs; more springs here give a denser tail that sounds less like a series of echoes. Original to this project (a physical model written for it).

- **Dwell** (0.0 to 10.0, default 5.0; Knob 1): how hard the tank is driven, as on a Fender amp. It sets the level of the reverb and how much it splashes. With a loud guitar and a high Dwell the springs are driven into saturation, which gives the crashing character of a spring hit hard.
- **Decay** (0.5 to 6.0 s, default 1.7 s; Knob 2): time for the echoes to die away. Real tanks are around 1 to 3 seconds; the upper range is for ambient use. The highs die away faster than the rest.
- **Tone** (1500 to 12000 Hz, default about 4200 Hz; Knob 3): high cut on the return. Low values are dark and soft, high values bright and "sizzly".
- **Mix** (0 to 100%, default 30%; Knob 4): how much reverb is added to the dry signal, like the reverb knob of an amp. The dry sound is never turned down, so switching the reverb on does not make you quieter. The default is a subtle reverb; much higher settings start to sound like an echo effect.
- **Drip** (0 to 100%, default 100%; Knob 5): how much each echo sweeps. At 100% the springs sweep like a real spring, the lows of an echo coming back first and the highs later. At 0% the echoes are plain repeats with no "boing". Most of the effect is there at about 75%.
- **Tank** (Short, Medium, Long, default Medium; Knob 6): length of the springs, which sets the spacing of the echoes. Short is tight and quick, Long is slower and more spread out. Changing it while playing gives a short glide in the tail.
- **Springs** (2 to 6, default 4; menu only): number of springs. Each adds a train of echoes at its own spacing, so more springs make a denser, less regular tail. With only 2 springs the echoes are easier to hear as a delay.
- **Output** (Stereo or Mono, default Mono; menu only): Mono puts all springs on both outputs. Stereo puts every other spring on each side (1st, 3rd and 5th left; 2nd, 4th and 6th right) for a wider tank.

Tip: If it sounds like a delay, lower the Mix, use more springs, keep Drip at 100%, shorten the Decay, or use the Short or Medium tank.

Tip: Surf and rockabilly: Mix 40 to 60%, Drip 100%, Decay about 2 s, Tone high, Dwell around 6, with a Booster in front. Subtle ambience: Mix 15 to 25%, Decay 1 s, Drip about 70%. In an effect chain, put it last.

### CloudSeed

A large, lush reverb with a long, smooth tail, built on the CloudSeed engine: pre-delay, diffusion, a modulated tail and a tone control, with eight built-in room and space presets. The tail can be very long (decay up to a minute), so it also works for ambient washes. Because the Daisy has limited processing power, the engine runs with fewer delay lines than the desktop plugin, so the presets sound a little different from it.

CloudSeed is an algorithmic reverb engine by Ghost Note Audio (MIT licence), adapted from the FunBox project for the Daisy Seed. CloudSeed can only be used in one effect slot at a time (it cannot be chained with other effects built on the same engine).

- **PreDelay** (0 to 950 ms, default 0 ms; Knob 1): time before the reverb starts. A longer pre-delay keeps the dry note clear of the reverb.
- **Mix** (0 to 100%, default 50%; Knob 2): balance between dry and reverb. In wet-only mode (see Footswitch 2) this knob becomes the dry volume.
- **Decay** (0.05 to 60 s, default about 1.9 s; Knob 3): length of the tail. The scale is exponential: most of the knob travel covers the shorter times.
- **Mod Amt** (0 to 2.5 ms, default 1.25 ms; Knob 4): amount of modulation in the tail. It adds a gentle shimmer and avoids a metallic ring.
- **Mod Rate** (0 to 5 Hz, default about 0.45 Hz; Knob 5): speed of that modulation.
- **Tone** (400 to 20000 Hz, default about 4300 Hz; Knob 6): low-pass cutoff on the output. Moving this knob switches the filter on.
- **Preset** (FChorus, DullEchos, Hyperplane, MedSpace, Hallway, RubiKa, SmallRoom, 90s; default RubiKa; menu only): the base character of the reverb. Choosing a preset loads its settings. With KnobsOvrd on, your current knob settings are applied on top of the preset; with it off, the preset's own settings apply until a knob is moved.
- **Sum2Mono** (off or on, default off; menu only): sum left and right into a mono signal before output.
- **StereoIn** (off or on, default off; menu only): feed the left and right inputs separately instead of the left input to both.
- **KnobsOvrd** (off or on, default off; menu only): see Preset.
- **DryVolume** (level in dB, default -6 dB; menu only, and Knob 2 in wet-only mode): dry signal level in wet-only mode.
- **Footswitch 2**: toggles wet-only mode. The input to the reverb is cut, so the existing tail rings out and the Mix knob becomes the dry volume; press again to return to normal. The left LED pulses while the mode is on.

### Dattorro

A plate reverb: smooth, dense and bright, without the distinct echoes of a room or a spring. It has the classic plate character, good on its own for a lush sustained tail, with a Size control to move from a small plate to a very large space. Mix can reach fully wet.

It is an implementation of Jon Dattorro's 1997 plate reverb algorithm, adapted from Valley Audio Soft's Plateau (Dale Johnson), via the Hothouse "Flick" and the AmpSim of the bkshepherd/DaisySeedProjects project. This code is licensed under GPLv3, not MIT like the rest of the project, so it is only included in builds that opt in to it (a build with this effect must be distributed under GPLv3).

- **Mix** (0 to 100%, default 9%; Knob 1): amount of reverb. The knob is not linear: it opens up slowly from the left and reaches fully wet at the right.
- **Pre Dly** (0 to 250 ms, default 0 ms; Knob 2): time before the reverb starts.
- **Decay** (0 to 100%, default 80%; Knob 3): length of the tail.
- **Tone** (about 14 to 14000 Hz, default about 2100 Hz; Knob 4): high cut inside the reverb tank. Lower values are darker.
- **Mod** (0 to 100%, default 30%; Knob 5): modulation of the tank, which adds movement and smooths out metallic ringing.
- **Diffuse** (0 to 100%, default 85%; Knob 6): density of the reverb. Higher values are smoother; lower values are sparser and more distinct.
- **Size** (0.50x to 4.00x, default 1.01x; menu only): scales the delay times in the reverb, from a small plate to a huge space.

### Verb Delay

A delay and a reverb in one effect, each with its own time, level and mix, for ambient and "spacey" tones without needing two effects. The delay has the same options as the Delay effect (triplet and dotted eighth timing, reverse, octave, dual, modulation). The two can run side by side, or the reverb can be placed after the delay so that the echoes themselves are reverberated (Series D>R). Original to this project, assembled from the Delay and Reverb effects.

- **Delay Time** (50 to 1500 ms, default 400 ms; Knob 1): time between echoes.
- **D Feedback** (0 to 100%, default 45%; Knob 2): number of repeats.
- **Delay Mix** (0 to 100%, default 45%; Knob 3): balance between the dry signal and the echoes.
- **Reverb Time** (0.00 to 1.00, default 0.45; Knob 4): decay of the reverb.
- **Reverb Damp** (16000 Hz down to 600 Hz, default about 6100 Hz; Knob 5): low-pass in the reverb. Higher knob values lower the cutoff and darken the tail.
- **Reverb Mix** (0 to 100%, default 45%; Knob 6): balance between dry and reverb.
- **Delay Mode** (Normal, Triplett, Dotted 8th, default Normal; menu only): adds a second echo at two thirds (triplet) or three quarters (dotted eighth) of the delay time, for rhythmic patterns.
- **Series D>R** (off or on, default off; menu only): when on, the reverb is fed by the delay output (echoes reverberate). When off, delay and reverb run in parallel.
- **Reverse** (off or on, default off; menu only): the echoes play backwards.
- **Octave** (off or on, default off; menu only): the echoes play an octave up.
- **Delay LPF** (300 to 20000 Hz, default about 19300 Hz; menu only): low-pass filter on the echoes. Lower values give darker repeats.
- **D Spread** (0.00 to 1.00, default 0.25; menu only): stereo spread. With Dual Delay off, it delays the right echoes by 0.5 to 50 ms; with Dual Delay on, it pans the forward and reverse delays apart.
- **Dual Delay** (off or on, default off; menu only): runs a forward and a reverse delay together, panned by D Spread.
- **Mod Amt** (0 to 100%, default 15%; menu only): depth of the modulation.
- **Mod Rate** (0 to 3 Hz, default 0.75 Hz; menu only): speed of the modulation.
- **Mod Param** (None, DelayTime, DelayLevel, ReverbLevel, DelayPan, default None; menu only): what the modulation controls (pitch wobble of the echoes, tremolo on the echoes or on the reverb, or auto-pan of the echoes).
- **Mod Wave** (Sine, Triangle, Saw, Ramp, Square, default Sine; menu only): shape of the modulation.
- **Sync Mod F** (off or on, default off; menu only): ties the modulation speed to the delay time instead of Mod Rate.
- **Footswitch 2**: tap tempo (tap three times) sets the delay time. The left LED blinks once every two delay times.

## Pitch

Effects that shift the pitch of the incoming signal or add octaves.

### Pitch

A pitch shifter with an optional footswitch-controlled transition, in the spirit of the Digitech Drop and Ricochet pedals. In Latch mode the signal is shifted all the time while the effect is on. In Moment mode the sound is normal until you press the second footswitch: it then glides to the shifted pitch while you hold it, and glides back when you release it, with separate glide times. Use it for drop-tuning on the fly, dive-bomb style effects or a one-octave jump. The output is mono.

Based on the DaisySP pitch shifter (Electrosmith, MIT), modified for this project.

- **Semitone** (1 to 7 semitones or OCT for a full octave, default 1; Knob 1): shift interval.
- **Crossfade** (0 to 100%, default 100%; Knob 2): blend of dry and shifted sound. At 100% only the shifted signal is heard; at 50% you get a harmonised interval.
- **Direction** (DOWN or UP, default DOWN; Knob 3): shift down or up.
- **Mode** (LATCH or MOMENT, default LATCH; Knob 4): LATCH shifts continuously; MOMENT shifts only while the second footswitch is held.
- **Shift** (0.00 to 2.00 s, default 0.00 s; Knob 5): time the glide to the shifted pitch takes after pressing the second footswitch (Moment mode).
- **Return** (0.00 to 2.00 s, default 0.00 s; Knob 6): time the glide back takes after releasing it (Moment mode).
- **Footswitch 2**: in Moment mode, press to shift and release to return. In Latch mode it does nothing. In Moment mode the display shows a row of blocks that fills while the glide runs.

### PolyOctave

A polyphonic octave effect: it adds notes one octave up, one octave down and two octaves down, and these work on chords as well as single notes, with no need to play single notes cleanly. Set each octave voice and the dry signal with their own level. The sub-octaves are filtered to keep the low end tidy. Tracking is best on a clean signal; it is not a distortion, so put the distortion after it. The output is mono.

The octave generator is adapted from the terrarium-poly-octave project by Steven Schulteis (MIT).

- **Dry** (0 to 100%, default 50%; Knob 1): level of the original signal.
- **2 OctDown** (0 to 100%, default 50%; Knob 2): level of the voice two octaves down.
- **1 OctDown** (0 to 100%, default 50%; Knob 3): level of the voice one octave down.
- **1 OctUp** (0 to 100%, default 50%; Knob 4): level of the voice one octave up.

### SciFi

Three effects in one: a poly-octave into a large, long reverb into an overdrive. The octaves are fed into the reverb, and the reverb tail is overdriven, which gives huge, glassy, other-worldly pads and drones from a few played notes. Set Mix low for a subtle shimmer behind your playing and high for the full effect. The output is stereo. It is single-instance only (it can be used once in a chain).

The octave section is the same as in PolyOctave.

- **Dry** (0 to 100%, default 50%; Knob 1): level of the original signal that goes into the reverb.
- **OctDown** (0 to 100%, default 50%; Knob 2): level of the one and two octave down voices (both set together).
- **OctUp** (0 to 100%, default 50%; Knob 3): level of the one octave up voice.
- **Time** (0.00 to 1.00, default 0.80; Knob 4): reverb length. High values give very long tails.
- **Drive** (0.00 to 1.00, default 0.50; Knob 5): amount of overdrive on the reverb (the range is kept moderate).
- **Mix** (0 to 100%, default 50%; Knob 6): processed signal against the unprocessed input.
- **Damp** (500 Hz to 16 kHz, default about 8100 Hz; menu only): low-pass cut-off in the reverb. Higher values of the control give a lower cut-off and a darker tail (the control runs from bright to dark).
- **Level** (0 to 100%, default 50%; menu only): output level of the whole effect.
- **OD Mix** (0 to 100%, default 100%; menu only): how much of the overdriven reverb is heard against the clean reverb.

## Dynamics

Effects that control the level of the signal: compressors and a noise gate.

### Comp

A straightforward single-knob-per-job compressor. It turns down the notes that go above the threshold by the set ratio, which evens out picking dynamics and adds sustain. Attack and Release set how quickly it clamps down and lets go; the Level control adds make-up gain after the compression (the compressor already applies some automatic make-up gain, so the default of +3 dB sits near unity for a typical signal). It is built on the DaisySP compressor.

- **Level** (-12 to +12 dB, default +3.0 dB; Knob 1): output level after compression.
- **Ratio** (1:1 to 20:1, default 4:1; Knob 2): how strongly signal above the threshold is reduced. 2:1 to 4:1 is gentle levelling, 10:1 and up is limiting.
- **Thresh** (-60 to 0 dB, default -20 dB; Knob 3): the level above which compression starts. Lower it to compress more of the playing.
- **Attack** (1 to 100 ms, default 10 ms; Knob 4): time to clamp down. A longer attack lets the pick transient through for a more percussive sound.
- **Release** (10 to 1000 ms, default 200 ms; Knob 5): time to let go after the signal drops. Short is pumpy and lively, long is smooth and sustained.

Tip: set Thresh so the compressor works on loud notes only, then raise Level until the bypassed and active levels match.

### DuckComp

A side-chain compressor: the audio is turned down by the level of a separate "key" signal instead of by its own level. With the key on the right input, you play or feed in a signal on the right input (it is not heard) and the audio on the left input ducks every time the key is loud, which gives the classic pumping or ducking effect, for example a drum loop making a pad or reverb tail get out of the way. Set Key Input to Left and the audio is its own key, which makes it an ordinary compressor.

Original to this project; it uses the Simple Compressor by ChunkWare Music Software (2006, open source).

For the side-chain to work, the key must arrive on the right input, so turn off the pedal's "Split mono input to stereo" setting.

- **Level** (-12 to +12 dB, default +3.0 dB; Knob 1): make-up gain.
- **Ratio** (1:1 to 40:1, default 8:1; Knob 2): how strongly the audio is reduced when the key is above the threshold.
- **Thresh** (-80 to 0 dB, default -30 dB; Knob 3): the key level above which ducking starts.
- **Attack** (1 to 100 ms, default 5 ms; Knob 4): how fast the audio is ducked.
- **Release** (10 to 2000 ms, default 150 ms; Knob 5): how fast it comes back after the key stops. Longer values give a slower, more obvious pump.
- **Key Input** (Left or Right, default Right; menu only): Right uses the right input as the key and does not play it (side-chain); Left uses the audio itself as the key (a normal compressor).
- **Display and LED**: the gain reduction in dB is shown on the screen, and the left LED lights in proportion to it (full at 12 dB of reduction or more).

### Noise Gate

A gate that silences the signal when it falls below a threshold, to remove hiss and hum between notes or during pauses, which is most useful after high-gain effects. When the signal is above the threshold the gate opens; when it drops below, the gate waits for the Hold time and then fades out over the Fade time. The left LED is lit while the gate is open.

Based on the Cycfi Q library's envelope follower. Original to this project as an effect.

- **Threshold** (about -100 to -14 dB, default -20 dB; Knob 1): the level below which the gate closes. Raise it until the noise disappears but your note tails are not cut off.
- **Attack** (1 to 20 ms, default 1 ms; Knob 2): how fast the level detector reacts to rising signal.
- **Release** (1 to 500 ms, default 50 ms; Knob 3): how fast the level detector reacts to falling signal.
- **Hold** (0 to 1000 ms, default 50 ms; Knob 4): how long the gate stays fully open after the signal drops below the threshold. Longer values stop it from chattering on decaying notes.
- **Fade** (0 to 500 ms, default 200 ms; Knob 5): how long the closing takes. Short is abrupt, long is a smooth fade-out.

## Equalizers

Tone-shaping effects that boost or cut parts of the spectrum.

### GraphicEQ

A six-band graphic equalizer with bands one octave apart, each with a boost or cut of up to 15 dB. The bands are peaking filters (a bell around the centre frequency), and the width of each band is set for even coverage, wider at the low end and narrower at the top. Use it to shape the tone before or after an amp or distortion, or to fix a room or a pickup.

Original to this project (a conventional graphic EQ design).

- **100** (-15 to +15 dB, default 0 dB; Knob 1): the 100 Hz band, body and low end.
- **200** (-15 to +15 dB, default 0 dB; Knob 2): the 200 Hz band, warmth and boominess.
- **400** (-15 to +15 dB, default 0 dB; Knob 3): the 400 Hz band, lower mids and mud.
- **800** (-15 to +15 dB, default 0 dB; Knob 4): the 800 Hz band, mids and honk.
- **1600** (-15 to +15 dB, default 0 dB; Knob 5): the 1600 Hz band, upper mids and presence.
- **3200** (-15 to +15 dB, default 0 dB; Knob 6): the 3200 Hz band, bite and brightness.

### ParaEQ

A three-band parametric equalizer: low, mid and high bands, each with its own frequency, gain of up to 15 dB boost or cut, and a choice of three widths. Because the frequencies are adjustable you can aim a band at a specific problem or a specific tone, such as a notch in the low mids or a lift in the upper mids for a solo. All bands are peaking (bell) filters.

Original to this project (a conventional parametric EQ design).

- **Low Freq** (35 to 500 Hz, default 130 Hz; Knob 1): centre frequency of the low band.
- **Mid Freq** (250 to 5000 Hz, default 1100 Hz; Knob 2): centre frequency of the mid band.
- **High Freq** (1000 to 20000 Hz, default 4400 Hz; Knob 3): centre frequency of the high band.
- **Low Gain** (-15 to +15 dB, default 0 dB; Knob 4): boost or cut of the low band.
- **Mid Gain** (-15 to +15 dB, default 0 dB; Knob 5): boost or cut of the mid band.
- **High Gain** (-15 to +15 dB, default 0 dB; Knob 6): boost or cut of the high band.
- **Low Q** (Narrow, Medium, Wide, default Medium; menu only): width of the low band.
- **Mid Q** (Narrow, Medium, Wide, default Medium; menu only): width of the mid band.
- **High Q** (Narrow, Medium, Wide, default Medium; menu only): width of the high band. Narrow is for surgical cuts and boosts, Wide for broad tone shaping.

## Amp and Cabinet

Amp and speaker cabinet simulation, for playing straight into a mixer, interface or power amp.

### NAM

A neural amp modeler: it runs a captured guitar amp (or amp and pedal) as a small neural network, so the response to your playing, touch and guitar volume is close to the real amplifier. The effect uses the A2 model format of the Neural Amp Modeler project (by Steve Atkinson), which is light enough to run on the pedal; the models are stored in the firmware. Put it where the amp would be in the chain, and optionally add one of the two built-in cabinet impulse responses with IR Cab (use a cabinet simulation, either here or an IR effect after it, unless you play into a guitar cabinet). A three-band tone control follows the model. Loudness is matched between the models, so switching models should not jump in volume.

The list of models is built into the firmware and depends on the build. In the current source the models are 67SRV, Ampeg, BE-100, friedman, JCM800, 1959BJA and Mesa (listed in this order, so the first one is the default). Models can be added or removed when the firmware is built, so the list on your pedal may differ, and because saved presets store the model by its position in the list, a preset can point at a different model after the list changes.

- **Gain** (-100 to +6 dB, default 0.0 dB; Knob 1): input drive into the model. More gain pushes the amp into more distortion. It is a logarithmic control.
- **Level** (-100 to +6 dB, default -4.4 dB; Knob 2): output level.
- **Model** (list of models, default the first; Knob 3): which amp capture to use.
- **Bass** (-10 to +10 dB, default 0.0 dB; Knob 4): low shelf-like boost or cut around 110 Hz.
- **Mid** (-10 to +10 dB, default 0.0 dB; Knob 5): boost or cut around 900 Hz.
- **Treble** (-10 to +10 dB, default 0.0 dB; Knob 6): boost or cut around 4 kHz.
- **EQ** (On or Off, default On; menu only): switches the three-band tone control in or out.
- **IR Cab** (Off, Rhythm, Lead, default Off; menu only): adds a built-in cabinet impulse response after the amp (see IR below). Off outputs the amp model directly.
- **LED**: the left LED follows the output level.

### IR

A cabinet simulator that convolves your signal with an impulse response (IR), which is a recording of how a speaker cabinet and microphone sound. Put it after an amp model, a preamp or a distortion effect to turn a raw "direct" sound into a miked-up cabinet sound. Two cabinet impulse responses are built into the firmware, named Rhythm and Lead; which responses are available depends on the firmware build. Original to this project (the impulse responses are stored in the firmware).

- **IR** (Rhythm or Lead, default Rhythm; Knob 1): the cabinet impulse response.
- **Level** (-100 to +6 dB, default 0.0 dB; Knob 2): output level.
- **LED**: the left LED follows the output level.

## Synths and Rhythm

Sound generators that are played from a MIDI keyboard (or, for the drum machine, run by themselves). The keys effects need a MIDI keyboard connected and MIDI turned on in the pedal's settings; they are played in addition to your guitar, which passes through (except in FmKeys, which outputs only the synth).

### MidiKeys

A simple polyphonic synthesizer played from a MIDI keyboard: up to eight notes at once, each a sawtooth wave through a resonant low-pass filter with a short attack and a quick release. Your guitar signal passes through unchanged and the synth is mixed in, so you can play both. Note velocity sets the loudness of each note. Original to this project (it is built from the standard DaisySP oscillator, filter and envelope).

- **CutoffFreq** (250 to 8500 Hz, default 4375 Hz; Knob 1): the low-pass cutoff of the voices. Low is dark and mellow, high is bright and buzzy.
- **Level** (0 to 100%, default 50%; Knob 2): volume of the synth against your guitar.

### StringKeys

A plucked-string voice played from a MIDI keyboard: a physical model of a string (Karplus-Strong style) that sounds like harp, guitar or other plucked and struck strings. One note at a time. Velocity sets how hard the string is plucked. Your guitar signal passes through and the synth is added. The voice is the DaisySP string voice, which comes from the Mutable Instruments Plaits code (MIT licence).

- **Structure** (0 to 1, default 0.5; Knob 1): the inharmonicity of the string, from a clean string to a stiff, metallic sound.
- **Brightness** (0 to 1, default 0.5; Knob 2): how bright the sound is.
- **Level** (0 to 100%, default 50%; Knob 3): volume of the synth against your guitar.
- **Damping** (0 to 1, default 0.5; Knob 4): how fast the note dies away. Low is a long ring, high is short and muted.

### ModalKeys

A struck-resonator voice played from a MIDI keyboard: a physical model of a bar, plate or bell being hit, which sounds like marimba, vibraphone, bells and other mallet instruments. One note at a time. Velocity sets how hard it is struck. Your guitar signal passes through and the synth is added. The voice is the DaisySP modal voice, which comes from the Mutable Instruments Plaits code (MIT licence).

- **Structure** (0 to 1, default 0.5; Knob 1): the spacing of the resonator's overtones, from wood-like to metallic.
- **Brightness** (0 to 1, default 0.5; Knob 2): how bright the sound is.
- **Level** (0 to 100%, default 50%; Knob 3): volume of the synth against your guitar.
- **Damping** (0 to 1, default 0.5; Knob 4): how long the note rings. Low is a long ring, high is short.

### FmKeys

A polyphonic two-operator FM synthesizer played from a MIDI keyboard. A modulator oscillator changes the frequency of the carrier (the oscillator you hear), producing bell, electric piano, bass and metallic tones. The ratios set the pitch relation of the two oscillators (whole and half numbers give harmonic tones, other values are bell-like and clangy), Mod Level sets how much FM there is, and each oscillator has its own envelope. This effect outputs only the synth, not your guitar signal. Velocity sets the loudness.

The Carrier is the sound you hear; the Mod (modulator) controls the brightness and character. Original to this project.

- **Level** (0 to 100%, default 50%; Knob 1): carrier level.
- **Ratio** (0 to 16x in steps of 0.5, default 1.0x; Knob 2): carrier frequency as a multiple of the note played.
- **ModLevel** (0 to 100%, default 4%; Knob 3): the amount of FM. The control is exponential, so most of the useful range is at the lower end.
- **ModRatio** (0 to 16x in steps of 0.5, default 16.0x; Knob 4): modulator frequency as a multiple of the note played.
- **CarAttack** (0.1 to 1000 ms, default 250 ms; Knob 5): carrier envelope attack time.
- **CarDecay** (0.1 to 1000 ms, default 250 ms; Knob 6): carrier envelope decay time.
- **CarSustain** (0.1 to 100%, default 50%; menu only): carrier envelope sustain level.
- **CarRelease** (1 to 1000 ms, default 500 ms; menu only): carrier envelope release time.
- **ModAttack** (0.1 to 1000 ms, default 250 ms; menu only): modulator envelope attack time.
- **ModDecay** (0.1 to 1000 ms, default 250 ms; menu only): modulator envelope decay time.
- **ModSustain** (0.1 to 100%, default 50%; menu only): modulator envelope sustain level.
- **ModRelease** (1 to 1000 ms, default 500 ms; menu only): modulator envelope release time. A modulator envelope that decays faster than the carrier gives a bright attack that mellows out.

### Drum

A small drum machine that can run on its own or be played from a MIDI keyboard. The voices are the DaisySP analog and synthesized bass drum, snare drum and hi-hat. In Machine mode a rock beat plays automatically at the set tempo, in 4/4, 3/4 or 6/8; in Midi mode it plays only the voice chosen in MidiVoice from the notes you play (or a five-drum kit on fixed keys). Tap the second footswitch three times to set the tempo, and the left LED flashes on the beat. Your guitar can pass through at the same time. Original to this project (the sound engines are from DaisySP).

- **Level** (0 to 100%, default 50%; Knob 1): drum volume.
- **Voice** (Analog, Synth, Mix1, Mix2, default Analog; Knob 2): which sound set the built-in beats use. Analog uses the analog bass and snare, Synth the synthesized bass and snare, Mix1 the synth bass with the analog snare, and Mix2 the analog bass with the synth snare. The hi-hat is the same in all.
- **Beats** (4/4 Rock1, 4/4 Rock2, 4/4 Rock3, 4/4 Rock4, 3/4 Rock1, 3/4 Rock2, 6/8 Rock1, 6/8 Rock2, default 4/4 Rock1; Knob 3): the pattern played in Machine mode. 4/4 Rock1 is a basic boom-chick, Rock2 adds hi-hat, and Rock4 is a "Walk This Way" style beat.
- **Tempo** (40 to 200 BPM, default 100 BPM; Knob 4): the speed of the beat.
- **MidiVoice** (AnalogBass, SynthBass, AnalogSnare, SynthSnare, HiHat, Kit, default AnalogBass; Knob 5): the voice played from the MIDI keyboard, and the voice that Timbre, Tone and Decay edit. In Kit, five drums are on fixed keys: C4 analog bass, D4 synth bass, E4 analog snare, F4 synth snare, G4 hi-hat. For the single voices the note you play sets the pitch.
- **Timbre** (0 to 100%, default 50%; Knob 6): a voice-specific character control: the self-FM amount of the analog bass, the dirtiness of the synth bass, the snappiness of the snares, and the noisiness of the hi-hat.
- **Tone** (0 to 100%, default 50%; menu only): the tone of the voice chosen in MidiVoice (for the synth snare it is the FM amount).
- **Decay** (0 to 100%, default 50%; menu only): how long the voice chosen in MidiVoice rings.
- **Mode** (Machine or Midi, default Machine; menu only): Machine plays the automatic beat; Midi plays only what you play on the keyboard.
- **DryThru** (None, Mono, Stereo, default Mono; menu only): whether the guitar passes through with the drums: not at all, as mono, or as stereo.
- **Footswitch 2**: three taps set the tempo (40 to 200 BPM). The left LED flashes with the beat.

Tip: in Machine mode the Timbre, Tone and Decay controls change the voice selected in MidiVoice, so pick the voice you want to shape there first.

## Utilities

Tools for practising, setting up and checking levels.

### Looper

A tape-style looper that records up to 60 seconds of audio and plays it back over what you are playing. It works in stereo (when the pedal is in stereo mode, with the MISO option off) or mono. The loop is controlled with the second footswitch, can be played at different speeds and in reverse like a tape reel, and has a low-pass filter to tame the noise from the speed changes. It is built on the DaisySP looper.

- **Input Level** (-100 to 0 dB, default -6.0 dB; Knob 1): level of the incoming signal that goes into the loop and to the output.
- **Loop Level** (-100 to 0 dB, default -6.0 dB; Knob 2): playback level of the recorded loop.
- **Mode** (Normal, One-time, Replace, Fripp, default Normal; Knob 3): how the loop behaves when you overdub. Normal records a loop and layers new takes over it; One-time layers a single overdub; Replace replaces the loop with the new take instead of layering; Fripp is Frippertronics style, where older layers fade as you add new ones.
- **SpeedMode** (None, Stepped, Smooth, default None; Knob 4): None plays the loop at normal speed and ignores the Speed control. Stepped moves the speed in steps of half a speed. Smooth glides to the new speed like a tape reel speeding up or slowing down.
- **Speed** (-3 to +3, default 0.5; Knob 5): playback speed (1 is normal speed); a negative value plays the loop in reverse. Only active in the Stepped and Smooth speed modes.
- **LP Filter** (120 to 20000 Hz, default 20000 Hz; Knob 6): low-pass filter on the output; turn it down to darken the loop or to hide aliasing noise at fast speeds. It is an exponential control.
- **MISO** (On or Off, default Off; menu only): when on, the left input is copied to the right, so a mono source is recorded as a stereo loop. When off, the loop is true stereo.
- **Footswitch 2**: press to record, play and overdub (record, then press again to start playing back, then again to overdub); hold for one second to clear the loop. Tap tempo is not used.
- **LED**: the left LED is lit while recording.

### Metronome

A click track played from the pedal: a clicking tone at the chosen tempo and time signature, mixed with your guitar so you can practise to it. One click per bar is sounded higher (440 Hz against 220 Hz for the others) to mark the bar. The display shows the time signature, the tempo and a moving bar that swings with the beat. Original to this project.

- **Tempo** (35 to 250 BPM, default 165 BPM; Knob 1): the number of beats per minute.
- **Mix** (0 to 100%, default 8%; Knob 2): the level of the click against your guitar. This is a crossfade, so your guitar gets quieter as the click gets louder.
- **Meter** (4/4, 3/4, 2/4, default 4/4; Knob 3): the number of beats per bar.
- **Footswitch 2**: three taps set the tempo (35 to 250 BPM). The left LED shows the beat.

### Tuner

A chromatic tuner. Play a note and the screen shows the note name and octave, a 21-block meter that moves to the left when you are flat and to the right when you are sharp (the centre block is in tune), and the detected frequency in Hz. The meter covers up to about 45 cents either way, and the centre block lights when you are within 1 cent. You can also reach it quickly by holding the bypass footswitch.

Original to this project (it uses a frequency detector based on the Cycfi Q library).

- **Mute** (On or Off, default On; Knob 1): when on, the output is silent while tuning, so you can tune quietly on stage; when off, your guitar is passed through.

### Scope

An oscilloscope on the pedal's display: the incoming signal is drawn as a waveform across the screen, which helps to see clipping, check what an effect does to the shape of the signal, or look at an LFO or a generated signal. The audio passes through unchanged. There are no controls. Original to this project.

### TestTone

A signal generator for checking levels together with the pedal's input and output gain settings and the level readout in the Show Debug setting. It replaces the input signal while the effect is on and plays a sine, square or triangle wave of a chosen frequency, or white noise, at a chosen level. The noise is set to the same RMS level as a sine of the chosen level. Changes of level and type are smoothed, so there are no clicks. Original to this project.

- **Level** (-60 to 0 dB, default -20.0 dB; Knob 1): the peak level of the tone (for noise, the same RMS level as a sine at that level).
- **Type** (Sine, Square, Triangle, Noise, default Sine; Knob 2): the signal shape.
- **Frequency** (20 to 20000 Hz, default 1000 Hz; Knob 3): the pitch of the tone. It is a logarithmic control and does not affect Noise.

## Funbox

FunBox is an open source Daisy Seed pedal project by GuitarML (Keith Bloemer), where each pedal is named after a planet or moon. The effects below are ports of those pedals; they appear in the effect list with the `funbox-` prefix (for example `funbox-venus`). The three-way and dip switches of the original pedals became menu items here, the original footswitch handling is replaced by this pedal's secondary footswitch, and the expression pedal is not supported. All of them are MIT licensed except Earth, which is GPLv3 and only built when you opt in. The five newest ports (Balnea, Pluto, Cassini, Mars, Mercury) still need testing on hardware.

### Venus

`funbox-venus` is a spectral reverb: it works on the frequency content of the signal, which gives a lush, string-like wash, with optional octave and fifth shimmer, detune, a spectral freeze, two lo-fi modes and two "drift" modes that slowly modulate the controls. Mono in, the same signal on both outputs. It is the most demanding effect in the set. Adapted from the Venus pedal of the FunBox project (GuitarML, Keith Bloemer, MIT); the spectral processing builds on DaisySTFT and the reverb maths on the Atlantis Reverb by Geraint Luff.

- **Decay** (0-100%, default 50%; Knob 1): reverb decay time.
- **Mix** (0-100%, default 40%; Knob 2): dry/wet balance.
- **Damp** (0-100%, default 70%; Knob 3): high frequency roll-off in the reverb tail.
- **Shimmer** (0-100%, default 0%; Knob 4): amount of the octave shimmer fed into the reverb.
- **Shim Tone** (0-100%, default 0%; Knob 5): adds the fifth on top of the octave in the shimmer.
- **Detune** (default centre; Knob 6): detunes the reverb down below the middle and up above it, with a small dead zone at the middle.
- **Shim Mode** (Oct Down / Oct Up / Oct Up+Dn, default Oct Up; menu only): direction of the shimmer. Oct Up+Dn adds both octaves.
- **LoFi** (Mild / Off / Heavy, default Off; menu only): degrades the wet signal only. Mild is about 9.6 kHz sample rate with a low-pass, Heavy about 6.4 kHz.
- **Drift** (Slow / Off / Fast, default Off; menu only): slow oscillators modulate Damp, Shimmer, Shim Tone and Detune.
- **Freeze** (on/off, default off; menu only): holds the reverb.
- **Footswitch 2**: hold to freeze the reverb for as long as it is held; LED 2 shows the freeze.

### Earth

`funbox-earth` is a plate reverb (Dattorro design) with an optional octave effect in front of it and a momentary action on the secondary footswitch (freeze the reverb, swell an overdrive on it, or switch the octave on). Mono in, stereo out. Adapted from the Earth pedal of the FunBox project (GuitarML, Keith Bloemer). The reverb is a modified version of the Dattorro implementation in VCV Rack Plateau by Valley Audio Soft, and the octave comes from Terrarium-Poly-Octave. Licence: GPLv3 or later, unlike the rest of this project. It is only available in builds that opt in to the Dattorro reverb (`ALLOW_GPL=y`), and a firmware binary containing it must be distributed under GPLv3.

- **PreDelay** (0-700 ms, default 0 ms; Knob 1): delay before the reverb starts.
- **Mix** (0-100%, default 40%; Knob 2): dry/wet balance.
- **Decay** (0-100%, default 80%; Knob 3): reverb decay.
- **Mod Depth** (0-100%, default 30%; Knob 4): intensity of the tank modulation.
- **Mod Speed** (0.3-15.3 x, default about 4.8 x; Knob 5): rate of the tank modulation.
- **Filter** (about 14 kHz down to 110 Hz, default about 5.3 kHz; Knob 6): low-pass on the reverb input. Fully left is open, fully right is dark.
- **Size** (Small / Medium / Big, default Medium; menu only): room size.
- **Octave** (None / Up / Up+Down, default None; menu only): octave effect in front of the reverb.
- **FS Action** (Freeze / Overdrive / Octave, default Freeze; menu only): what the secondary footswitch does while held. Freeze holds the reverb, Overdrive swells an overdrive on the reverb, Octave switches the octave on.
- **In Diffuse** (on/off, default on; menu only): input diffusion of the reverb.
- **Oct Only** (on/off, default off; menu only): with an octave on, send only the octave to the reverb and not the dry signal. Up+Down always includes the dry signal.
- **Footswitch 2**: momentary, per FS Action, while held; LED 2 lights while it is held.

Tip: with the octave on and a loud, sustained input the output can clip.

### Jupiter

`funbox-jupiter` is a stereo reverb with long tails (up to a minute) and a low and a high shelf cut after the reverb to keep the tail out of the way of the dry signal. It is built on the CloudSeed reverb engine by Valdemar Erlingsson (MIT). Adapted from the Jupiter pedal of the FunBox project (GuitarML, Keith Bloemer, MIT).

- **Decay** (about 0.7-60 s, default about 6 s; Knob 1): decay time, with an exponential taper so most of the knob is useful.
- **Mix** (0-100%, default 40%; Knob 2): dry/wet balance.
- **Low Gain** (-60 dB to 0 dB, default about -12 dB; Knob 3): how much the low shelf cuts the reverb. 0 dB is no cut.
- **Low Freq** (20 Hz-1 kHz, default about 200 Hz; Knob 4): where the low cut begins. Only audible when Low Gain is below 0 dB.
- **High Gain** (about -30 dB to 0 dB, default about -14 dB; Knob 5): how much the high shelf cuts the reverb. 0 dB is no cut.
- **High Freq** (400 Hz-20 kHz, default about 4.3 kHz; Knob 6): where the high cut begins. Only audible when High Gain is below 0 dB.
- **Lush** (Low / Medium / High, default Medium; menu only): how many reverb lines and how much late diffusion. Low is one line without late diffusion, High is two lines with the most diffusion.
- **Modulation** (Low / Medium / High, default Medium; menu only): amount and rate of the delay line modulation.
- **PreDelay** (Off / 100 ms / 200 ms, default Off; menu only): delay before the reverb.
- **Stereo In** (on/off, default off; menu only): off means the left input feeds both sides, on means true stereo in.

### Saturn

`funbox-saturn` is a spectral delay. The signal is split into frequency bins and every bin has its own delay line with feedback, so the delay time can grow with frequency, follow a sine across the spectrum, or be random per bin. Sounds range from smeared, sweeping repeats to glassy clouds. It also has a pre-delay (with optional feedback), a filter, slow drift, a mono mode and a random per-bin level. Only the lower bins (up to about 8 kHz) are delayed. Adapted from the Saturn pedal of the FunBox project (GuitarML, Keith Bloemer, MIT); the spectral processing builds on DaisySTFT.

- **PreDelay** (50 ms-1 s, default 50 ms; Knob 1): delay before the spectral delay.
- **Mix** (0-100%, default 40%; Knob 2): dry/wet balance.
- **Filter** (default centre, off; Knob 3): low-pass to the left of the middle (only the lower bins), high-pass to the right, nothing in the middle.
- **Time** (0-4 s, default 2 s; Knob 4): delay spread over the bins; what exactly it does depends on Time Mode.
- **Feedback** (0-100%, default 30%; Knob 5): feedback in the delay lines, shaped by Fdbk Mode.
- **DriftRate** (0-1 Hz, default 0.1 Hz; Knob 6): speed of the drift oscillator. Only matters when Drift is on.
- **Time Mode** (Linear / Sine / Random, default Linear; menu only): Linear lets the delay grow with frequency (Time sets the slope), Sine puts a sine wave of delays across the bins, Random gives every bin its own delay.
- **Fdbk Mode** (Same / Linear / Random, default Same; menu only): Same uses the same feedback for every bin, Linear gives less feedback towards high frequencies, Random is random per bin.
- **Drift** (Delay / Off / +PreDelay, default Off; menu only): Delay slowly modulates the delay time and the feedback, +PreDelay modulates the pre-delay too (this warps the pitch).
- **Mono** (on/off, default off; menu only): off is stereo (two transforms), on uses one transform with twice the bins and the same signal on both outputs. Cleared lines make a brief gap when you switch.
- **Stereo In** (on/off, default off; menu only): off means the left input feeds both sides, on means true stereo. Ignored in mono mode.
- **St Spread** (on/off, default off; menu only): gives every bin a random level (the same on both sides).
- **PreFdbk** (on/off, default off; menu only): adds feedback (0.75) to the pre-delay.

Tip: when you turn Time, the delays slide slowly into place, which gives a pitch-bending effect.

### Neptune

`funbox-neptune` is a stereo reverb and delay: the CloudSeed reverb in three modes (Chorus, Medium Space, RubiKa) and a delay of up to 4 seconds with normal, octave and reverse modes, with three ways of routing the two together. Adapted from the Neptune pedal of the FunBox project (GuitarML, Keith Bloemer, MIT). The reverb is the CloudSeed engine by Valdemar Erlingsson (MIT), and the reverse delay line is from the Veno-Echo pedal by Adam Fulford (MIT). The delay times assume a 48 kHz sample rate.

- **Decay** (0.05-60 s, default about 1.9 s; Knob 1): reverb decay time.
- **Mix** (0-100%, default 40%; Knob 2): dry/effect balance. At half Mix the effect is about 3 dB quieter than the dry signal.
- **Dly Time** (50 ms-4 s, default about 1.2 s; Knob 3): delay time. The delay is off at the very bottom of the knob (its repeats die out).
- **Mod** (0-2.5 ms, default 0.75 ms; Knob 4): amount of reverb modulation.
- **LowPass** (400 Hz-20 kHz, default about 4.5 kHz; Knob 5): low-pass on the reverb. The taper is cubic, so most of the knob travel is in the lower range and the top is nearly open.
- **Dly Fdbk** (0-100%, default 30%; Knob 6): delay feedback.
- **Rev Mode** (Chorus / MedSpace / RubiKa, default MedSpace; menu only): the reverb program. Changing it mutes the output briefly while the reverb is reloaded.
- **Routing** (Send / Parallel / Series, default Parallel; menu only): Send also feeds the delay into the reverb (both are heard), Parallel puts the reverb and delay side by side, Series sends the delay only into the reverb (only the reverb is heard).
- **Dly Mode** (Normal / Octave / Reverse, default Normal; menu only): the delay type.
- **Stereo In** (on/off, default off; menu only): off means the left input feeds both sides, on means true stereo.
- **PreDelay** (0-950 ms, default 0 ms; menu only): reverb pre-delay.
- **Mod Rate** (0-5 Hz, default about 0.08 Hz; menu only): reverb modulation rate.
- **Rev/Delay** (default 50%; menu only): balance between the reverb and the delay.

### Uranus

`funbox-uranus` is a granular delay and synth. It cuts the input into small grains and plays them back transposed, at a chosen speed, in a stereo field; half a second of audio (with feedback) is the source for two grain players. With a MIDI keyboard the same grains become a monophonic granular synth (an ADSR over the grains, transposed from the key relative to middle C), or the input is replaced by a polyphonic two-operator FM synth. Adapted from the Uranus pedal of the FunBox project (GuitarML, Keith Bloemer, MIT).

- **Size** (1-301 ms, default about 91 ms; Knob 1): grain size.
- **Mix** (0-100%, default 50%; Knob 2): dry/wet balance.
- **Pitch** (-13 to +13 semitones in steps of one, default 0; Knob 3): transposition of the grains. No change in the middle.
- **Feedback** (1-100%, default 40%; Knob 4): feedback of the buffer the grains are cut from.
- **Width** (0-50 ms, default 25 ms; Knob 5): how far back in time grains can start. Low gives a concise sound, high lets the grains sample a wider range of the audio.
- **Speed** (-2 to +2 x, default +1 x at 75% of the knob; Knob 6): playback speed of the grains. Stopped in the middle, backwards below it.
- **Grain Mode** (Oct Up / Oct Down / Up+Down, default Oct Up; menu only): the two grain players play the unchanged pitch and an octave up, the unchanged pitch and an octave down, or an octave down and an octave up.
- **Grain Env** (Cos / SlowAtk / FastAtk, default Cos; menu only): envelope of every grain: smooth, slow attack with fast decay, or fast attack.
- **Synth Mode** (Off / Granular / FM, default Off; menu only): Off is audio in only. Granular lets a MIDI key play the grains through an ADSR, transposed from middle C (note 60). FM replaces the input with a polyphonic FM synth (12 voices).
- **Hold** (on/off, default off; menu only): stops writing new audio into the buffer so the grains keep playing what is there.
- **Spread** (0-100%, default 40%; menu only): stereo spread of every grain.
- **LFO Depth** (0-100 cents, default 0; menu only): pitch LFO depth.
- **LFO Rate** (0-6 Hz, default 0.24 Hz; menu only): pitch LFO speed.
- **Car Level, Car Ratio, Mod Level, Mod Ratio, Mod Attack, Mod Decay, Mod Sustain, Mod Release** (menu or MIDI CC 20-27): FM synth controls. Ratios run from 0.5 to 16.5 in steps of 0.5 (defaults: carrier 1.0, modulator 16.0).
- **Env Attack, Env Decay, Env Sustain, Env Release** (menu or MIDI CC 28-31): the ADSR of the granular synth and of the FM carrier.
- **Footswitch 2**: toggles Hold; LED 2 shows it.
- **MIDI**: the knobs are CC 14-19. Notes go to the active effect on the pedal's MIDI channel (Global Settings); a note on with velocity 0 is a note off.

### Balnea

`funbox-balnea` is a digital delay and pitch shifter inspired by the CBA Thermae pedal. The delay lines are read at a different speed than they are written, which transposes the repeats; a sequencer steps through up to three speeds, one per delay time (the base speed, the first interval, both intervals together), so the repeats climb or fall in pitch as they recirculate. It adds tap tempo, a reverse mode, tape wow and flutter, a self-oscillating hold and a small MIDI synth to play along with. Adapted from the Balnea pedal of the FunBox project (GuitarML, Keith Bloemer, MIT); the reverse delay line is by Adam Fulford (MIT). It needs more testing on hardware.

- **Mix** (0-100%, default 50%; Knob 1): dry/wet balance.
- **Tone** (100 Hz-14 kHz, default about 9 kHz; Knob 2): low-pass filter on the repeats.
- **Regen** (0-100%, default 50%; Knob 3): feedback.
- **Glide** (default 0; Knob 4): how slowly the pitch slides from one interval to the next. No glide below 5%.
- **Interval 1** (11 steps, default centre = no change; Knob 5): the first interval.
- **Interval 2** (11 steps, default centre; Knob 6): the second interval. In the middle the third stage is skipped.
- **Time** (100 ms-2 s, default 500 ms; menu only): delay time. Tap tempo sets it.
- **Mod Rate** (default 0; menu only): speed of the tape wow and flutter.
- **Mod Depth** (0-100%, default 0%; menu only): depth of the wow and flutter. 0 is off.
- **Base Stage, Int 1 Stage, Int 2 Stage** (1 / 3/4 / 1/2, default 1; menu only): how long the sequencer stays at each speed, as a part of the delay time.
- **Scale** (Octaves / Dissonant / Chromatic, default Octaves; menu only): what the interval knobs select. Octaves gives octaves, fifths and fourths, Dissonant gives sevenths, sixths and seconds, Chromatic gives semitones from -5 to +5.
- **Reverse** (on/off, default off; menu only): plays the repeats backwards.
- **Tap Sequencer** (on/off, default off; menu only): every tap steps the sequencer to its next stage instead of setting the tempo.
- **Footswitch 2**: tap three times to set the delay time (it uses the interval between the last two taps; a series ends after a 2 second pause); keep tapping within the series to adjust it. Hold for 300 ms and the feedback goes above 1 so the repeats build up (self-oscillation) until you let go. LED 2 blinks once per delay time and is bright while self-oscillating.
- **MIDI notes**: play a sawtooth synth (12 voices) that is mixed into the input through a 1300 Hz low-pass.

With one input both delay lines get the same signal (the wow differs left and right).

### Pluto

`funbox-pluto` is a wacky dual (or stereo) looper for endless experimentation. Each of the two loopers records up to a minute and plays it at a variable speed (smooth or stepped, forwards or backwards, up to 3x). They share one live effect that the two Mod controls shape: random speed variations, a low/high pass filter, or a stereo reverb. Loops are recorded at normal speed, so turning Speed changes their pitch and tempo. Adapted from the Pluto pedal of the FunBox project (GuitarML, Keith Bloemer, MIT); the looper is a modified version of the DaisySP Looper. It needs more testing on hardware.

- **Level A** (default 0 dB at 50%; Knob 1): volume of looper A. The middle is unity.
- **Mod A** (default 0; Knob 2): looper A's effect control, depending on Effect: Stability (speed variations), Filter (low-pass to the left, high-pass to the right) or Reverb (decay time).
- **Level B** (default unity; Knob 3): volume of looper B.
- **Speed A** (default 1x at noon; Knob 4): speed and direction of looper A, from -2x (reverse) to +2x. In Stepped mode it has fixed steps from -2x to 3x.
- **Mod B** (default 0; Knob 5): looper B's effect control (for Reverb: the damping).
- **Speed B** (default 1x at noon; Knob 6): speed and direction of looper B.
- **Speed Mode** (Smooth / Stepped, default Smooth; menu or CC 21): Smooth glides like a tape reel, Stepped jumps between fixed speeds.
- **Effect** (Stability / Filter / Reverb, default Stability; menu or CC 22): the shared effect. In Reverb, A and B are the left and right send.
- **Loop Mode** (Normal / One-time / Fripp, default Normal; menu or CC 23): Normal overdubs for as long as it records, One-time records one pass and stops, Fripp (frippertronics) records endlessly with every pass decaying, like a tape delay.
- **Footswitch** (A + B / A / B, default A + B; menu only): which looper or loopers the secondary footswitch controls.
- **Footswitch 2**: tap to start recording, tap again to stop and loop (the next tap overdubs). A second tap within a second pauses or resumes playback. Hold for a second to clear. The LED pulses while recording, blinks while paused and is lit while a loop plays.

With one input (mono) both loopers record it and both loops play on both channels; with two inputs (stereo) looper A is the left channel and looper B the right.

### Cassini

`funbox-cassini` is a chromatic sampler to play with a MIDI keyboard. Hold the footswitch to sample the input (up to 20 seconds, into one of three banks); every key then plays the sample at its own pitch (middle C is the unchanged pitch) through an envelope, a low/high pass filter and a reverb. A looper can record and loop what you play, at a variable speed (also backwards). Adapted from the Cassini pedal of the FunBox project (GuitarML, Keith Bloemer, MIT); the looper is shared with Pluto and the reverb is DaisySP's ReverbSc. It needs more testing on hardware.

- **Trim Start** (0-100%, default 0%; Knob 1): where the sample starts, as a part of its length.
- **Trim End** (0-100%, default 100%; Knob 2): where the sample ends. Set before the start, the sample plays backwards.
- **Level** (default centre; Knob 3): volume of the keys. The middle is 1.25x.
- **Loop Speed** (default 1x at noon; Knob 4): speed and direction of the looper, from -2x (reverse) to +2x.
- **Filter** (default almost open; Knob 5): low-pass to the left of the middle, high-pass to the right.
- **Reverb** (0-100%, default 30%; Knob 6): decay time of the reverb on the keys.
- **Speed Mode** (Smooth / Stepped, default Smooth; menu only): Stepped gives fixed looper speeds from -2x to 3x.
- **Sample Mode** (One-shot / Looped, default Looped; menu only): One-shot plays the sample once per key, Looped repeats it while the key is down.
- **Bank** (1 / 2 / 3, default 1; menu only): which of the three samples is recorded and played.
- **Footswitch** (Sample / Looper, default Sample; menu only): what the secondary footswitch does. Sample: hold to record the sample of the current bank. Looper: tap to start recording the loop, tap again to loop, tap twice within a second to pause, hold for a second to clear.
- **MIDI**: notes play the sample (12 voices); CCs 14-19 set the knobs.
- **LED**: shows the recording of the sample or the state of the looper (pulsing while recording, blinking while paused, lit while a loop plays).

### Mars

`funbox-mars` is an amp sim and delay: three neural amp/pedal models (small neural networks running in real time), three cabinet impulse responses, a low/high pass filter, and a delay with dotted eighth and triplet modes. Mono: only the left input is used and the signal goes to both outputs. The signal chain is gain, neural model, filter, delay, mix with the amp, cabinet. Adapted from the Mars pedal of the FunBox project (GuitarML, Keith Bloemer, MIT). It needs more testing on hardware.

- **Gain** (0.1-2.5x, default 1x; Knob 1): input gain of the neural model, how hard it is driven.
- **Mix** (0-100%, default 30%; Knob 2): balance between the amp and the delay.
- **Level** (default centre; Knob 3): overall volume.
- **Filter** (default open; Knob 4): low-pass to the left, high-pass to the right, on a cube taper so it is open from about 80% of the knob.
- **Delay Time** (0 = off, then 50 ms-1 s over the first three quarters of the knob and up to 2 s beyond, default about 0.4 s; Knob 5): delay time. At 0 the delay stops taking input.
- **Delay Feedback** (0-100%, default 30%; Knob 6): delay feedback.
- **Amp Model** (Matchless / Klon / Mesa IIC, default Matchless; menu or CC 21): the neural model (Matchless SC30, Klon, Mesa IIC).
- **Cabinet** (Proteus / US Deluxe / Vox Bright, default Proteus; menu or CC 22): the cabinet impulse response.
- **Delay Mode** (Normal / Dotted 8th / Triplet, default Normal; menu or CC 23): the dotted and triplet modes add a second repeat at 3/4 or 2/3 of the delay time after the first.
- **Amp** (on/off, default on; menu only): switches the neural model on or off.
- **Cabinet On** (on/off, default on; menu only): switches the cabinet on or off.

### Mercury

`funbox-mercury` is an amp sim built on Neural Amp Modeler (NAM) models: nine "pico" models (a NAM Nano with two channels in the first layer array instead of four, small enough to run in real time on the Daisy) and a four band EQ. It has no cabinet, so use it with a separate cab or IR effect or a real amp. Mono: only the left input is used and the signal goes to both outputs. Models, by gain: low gain Matchless SC30, Twin Reverb, Dumble Low Gain; medium gain Dumble Higher Gain, Ethos Tube Pedal, JCM800 Medium Gain; high gain Mesa IIB, Splawn, PRS Archon. Adapted from the Mercury pedal of the FunBox project (GuitarML, Keith Bloemer, MIT); the network code derives from RTNeural-NAM by Jatin Chowdhury (BSD 3-Clause). It needs more testing on hardware. For official NAM A2 Nano models use the NAM A2 effect instead.

- **Gain** (0.1-2.5x, default 1x; Knob 1): input gain of the model, how hard it is driven.
- **Level** (default centre; Knob 2): overall volume.
- **Presence** (-10 to +10 dB, default 0; Knob 3): high shelf area around 8 kHz.
- **Bass** (-10 to +10 dB, default 0; Knob 4): around 180 Hz.
- **Mid** (-10 to +10 dB, default 0; Knob 5): around 1.2 kHz.
- **Treble** (-10 to +10 dB, default 0; Knob 6): around 4 kHz.
- **Amp Model** (nine models: Matchless SC30, Twin Reverb, Dumble Low, Dumble High, Ethos Pedal, JCM800, Mesa IIB, Splawn, PRS Archon, default Matchless SC30; menu or CC 21): the neural model.
- **EQ** (on/off, default on; menu only): switches the four EQ bands on or off.
