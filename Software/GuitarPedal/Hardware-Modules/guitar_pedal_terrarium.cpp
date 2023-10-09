#include "guitar_pedal_terrarium.h"

using namespace bkshepherd;
  
GuitarPedalTerrarium::GuitarPedalTerrarium() : BaseHardwareModule()
{

}

GuitarPedalTerrarium::~GuitarPedalTerrarium()
{

}

void GuitarPedalTerrarium::Init(bool boost)
{
    BaseHardwareModule::Init(boost);

    Pin knobPins[] = {seed::D16, seed::D17, seed::D18, seed::D19, seed::D20, seed::D21};
    InitKnobs(6, knobPins);

    Pin switchPins[] = {seed::D26, seed::D25, seed::D10, seed::D9, seed::D8, seed::D7};
    InitSwitches(6, switchPins);

    Pin ledPins[] = {seed::D23, seed::D22};
    InitLeds(2, ledPins);
}
