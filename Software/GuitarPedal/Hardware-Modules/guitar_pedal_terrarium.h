#pragma once
#ifndef GUITAR_PEDAL_TERRARIUM_H
#define GUITAR_PEDAL_TERRARIUM_H /**< & */

#include "base_hardware_module.h"

#ifdef __cplusplus

/** @file guitar_pedal_terrarium.h */

using namespace daisy;

namespace bkshepherd {

/**
   @brief Helpers and hardware definitions for a 1590B sized Guitar Pedal based on the Daisy Seed.
*/
class GuitarPedalTerrarium : public BaseHardwareModule {
  public:
    GuitarPedalTerrarium();
    ~GuitarPedalTerrarium();
    void Init(bool boost = false) override;
};
} // namespace bkshepherd
#endif
#endif