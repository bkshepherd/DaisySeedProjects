#pragma once
#ifndef GEQ_MODULE_H
#define GEQ_MODULE_H

#include <stdint.h>

#include "base_effect_module.h"

#ifdef __cplusplus

/** @file geq_module.h */

namespace bkshepherd {

class GraphicEQModule : public BaseEffectModule {
  public:
    GraphicEQModule();
    ~GraphicEQModule();

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
