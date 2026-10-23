# Booster

A booster makes your guitar louder *before* it reaches an amp or a distortion, so that it pushes them harder. It is not an effect you hear on its own so much as a way of changing how the next thing in the chain reacts: put it in front of an overdriven amp and the amp distorts more; put it in front of a clean amp and it just gets louder.

The four **types** differ in *which part of the sound* they boost, and that changes what the amp does with it:

| Type | What it boosts | What it sounds like | Use it to |
|---|---|---|---|
| **Clean** | everything, equally | the same guitar, louder, with almost no coloring | push an amp or a pedal without changing its character |
| **Treble** | the highs; the bass is cut | thin, bright and cutting, with a little grit when pushed (the classic Rangemaster-style treble booster) | make a smooth amp bite and sing in the upper mids; a lead tone that cuts through |
| **Mid** | a bump around one frequency | pushed-forward, "honky" mids, the lows and highs left alone | make a guitar jump out in a mix, or tighten a muddy distortion |
| **Warm** | the mids, with more lows and fewer highs | thick, rounded and slightly saturated (in the spirit of the Echoplex preamp boosters) | fatten a thin single-coil, or add body and a little compression |

## The controls

| Control | Range | Default | Knob |
|---|---|---|---|
| **Gain** | 0 to 24 dB | 12 dB | 1 |
| **Tone** | 0 to 100% | 50% | 2 |
| **Level** | -24 to +6 dB | 0 dB | 3 |
| **Type** | Clean, Treble, Mid, Warm | Clean | 4 |

**Gain** is how much the type boosts the part of the sound it is built for: everything for Clean, the highs for Treble, the centre frequency for Mid, the mids for Warm. At 0 dB there is no boost: Clean and Mid then do nothing at all, while Treble still cuts the bass and Warm still lifts the lows and rolls off the highs. At 24 dB it is a very hot boost that will saturate when you play hard.

**Tone** does something different for each type:

| Type | What Tone does | 0% | 50% | 100% |
|---|---|---|---|---|
| Clean | tilts the balance between lows and highs (flat at 50%) | darker: highs -4 dB, lows +4 dB | flat | brighter: highs +4 dB, lows -4 dB |
| Treble | the corner below which the bass is cut | 200 Hz (thick, lets the lows through) | 800 Hz | 3.2 kHz (very thin, only the highs) |
| Mid | the frequency of the bump | 400 Hz | 1.1 kHz | 3.2 kHz |
| Warm | the high cut (how dark it is) | 2 kHz (dark) | 4.9 kHz | 12 kHz (open) |

**Level** is the output level after the boost, in case the boosted signal is too loud for what comes next. It is 0 dB by default.

## Loudness differs between the types

Gain is the boost *where the type works*, not the loudness of the whole guitar. A treble booster cuts the bass, which is most of the energy of a guitar, so at the same Gain it is quieter overall than Clean even though its highs are boosted just as much. Measured on a low note at the default Gain of 12 dB: Clean is about 12 dB louder, Warm about 13 dB, Mid about 4 dB and Treble about 3 dB. When you switch the type, expect to adjust Gain or Level by ear.

## Saturation

- **Clean** and **Mid** stay clean: they only limit softly near full scale, with no even harmonics. At a normal playing level a Clean boost of 12 dB is free of audible distortion.
- **Treble** and **Warm** clip asymmetrically when pushed, which adds even harmonics (the "germanium" and "tube" sound): subtle at the default, gritty with a hot guitar or a high Gain.

## Ideas

- **Boost into a distortion:** Booster (Clean, 8 to 12 dB) → Screamer or Chainsaw, in an effect chain. With the chain's **Switches** parameter at 1+2 the right footswitch switches the booster and the left one the distortion (see the user manual).
- **Lead tone:** Treble with Tone around 50 to 70% in front of an overdrive: the overdrive hears less bass, so it stays tight, and more highs, so it cuts.
- **Thickening:** Warm in front of a clean amp, Gain 6 to 10 dB, Tone around 50%.
- **Pushing a Preamp:** Mid at 1 to 1.5 kHz with Gain 6 to 12 dB in front of the Preamp's Crunch voicing.

The Booster has no MIDI CC assignments yet.
