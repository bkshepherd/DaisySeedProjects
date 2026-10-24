# SETUP

This document goes into detail on how to get everything up and running.

## Getting Started

It is recommended to try to setup your local development environment first, though there is an alternative with [pre-compiled binaries](#using-pre-compiled-releases).
 
_However this means you can't make any changes which can be limiting! And since a while there are too many effects to fit in one binary so you will be missing out on some good stuff._

### 1. Setup your Development Environment

The code in this project is supplied with a Microsoft Visual Code project. Detailed instructions on setting up your dev environment can be found here:

https://docs.daisy.audio/tutorials/cpp-dev-env/

Note that previously an older version of the arm-gcc compiler was need, this is no longer the case and the current (at the time of writing) version 15.3 should work fine.

### 2. Setup dependencies

Check to make sure that the following directories have files in them:

1. `Software/GuitarPedal/dependencies/libDaisy`
1. `Software/GuitarPedal/dependencies/DaisySP`
1. `Software/GuitarPedal/dependencies/q/q`
1. `Software/GuitarPedal/dependencies/q/infra`
1. `Software/GuitarPedal/dependencies/gcem`

If they do not have anything in them, you may need to do a `git submodule update --init --recursive`.

Once those directories exist and have files in them, you need to build libDaisy, DaisySP, and CloudSeed. This can be done with the following command:

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

_It should be noted that the 125B version is highly recommended as a lot of functionality depend on the display._

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

and so on for the other possible options...


### 4. Build and Deploy a firmware

Before building, choose which effects the firmware contains. See [GENERATE_FIRMWARE.md](./docs/GENERATE_FIRMWARE.md) for how to generate the firmware configuration with `./effects_config.sh`.

By default this project is configured to use the custom Boot Loader. To get up and running you'll need to do the following:

1. Build the software with `make -j4`, confirm that `build/guitarpedal.bin` exists
1. Put your Daisy Seed into DFU mode.
1. From the terminal, in the GuitarPedal folder run "make program-boot"
1. Once this finishes installing the custom boot loader on the Daisy Seed, press the Reset button. The led will temporary blink for about 3 second.
1. While the LED is blinking run "make program-dfu"
1. That's it!
1. If you make code changes, you can simply run `make -j4` to rebuild them, and then rerun the previous 2 steps (reset button + `make program-dfu`)

If you run into trouble with the bootloader. Electro-Smith has better documentation on how to get it working here: https://github.com/daisyaudio/libDaisy/blob/master/doc/md/_a7_Getting-Started-Daisy-Bootloader.md

If you want to use built in flash memory only, you _can_, but it severely limits which effects you can use and how many you can have installed at once. I'd recommend choosing just 1 or 2 effects (see [GENERATE_FIRMWARE.md](docs/GENERATE_FIRMWARE.md)). Then do the following to get running on internal flash:

1. Remove the "APP_TYPE = BOOT_SRAM" line from the Make File:
2. Put your Daisy Seed into DFU mode.
3. make build-and-program-dfu

### 5. Connect your Guitar and Amp

Plug your guitar into the Input and connect the Output to your amp.

- Right button.
  Turn effect on/off
- 
### 6. Enjoy!!!

## Choosing which effects are built

Which effects (and chains of effects) go into the firmware is chosen with the tool `./effects_config.sh`; see [GENERATE_FIRMWARE.md](docs/GENERATE_FIRMWARE.md).

## MIDI Effect and Preset Recall

On MIDI-enabled hardware, Program Change always selects an effect using its zero-based position in the effect list (the enabled effects, in the order of `effects.json`). To also recall a saved preset for that effect, send standard Bank Select messages before the Program Change:

```text
CC 0  = Bank Select MSB
CC 32 = Bank Select LSB
PC    = effect index
```

The preset number is `(CC0 * 128) + CC32`, matching the "Preset #" shown on the pedal (Preset 0 is the power-on preset). For example, effect 4, preset 2 is `CC0=0`, `CC32=2`, `PC=4`. A Program Change without a preceding Bank Select just switches effect without loading a preset. If the requested preset doesn't exist, the effect is selected without changing its parameters.

Many switchers display Program Change numbers starting at 1 and send the displayed number minus 1, so effect index 4 is entered as PC 5 on such devices. A MIDI monitor is the easiest way to confirm what your switcher actually sends.

CC0 and CC32 are reserved for Bank Select. Multi Delay Tap Mode moved from CC32 to CC33; update controller assignments that used its old CC. Bank selection is consumed by the next Program Change, so send the complete sequence for each combined recall. The effect order is build-specific and follows `effects.json`.

## Using pre-compiled releases

_Please note that this step is currently not recommended as it will not contain all effects and a lot of configuration options will be missing, for instance effect-chains._

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
