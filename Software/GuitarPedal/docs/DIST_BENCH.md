# Dist Bench

Dist Bench chains up to four distortion effects, one after another, inside a single effect. Use it to compare distortion types, to stack them (a boost into an amp, a fuzz into tape saturation) and to find out how hard each one should be driven.

Each of the four **stages** is either **Off** or one of eleven effects:

| Type | What it is |
|---|---|
| Preamp | an amp-like preamp with Clean, Crunch and Lead voicings |
| Screamer | a Tube Screamer-style overdrive: mid hump, tight lows |
| Muff Fuzz | a Big Muff-style fuzz: thick, sustaining, scooped mids |
| Wavefolder | a wavefolder: metallic, synth-like harmonics |
| Exciter | adds 2nd and 3rd harmonics to the highs, for presence without obvious distortion |
| Bass Dist | distorts only the part above a crossover, so the lows stay clean |
| Octa Fuzz | an Octavia-style fuzz with an octave up |
| Tape Sat | tape saturation: soft compression, warm lows |
| Overdrive | the pedal's original overdrive |
| Distortion | the pedal's original distortion, with six clipping types (without its oversampling, see below) |
| Chainsaw | an HM-2-style heavy metal distortion: hard clipping into big resonant boosts at 87 Hz and in the upper mids, the "Swedish chainsaw" |

All eleven also exist as effects of their own. Dist Bench gives every type the **same four controls** (Drive, Tone, Char, Level), so you can swap the type of a stage and keep your settings.

The signal runs through the stages in order, 1 → 2 → 3 → 4. A stage set to Off passes the signal through unchanged.

## The parameters

The menu has 22 parameters: five for each stage, and two for the whole effect.

| Parameter | Range | Default | Knob |
|---|---|---|---|
| `1 Type` … `4 Type` | Off and the eleven types above | stage 1: Screamer, stages 2–4: Off | menu |
| `1 Drive` … `4 Drive` | 0–100% | 50% | knobs 1–4 (stage 1 on knob 1, and so on) |
| `1 Tone` … `4 Tone` | 0–100% | 50% | menu |
| `1 Char` … `4 Char` | 0–100% | 50% | menu |
| `1 Level` … `4 Level` | -24 to +6 dB | 0 dB | menu |
| `Out Level` | -24 to +6 dB | 0 dB | knob 5 |
| `Mix` | 0–100% | 100% | knob 6 |

There are no MIDI CCs for Dist Bench yet.

### 50% means "the effect's own default"

Drive, Tone and Char are percentages, and **50% always means the effect's default setting**: a stage at 50% sounds exactly like the effect on its own with its default settings. Below 50% the control moves towards the effect's minimum, and above 50% towards its maximum. 0% is the minimum and 100% the maximum.

So the two halves of the knob can cover different amounts. On the Screamer, for example, Drive 0–50% covers the effect's Drive 0 to 6.6, and 50–100% covers 6.6 to 10.

The menu shows the percentage, not the effect's own value (it shows "50%", not "6.6" or "Asymmetric"). The tables below give the real values.

### Drive

How hard the stage distorts.

| Type | Drive controls | 0% | 50% | 100% |
|---|---|---|---|---|
| Preamp | Gain | 0 | 4.0 | 10 |
| Screamer | Drive | 0 | 6.6 | 10 |
| Muff Fuzz | Sustain | 0 | 6.0 | 10 |
| Wavefolder | Fold | 0.25x | 30x | 130x |
| Exciter | Drive | 0 dB | 28 dB | 40 dB |
| Bass Dist | Drive | 0 | 5.0 | 10 |
| Octa Fuzz | Fuzz | 0 | 6.0 | 10 |
| Tape Sat | Drive | 0 dB | 28.8 dB | 36 dB |
| Overdrive | Drive | minimum | default | maximum |
| Distortion | Gain | 0 dB | 20.4 dB | 26 dB |
| Chainsaw | Dist | 0 | 8.0 | 10 |

The top of Drive gets much more gain on several types (Preamp Clean, Screamer, Wavefolder, Bass Dist), so that 100% saturates fully with a hot guitar signal.

