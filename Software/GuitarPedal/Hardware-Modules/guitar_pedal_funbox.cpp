#include "guitar_pedal_funbox.h"

using namespace bkshepherd;

static const int s_switchParamCount = 2;
static const PreferredSwitchMetaData s_switchMetaData[s_switchParamCount] = {
    {sfType : SpecialFunctionType::Bypass, switchMapping : 0}, {sfType : SpecialFunctionType::Alternate, switchMapping : 1}};

GuitarPedalFunbox::GuitarPedalFunbox() : BaseHardwareModule() {
    // Setup the Switch Meta Data for this hardware
    m_switchMetaDataParamCount = s_switchParamCount;
    m_switchMetaData = s_switchMetaData;
}

GuitarPedalFunbox::~GuitarPedalFunbox() {}

void GuitarPedalFunbox::Init(size_t blockSize, bool boost) {
    BaseHardwareModule::Init(blockSize, boost);

    m_supportsStereo = true;

    Pin knobPins[] = {seed::D16, seed::D17, seed::D18, seed::D19,
                      seed::D20, seed::D21, seed::D15}; // seed::D15, the 7th knobPin is for Expression pedal input
    InitKnobs(7, knobPins);

    // Index reference: {0:footswitch1, 1:footswitch2, 2:Switch1Left, 3:Switch1Right, 4:Switch1Left, 5:Switch2Right, 6:Switch3Left,
    // 7:Switch3Right, 8:Dip1, 9:Dip2, 10:Dip3, 11:Dip4}
    Pin switchPins[] = {seed::D26, seed::D25, seed::D14, seed::D13, seed::D7, seed::D10,
                        seed::D2,  seed::D4,  seed::D1,  seed::D3,  seed::D5, seed::D6};

    InitSwitches(12, switchPins);

    Pin ledPins[] = {seed::D23, seed::D22};
    InitLeds(2, ledPins);

    InitMidi(seed::D30, seed::D29);
}
