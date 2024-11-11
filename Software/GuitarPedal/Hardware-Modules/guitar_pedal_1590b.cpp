#include "guitar_pedal_1590b.h"

using namespace bkshepherd;

static const int s_switchParamCount = 2;
static const PreferredSwitchMetaData s_switchMetaData[s_switchParamCount] = {
    {sfType : SpecialFunctionType::Bypass, switchMapping : 0}, {sfType : SpecialFunctionType::Alternate, switchMapping : 1}};

GuitarPedal1590B::GuitarPedal1590B() : BaseHardwareModule() {
    // Setup the Switch Meta Data for this hardware
    m_switchMetaDataParamCount = s_switchParamCount;
    m_switchMetaData = s_switchMetaData;
}

GuitarPedal1590B::~GuitarPedal1590B() {}

void GuitarPedal1590B::Init(bool boost) {
    BaseHardwareModule::Init(boost);

    m_supportsStereo = true;

    Pin knobPins[] = {seed::D15, seed::D16, seed::D17, seed::D18};
    InitKnobs(4, knobPins);

    Pin switchPins[] = {seed::D6, seed::D5};
    InitSwitches(2, switchPins);

    Pin ledPins[] = {seed::D22, seed::D23};
    InitLeds(2, ledPins);

    InitMidi(seed::D30, seed::D29);
}