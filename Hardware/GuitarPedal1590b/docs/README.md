# Building the Hardware
Some quick instructions for how to make the GuitarPedal1590B Hardware.  This document assumes you know the basics about soldering and circuit board assembly.  You'll also need to be fairly handy with a drill to mount the various knobs and switches on the enclosure (or you can order one pre-made from Tayda).

Quick note, I couldn't include some of the custom footprints for specific components due to licenses that wouldn't allow redistribution.  Here is a list of where you can find some of them:

* PDS1-S5-S5-D - https://www.cui.com/product/dc-dc-converters/isolated/pds1-d-series (click on "all models" for downloadable footprints)
* Neutrik NMJ6HCD2 - https://www.snapeda.com/parts/NMJ6HCD2/Neutrik/view-part/
* PJ-320A - Footprint and STEP Model found on this repository https://github.com/keebio/Keebio-Parts.pretty

Also, I just want to acknowledge that the schematics for the circuits were kit bashed together from the following sources with some additions of my own:

* [Electro-Smith: Daisy Petal Rev 5 Schematics](https://github.com/electro-smith/Hardware/blob/master/reference/daisy_petal/ES_Daisy_Petal_Rev5.pdf)
* [Simple DIY Electronic Music Projects](https://diyelectromusic.wordpress.com/2022/08/29/3v3-midi-module-pcb/)

Getting everything into KiCad and the PCB layouts are all my own work.

## 1. Order the PCBs

The KiCad files are included for the full schematic and pcb board layout in the **pcb** folder.  You really don't need to know anything about KiCad to order the PCB and get it made, but they are there for reference.

I like to order my PCBs from https://www.jlcpcb.com, but anywhere should work fine.

Follow these steps to get the PCB made by JLCPCB:

1. Download the ready-made [JLCPCB Gerber Files](../pcb/JLCPCB-Gerbers/DaisySeedPedal1590-Rev5-gerbers.zip) for this project to your computer. (Keep the files zipped)
2. Visit the [JLCPCB Website](https://www.jlcpcb.com).
3. Click the **Order Now** button in the top menu bar.
4. Click the **Add Gerber File** button. ![Add Gerbers](images/JLCPCB-OrderNow.png)
5. Upload the zipped Gerber files you downloaded above.
6. After processing the files it should provide a screen with various options. Everything is fine by default, but you may want to change the color of the board and I like to use ENIG finishing options instead of HASL, but this is up to you. ![Processed Gerbers](images/JLCPCB-Options.png)
7. Add the item to your cart, and order the PCB.

They will give you a price for ordering 5 pcbs (that's their minimum order).  I think I paid ~$50 for a Purple board with ENIG (this is a more expensive option).  It will take a week or two for it arrive and they look like this:

![PCBs](images/PCBs.png)

## 2. Source the Components

A full list of all the required components can be found in the [Bill_of_Materials_BOM.xlsx](Bill_of_Materials_BOM.xlsx) file.  Everything uses Through-Hole type components for simplicity sake.

## 3. Solder Everything

It will roughly look like this when finished with this step:

![CircuitBoard](images/CircuitBoard.png)

(I omitted the 2nd foot switch from my build)

## 4. Flash the Software to the Hardware

Before you attempt to get everything fitted into the enclosure, it's best to make sure all the hardware works.

You'll need to compile the code and flash it into the Daisy Seed on the hardware. Instructions can be found [here](../../../Software/GuitarPedal/README.md).

## 5. Order the Enclosure

You'll also need to order the enclosure, which is 1590B sized.

You can either order a generic one like this from Amazon: https://www.amazon.com/dp/B07VKR51NN and drill your own holes (which is a pain in the butt), or you can order a custom drilled & powder coated enclosure from Tayda using this [Custom Drill Template](https://drill.taydakits.com/box-designs/new?public_key=YWRhVFFGU0Z2c3RJR09VQ1U4S3EvUT09Cg==)

Please note, the top two tiny holes inbetween the knobs are so you can reach the flash and reset buttons on the DaisySeed, for easy re-programming during development.  You can keep or omit them. 

## 6. Jam everything into the enclosure, and ENJOY!

![FinalProduct](images/FinalProduct.png)