### Tone

The brightness of the stage, usually a high cut.

| Type | Tone controls | 0% | 50% | 100% |
|---|---|---|---|---|
| Preamp | Treble (high shelf at 3.2 kHz) | -12 dB | 0 dB | +12 dB |
| Screamer | Tone (high cut) | 500 Hz | 2.6 kHz | 8 kHz |
| Muff Fuzz | Tone (bass ↔ treble blend) | all bass | 50% | all treble |
| Wavefolder | Tone (high cut) | 1 kHz | 7.0 kHz | 16 kHz |
| Exciter | Focus (where the harmonics start) | 500 Hz | 1.1 kHz | 8 kHz |
| Bass Dist | Tone (high cut of the distorted part) | 1.5 kHz | 5.2 kHz | 12 kHz |
| Octa Fuzz | Tone (high cut) | 800 Hz | 3.2 kHz | 8 kHz |
| Tape Sat | Tone (high cut) | 2 kHz | 8.6 kHz | 16 kHz |
| Overdrive | – (no effect) | | | |
| Distortion | Tone (bass ↔ treble tilt) | all bass | 50% | all treble |
| Chainsaw | High (two peaks at 960 Hz and 1.28 kHz) | -15 dB | +10 dB | +15 dB |

On the Exciter, Focus is the frequency above which the harmonics are made. A **lower** Focus adds them to more of the signal, down into the mids; a higher Focus keeps them to the top end.

### Char

The effect's most characteristic second control. For the effects that have modes, Char switches between them.

| Type | Char controls | 0% | 50% | 100% |
|---|---|---|---|---|
| Preamp | Voicing | Clean (below 25%) | Crunch (25–74%) | Lead (75% and up) |
| Screamer | Mode | Classic (up to 62%) | Classic | Asymmetric (63–87%), Hard (88% and up) |
| Muff Fuzz | Mode | Classic (up to 62%) | Classic | Fat (63–87%), Bright (88% and up) |
| Wavefolder | Symmetry | -100% | 0% (symmetric) | +100% |
| Exciter | 2nd against 3rd harmonic | 2nd 100%, 3rd 0% | 2nd 50%, 3rd 30% | 2nd 0%, 3rd 60% |
| Bass Dist | Clip | Soft (up to 62%) | Soft | Hard (63–87%), Fuzz (88% and up) |
| Octa Fuzz | Octave (how much octave-up) | 0% | 50% | 100% |
| Tape Sat | Warmth (how much more the lows saturate) | 0 dB | 4.8 dB | 12 dB |
| Overdrive | – (no effect) | | | |
| Distortion | Type | Hard Clip (up to 54%) | Hard Clip | Soft Clip (from 55%), Fuzz (65%), Tube (75%), Multi Stage (85%), Diode Clip (95%) |
| Chainsaw | Low (a peak at 87 Hz) | -15 dB | +10 dB | +15 dB |

The default mode is at 50%. When the default is the first mode (Screamer, Muff Fuzz, Bass Dist, Distortion), Char below 50% stays on it. The thresholds above 50% are rounded; the exact switching point can be one percent off. More about the modes:

- **Screamer:** Classic is a symmetric soft clip. Asymmetric adds even harmonics, for a warmer, more open sound. Hard is a harder, more compressed clip.
- **Muff Fuzz:** the three modes differ in the centre of the tone control: Classic 1.5 kHz, Fat 800 Hz, Bright 3 kHz.
- **Bass Dist:** Soft is smooth, Hard is edgier, and Fuzz is an asymmetric, fuzzier clip.
- **Exciter:** the 2nd harmonic sounds warm, the 3rd harder and more forward.
- **Distortion:** Hard Clip and Diode Clip cut the peaks off sharply, Soft Clip and Tube round them, Fuzz adds a buzzy, compressed edge, and Multi Stage is three clipping stages in a row, the most saturated.
- **Chainsaw:** Tone and Char are its Color Mix controls, High and Low. The classic chainsaw is everything at 100%; the Chainsaw's own default (50% in the bench) is Dist 8 with both at +10 dB. High boosts two overlapping peaks, so at +15 dB the upper mids rise by about 23 dB.
- **Wavefolder:** moving Symmetry away from 0% makes the folding uneven, which adds even harmonics.

