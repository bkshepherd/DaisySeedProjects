# Multi Phaser

A phaser with up to four phasers in one effect, each with its own settings. A phaser sends the signal through a chain of allpass filters, which change the phase of the sound and not its level, and adds the result to the original: where the two cancel, the spectrum has a **notch**, and a low-frequency oscillator (LFO) sweeps the notches up and down. Every two stages of the chain make one notch. The Multi Phaser lets you run several of them at once, each at its own speed, depth, centre and with its own number of stages, so that the notches sweep against each other in a way that a single phaser cannot do.

## The controls

The first three are for the whole effect, the rest are repeated for each of the four phasers (P1 to P4).

| Control | Range | Default | Knob |
|---|---|---|---|
| **Mix** | 0 to 100% | 100% | 1 |
| **Routing** | Series, Parallel | Series | (menu only) |
| **Output** | Mono, Stereo | Mono | (menu only) |
| **P1 Rate** | 0.05 to 10 Hz | 0.5 Hz | 2 |
| **P1 Depth** | 0 to 100% | 80% | 3 |
| **P1 Feedback** | -85% to +85% | +30% | 4 |
| **P2 Rate** | 0.05 to 10 Hz | 0.3 Hz | 5 |
| **P2 Depth** | 0 to 100% | 70% | 6 |
| **P1..P4 On** | Off, On | P1 and P2 On | (menu only) |
| **P1..P4 Stages** | 2, 4, 6, 8, 10, 12 | P1: 4, P2: 6, P3: 8, P4: 2 | (menu only) |
| **P1..P4 Center** | 100 Hz to 4 kHz | 630 Hz, 900 Hz, 400 Hz, 1.3 kHz | (menu only) |
| **P1..P4 Wave** | Sine, Triangle, Random | Sine, Triangle, Sine, Random | (menu only) |
| **P1..P4 Phase** | 0 to 360 degrees | 0, 90, 180, 270 | (menu only) |
| **P2..P4 Rate, Depth, Feedback** | as P1 | | (menu only) |

Phasers 3 and 4 are off by default and are set up so that they sound different from the first two when you switch them on.

**Mix** is how much of the phased signal is heard against the dry one. At 100% the notches are deepest: every phaser adds the dry signal and the phase-shifted one in equal parts, as the classic units do. Lower the Mix for a shallower sweep.

**Routing**: in **Series** each phaser phases the output of the one before it, so the notches of all of them are in the sound at once and the sweep gets deeper and more complex with every phaser. In **Parallel** the phasers all process the dry signal and their outputs are averaged: the notches of all of them are still there, but the sound is lighter and each phaser stays separate. Switching the routing while you play fades the effect out and in again (about 100 ms) so that it does not click.

**Output**: in **Mono** the signal is summed to mono, as in the other phaser, and the same signal goes to both outputs. In **Stereo** the right channel is the same phasers a quarter of a cycle later, a wide moving sweep; the effect then needs about twice the processing.

**On** switches a phaser in or out of the effect, with a short fade so that it does not click. **Rate** is the speed of its LFO. **Depth** is how far the notches sweep from the Center, up to two and a half octaves each way. **Center** is the frequency the sweep is centred on (where the notch rests with Depth at 0). **Stages** is the number of allpass stages: 2 gives one notch, 12 gives six, and more stages sound deeper and more "liquid". Changing it while playing fades the phaser out and in again (about 100 ms).

**Feedback** sends the end of the chain back to its input. Positive feedback makes the notches narrow and the peaks between them resonant, a whistling, "jet" sound; negative feedback gives a hollower, more nasal one. The level is corrected so that more feedback does not make the effect louder (at 85% the peaks are about 8 dB above the level without feedback).

**Wave**: Sine and Triangle are the usual oscillators. **Random** picks a new position for every cycle and glides to it, so the sweep wanders. **Phase** is where in its cycle the LFO is, against the other phasers: with the same Rate two phasers stay that distance apart, with different rates it is only where they start.

## Level

Every phaser takes some level off (the sum of the signal and its phase-shifted copy is about 3 dB quieter for noise) and gives it back at the peaks between the notches. Each phaser makes this up with a gain of 1.7 (+4.6 dB), so that the default settings keep a guitar-like signal at the level of the input, about +2 dB on average, and one phaser or two both stay inside the project's rule of +0 to +3 dB and never attenuating. More phasers in series make it up each: four are about 5 dB louder. In Parallel they average, so the level does not grow. Strong positive feedback takes a little level off (-2.5 dB at 85%), negative feedback adds some (+2.5 dB). With a full-scale signal and several phasers in series with high feedback the output can exceed full scale; turn the level down before the effect, or turn the Mix or Feedback down.

## Ideas

- **Classic phaser:** one phaser (turn P2 Off), 4 stages, Rate 0.5 Hz, Depth 80%, Feedback +30%.
- **Deep, slow and rich:** P1 and P2 in Series, different rates (0.3 Hz and 0.45 Hz) and centres (500 Hz and 1.2 kHz), 4 and 6 stages. This is the default.
- **Jet sweep:** one phaser with 12 stages, Feedback +70%, Rate 0.15 Hz, Depth 100%, Center 800 Hz.
- **Shimmer:** four phasers in Parallel with Random waves at different rates.
- **Stereo swirl:** Output Stereo with two phasers in Series.

The Multi Phaser has no MIDI CC assignments yet; the knobs control the Mix and the Rate, Depth and Feedback of P1 and the Rate and Depth of P2 (the rest is in the menu).

## How it is built

For anyone changing it: every phaser is a chain of up to 12 first-order allpass filters H(z) = (a + z^-1) / (1 + a z^-1) with a = (tan(pi f / fs) - 1) / (tan(pi f / fs) + 1), which gives a quarter turn of phase at the corner frequency f, all stages with the same coefficient, with the feedback from the output of the chain (one sample late) to its input. The output of a phaser is half the sum of its input and the chain, times the make-up and feedback-correction gain. The LFOs and the coefficients are computed every 16 samples (the coefficient is interpolated in between), so the cost per sample is the allpass stages only (two multiply-adds each): about 1.5% of the CPU on the pedal for four phasers of 12 stages is the estimate from the arithmetic; the PC timing of the whole effect is 0.06 s for 20 s of audio with the default settings (2 phasers) and 0.2 s for all four at 12 stages. A phaser that is off is not processed; switching is done with a fade, and the number of stages and the routing are changed while the phaser (or the effect) is faded out. It takes about 1 KB of heap and no SDRAM, so it can be used in a chain, and more than once.
