# Building the Hardware
Some quick instructions for how to make the GuitarPedal125B Hardware.  This document assumes you know the basics about soldering and circuit board assembly.  You'll also need to be fairly handy with a drill to mount the various knobs and switches on the enclosure (or you can order one pre-made from Tayda).

Quick note, I couldn't include some of the custom footprints for specific components due to licenses that wouldn't allow redistribution.  Here is a list of where you can find some of them:

* PDS1-S5-S5-M - https://www.cui.com/product/dc-dc-converters/isolated/pds1-m-series (click on "all models" for downloadable footprints)
* Neutrik NMJ6HCD2 - https://www.snapeda.com/parts/NMJ6HCD2/Neutrik/view-part/
* PJ-320A - Footprint and STEP Model found on this repository https://github.com/keebio/Keebio-Parts.pretty

Also, I just want to acknowledge that the schematics for the circuits were kit bashed together from the following sources with some additions of my own:

* [Electro-Smith: Daisy Petal Rev 5 Schematics](https://github.com/electro-smith/Hardware/blob/master/reference/daisy_petal/ES_Daisy_Petal_Rev5.pdf)
* [Simple DIY Electronic Music Projects](https://diyelectromusic.wordpress.com/2022/08/29/3v3-midi-module-pcb/)

Getting everything into KiCad and the PCB layouts are all my own work.

The KiCad files are included for the full schematic and pcb board layout in the **pcb** folder.  You really don't need to know anything about KiCad to order the PCB and get it made, but they are there for reference.

I like to order my PCBs from https://www.jlcpcb.com, but anywhere should work fine.

Follow these steps to get the PCB made by JLCPCB:

## 1. Generate the Gerber & Drill Files

The first step is to use KiCad to export the Gerber & Drill files for production like this: [JLCPCB Gerber and Drill File Generation](https://jlcpcb.com/help/article/16-How-to-generate-Gerber-and-Drill-files-in-KiCad-6). You can also skip this part and simply use the ready-made [JLCPCB Gerber Files](../pcb/JLCPCB-Gerbers/DaisySeedPedal125b-Rev6-gerbers.zip) for this project. (Keep the files zipped)

## 2. Generate BOM and Centroid Files

To use SMT Assemble service from JLCPCB, which you will want to do, the BOM and centroid files need to be generated as well. [Follow these instructions provided by JLCPCB](https://jlcpcb.com/help/article/81-How-to-generate-the-BOM-and-Centroid-file-from-KiCAD). You can also skip this part and simply use the ready-made files for this project:

1. [JLCPCB BOM](../pcb/BOM_JLCSMT_DaisySeedGuitarPedal125b.xlsx)
2. [JLCPCB Centroid](../pcb/JLCPCB-Gerbers/DaisySeedPedal125b-top-pos.csv)

## 3. Pre-Order Parts for JLCPCB

There are a few parts that JLCPCB doesn't stock normally.  You can either pre-order these parts through JLCPCB through their Global Sourcing Service so they can be assembled for you (recommended) of you can order the parts yourself and solder them yourself (advanced).

The main parts they don't typically stock are:

1. PDS1-S5-S5-M (JLCPCB Part #C5377809)
2. CPC1018 This one is usually stocked these days. Probably good to double check though. (JLCPCB Part #C133069 or #C1558973), or as a substitute CPC1019 (JLCPCB Part #C1854946 or #C2760117) And of those work fine.

To order these parts through JLCPCB follow these instructions:

1. Visit the [JLCPCB Website](https://www.jlcpcb.com) and log into your account.
2. Go to the [Parts Manager Page](https://jlcpcb.com/user-center/smtPrivateLibrary/).
3. Click on Order Parts ![Processed Gerbers](images/JLCPCB-OrderParts.png)
4. There are two options for ordering parts JLCPCB Parts and Global Parts Sourcing.  I always try to use JLCPCB Parts ordering first, but sometimes you have to use Global Parts Sourcing.
5. First try looking in JLCPCB Parts and search using the above JLCPCB Part #s.  If you can order it there, great, do so.  You'll need to order at least 5 of each part.
6. If you can't find it there (sometimes the prices are crazy high, or the min quantity is a huge number), then try Global Parts Sourcing. 
7. Search for the part by name in Global Part Sourcing.
5. Find a supplier that lets you order a small quantity and double check the part name to make sure it's the right part.
6. Order at least 5 parts. (Minimum PCB assembly order is 5, so you need 5 of each part).
7. Repeat this process for both parts. 

Please note, that it typically takes a week or two to get the parts ordered and into your parts library for use.  So I like to order these parts in batches enough to place a few orders. 

## 4. Order the Assembled PCBs from JLCPCB

1. Have the Gerber, Drill, BOM, and Centroid files handy. (Keep the files zipped)
2. Visit the [JLCPCB Website](https://www.jlcpcb.com).
3. Click the **Order Now** button in the top menu bar.
4. Click the **Add Gerber File** button.
  
![Add Gerbers](images/JLCPCB-OrderNow.png)
   
5. Upload the zipped Gerber files you downloaded above.
6. After processing the files it should provide a screen with various options. Most everything is fine by default, but you should use ENIG finishing options instead of HASL, you should check the Confirm Production File option and turn on PCB Assembly.

![Processed Gerbers](images/JLCPCB-Options.png)

7. Once you turn on PCB Assembly, you will need to confirm that you want the Top Side assembled and definitely select the Confirm Parts Placement option.

![PCB Assembly Options](images/JLCPCB-AssemblyOptions.png)

8. Click the Confirm button on the PCB Assembly panel.
9. This page shows you the side being assembled. It should look like this.

![PCB Assembly Confirm](images/JLCPCB-AssemblyPCBConfirm.png)

Click Next.

10. Next you need to upload the BOM and Centroid File.

![PCB Assembly Options](images/JLCPCB-AssemblyBOMCentroid.png)

11. Click Add BOM and upload the BOM file. Either the one you generated or the one you downloaded above [JLCPCB BOM](../pcb/BOM_JLCSMT_DaisySeedGuitarPedal125b.xlsx).
12. Click Add CPL and upload the Centroid file. Either the one you generated or the one you downloaded above [JLCPCB Centroid](../pcb/JLCPCB-Gerbers/DaisySeedPedal125b-top-pos.csv). 
13. After the files are uploaded click Process BOM and CPL.

![PCB Assembly Options](images/JLCPCB-AssemblyProcessBOMCentroid.png)

14. This page asks you to confirm the parts.  You should see at the top 28 Parts Detected and 28 Parts Confirmed.

![PCB Assembly Options](images/JLCPCB-AssemblyBOMCentroidConfirm.png)

If it tells you any parts are missing, you may need to substitute. For instance, the parts you ordered from global parts sourcing may have ended up with a different JLCPCB Part number than the ones in the supplied files.  If that is the case you can click on the part in the list and search for the proper part and replace it.  Repeat this process until all parts are accounted for.
15. Click Next.
16. This next page allows you to view the placement of the Components.  Notice that a few of the parts of not placed or rotated properly. (this is due to differences in KiCad's footprint library and JLCPCB's library).

![PCB Assembly Options](images/JLCPCB-AssemblyPlacement.png)

17. You will have to manually fix them in this viewer.

Simply click each part that is not properly placed, and use the buttons to rotate and move them until they are in place. The parts that need fixing are J1,J4,J5,U1,U3,U4,U5,U7,U8,K1,K2,Q1.

* Note U2 (PDS1-S5-S5-M) will not show up as available to change placement / rotation.  JCLPCB doesn't have a 3D model for this part since it's from global sourcing. Don't worry that it's not shown on this screen.

It should look like this when fixed.

![PCB Assembly Options](images/JLCPCB-AssemblyPlacementFixed.png)

18. Click Next.
19. Add the item to your cart, and order the PCB.

![PCB Assembly Options](images/JLCPCB-AssemblyAddToCart.png)

They will give you a price for ordering 5 assembled pcbs (that's their minimum order).  I think I paid ~$175 to have the PCB manufactured, assembled with additional parts we ordered, and shipped to my door. Keep an eye on your email to confirm the order at the various steps. It will take a week or two for it arrive and they look like this:

![PCBs](images/PCBs.png)

## 5. Order the Additional Parts JLCPCB doesn't Assemble

There are a few parts that you will need to order yourself and solder on the PCBs after you recieve them from JLCPCB.

Here is a list of the additional parts to order for the PCB:

1. 2 Headers 20-Pin Single Row for the Daisy Seed. I used these from [Amazon](https://www.amazon.com/dp/B09MYBZZKZ)
2. 6 Pots. These are the ones I used [Alpha RD901F-40-15R1-B10K](https://www.taydaelectronics.com/10k-ohm-linear-taper-potentiometer-round-shaft-pcb-9mm.html).
3. 1 Rotary Encoder. This is the one I used [PEC11R-4220K-S24](https://www.mouser.com/ProductDetail/652-PEC11R-4220K-S24)
4. 1 OLED Screen. This is the one I used from [Amazon](https://www.amazon.com/dp/B01MQPQF24)
5. 2 Leds (3mm Any Color). These are the one I used from [Amazon](https://www.amazon.com/dp/B07QXR5MZB)
6. 2 Footswitches. These are the ones I used from [Amazon](https://www.amazon.com/dp/B08TBTWDYV)
7. Daisy Seed. You will need a Daisy Seed from [Electro-Smith](https://www.electro-smith.com/daisy/daisy)

![PCB Extra Parts](images/PCBExtraParts.png)

## 6. Finish the PCB (Solder Everything!)

Once you receive the PCB from JLCPCB and the extra parts you ordered separately (Headers, Pots, Encoder, Screen, LEDs, Foot Switches), you will need to solder everything to finish the PCB.

Make sure to solder them in the following order (everything goes on the side without the SMD parts, except the 20-pin headers):

1. Both 20pin Headers (make sure this goes on the side with all the SMD parts!)
2. Pots 1 - 6 (Please note these require special care as mentioned below)
3. Rotary Encoder
4. OLED Screen (Using the header that's already on the OLED, don't try to use a female socket on the pcb, but I did use some Nylon 6mm hex spacers to get it to line up flush with the enclosure when using the Alpha Pots)
5. 2 Leds (You'll need to cut the legs a little bit short to get it to line up with the enclosure, Short pin goes to the square pad)
6. 2 Footswitches
   
* PLEASE NOTE - the Pots need to be electrically isolated from some of the soldered pins that stick out on the PCB where some of them go. With the Alpha pots it's mostly an issue with the bottom row of pots (Pots 4-6).  Some of the little metal standoffs on the pots touch the pins from the daisy seed headers.  This will cause problems.  My solution was to simply cut off the metal standoff from the bottom 3 pots from the side that is closest to the pins. Like shown below.  Definitely spot check the other pots to make sure they aren't touching any pins too.

![Pot Prep](images/PotPrep.png)

It will roughly look like this when finished with this step:

![CircuitBoard](images/CircuitBoard-Front.png)

![CircuitBoard](images/CircuitBoard-Back.png)

## 7. Order the Enclosure and External Hardware

You'll also need to order an enclosure and the external hardware such as the knobs and the Led Holders.

You can either order a generic 125B sized enclosure like these from [Tayda](https://www.taydaelectronics.com/hardware/enclosures/1590b-style-1.html) and drill your own holes (which is a pain in the butt), or you can order a custom drilled & powder coated enclosure from Tayda using this [Custom Drill Template](https://drill.taydakits.com/box-designs/new?public_key=MlRHaUhqaEVXOVVFZ1ErV1FiTXlrQT09Cg==)

Additional External Hardware:

1. 6 Knobs for the POTS.  These are the ones I used from [LoveMySwitches](https://lovemyswitches.com/anodized-aluminum-knob-the-magpie-1-4-smooth-shaft-12-5mm-od/)
2. 1 Knob for the Rotary Encoder. This is what I used from [Amazon](https://www.amazon.com/gp/product/B0829WGW42)
3. 2 Led Holders (for the enclosure). These are the ones I used from [Amazon](https://www.amazon.com/gp/product/B083Q9QMN4)

![Enclosure and Knobs](images/EnclosureAndKnobs.png)

## 8. Flash the Software to the Daisy Seed on the Hardware

Before you attempt to get everything fitted into the enclosure, it's best to make sure all the hardware works.

Install the Daisy Seed into the headers on the PCB.

You'll then need to compile the code and flash it into the Daisy Seed on the hardware. Instructions can be found [here](../../../Software/GuitarPedal/README.md).

## 9. Jam everything into the enclosure, and ENJOY!

You'll want to use the LED Holders on on the enclosure and use the washers and nuts that came with the Pots and Encoder to secure them to the enclosure. 

![FinalProduct](images/FinalProduct.png)
