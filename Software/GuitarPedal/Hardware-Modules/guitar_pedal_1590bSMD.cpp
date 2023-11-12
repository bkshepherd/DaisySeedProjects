#include "guitar_pedal_1590bSMD.h"

using namespace bkshepherd;
  
GuitarPedal1590BSMD::GuitarPedal1590BSMD() : BaseHardwareModule()
{

}

GuitarPedal1590BSMD::~GuitarPedal1590BSMD()
{

}

void GuitarPedal1590BSMD::Init(bool boost)
{
    BaseHardwareModule::Init(boost);

    m_supportsStereo = true;
    
    Pin knobPins[] = {seed::D15, seed::D16, seed::D17, seed::D18};
    InitKnobs(4, knobPins);

    Pin switchPins[] = {seed::D6, seed::D5};
    InitSwitches(2, switchPins);

    Pin ledPins[] = {seed::D22, seed::D23};
    InitLeds(2, ledPins);

    InitMidi(seed::D30, seed::D29);
    InitTrueBypass(seed::D1, seed::D12);
}