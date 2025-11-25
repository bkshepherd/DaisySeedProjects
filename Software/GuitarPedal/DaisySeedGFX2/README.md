# DaisySeedGFX MK2

Version MK2 of the graphical library dedicated to the Daisy Seed platform by Electrosmith.

## Author

DAD Design

## Overview

DaisySeedGFX is a graphical library designed for the Daisy Seed platform by Electrosmith. Currently, it supports only the ST7735 and ST7789 controllers, but it can potentially be adapted to other controllers with minimal effort.

This library provides a set of essential graphical primitives, such as text, lines, rectangles, circles, arcs, and bitmaps. These primitives can be easily extended based on your needs.

### Key Features in MK2

- **Layered Frame Buffer**: A major update in MK2, the frame buffer is now divided into layers with support for transparency between them.
- **Efficient Updates**: Changes in the layers are concatenated and transmitted to the controller using SPI transfers under DMA. To minimize data transfer, the frame buffer is divided into blocks—only modified blocks are sent to the screen.
- **Flash Storage Support**: MK2 now supports fonts and bitmaps stored in flash memory.
  - Font files (.h for direct integration, .bin for memory download) can be generated using the TrueType-to-Bitmap-Converter utility (repository: [https://github.com/DADDesign-Projects/TrueType-to-Bitmap-Converter](https://github.com/DADDesign-Projects/TrueType-to-Bitmap-Converter)).
  - Image and binary bitmap files can be transferred to flash memory using the **Daisy\_QSPI\_Flasher** utility (repository: [https://github.com/DADDesign-Projects/Daisy\_QSPI\_Flasher](https://github.com/DADDesign-Projects/Daisy_QSPI_Flasher)).

## Implementation

The library code can be compiled in VS Code within the Daisy Seed development environment (see [https://github.com/electro-smith](https://github.com/electro-smith)).

### Configuration

1. Create a project using the `helper.py` tool (or an alternative method).
2. Clone the library into the `DaisySeedGFX` folder inside your project directory.
3. Edit the `.vscode/c_cpp_properties.json` file and add `${workspaceFolder}/DaisySeedGFX2//**` to the "includePath" section.
4. Copy the `DaisySeedGFX/UserConfig.h` file into your project folder and configure it based on your screen and pin setup.

### Examples

Implementation examples can be found in the repository: https://github.com/DADDesign-Projects/Demo_DaisySeedGFX2




 
