# Multi-Effect Guitar Pedal Software

This directory includes all the source code for my Multi-Effect Guitar Pedal that runs on various hardware platforms.

## Getting Started

Before you can use the software you'll need to do the following steps. There is an option [down below](#using-pre-compiled-releases) to skip setup of a development environment and use a pre-compiled .bin file, however this means you can't make any changes which can be limiting! It is recommended to try to setup your local development environment first.

### 1. Setup your Development Environment

The code in this project is supplied with a Microsoft Visual Code project and depends on both **LibDaisy** and **DaisySP** from Electro-Smith. Detailed instructions on setting these up for your dev environment can be found here:

https://electro-smith.github.io/libDaisy/index.html

Some issues have been reported with newer ARM compiler versions. The documentation above from Electrosmith should be complete but for reference many users have had success with `10.3-2021.10`

Version can be checked with `arm-none-eabi-gcc --version`

### 2. Setup dependencies

Check to make sure that the following directories have files in them:

1. `Software/GuitarPedal/dependencies/libDaisy`
1. `Software/GuitarPedal/dependencies/DaisySP`
1. `Software/GuitarPedal/dependencies/q/q`
1. `Software/GuitarPedal/dependencies/q/infra`
1. `Software/GuitarPedal/dependencies/gcem`

If they do not have anything in them, you may need to do a `git submodule update --init --recursive`.

Once those directories exist and have files in them, you should build libDaisy, DaisySP, and CloudSeed. This can be done with the following command:

1. From `Software/GuitarPedal` directiory: `./ci/build_libs.sh`

If this doesn't work, you can do it manually:

1. `cd libDaisy`
1. `make clean && make -j4`
1. `cd ..`
1. `cd DaisySP`
1. `make clean && make -j4`
1. `cd ..`
1. `cd CloudSeed`
1. `make clean && make -j4`

#### Additional information for this step:

- Note - You may not need to do all parts of this step if the project pulled down from GitHub with the dependencies already included as sub-modules. The desktop GUI client always should pull down submodules. If it still doesn't pull down and you have libDaisy and DaisySP installed somehwere else, you can follow the steps below, (note that you will still need to setup cycfi/q and cycfi/infra dependencies separately so the submodule way should be preferred):

  - You'll need to update the paths in the **Makefile**.
  - You'll also need to update the paths in the **c_cpp_properties.json** file in the **.vscode/** folder.
  - You'll also need to update the paths in the **task.json** file in the **.vscode/** folder.

### 3. Configure your specific Target Hardware

You'll need to open the guitar_pedal.cpp file and uncomment a line to configure which hardware you are targetting. If you don't do this, it will build for the 125B variant.

In guitar_pedal.cpp:

```cpp
// Uncomment the version you are trying to use, by default (and if nothing is
// uncommented), the 125B with 2 footswitch variant will be used

// #define VARIANT_125B
// #define VARIANT_1590B
// #define VARIANT_1590B_SMD
// #define VARIANT_TERRARIUM
// #define VARIANT_FUNBOX
```

If you want to target the GuitarPedal125B hardware change the lines to:

```cpp
#define VARIANT_125B
// #define VARIANT_1590B
// #define VARIANT_1590B_SMD
// #define VARIANT_TERRARIUM
// #define VARIANT_FUNBOX
```

If you want to target the GuitarPedal1590B hardware change the lines to:

```cpp
// #define VARIANT_125B
#define VARIANT_1590B
// #define VARIANT_1590B_SMD
// #define VARIANT_TERRARIUM
// #define VARIANT_FUNBOX
```

If you want to target the GuitarPedal1590B-SMD hardware change the lines to:

```cpp
// #define VARIANT_125B
// #define VARIANT_1590B
#define VARIANT_1590B_SMD
// #define VARIANT_TERRARIUM
// #define VARIANT_FUNBOX
```

If you want to target the Pedal PCB hardware change the lines to:

```cpp
// #define VARIANT_125B
// #define VARIANT_1590B
// #define VARIANT_1590B_SMD
#define VARIANT_TERRARIUM
// #define VARIANT_FUNBOX
```

### 4. Build and Deploy the Code

By default this project is configured to use the custom Boot Loader. To get up and running you'll need to do the following:

1. Build the software with `make -j4`, confirm that `build/guitarpedal.bin` exists
1. Put your Daisy Seed into DFU mode.
1. From the terminal, in the GuitarPedal folder run "make program-boot"
1. Once this finishes installing the custom boot loader on the Daisy Seed, press the Reset button. The led will temporary blink for about 3 second.
1. While the LED is blinking run "make program-dfu"
1. That's it!
1. If you make code changes, you can simply run `make -j4` to rebuild them, and then rerun the previous 2 steps (reset button + `make program-dfu`)

