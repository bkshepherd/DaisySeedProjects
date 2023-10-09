#include "guitar_pedal_1590b.h"

using namespace bkshepherd;
  
GuitarPedal1590B::GuitarPedal1590B() : BaseHardwareModule()
{

}

GuitarPedal1590B::~GuitarPedal1590B()
{

}

void GuitarPedal1590B::Init(bool boost)
{
    BaseHardwareModule::Init(boost);

    m_supportsStereo = true;
    
    Pin knobPins[] = {seed::D15, seed::D16, seed::D17, seed::D18, seed::D19, seed::D20};
    InitKnobs(6, knobPins);

    Pin switchPins[] = {seed::D6, seed::D5};
    InitSwitches(2, switchPins);

    Pin encoderPins[][3] = {{seed::D3, seed::D2, seed::D4}};
    InitEncoders(1, encoderPins);

    Pin ledPins[] = {seed::D22, seed::D23};
    InitLeds(2, ledPins);

    InitMidi(seed::D30, seed::D29);
}