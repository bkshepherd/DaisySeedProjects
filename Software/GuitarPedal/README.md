# Multi-Effect Guitar Pedal Software

This directory includes all the source code for my Multi-Effect Guitar Pedal that runs on various hardware platforms.

## User Manual

For assembly and day-to-day operation of the 125B version, see the [125B user manual](./docs/USER_MANUAL_125B.md).

## Setup

For general setup see [setup](./docs/SETUP.md) document

## Software Updates

### Software Update - October 2026

See ReleaseNote

### Software Update - September 2026

Added over a long time.

1. Added distortion, granular delay, drum machine, IR loader, tape delay, phaser, flanger, crusher, harmonic tremolo, and dual echo (aka "halo") effects.
2. Added NAM A2 support, optional IR loading, and a default set of amp models.
3. Added effect chains with per-effect bypass, expanded knob banks, and footswitch/MIDI mapping.
4. Added optional GPL-3.0 Dattorro plate reverb and basic Funbox hardware support.
5. Improved pitch shifting, delay controls and preset storage.
6. Fixed a memory leak when switching effects and improved memory use.
7. Added an "autosave" option, whereby the most recent pedal state will be saved across restarts (note: this overwrites Preset 0 for each effect a few seconds after every parameter change).

### Software Update - November 2024

1. New Effect Modules:
   - Compressor
   - Chromatic Tuner
   - Looper
   - Pitch Shifter (similar to digitech drop/ricochet)
2. Fixes to tap tempo across several effects
3. Quick-switch to tuner by press-and-hold bypass switch
4. Allow effects to utilize alternate footswitch (looper, pitch shifter)
5. Adjusts saving to require both footswitches to be held if the hw has 2 footswitches
6. Updated to C++20, updated dependencies, added code formatting with clang-format
7. Added a compile-time boolean flag if hardware only has 1 footswitch
8. Adjusted bypass logic to try and mitigate double activations

### Software Update - February 2024

Big update to the handling of Persistant Storage. Thank you @jaching!

Updates include:

1. Removed the hard limit the number of parameters stored per EffectModule.
2. Ability to have multiple stored Presets for each EffectModule
3. Reset All Presets button now located in the Preset Menu
4. Changing Midi to call custom callback when a midi cc cannot be found.
5. Added a NEW Multi tap delay Effect Module!
6. The software is now configured to use the Boot Loader by default to allow for more memory usage.

### Software Update - 11/11/2023

Updates include:

1. Refactored the code to move Display UI handling and Persistent Storage out of the main class file.
2. Added functionality to make it easy for an Effect Module to provide custom UI for the Display while the Effect is Active.

### Software Update - 10/9/2023

Updates Include:

1. Added support for multiple effects. Included are a simple tremolo, chorus, overdrive, and stereo auto-panning effects.
2. Created a Hardware Abstraction Layer allowing this software to run on different Daisy seed based hardware targets including my custom hardware as well as the Pedal PCB Terrarium.
3. Updated the menu system to support multiple effects each with their own settings and global hardware settings. Parameters can be updated directly through the menu UI or using the pots on the device.
4. All settings / parameters are now saved to the device memory and restored when the device powers up.
5. Added Midi support to make it simply to map effect parameters to Midi CC commands for controlling presets via midi.
