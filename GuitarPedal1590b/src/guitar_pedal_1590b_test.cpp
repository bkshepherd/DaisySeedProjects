#include <string.h>
#include "guitar_pedal_1590b.h"
#include "daisysp.h"
#include "dev/oled_ssd130x.h"

using namespace daisy;
using namespace daisysp;
using namespace bkshepherd;

/** Typedef the OledDisplay to make syntax cleaner below 
 *  This is a 4Wire SPI Transport controlling an 128x64 sized SSDD1306
 * 
 *  There are several other premade test 
*/
using MyOledDisplay = OledDisplay<SSD130x4WireSpi128x64Driver>;

GuitarPedal1590B hardware;
MyOledDisplay display;
Chorus     ch;

bool  effectOn;
float led2Brightness;
float wet;

float deltarget, del;
float lfotarget, lfo;

static void AudioCallback(AudioHandle::InputBuffer  in,
                     AudioHandle::OutputBuffer out,
                     size_t                    size)
{
    // Handle Inputs
    hardware.ProcessAnalogControls();
    hardware.ProcessDigitalControls();

    // Handle knobs
    float k = hardware.knobs[0].Process();
    ch.SetLfoFreq(k * k * 20.f);
    lfo = hardware.knobs[1].Process();
    del = hardware.knobs[2].Process();
    ch.SetFeedback(hardware.knobs[3].Process());

    //If the First Footswitch button is pressed, toggle the effect enabled
    effectOn ^= hardware.switches[0].RisingEdge();

    // Process Audio
    for(size_t i = 0; i < size; i++)
    {
        fonepole(del, deltarget, .0001f); //smooth at audio rate
        ch.SetDelay(del);

        fonepole(lfo, lfotarget, .0001f); //smooth at audio rate
        ch.SetLfoDepth(lfo);


        out[0][i] = in[0][i];
        out[1][i] = in[1][i];

        if(effectOn)
        {
            ch.Process(in[0][i]);

            out[0][i] = ch.GetLeft() * wet + in[0][i] * (1.f - wet);
            out[1][i] = ch.GetRight() * wet + in[1][i] * (1.f - wet);
        }
    }
}

// Typical Switch case for Message Type.
void HandleMidiMessage(MidiEvent m)
{
    switch(m.type)
    {
        case NoteOn:
        {
            NoteOnEvent p = m.AsNoteOn();
            char        buff[512];
            sprintf(buff,
                    "Note Received:\t%d\t%d\t%d\r\n",
                    m.channel,
                    m.data[0],
                    m.data[1]);
            //hareware.seed.usb_handle.TransmitInternal((uint8_t *)buff, strlen(buff));
            // This is to avoid Max/MSP Note outs for now..
            if(m.data[1] != 0)
            {
                p = m.AsNoteOn();
                led2Brightness = ((float)p.velocity / 127.0f);
                //osc.SetFreq(mtof(p.note));
                //osc.SetAmp((p.velocity / 127.0f));
            }
        }
        break;
        case ControlChange:
        {
            ControlChangeEvent p = m.AsControlChange();
            switch(p.control_number)
            {
                case 1:
                    // CC 1 for cutoff.
                    //filt.SetFreq(mtof((float)p.value));
                    break;
                case 2:
                    // CC 2 for res.
                    //filt.SetRes(((float)p.value / 127.0f));
                    break;
                default: 
                    led2Brightness = ((float)p.value / 127.0f);
                break;
            }
            break;
        }
        default: break;
    }
}

int main(void)
{
    hardware.Init();
    hardware.SetAudioBlockSize(4);

    /* Configure the Display
    MyOledDisplay::Config disp_cfg;
    disp_cfg.driver_config.transport_config.pin_config.dc    = hw.GetPin(9);
    disp_cfg.driver_config.transport_config.pin_config.reset = hw.GetPin(30);
    display.Init(disp_cfg);
    */

    float sample_rate = hardware.AudioSampleRate();
    ch.Init(sample_rate);

    effectOn  = true;
    led2Brightness = 0.f;
    wet       = .9f;
    deltarget = del = 0.f;
    lfotarget = lfo = 0.f;
 
    // start callback
    hardware.StartAdc();
    hardware.StartAudio(AudioCallback);

    hardware.midi.StartReceive();

    // Send Test Midi Message
    uint8_t midiData[3];
    midiData[0] = 0b10110000;
    midiData[1] = 0b00001010;
    midiData[2] = 0b01111111;
    hardware.midi.SendMessage(midiData, sizeof(uint8_t) * 3);

    //char strbuff[128], szF[8];

    while(1)
    {
        //LED stuff
        hardware.SetLed((GuitarPedal1590B::LedIndex)0, effectOn);
        hardware.SetLed((GuitarPedal1590B::LedIndex)1, led2Brightness);
        hardware.UpdateLeds();
        
        // Handle MIDI Events
        hardware.midi.Listen();

        while(hardware.midi.HasEvents())
        {
            HandleMidiMessage(hardware.midi.PopEvent());
        }

        /* Do something with display
        display.Fill(false);
        display.SetCursor(0, 0);
        display.WriteString("Guitar Pedal", Font_7x10, true);
        display.SetCursor(0, 15);
        display.WriteString("Made by Keith", Font_7x10, true);
        display.SetCursor(0, 30);
        dtostrf(hw.adc.GetFloat(0), 6, 4, szF);
        sprintf(strbuff, "ADC0: %s", szF);
        display.WriteString(strbuff, Font_7x10, true);
        display.SetCursor(0, 45);
        dtostrf(hw.adc.GetFloat(1), 6, 4, szF);
        sprintf(strbuff, "ADC1: %s", szF);
        display.WriteString(strbuff, Font_7x10, true);
        display.Update();
        */

    }
}
