# DEVELOPMENT OF GUITARPEDAL

## Setting up VS Code development environment
Make sure it can find the compiler
Intellisense configuration

## Main entry
guitar_pedal.cpp


## Effects

loaded_effects.h

Effect-Modules

## Hardware adaptation

Hardware-Modules




Från xconverge 27:e maj
------------------------
Printing log statements:

    enable debug logging by uncommenting this line DaisySeedProjects/Software/GuitarPedal/guitar_pedal.cpp at main · bkshepherd/DaisySeedProjects · GitHub
    If you want to print floats you can temporarily add this to the makefile: LDFLAGS += -u _printf_float Lost half day on figuring out how to debug/ print floats :/
    Add your print statements
    Run and connect with serial monitor, I use this Serial Monitor - Visual Studio Marketplace

Additionally, you could also put the tap tempo on the UI if you have a screen

The code used to print the CPU should be a good example of that and you could just pop it into your effects DrawUI (or even the base_effect_module itself)
github.com/bkshepherd/DaisySeedProjects
Software/GuitarPedal/Effect-Modules/base_effect_module.cpp
main

                rightArrowRect = rightArrowRect.Reduced(0, 1);
                if (rightArrowRect.IsEmpty())
                    break;
            }
        }
        display.WriteStringAligned(m_name, Font_11x18, topRowRect, Alignment::centered, true);
        display.WriteStringAligned("...", Font_11x18, boundsToDrawIn, Alignment::centered, true);
        if (showCPU) {
            char cpuStr[64];
            sprintf(cpuStr, FLT_FMT(3), FLT_VAR3(GetCPUUsage()));
            display.WriteStringAligned(cpuStr, Font_11x18, boundsToDrawIn, Alignment::bottomCentered, true);
        }
    }

This also shows how you can format floats for printing WITHOUT needing the above flags/configuration

Edit: Also because I had posted 3 times in a row before I was unable to post a 4th time a few days ago when something came up I wanted to post here, so in a way you freed me, thank you :slight_smile:

My suggestion is to start small and add to it and ask any questions you want! If you want to start a long async discussion as a single github issue feel free as well, here is fine too!

What existing effect is most similar to what you are trying to do?

The simplest effect I would reference for the boiler plate would be compressor_module.cpp

    Create new effect with boiler plate copy/paste and adjust the name and add it to makefile and LoadedEffects.h and make sure you can build/flash this and it shows up when you switch to it (either with menu or hold the alternate footswitch and rotate the encoder to switch effects)
    ProcessMono() adjusted to take input and assign it to the output
    Parameter/knobs setup
    ParameterChanged configured to adjust member variables
    Override tap tempo logic by referencing the metronome, tremolo, or delay which are already working “fine”. Some of this logic is in the base_effect_module too

guitar_pedal.cpp has some of the meat for how it all comes together, but you probably don’t need to make any changes here to get an effect implemented. This would probably require some tweaks to make adjustments with how MIDI is processed to then set tempo (instead of relying on footswitch logic which is also done in this file)

Besides that, I have found chatgpt is stellar for being able to paste in the whole effect module code and ask specific questions to really hone in on some stuff and what you are trying to do, especially if you are a bit unfamiliar with some of the DSP you are trying to do.

I think I would start with the non-tempo oriented stuff and then once you are happy with it come back and we figure out the tempo stuff and how to fit it in
