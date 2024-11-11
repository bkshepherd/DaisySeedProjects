#pragma once
#ifndef GUITAR_PEDAL_125B_H
#define GUITAR_PEDAL_125B_H /**< & */

#include "base_hardware_module.h"

#ifdef __cplusplus

/** @file guitar_pedal_125b.h */

using namespace daisy;

namespace bkshepherd {

/**
   @brief Helpers and hardware definitions for a 125B sized Guitar Pedal based on the Daisy Seed.
*/
class GuitarPedal125B : public BaseHardwareModule {
  public:
    GuitarPedal125B();
    ~GuitarPedal125B();
    void Init(bool boost = false) override;
};
} // namespace bkshepherd
#endif
#endif