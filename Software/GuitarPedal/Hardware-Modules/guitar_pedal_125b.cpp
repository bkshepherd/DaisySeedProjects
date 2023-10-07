#include "guitar_pedal_125b.h"

using namespace bkshepherd;
  
GuitarPedal125B::GuitarPedal125B() : BaseHardwareModule()
{
    m_knobCount = 6;
    m_switchCount = 2;
    m_encoderCount = 1;
    m_ledCount = 2; 
    m_supportsMidi = true;
}

GuitarPedal125B::~GuitarPedal125B()
{

}
