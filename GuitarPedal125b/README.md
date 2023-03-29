# Guitar Pedal 125b

### Rev 2 - 3/29/2023

Updates include:
1. Added 3 Additional Pots (Total of 6)
2. Improved OLED Screen Placement
3. Removed an Encoder (now just 1)
4. Tayda Custom Drill Template for easily ordering a pre-drilled enclosure

### Overview

A project to create a digital effect pedal based on the Electro-Smith Daisy Seed that fits into a standard 125B sized Guitar Pedal enclosure.

This project is still work in progress and is an evolution of my [1590B sized Pedal](https://github.com/bkshepherd/DaisySeedProjects/tree/main/GuitarPedal1590b). Documenation is sparse at the moment, but I hope to improve it over time.

![CircuitBoard](docs/images/CircuitBoard-Front.png) ![CircuitBoard](docs/images/CircuitBoard-Back.png) ![PCBs](docs/images/PCBs.png)

### Features
This one uses a slightly larger enclosure, incorporates primarily SMD parts, includes a display two digital rotary encoders with push buttons, 3 analog potentiometers, relayed based audio bypass switching, stereo in / out audio trs jacks, and midi in / out mini-trs jacks, so it's quite flexible and can be programmed to do a lot.

This project includes a KiCad project with the necessary schematics and PCB layout files to create everything you need to build the hardware. I actually used JLCPCB to manufacture the PCB and assemble the first prototype board.  The BOM file is in the proper format for them to assemble it with their available parts.  Sample Code is also provided with a hardware attraction layer as well as a custom Tremolo effect that uses the hardware.

Information about the Daisy Seed can be found at:

http://electro-smith.com

Information about KiCad can be found at:

https://www.kicad.org