If you run into trouble with the bootloader. Electro-Smith has better documentation on how to get it working here: https://github.com/electro-smith/libDaisy/blob/master/doc/md/_a7_Getting-Started-Daisy-Bootloader.md

If you want to use built in flash memory only, you _can_, but it severely limits which effects you can use and how many you can have installed at once. I'd recommend editing the list of active effects in the loaded_effects.h file to perhaps just 1 or 2. Then do the following to get running on internal flash:

1. Remove the "APP_TYPE = BOOT_SRAM" line from the Make File:
2. Put your Daisy Seed into DFU mode.
3. make build-and-program-dfu

### 5. Connect your Guitar and Amp

Plug your guitar into the Input and connect the Output to your amp.

### 6. Enjoy!!!

## Optional: Dattorro plate reverb (GPLv3)

`Effect-Modules/Dattorro/` contains a Dattorro (1997) plate reverb, in the same family used by
the Hothouse "Flick" and "MuleBox" pedals. It's disabled by default and commented out of both the
`Makefile` and `loaded_effects.h`, because that code is licensed **GPL-3.0-or-later**, not MIT
like the rest of this repository — see [`Effect-Modules/Dattorro/README.md`](Effect-Modules/Dattorro/README.md)
for the full provenance chain and license text. Enabling it means the firmware binary you build
must be distributed under GPLv3 (or not distributed at all); leaving it commented out keeps your
build entirely MIT.

To opt in:

1. Uncomment the two `Dattorro` lines in the `Makefile`.
2. Uncomment the `#include` and `new DattorroReverbModule(),` lines in `loaded_effects.h`.
3. Rebuild and flash.

## Combining Effects (EffectChain)

`Effect-Modules/effect_chain.h` lets you wire two or more effects into a single chained entry in
`loaded_effects.h`, processed serially (e.g. tremolo into reverb), without changing any of the
effect modules themselves or the menu/UI code. For example:

```cpp
new EffectChain(
    "Trem+Verb",
    // Slots: a short menu tag plus the child effect instance (owned by the chain).
    {{"Tr", new ModulatedTremoloModule()}, {"Rv", new ReverbModule()}},
    // Knob/MIDI CC mappings: {slot, child param id, knob (-1 = none), midi CC (-1 = none)}.
    {{0, ModulatedTremoloModule::DEPTH, 0, 20},
     {0, ModulatedTremoloModule::FREQ, 1, 21},
     {1, ReverbModule::TIME, 2, 22},
     {1, ReverbModule::DAMP, 3, 23},
     {1, ReverbModule::MIX, 4, 24}}),
```

Every child parameter shows up in the effect's parameter menu, tagged with its slot's prefix (e.g.
"Tr Depth", "Rv Mix") so identically-named parameters from different children stay distinct; only
the parameters listed in the mapping array are reachable from a knob and/or MIDI CC. One slot (the
first, by default) is the "primary" child, which owns the LED and the alternate footswitch.

Every slot also gets its own "\<tag\> On" checkbox in the menu, defaulting to on, so any slot can be
bypassed independently. This is saved with presets exactly like any other parameter.

By default the alternate footswitch just forwards to the primary child (e.g. for tap tempo).
Pass `footswitchTogglesSlots` to repurpose it as an on-off slot for one or more children, e.g.:

```cpp
new EffectChain(
    "HT+Dly+Rv",
    {{"Tr", new HarmonicTremoloModule()}, {"Dl", new DelayModule()}, {"Rv", new DattorroReverbModule()}},
    {{2, DattorroReverbModule::MIX,    0, 20},
     {0, HarmonicTremoloModule::DEPTH, 1, 21},
     {0, HarmonicTremoloModule::SPEED, 2, 22},
     {1, DelayModule::DELAY_TIME,      3, 23},
     {1, DelayModule::D_FEEDBACK,      4, 24},
     {1, DelayModule::DELAY_MIX,       5, 25}},
    /* primarySlot */ 1,
    /* footswitchTogglesSlots */ {0}), // or {0,1} to toggle trem+delay together
```

A `ChainMapping` can also target a slot's own "On" parameter instead of a child parameter, using the
`EffectChain::SLOT_ENABLE` sentinel (e.g. `{0, EffectChain::SLOT_ENABLE, -1, 30}`), which puts that
bypass on a MIDI CC regardless of whether the footswitch is also toggling it.

Turning a slot off crossfades it out over about a tenth of a second and then stops processing it
entirely, both to avoid a click and to save CPU. Note the delay/reverb trails are not preserved.

