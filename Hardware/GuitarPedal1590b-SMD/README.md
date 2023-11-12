# Guitar Pedal 1590b using SMD Parts

### Rev 2 - 11/3/2023

Updates include:
1. Updated PCB to include the hardware mute
2. Included exported Gerber files to make it easy to order PCBs from JCLPCB. Detailed instructions can be found on the [Build the Hardware](docs/README.md) page. This requires no knowledge of KiCad to get the PCBs made.

### Overview

A project to create a digital effect pedal based on the Electro-Smith Daisy Seed that fits into a standard 1590B sized Guitar Pedal enclosure. The video below shows a pedal board I made using this design, the smaller pedal on the far left, along with 2 of my [125B pedals](../GuitarPedal125b/README.md).

[![Demo Video](https://img.youtube.com/vi/ZkLnS43acQo/0.jpg)](https://www.youtube.com/watch?v=ZkLnS43acQo)

This project is the SMD parts version of my [1590b pedal](../GuitarPedal1590b/README.md). Features are the same as the Through-Hole version with the addition of "True Bypass". The majority of all parts can be pre-assembled on the PCB by a fab like JLCPCB. If you'd prefer a design where all parts are through-hole components, please check out the through-hole version of this project [Guitar Pedal 1590 project](../GuitarPedal1590b/README.md).

![FinalProduct](docs/images/FinalProduct.png) ![Backside](docs/images/Alive.png)

![CircuitBoard](docs/images/CircuitBoard.png) ![PCBs](docs/images/PCBs.png)

### Features

1. Small 1590B pedal board friendly enclosure!
2. Buffered Stereo Input and Outputs for Guitar Level signals
3. TRS Mini MIDI Input and Outputs
4. 4 Pots
5. Up to 2 Foot Switches
6. Up to 2 Leds
7. Standard 9v center pin negative power jack
8. Setup for Easy Manufacturing with JLCPCB.  Minimal soldering required!
9. Easily order a custom drilled enclosure from [Tayda](https://drill.taydakits.com/box-designs/new?public_key=YWRhVFFGU0Z2c3RJR09VQ1U4S3EvUT09Cg==)

This project includes a KiCad project with the necessary schematics and PCB layout files to create everything you need to build the hardware. The included exported Gerber and Assembly files make it easy to order PCBs from JCLPCB. Detailed instructions can be found on the [Build the Hardware](docs/README.md) page. This requires no knowledge of KiCad to get the PCBs made.

Once you've built the hardware you can deploy the software from my Multi-Effect Guitar Pedal software project by following these directions: [Deploy the Software to the Hardware](../../Software/GuitarPedal/README.md)

This software provides a hardware abstraction layer as well as a few custom FX including Tremolo, Chorus, Overdrive, and Stereo Auto-Pan.

## Past Updates
### Rev 1 - ?

Updates include:
1. Original Design Revision

More Information about the Daisy Seed can be found at:

http://electro-smith.com

More Information about KiCad can be found at:

https://www.kicad.org