### Level (per stage)

The stage's output level, from -24 to +6 dB, 0 dB by default. It is also the way to **drive the next stage harder or softer**. Raise Level on stage 1 and stage 2 distorts more, like turning up the output of a boost pedal in front of an amp.

### Out Level

The level of the whole effect, from -24 to +6 dB, 0 dB by default. Use it to match Dist Bench to your clean sound after the stages have been set.

### Mix

The blend of the clean signal and the distorted signal, 100% (fully distorted) by default. Lower settings mix in the clean signal, which keeps the attack and the lows clear under a heavy fuzz. The clean part is at its original level and the distorted part at the bench's level (below), so the overall level rises as Mix goes up.

## Levels

Each effect on its own is set to make the signal **6 dB louder** at default settings, so all the distortions are about equally loud when you switch between them. Three effects that are already fully saturated at a normal playing level, the Preamp's Lead voicing, the Muff Fuzz and the Chainsaw, are instead matched to the Preamp's Crunch voicing when played hard. Dist Bench keeps that rule for the chain as a whole:

- **Each active stage is lowered by 6 dB.** A stage at its defaults therefore comes out at the level that went in.
- **Dist Bench adds 6 dB at the end.**

The result: **one stage at its defaults is exactly as loud as that effect on its own**, and four stages are not 24 dB louder than one. With every stage Off, Dist Bench is a clean 6 dB boost.

Stages that saturate keep their output level more or less fixed whatever goes in. Stages that hardly distort (Exciter, Tape Sat at low Drive, Preamp Clean) pass level changes on. If a chain gets too loud or too quiet, adjust the stage Levels first and Out Level last.

## Changing the type of a stage

Changing `n Type` while playing **crossfades over a tenth of a second** from the old type to the new one, so there is no click. Each stage keeps a ready copy of every type, so switching doesn't interrupt the audio. An effect you return to resumes from the state it was left in; the crossfade hides that.

## Where to start

- **Compare types:** leave stages 2–4 Off, play, and step through `1 Type`. With everything at 50%, every type sounds as it does as an effect of its own, at the same loudness.
- **Boost into an amp:** stage 1 Screamer with Drive low (20–30%) and Level up (+3 to +6 dB), stage 2 Preamp with Char at 50% (Crunch). The Screamer tightens the lows and pushes the Preamp into saturation.
- **Fuzz with a clean low end:** stage 1 Muff Fuzz, Mix around 60–70%.
- **Warm the top of a chain:** put Tape Sat last, with Drive a little below 50% and Char (Warmth) up, to smooth out a harsh fuzz.
- **Bring a dark sound forward:** put the Exciter last, with a low Mix overall, or a low Drive on the Exciter stage.
- **Find a type's sweet spot:** sweep Drive from 0% upwards while playing. How early a type distorts depends a lot on how hot your guitar's signal is. At a normal playing level, Bass Dist, Tape Sat, the Preamp's Crunch voicing and the Screamer's Hard mode are still fairly clean at 50% and come alive above it. Muff Fuzz is saturated at almost any setting, and the Lead voicing from about 50%.

## How much processing it needs

Each active stage needs roughly as much processing as the effect on its own, about 4–5% of the processor. That is an estimate, not measured on the pedal. Four active stages come to about 20%; stages set to Off cost nothing. Turn on Show Debug in the global settings to see the actual figure.

## What Dist Bench leaves out

Only Drive, Tone and Char are mapped. Every other parameter of an effect stays at its default inside the bench. For example, the Preamp's Bass and Mid, the Exciter's 4th and 5th harmonic and its own Mix, the Tape Sat's Trafo mode and the Distortion's Intensity are fixed. For those, use the effect on its own.

The Distortion runs **without its 16x oversampling** in the bench, to keep four stages affordable. It is then cheaper, but at high Gain its upper harmonics alias more (a grainier top end) than the Distortion on its own with Oversamp on. Its level is the same within about 1 dB.