If you want to have more than six physical knobs, give any `ChainMapping` a `knobMapping` of 6 or
higher, and that parameter moves to a second knob bank instead of being unreachable: holding the alternate
footswitch for a second toggles between the two banks (an on-screen "SHIFT" label shows when the
second bank is active), and holding it again switches back. Physical knob 1 reads whatever's mapped
to knob 0 normally, and whatever's mapped to knob 6 while shifted; knob 2 maps to 1/7, and so on
through knob 6 mapping to 5/11. This only kicks in when a chain actually uses a `knobMapping` of 6 or
higher somewhere — with every mapping in 0-5, holding the footswitch keeps forwarding to the primary
child (or stays a no-op under `footswitchTogglesSlots`) exactly as before:

```cpp
new EffectChain(
    "HT+Dly+Rv",
    {{"Tr", new HarmonicTremoloModule()}, {"Dl", new DelayModule()}, {"Rv", new DattorroReverbModule()}},
    {{2, DattorroReverbModule::MIX,       0, 20},
     {0, HarmonicTremoloModule::DEPTH,    1, 21},
     {0, HarmonicTremoloModule::SPEED,    2, 22},
     {1, DelayModule::DELAY_TIME,         3, 23},
     {1, DelayModule::D_FEEDBACK,         4, 24},
     {1, DelayModule::DELAY_MIX,          5, 25},
     // Shift bank - reachable by holding the alternate footswitch for 1s.
     {2, DattorroReverbModule::PRE_DELAY, 6, 26},
     {2, DattorroReverbModule::DECAY,     7, 27},
     {2, DattorroReverbModule::TONE,      8, 28},
     {2, DattorroReverbModule::DIFFUSE,   9, 29},
     {1, DelayModule::DELAY_LPF,          10, 30},
     {1, DelayModule::MOD_AMT,            11, 31}},
    /* primarySlot */ 1,
    /* footswitchTogglesSlots */ {0}),
```

This isn't meant to support every combination of effects — some modules need caution, or are a poor
fit for chaining altogether:

- **Single-instance-only modules.** Many modules that keep DSP buffers or other resources in a file-scope
  global, where every *instance* of that class aliases the same memory. In practice this means you can
  only create one instance. You can*use* the one instance more than once, e.g. in more than one effect
  chain or in a combination of an effect chain and a standalone effect, but since they share their
  parameters, any changes when viewing one patch affects all references.
  
  Modules in this category: `ReverbModule`, `DattorroReverbModule`, `DelayModule`, `TapeDelayModule`,
  `MultiDelayModule`, `GranularDelayModule`, `SpectralDelayModule`, `SciFiModule`,
  `PitchShifterModule`, `LooperModule`, `PluckEchoModule`, `NamA2Module`, `CloudSeedModule`,
  `TunerModule`.
- **Custom on-screen displays are lost.** A chain always draws its own generic name/parameter screen
  instead of forwarding to a child's `DrawUI`, so these modules lose some or all of their custom
  display when chained: `AutoPanModule`, `ChopperModule`, `DelayModule`, `GraphicEQModule`,
  `LooperModule`, `MetroModule`, `ParametricEQModule`, `PitchShifterModule`, `TapeDelayModule`,
  `TunerModule`. This doesn't stop them working, but it can be confusing.

### Sharing a single-instance effect between chains

A single-instance-only module (see above) can still show up in more than one chain, as long as
it's the same object every time. Construct it once, put it in every `ChainSlot` that wants it, and
mark every slot but one `ownsEffect = false` so only one of them deletes it:

```cpp
static DattorroReverbModule* reverb = new DattorroReverbModule();

new EffectChain("HarmTremVerb",    {{"HT", new HarmonicTremoloModule()}, {"Rv", reverb}}, ...),                    // owns it
new EffectChain("ModTremVerb", {{"Tr", new ModulatedTremoloModule()}, {"Rv", reverb, /* ownsEffect */ false}}, ...), // reuses it
```

The two chains are then processing the exact same reverb - its parameters are shared state, not a
copy per chain. Switching between the chains never resets or overwrites the other's settings by
itself; only turning that shared knob, editing it in the menu, or a MIDI CC actually changes it. The
one gap is at power-on: every chain restores its own saved values into whatever it maps parameters
to, so whichever chain happens to come later in `effectList` decides the shared effect's values after
a reboot - not necessarily the chain you had active when you last powered off.

## Using pre-compiled releases

1. Download the .zip for the hardware variant you have built from the latest release https://github.com/bkshepherd/DaisySeedProjects/releases
1. Unzip it so that you have a guitarpedal.bin
1. Open https://flash.daisy.audio/ in a supported web browser
   1. Flash the bootloader (instructions on the Bootloader tab of the web flashing tool)
      - This step can be skipped if the bootloader has already been flashed
   1. Flash the firmware .bin file (and repeat these steps for changing firmware versions)
      1. Press RESET button on the daisy
      1. Within 5 seconds single press the BOOT button on the daisy (this locks it into bootloader mode so that you can take your time to do the following steps instead of trying to do it all within 5 seconds)
      1. Use File Upload to select the .bin file
      1. Press Flash

## Software Updates

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
