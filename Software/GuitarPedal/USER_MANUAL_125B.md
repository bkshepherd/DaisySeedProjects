# 125B User Manual

This is a DIY pedal project, not a finished product. You build the PCB-based hardware, fit it into an enclosure, then compile and flash the firmware configuration that suits you.

## Build and Configure

- Follow the [125B hardware build guide](../../Hardware/GuitarPedal125b/docs/README.md) for PCB manufacture, parts, assembly, and enclosure details. The guide includes ready-to-order board files and describes the additional parts and soldering required.
- Install the toolchain and dependencies, then follow the [software build and deployment instructions](README.md#4-build-and-deploy-the-code). The 125B is the default hardware target; check `guitar_pedal.cpp` if you have changed variants.
- Choose the installed effects and their order by editing `loaded_effects.h`, then build and flash, per the instruction in the README. Since only one effect is active at a time, there is little penalty for enabling a lot of different effects, but if there are ones you won't use, it's better to leave them out so they don't clutter the menu system.

If you want to have two or three active effects at the same time, it is possible (with some limitations) to make a combined effect, by defining an `EffectChain` in `loaded_effects.h`; it appears as one selectable effect and processes its child effects in sequence. Keep an eye on CPU headroom: a large chain can exceed the processor's capacity, and some modules have restrictions on chaining. See the [EffectChain notes](README.md#combining-effects-effectchain) before building a chain.

## Connections

Connect your instrument to Input and your amplifier or next device to Output. The 125B supports stereo or mono input and output; it also has MIDI In/Out, USB access for firmware updates, and a standard 9 V center-negative "pedal" power input. Follow the hardware guide for wiring and assembly details.

## Controls

### Knobs and Encoder

Each effect assigns up to six parameters to the six knobs. The assignments vary by effect and firmware. A knob may be unused. Turning a mapped knob changes its parameter immediately; the display briefly shows the parameter name and value. Other parameters can be edited from the effect's parameter menu.

Note that each effect starts out with the parameter values defined in its "Preset 0", which is initially configured by the effect's authors to (hopefully) be a sensible starting point. Only when turning a knob does the relevant parameter value get updated. This means that, until a knob is turned, its physical position may not match the parameter value.

The rotary encoder underneath the screen controls the menu system. Turn the encoder to move through menus or adjust a selected value. Press it to open a menu item, start editing, or confirm a choice. Use the on-screen Back item to return.

### Footswitches and LEDs

- **Primary (right):** Press to turn the selected effect on or bypass it. Hold for about two seconds to jump to the tuner (assuming its included in the build).
- **Secondary (left):** Its action depends on the selected effect. Double-tap to set tap tempo when that effect supports tempo. Press and release actions can also control effect-specific features such as a looper. Holding for about one second triggers the effect's hold action; in a chain configured with a second knob bank, this switches banks and displays `SHIFT`.
- **Effect selection shortcut:** Hold the secondary switch and turn the encoder to move through the effect list. Note that you can also select the effect from the menu system, by turning the encoder to the "Effect" menu item.
- **Manual save:** Hold both footswitches for about two seconds to save the current effect's settings to its selected preset. The display shows a save confirmation. If auto-save is off (the default), effect parameters will return to their previous state if the pedal is rebooted, or when switching effects.

The right LED (above the primary footswitch) generally indicates if the pedal is active (light on) or bypassed (light off). The right LED is effect-dependent. For example, it may blink in time to the delay temp, or indicate a secondary effect.

## Menus and Presets

Press the encoder from the main screen to open the current effect's parameter list. Turn to a parameter and press the encoder to edit it; choose **Defaults** to reset that effect's parameters to their defaults. The main menu also contains:

- **Preset:** Select a preset for the current effect. Presets are stored separately for each effect. Hold both footswitches to save to the selected preset; **Erase All** restores storage defaults, clearing all presets and global settings.
- **Effect:** Select which effect is active.
- **Settings:** Configure global pedal behavior, described below.

The selected effect and pedal on/bypass state are retained across power cycles. Preset saving is separate unless Auto-save is enabled.

## Settings

- **True Bypass:** Uses the hardware relay to route around the effect when bypassed. Turn it off to use buffered bypass instead.
- **Split Mono:** Sends the left input to both processing channels for a mono source. This applies when True Bypass is off.
- **MIDI On:** Enables MIDI input handling, including mapped controls and effect selection by program change.
- **MIDI Thru:** Forwards incoming MIDI messages to MIDI Out.
- **MIDI Ch:** Selects the MIDI channel the pedal responds to (1-16).
- **Auto-save:** After parameter changes stop for five seconds, saves that effect's settings to Preset 0. This overwrites Preset 0; turn it off if you use that preset for a manual patch, or want the pedal to always return to a saved state on startup.
- **Reboot:** Restarts the Daisy Seed in bootloader mode – useful for firmware updates.

## Effects

The default firmware currently includes the effects below. Their exact controls and knob assignments can change with the build; the effect parameter menu lists the controls available in your version.

- **Tremolo, Harm Trem, AutoPan, Chopper:** Amplitude or stereo-pan modulation; rate/tempo, depth, waveform, and related modulation controls.
- **Chorus, Phaser, Flanger:** Sweeping modulation effects; rate, depth, mix, feedback, and tone controls.
- **Overdrive, Distortion, Crusher:** Saturation and digital reduction; drive/gain, tone, output level, bit depth, or sample-rate reduction.
- **Compressor, Noise Gate:** Dynamic range and noise control; level, ratio, threshold, attack, and release.
- **Reverb, CloudSeed:** Short room-like to expansive reverberation; mix, decay, pre-delay, size, and tone/damping.
- **Delay, Multi Delay, Tape Delay, Dual Echo ("Halo"), Granular Delay, Spectral Delay:** Echo and time-based textures; delay/tap times, feedback, mix, filtering, modulation, and grain or spectral controls.
- **Pitch, PolyOctave, SciFi:** Pitch shifting, octave generation, and processed pitch effects; interval, dry/wet balance, and effect-specific controls.
- **Graphic EQ, Parametric EQ:** Tone shaping; band gain and, for parametric bands, frequency and bandwidth.
- **NAM, IR:** Amp and cabinet impulse-response processing; model or IR selection, gain, level, and EQ.
- **Looper, Drum, Metronome, Tuner:** Performance and utility tools; loop levels/actions, rhythm or tempo controls, and tuning options.

