# Getting Started
This directory includes all the source code for my Multi-Effect Guitar Pedal that runs on various hardware platforms. 

### SW Updates - 9/29/2023

Software updates include:
1. Added support for multiple effects.  Included are a simple tremolo and overdrive.
2. Updated the menu system to support multiple effects each with their own settings and global hardware settings. Parameters can be updated directly through the menu UI or using the pots on the device.
3. All settings / parameters are now saved to the device memory and restored when the device powers up.
4. Added Midi support to make it simply to map effect parameters to Midi CC commands for controlling presets via midi.

Before you can use the software you'll need to do the following:

## 1. Setup your Development Environment

The code in this project is supplied with a Microsoft Visual Code project and depends on both **LibDaisy** and **DaisySP** from Electro-Smith. Detailed instructions on setting these up for your dev environment can be found here:

https://electro-smith.github.io/libDaisy/index.html

## 2. Update Paths to your install of LibDaisy and DaisySP

You'll need to update the paths in the **Makefile**.

You'll also need to update the paths in the **c_cpp_properties.json** file in the **.vscode/** folder.

You'll also need to update the paths in the **task.json** file in the **.vscode/** folder.

## 3. Build and Deploy the Code

Next you have to get the code onto your daisy seed based Guitar Pedal hardware using **task build_and_program_dfu**

## 4. Connect your Guitar and Amp

Plug your guitar into the Input and connect the Output to your amp.

## 5. Enjoy!!!
