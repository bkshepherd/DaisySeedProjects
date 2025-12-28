#pragma once
#ifndef GUITAR_PEDAL_FUNBOX_H
#define GUITAR_PEDAL_FUNBOX_H /**< & */

#include "base_hardware_module.h"

#ifdef __cplusplus

/** @file guitar_pedal_funbox.h */

using namespace daisy;

namespace bkshepherd {

/**
   @brief Helpers and hardware definitions for the Funbox pedal platform by GuitarML.
*/
class GuitarPedalFunbox : public BaseHardwareModule {
  public:
    GuitarPedalFunbox();
    ~GuitarPedalFunbox();
    void Init(size_t blockSize, bool boost) override;
};
} // namespace bkshepherd
#endif
#endif