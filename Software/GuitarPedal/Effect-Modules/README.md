# GuitarML Modules
Additional modules for bkshepherd framework developed by GuitarML

# Verb Delay

Stereo Reverb and Delay effect with lots of options. <br>
Capable of normal ethereal reverb and tap tempo delay (with triplett or dotted 8th), or get weird with reverse, octave (reverse octave!), stereo options, and modulation. <br>
This module is compatible with bkshepherd/DaisySeedProjects 125B pedal with OLED Screen.
<br><br>
If you have a 125B OLED pedal, go to [Releases](https://github.com/GuitarML/DaisySeedProjects/releases/tag/v1.0) to download the compatible .bin containing this ReverbDelay module.

## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Delay Time | 50ms to 4 seconds |
| Ctrl 2 | D Feedback | 0 to 1 delay feedback control |
| Ctrl 3 | Delay Mix | Dry / Wet delay mix |
| Ctrl 4 | Reverb Time | Sets the reverb decay time, all the way right is infinite |
| Ctrl 5 | Reverb Damp | Sets the dampening of the reverb via a low pass filter, knob right is more dampening |
| Ctrl 6 | Reverb Mix | Dry / Wet reverb mix |
| FS 1 | Delay Tap Tempo | Sets the delay time via tap tempo |
| FS 2 | Bypass/Active | Bypass / effect engaged |
| LED 1 | Tap Tempo Indicator | Blinks with the tempo of the delay time |
| LED 2 | Bypass/Active Indicator |Illuminated when effect is set to Active |
| Audio In 1 | Audio input | Stereo or Mono in  |
| Audio Out 1 | Mix Out | Stereo or Mono out |

## Effect Parameters (accesible from the rotary encoder / OLED screen)
This is where things get interesting..

| Parameter | Description | Comment |
| --- | --- | --- |
| Delay Mode | Normal / Triplett / Dotted 8th | Adds a second tap at 2/3 (triplett) or 3/4 (dotted 8th) of main delay time |
| Series D>R | Delay into Reverb in Series | Set to Parallel Reverb/Delay by default, this pushes the delay output directly into the reverb |
| Reverse | Reverse Delay (on/off) | Reverse delay up to 4 seconds (when Octave is also on this will do Reverse Octave!) |
| Octave | Octave Delay (on/off) | Octave delay, each pass through the delay doubles the speed, for steadily rising delay octaves |
| Delay LPF | Internal Delay Low Pass Filter | Internal low pass filter, which is applied for each delay pass. Tames the harsh Octave sounds, or creates a "fading into the distance" effect |
| D Spread | Delay Spread (Stereo) up to 50ms, or L/R separation for Dual Delay | Up to 50 ms of separation added to right delay channel, or amount of stereo separation with Dual Delay active  |
| Dual Delay | Both Fwd and Reverse Delays activated (on/off) | Plays both forward and reverse delays (or fwd octave / reverse octave if Octave mode activated) |
| Mod Amt | Modulation Amount/Depth | The amount of modulation applied |
| Mod Rate | Modulation Rate (0 to 3 Hz) | Rate of modulation |
| Mod Param | Parameter to Modulate | None / DelayTime / DelayLevel / ReverbLevel / DelayPan (Stereo) |
| Mod Wave | Waveform of the modulation | Sine / Triangle / Saw / Ramp / Square |
| Sync Mod F | Sync the mod rate to delay time | Overrides the Mod Rate setting |


## Build
Important: If you are building this module yourself, ensure the following items are set correctly in the DaisySeedProjects GuitarPedal framework.

1. In ```guitar_pedal.cpp```, set the block size to 48 to allow for more intense processing of this effect module. ```hardware.SetAudioBlockSize(48)```  (normally set to 4)
2. In ```guitar_pedal_storage.h```, set the max param count to 20, to properly load all of the parameters for this module. ```#define SETTINGS_MAX_EFFECT_PARAM_COUNT 20``` (normally set to 16)
3. In ```guitar_pedal.cpp```, you may need to remove some or all other effect modules besides ReverbDelayModule(). If you experience a frozen OLED screen, there is probably too much data on the stack in addition to this effect.
