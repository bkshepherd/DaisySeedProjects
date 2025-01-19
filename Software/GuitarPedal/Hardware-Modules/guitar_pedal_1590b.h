#pragma once
#ifndef GUITAR_PEDAL_1590B_H
#define GUITAR_PEDAL_1590B_H /**< & */

#include "base_hardware_module.h"

#ifdef __cplusplus

/** @file guitar_pedal_1590b.h */

using namespace daisy;

namespace bkshepherd {

/**
   @brief Helpers and hardware definitions for a 1590B sized Guitar Pedal based on the Daisy Seed.
*/
class GuitarPedal1590B : public BaseHardwareModule {
  public:
    GuitarPedal1590B();
    ~GuitarPedal1590B();
    void Init(size_t blockSize, bool boost) override;
};
} // namespace bkshepherd
#endif
#endif