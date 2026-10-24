# 125B User Manual

This is a DIY pedal project, not a finished product. You build the PCB-based hardware, fit it into an enclosure, then compile and flash the firmware configuration that suits you.

## Build and Configure

- Follow the [125B hardware build guide](../../../Hardware/GuitarPedal125b/docs/README.md) for PCB manufacture, parts, assembly, and enclosure details. The guide includes ready-to-order board files and describes the additional parts and soldering required.
- Install the toolchain and dependencies, then follow the [software build and deployment instructions](../README.md#4-build-and-deploy-the-code). The 125B is the default hardware target; check `guitar_pedal.cpp` if you have changed variants.
- Choose the installed effects and their order with `./effects_config.sh`, as described in [GENERATE_FIRMWARE.md](GENERATE_FIRMWARE.md), then build and flash, per the instruction in [SETUP.md](SETUP.md).

## Effect Chains

There is now support for combining several effects into an effect chain, meaning you can enable more than one effect at the same time.

By defining an `EffectChain` (in `./effects_config.sh`, press `c`); it appears as one selectable effect and processes its child effects in sequence. Each effect in a chain can be switched on and off on its own, and with the chain's **Switches** parameter two of them can be put on the footswitches, like two pedals on a board (see [Footswitches and LEDs](#footswitches-and-leds)). Keep an eye on CPU headroom: a large chain can exceed the processor's capacity, and some modules have restrictions on chaining. 

## Connections

Connect your instrument to Input and your amplifier or next device to Output. The 125B supports stereo or mono input and output; it also has MIDI In/Out, USB access for firmware updates, and a standard 9 V center-negative "pedal" power input. Follow the hardware guide for wiring and assembly details.

## Controls

### Knobs and Encoder

Each effect assigns up to six parameters to the six knobs. The assignments vary by effect and firmware. A knob may be unused. Turning a mapped knob changes its parameter immediately; the display briefly shows the parameter name and value. Other parameters can be edited from the effect's parameter menu.

Note that each effect starts out with the parameter values defined in its "Preset 0", which is initially configured by the effect's authors to (hopefully) be a sensible starting point. Only when turning a knob does the relevant parameter value get updated. This means that, until a knob is turned, its physical position may not match the parameter value.

The rotary encoder underneath the screen controls the menu system. Turn the encoder to move through menus or adjust a selected value. Press it to open a menu item, start editing, or confirm a choice. Use the on-screen Back item to return.

### Footswitches and LEDs

- **Primary (right):** Press to turn the selected effect on or bypass it. Hold for about two seconds to jump to the tuner (assuming it's included in the build). In an effect chain whose **Switches** parameter is not Normal, a press switches one of the chain's effects instead (see below).
- **Secondary (left):** Its action depends on the selected effect. Tap it three times, each within two seconds of the one before, to set the tempo when that effect supports tempo; the tempo is taken from the last two taps, and every further tap in the series adjusts it again. Press and release actions can also control effect-specific features such as a looper. Holding for about one second triggers the effect's hold action; in a chain configured with a second knob bank, this switches banks and displays `SHIFT`. In an effect chain whose **Switches** parameter is not Normal, a press switches one of the chain's effects instead, and none of the above applies (see below).
- **Effect selection shortcut:** Hold the secondary switch and turn the encoder to move through the effect list. Note that you can also select the effect from the menu system, by turning the encoder to the "Effect" menu item.
- **Manual save:** Hold both footswitches for about two seconds to save the current effect's settings to its selected preset. The display shows a save confirmation. If auto-save is off (the default), effect parameters will return to their previous state if the pedal is rebooted, or when switching effects.

The right LED (above the primary footswitch) generally indicates if the pedal is active (light on) or bypassed (light off). The left LED (above the secondary footswitch) is effect-dependent. For example, it may blink in time to the delay temp, or indicate a secondary effect.

### Effect chains: the Switches parameter
An effect chain has a **Switches** parameter in its menu (after the effects' On switches) with options: 1+2, 1+3 or 2+3.

This allows for assigning the two footswitches to both enable/disable one of the effects in the chain.
The right footswitch the first of the two (in 2+3, effect 2), the left one the second, and each LED shows its footswitch's effect. 

With this feature enabled the chain itself always stays on; an effect without a footswitch (effect 3 in 1+2) stays as set in the menu, and with both footswitch effects off you hear whatever more is enabled, or the dry signal. 

Tap tempo and the left footswitch's own functions (including the second knob bank) are not available in this mode.

## Menus and Presets

Press the encoder from the main screen to open the current effect's parameter list. Turn to a parameter and press the encoder to edit it; choose **Defaults** to reset that effect's parameters to their "factory" defaults. The main menu also contains:

- **Preset:** Opens the preset menu: **Preset #** selects a preset for the current effect, and **Erase All** erases all saved settings and presets (a factory reset). Presets are stored separately for each effect, and there is always a "Preset 0" that is the "power-on" state. After changing settings via knobs or the effect parameter menu, hold both footswitches down to save to the selected preset.
- **Effect:** Select which effect is active.
- **Settings:** Configure global pedal behavior, described below.

The selected effect and pedal on/bypass state are retained across power cycles. Preset saving is separate unless Auto-save is enabled.

## Settings

In the settings menu a number of global options can be configured.

- **True Bypass:** Uses the hardware relay to route around the effect when bypassed. Turn it off to use buffered bypass instead.
- **Split Mono:** Sends the left input to both processing channels for a mono source. This applies when True Bypass is off.
- **MIDI On:** Enables MIDI input handling, including mapped controls and effect selection by program change.
- **MIDI Thru:** Forwards incoming MIDI messages to MIDI Out.
- **MIDI Ch:** Selects the MIDI channel the pedal responds to (1-16).
- **MIDI effect and preset recall:** A plain Program Change still selects an effect by its zero-based position in the firmware's effect list. To select both an effect and one of its saved presets, send Bank Select MSB (CC0), Bank Select LSB (CC32), then Program Change. The pedal uses the combined bank number to select the effect and the Program Change value to select that effect's saved preset. Bank 0 is reserved for legacy behavior, so the effect's bank number is its zero-based effect index plus one. For example, effect 4, preset 2 uses `CC0=0`, `CC32=5`, `PC=2`. A bank number of 5 is encoded as MSB 0 and LSB 5. Some controller editors display values as 1-128; the transmitted MIDI values are 0-127. The bank is consumed by the next Program Change, so configure the switcher to send the bank and Program Change with each recall. If the bank/effect or preset does not exist, the selection is ignored or the effect is selected without loading a preset, respectively.
- **Reserved MIDI CCs:** CC0 and CC32 are used for Bank Select and are not available for effect parameters. Multi Delay Tap Mode now uses CC33 instead of CC32.
- **Auto-save:** After parameter changes stop for five seconds, saves that effect's settings to Preset 0. This overwrites Preset 0; turn it off if you use that preset for a manual patch, or want the pedal to always return to a saved state on startup.
- **Lock Knobs:** Ignores the knobs, so that parameters are only changed from the display and a bumped knob changes nothing. Currently on by default.
- **Show Debug:** Draws the CPU load (average/peak %) and the input and output levels on the effect screen, along with the heap use, to help calibrate levels and check the load of an effect. Off by default.
- **Trails:** Lets delay and reverb tails ring out after the effect is turned off instead of cutting them. It does not apply with True Bypass, and the length of the tail depends on the effect. On by default.
- **In Gain:** Gain in dB (-24 to +24, in steps of 0.5; the encoder's coarse step is 3 dB) applied to the input before it reaches the effect and the dry signal, to calibrate the pedal to your instrument or to the level of the rest of the rig. The input level readout of Show Debug shows the level after this gain.
- **Out Gain:** Gain in dB (-24 to +24) applied to the output before it goes to the output jack, to calibrate the level that leaves the pedal. The output level readout of Show Debug shows the level after this gain. (With True Bypass the relay routes the signal around the pedal, so neither gain applies to the bypassed sound.)
- **Reboot:** Restarts the Daisy Seed in bootloader mode – useful for firmware updates.

## Effects

For the current set see the [effects](EFFECTS.md) document.

