# Mercury (amp sim with NAM pico models)

Mercury is an amp sim built on [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) (NAM) models: nine "pico" models (a NAM Nano with two channels in the first layer array instead
of four, small enough to run in real time on the Daisy) and a four band EQ (bass, mid, treble, presence). It has no cabinet: use it with a separate IR/cab sim or an amp. It is mono: only the left
input is used and the signal is copied to both outputs. (The NAM A2 module is the repository's other amp sim, for official NAM A2 Nano models.)

Models: low gain: Matchless SC30, Twin Reverb, Dumble Low Gain (from a kit build); medium gain: Dumble Higher Gain, Ethos Tube Pedal, JCM800 Medium Gain; high gain: Mesa IIB, Splawn, PRS Archon.

## Origin and licence

Adapted from the **Mercury** pedal of the **FunBox** project by GuitarML: <https://github.com/GuitarML/Funbox> (`software/Mercury/`), Copyright (c) 2024 Keith Bloemer, **MIT License**
([`LICENSE-FunBox-MIT.txt`](LICENSE-FunBox-MIT.txt)). The network is a plain C++ rewrite of the WaveNet of [RTNeural-NAM](https://github.com/jatinchowdhury18/RTNeural-NAM) by jatinchowdhury18, which
the original used on [RTNeural](https://github.com/jatinchowdhury18/RTNeural); that code is **BSD 3-Clause** licensed and its licence text is in
[`LICENSE-RTNeural-NAM-BSD3.txt`](LICENSE-RTNeural-NAM-BSD3.txt) (the original Mercury directory carries it too). The weights are the models of the original's `model_data_nam.h`. The EQ is the
peaking biquad of the Q library, already used by the NAM modules.

## Files

| File | |
|---|---|
| `../mercury_module.{h,cpp}` | The effect module: parameters, gain, EQ, model selection |
| `wavenet_pico.h` | The WaveNet (two layer arrays of two channels, 20 layers), one sample at a time without allocation |
| `mercury_models.h` | The weights of the nine models, and their level adjustments |

## Controls

| Control | Parameter | What it does (original control) |
|---|---|---|
| Knob 1 | Gain | The input gain of the model, 0.1x to 2.5x (1x at the default) |
| Knob 2 | Level | Overall volume |
| Knob 3 | Presence | -10 to +10 dB at 8 kHz |
| Knob 4 | Bass | -10 to +10 dB at 180 Hz |
| Knob 5 | Mid | -10 to +10 dB at 1.2 kHz |
| Knob 6 | Treble | -10 to +10 dB at 4 kHz |
| Menu / CC 21 | Amp Model | One of the nine models (the original's switches: three groups of three) |
| Menu | EQ | Switches the four bands off |

## Changes from the original

* The hardware code and the original's own preset and expression handling are gone.
* The network is a plain C++ implementation instead of RTNeural-NAM's templates on Eigen: the same arithmetic (including the tanh approximation of the original, `math_approx::tanh<3>`), checked against the output of the original network for all nine models (`tests/host/test_mercury.cpp`, reference data `tests/host/mercury/wavenet_reference.bin` made by `wavenet_reference_gen.cpp`): the difference is at most 4e-6.
* The history of the network is exactly as long as it needs to be (33 KB, in the fast on-chip RAM); the settling of the network after a model change (the original ran 16384 samples of silence) takes the receptive field of 4093 samples plus a few, which leaves exactly the same state (checked), so a model change takes about 25 ms. The output is muted meanwhile, and a model change runs on the main loop, not in the audio callback.
* An output gain for each model brings every model to about +3 dB at the default settings (see `tests/host/run_level_tests.sh`); the original's level adjustments are kept.
