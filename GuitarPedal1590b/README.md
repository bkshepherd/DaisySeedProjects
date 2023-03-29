# Guitar Pedal 1590b

### Rev 3 - 3/29/2023

Updates include:
1. Improved 4 Layer PCB Layout, much easier to assemble!
2. Replaced all SMD parts with through-hole alternative
3. Tayda Custom Drill Template for easily ordering a pre-drilled enclosure

### Overview

A project to create a digital effect pedal based on the Electro-Smith Daisy Seed that fits into a standard 1590B sized Guitar Pedal enclosure. Electro-Smith sells a guitar pedal, but it's a much larger format, and I wanted something as small as possible.  The PedalPCB Terrarium was another option, but it's mono only, so I decided to build my own. This one is Stereo In / Out and has Midi In / Out, so it's quite flexible and can be programmed to do a lot.

![FinalProduct](docs/images/FinalProduct.png) ![Backside](docs/images/Alive.png)

![CircuitBoard](docs/images/CircuitBoard.png) ![PCBs](docs/images/PCBs.png)

### Features

1. Buffered Stereo Input and Outputs for Guitar Level signals
2. TRS Mini MIDI Input and Outputs
3. 4 Pots
4. Up to 2 Footswitches
5. Up to 2 Leds
6. Standard 9v center pin negative power jack
7. Easily Assembled from easily sourced through-hole parts, no SMD soldering required!
8. Easily order a custom drilled enclosure from Tayda!

This project includes a KiCad project with the necessary schematics and PCB layout files to create everything you need to build the hardware and code the provides a hardware attraction layer as well as sample code with a custom Tremolo effect that uses the hardware.

Click on this image for a Demo Video (of an older revision):

[![Demo Video](https://img.youtube.com/vi/gWRPFADz1Wg/0.jpg)](https://www.youtube.com/watch?v=gWRPFADz1Wg)

Information about the Daisy Seed can be found at:

http://electro-smith.com

Information about KiCad can be found at:

https://www.kicad.org

Getting Started:

1. [Build the Hardware](docs/README.md)
2. [Deploy the Software to the Hardware](src/README.md)
