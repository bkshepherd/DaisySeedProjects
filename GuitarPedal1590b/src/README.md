#Getting Started
This directory includes all the Sample Code that uses the custom guitar_pedal_1590b hardware. 

Currently, the sample code implements a chorus effect on the Audio input and routes it to the Audio Output on the hardware.  The various hardware knobs and switches are also mapped to parameters on the effect. There is also some limited Midi In/Out and OLED display screen test code.

Before you can use the software you'll need to do the following:

## 1. Setup your Development Environment

The code in this project is supplied with a Microsoft Visual Code project and depends on both **LibDaisy** and **DaisySP** from Electro-Smith. Detailed instructions on setting these up for your dev environment can be found here:

https://electro-smith.github.io/libDaisy/index.html

## 2. Update Paths to your install of LibDaisy and DaisySP

You'll need to update the paths in the **Makefile**.

You'll also need to update the paths in the **c_cpp_properties.json** file in the **.vscode/** folder.

You'll also need to update the paths in the **task.json** file in the **.vscode/** folder.

## 3. Build and Deploy the Code

Next you have to get the code onto the GuitarPedal1590B hardware you've built using **task build_and_program_dfu**

## 4. Connect your Guitar and Amp

Plug your guitar into the Input and connect the Output to your amp.

## 5. Enjoy!!!

The foot switch toggles the Chorus effect on / off and the 4 dials control different aspects of the chorus effect.
