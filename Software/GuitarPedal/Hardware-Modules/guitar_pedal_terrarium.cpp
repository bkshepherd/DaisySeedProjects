#include "guitar_pedal_terrarium.h"

using namespace bkshepherd;

static const int s_switchParamCount = 2;
static const PreferredSwitchMetaData s_switchMetaData[s_switchParamCount] = {
    {sfType : SpecialFunctionType::Bypass, switchMapping : 0}, {sfType : SpecialFunctionType::Alternate, switchMapping : 1}};

GuitarPedalTerrarium::GuitarPedalTerrarium() : BaseHardwareModule() {
    // Setup the Switch Meta Data for this hardware
    m_switchMetaDataParamCount = s_switchParamCount;
    m_switchMetaData = s_switchMetaData;
}

GuitarPedalTerrarium::~GuitarPedalTerrarium() {}

void GuitarPedalTerrarium::Init(size_t blockSize, bool boost) {
    BaseHardwareModule::Init(blockSize, boost);

    Pin knobPins[] = {seed::D16, seed::D17, seed::D18, seed::D19, seed::D20, seed::D21};
    InitKnobs(6, knobPins);

    Pin switchPins[] = {seed::D26, seed::D25, seed::D10, seed::D9, seed::D8, seed::D7};
    InitSwitches(6, switchPins);

    Pin ledPins[] = {seed::D23, seed::D22};
    InitLeds(2, ledPins);
}
