# Spring Reverb

The reverb of old guitar amps: a signal sent down one or more metal springs and picked up at the other end. It is not a room or a hall, and it does not sound like one. It is thin and bright, with a characteristic "drip" or "boing": every echo is a short upward sweep, because the lows travel along a spring at the same speed but the highs slow down as they approach the spring's transition frequency (about 4 kHz) and arrive last.

This effect models that physically. It has 2 to 6 springs of different lengths and a mono input (the left and right inputs are summed, like the driver of a real tank). Real tanks have two or three springs sounding together; the model goes further, because on a pedal more springs is the cheap way to a denser tail that sounds less like an echo.

## The controls

| Control | Range | Default | Knob |
|---|---|---|---|
| **Dwell** | 0 to 10 | 5.0 | 1 |
| **Decay** | 0.5 to 6 s | 1.7 s | 2 |
| **Tone** | 1.5 to 12 kHz | 4.2 kHz | 3 |
| **Mix** | 0 to 100% | 30% | 4 |
| **Drip** | 0 to 100% | 100% | 5 |
| **Tank** | Short, Medium, Long | Medium | 6 |
| **Output** | Stereo, Mono | Mono | (menu only) |
| **Springs** | 2 to 6 | 4 | (menu only) |

**Dwell** is how hard the tank is driven, as on a Fender amp. It sets the level of the reverb and how much it splashes: at the low end the tank is quiet and clean, turned up it gets louder, and with a loud guitar and a high Dwell the springs are driven into saturation, which adds the splashy, crashing character of a spring reverb hit hard. (Dwell goes from 12 dB below to 24 dB above the level that goes into the tank.)

**Decay** is the time the echoes take to die away by 60 dB, measured at 1 kHz. Real tanks are around 1 to 3 seconds; the range goes to 6 s for ambient use. The highs die away faster than that.

**Tone** is a high cut on the return: low values are dark and soft, high values bright and "sizzly". The input of the tank already cuts everything above about 4 kHz, as a real spring does, so Tone mostly shapes what is below that.

**Mix** is how much reverb is **added** to the dry signal, as the reverb knob of an amp does: the dry sound is never turned down. At 100% the reverb adds about as much energy as the dry signal; at the default 30% it is about 8 to 10 dB below it. (An earlier version crossfaded the Mix, and to keep the level up it returned the reverb louder than the dry note at the default, which sounded like an echo effect.)

**Drip** is how dispersive the springs are, that is how much each echo sweeps. At 100% (the default) the springs have the dispersion of a real spring: the lows of an echo come back at once and the highs later, about 2 ms later at 2 kHz, 8 ms at 3 kHz, 20 ms at 3.6 kHz and more toward 4 kHz, so that each echo is a short upward sweep that spreads further each time it goes round the spring. At 0% there is no sweep and the echoes are plain repeats, like a short echo with no "boing" in it. Most of the sweep is there at about 75%.

**Tank** is the length of the springs, which sets the spacing of the echoes: **Short** about 24, 29, 33, 38, 21 and 27 ms, **Medium** about 34, 41, 47.5, 54, 29.5 and 38.5 ms, **Long** about 46, 55, 64, 73, 40 and 52 ms (one number for each spring, which are of different lengths so that their echoes do not line up). Short is tight and quick, Long is slower and more spread out. Changing it while playing gives a short glide of the tail.

**Springs** is how many springs run, from 2 to 6. Each one adds a train of echoes at its own spacing, so more springs make the tail denser and the echoes less regular: in a measurement of how strongly the envelope of a note repeats (1 is a clean delay), one spring scores about 0.8, two about 0.3, three 0.2 and four 0.15. The level of the return is the same for any number. Each spring costs about the same, and in a test on a PC 2 springs take 0.17 s per 20 s of audio, 4 springs 0.30 s and 6 springs 0.43 s (the version with two springs at the full sample rate took 0.43 s). Changing it while playing fades springs in (about 100 ms) and out (about 10 ms).

**Output** is **Mono** (the default): all the springs on both outputs. **Stereo** puts every other spring on each output (the first, third and fifth on the left, the second, fourth and sixth on the right), for a wider tank. It costs nothing: all the springs run in both. Switching is smooth (a crossfade of about 10 ms).

