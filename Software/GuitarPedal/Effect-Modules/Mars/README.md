# Mars (amp sim and delay)

Mars is an amp sim and delay: three neural amp/pedal models (small GRU networks running in real time), three impulse responses (cabinets), a low/high pass filter, and a delay with dotted eighth and
triplet modes. It is mono: only the left input is used, and the signal is copied to both outputs.

## Origin and licence

Adapted from the **Mars** pedal of the **FunBox** project by GuitarML: <https://github.com/GuitarML/Funbox> (`software/Mars/`), Copyright (c) 2024 Keith Bloemer, **MIT License**
([`LICENSE-FunBox-MIT.txt`](LICENSE-FunBox-MIT.txt)); like the rest of this repository it is MIT, so there is no licence change to opt into. The original ran its networks on
[RTNeural](https://github.com/jatinchowdhury18/RTNeural) (BSD-3-Clause) and Eigen; here the network is a few lines of plain C++ (`Mars/mars_dsp.h`), which gives the same output as RTNeural (checked
against reference data generated with RTNeural, see below). The filter is DaisySP's `Tone`/`ATone`/`Balance` (LGPL), the cabinet convolution is this repository's `ImpulseResponse` (CMSIS FIR).

## Files

| File | |
|---|---|
| `../mars_module.{h,cpp}` | The effect module: parameters, the signal chain, the delay |
| `mars_dsp.h` | The GRU network and the two-tap delay line, free of hardware dependencies |
| `mars_models.h` | The weights of the three models (models 2 to 4 of the original's collection) |
| `mars_ir_data.h` | The three impulse responses (400 samples each) |

## Controls

| Control | Parameter | What it does (original control) |
|---|---|---|
| Knob 1 | Gain | The input gain of the neural model, 0.1x to 2.5x (1x at the default) |
| Knob 2 | Mix | Between the amp and the delay |
| Knob 3 | Level | The overall volume |
| Knob 4 | Filter | Low pass to the left, high pass to the right (on a cube taper, as the original's knob: open from 0.8 on) |
| Knob 5 | Delay Time | 0 is off (the delay stops taking input), then 50 ms to 1 s over the first three quarters and up to 2 s beyond |
| Knob 6 | Delay Feedback | |
| Menu / CC 21 | Amp Model | Matchless SC30, Klon, Mesa IIC |
| Menu / CC 22 | Cabinet | Proteus, US Deluxe, Vox Bright |
| Menu / CC 23 | Delay Mode | Normal, Dotted 8th or Triplet (a second repeat at 3/4 or 2/3 of the delay time after the first) |
| Menu | Amp, Cabinet On | Switch the neural model and the cabinet on and off (the original's dip switches) |

Signal chain: gain, neural model (with a skip connection), filter, delay, mix with the amp, cabinet.

## Changes from the original

* The hardware code is gone, as is the original's own preset system (the pedal has its own presets); the three switches and two dip switches are parameters.
* The network is a hand-written GRU (9 units) and dense layer instead of RTNeural, with a rational approximation for `tanh` and the logistic function (largest error 1e-4 for `tanh`), which costs about a fifth of the exact functions. `tests/host/test_mars.cpp` compares it to the output of the original network (`tests/host/mars/gru_reference.bin`, made by `gru_reference_gen.cpp` with RTNeural): the exact version agrees to 4e-6, the fast one to 2e-4.
* The cabinet is convolved a block at a time with CMSIS' FIR instead of Eigen's dot product, sample by sample.
* The delay buffer is long enough for the second tap at the longest delay (the original wrapped around and read newer audio there).
* An output gain for each model and each cabinet brings every combination to about +3 dB at the default settings (see `tests/host/run_level_tests.sh`).
