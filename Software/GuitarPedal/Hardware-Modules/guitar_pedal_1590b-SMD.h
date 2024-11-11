#pragma once
#ifndef GUITAR_PEDAL_1590BSMD_H
#define GUITAR_PEDAL_1590BSMD_H /**< & */

#include "base_hardware_module.h"

#ifdef __cplusplus

/** @file guitar_pedal_1590b-SMD.h */

using namespace daisy;

namespace bkshepherd {

/**
   @brief Helpers and hardware definitions for the SMD version of my 1590B sized Guitar Pedal based on the Daisy Seed.
*/
class GuitarPedal1590BSMD : public BaseHardwareModule {
  public:
    GuitarPedal1590BSMD();
    ~GuitarPedal1590BSMD();
    void Init(bool boost = false) override;
};
} // namespace bkshepherd
#endif
#endif