## If it sounds like a delay

A spring's echoes are regular, so a spring reverb can sound like a delay if the echoes stand out. In order of what matters most:

1. **Turn Mix down.** The echoes are only noticeable when the return is loud. About 8 to 10 dB below the dry sound (the default) or lower is a reverb; much louder is an echo effect.
2. **Use more springs.** One spring on its own repeats very regularly (about 0.8 in the repetition measure of a note, 1 being a clean delay), two about 0.3 and four about 0.15. The default is four.
3. **Keep Drip at 100%.** Without the sweep the echoes are plain repeats (0.5 against 0.3 for a pick).
4. **Shorten Decay.** The echoes only blur as they go round the spring again and again.
5. **Use the Short or Medium tank.** The Long tank has the longest gaps between echoes.

## Level

The default settings add the reverb to the dry signal (about +1.2 dB on average over the six open strings), so that turning the reverb on never makes you quieter. A tank is a comb of echoes, so the return is louder on some notes than on others, by several dB, and where it falls against the dry note it can take a little off that note (at most about 3 dB); that is the character of a spring. The tank also does not carry the lowest notes well (its input has no bass below about 120 Hz), as in a real spring.

## Ideas

- **Surf and rockabilly:** Mix 40 to 60%, Drip 100%, Decay 2 s, Tone high, Dwell around 6. Put a Booster in front of it (a Treble or Clean type) for more splash.
- **Subtle ambience:** Mix 15 to 25%, Decay 1 s, Tone 40%, Drip 70%.
- **Dub and effects:** Dwell at 10 with a hard-played guitar, Decay 4 s or more: the springs crash.
- **After a distortion:** in an effect chain, Spring Reverb last; with **Switches** set to 2+3 the left footswitch can switch the reverb (see the user manual).

The Spring Reverb has no MIDI CC assignments yet, and Output and Springs are only in the menu (the knobs are used by the first six parameters). With **Trails** on, its tail rings out when the effect is switched off, and switching it on again always starts from silence.

## How it is built

For anyone changing it: each spring is a feedback loop of a chain of stretched first-order allpass filters (about 91 sections, a few more or less in each spring) and a delay line (the transit of the wave along the spring and back), with a low-pass in the loop (each round trip loses some highs) and a gain that sets the decay time. The chain follows the main chirp of a helical spring below its transition frequency: its delay above the 4 ms it has at DC is 15.5 ms * (1 / sqrt(1 - (f / 4 kHz)^2) - 1), that of a low-pass ladder: nothing at the lows, 0.5 ms at 1 kHz, 2.4 ms at 2 kHz, 8 ms at 3 kHz, 20 ms at 3.6 kHz, a maximum at 4 kHz. This comes from stretched allpass sections with a *positive* coefficient and a stretch that puts the first peak of their delay at 4 kHz (6 at 48 kHz, or 2 at 16 kHz: the delay in seconds depends on stretch / sample rate only, so the same sections serve both); the model is that of Abel et al. 2006 and Valimaki, Parker and Abel 2010, with the sign of Parker 2011, and the fit is reproducible with `tools/fit_spring_dispersion.py`. A second family of weaker echoes (30 dB below) sweeps the other way in a real spring and is left out, as Parker allows. The input is low-passed (4th order, 4.2 kHz) because the stretched chain repeats and mirrors above its peak, and, since a spring carries nothing above 4 kHz, the springs run at **a third of the sample rate** (16 kHz): the input is low-passed before it is taken every third sample, and the sum of the springs is interpolated back with a 4th-order low-pass at 4.6 kHz. That, with the stretch of 2, makes a spring about a third of the cost. Drip scales the coefficients. The DSP pieces are in `Util/spring_dsp.h` and the effect in `Effect-Modules/spring_reverb_module.cpp`. It takes about 37 KB of heap (the six delay lines and chains are most of it) and no SDRAM, so it can be used in a chain and more than once. The springs that are switched off are not processed; one that is switched on starts from silence and fades in.

The first version of this effect had the sweep the wrong way round (the highs first, the lows up to 26 ms late: the weaker secondary family instead of the main one), a Drip that scaled the wrong thing, and a wet gain that made the reverb louder than the dry signal. The tests of the sweep direction, of Drip and of the level of the return guard against all three.
