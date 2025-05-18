# Multi-Effect Guitar Pedal Software

This directory includes all the source code for my Multi-Effect Guitar Pedal that runs on various hardware platforms.

## Getting Started

Before you can use the software you'll need to do the following steps. There is an option [down below](#using-pre-compiled-releases) to skip setup of a development environment and use a pre-compiled .bin file, however this means you can't make any changes which can be limiting! It is recommended to try to setup your local development environment first.

### 1. Setup your Development Environment

The code in this project is supplied with a Microsoft Visual Code project and depends on both **LibDaisy** and **DaisySP** from Electro-Smith. Detailed instructions on setting these up for your dev environment can be found here:

https://electro-smith.github.io/libDaisy/index.html

### 2. Setup dependencies

Check to make sure that the following directories have files in them:

1. `Software/GuitarPedal/dependencies/libDaisy`
1. `Software/GuitarPedal/dependencies/DaisySP`
1. `Software/GuitarPedal/dependencies/q/q`
1. `Software/GuitarPedal/dependencies/q/infra`
1. `Software/GuitarPedal/dependencies/gcem`
1. `Software/GuitarPedal/dependencies/RTNeural`
1. `Software/GuitarPedal/dependencies/eigen`

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
```

If you want to target the GuitarPedal125B hardware change the lines to:

```cpp
#define VARIANT_125B
// #define VARIANT_1590B
// #define VARIANT_1590B_SMD
// #define VARIANT_TERRARIUM
```

If you want to target the GuitarPedal1590B hardware change the lines to:

```cpp
// #define VARIANT_125B
#define VARIANT_1590B
// #define VARIANT_1590B_SMD
// #define VARIANT_TERRARIUM
```

If you want to target the GuitarPedal1590B-SMD hardware change the lines to:

```cpp
// #define VARIANT_125B
// #define VARIANT_1590B
#define VARIANT_1590B_SMD
// #define VARIANT_TERRARIUM
```

If you want to target the Pedal PCB hardware change the lines to:

```cpp
// #define VARIANT_125B
// #define VARIANT_1590B
// #define VARIANT_1590B_SMD
#define VARIANT_TERRARIUM
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

## Using pre-compiled releases

1. Download the .zip for the hardware variant you have built from the latest release https://github.com/bkshepherd/DaisySeedProjects/releases
1. Unzip it so that you have a guitarpedal.bin
1. Follow the instructions on https://flash.daisy.audio/
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
