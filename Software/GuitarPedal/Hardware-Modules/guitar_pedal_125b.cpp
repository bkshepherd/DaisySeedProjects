#include "guitar_pedal_125b.h"

using namespace bkshepherd;

static const int s_switchParamCount = 2;
static const PreferredSwitchMetaData s_switchMetaData[s_switchParamCount] = {{sfType: SpecialFunctionType::Bypass, switchMapping: 0},
                                                                            {sfType: SpecialFunctionType::TapTempo, switchMapping: 1}}; 

GuitarPedal125B::GuitarPedal125B() : BaseHardwareModule()
{
    // Setup the Switch Meta Data for this hardware
    m_switchMetaDataParamCount = s_switchParamCount;
    m_switchMetaData = s_switchMetaData;
}

GuitarPedal125B::~GuitarPedal125B()
{

}

void GuitarPedal125B::Init(bool boost)
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
    InitDisplay(seed::D9, seed::D11);
    InitTrueBypass(seed::D1, seed::D12);
}