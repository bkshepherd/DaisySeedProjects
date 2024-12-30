#pragma once
#ifndef EQ_MODULE_H
#define EQ_MODULE_H

#include <stdint.h>

#include "base_effect_module.h"

#ifdef __cplusplus

/** @file eq_module.h */

namespace bkshepherd {

class EQModule : public BaseEffectModule {
  public:
    EQModule();
    ~EQModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ParameterChanged(int parameter_id) override;
    void DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                bool isEditing) override;
};
} // namespace bkshepherd
#endif
#endif
