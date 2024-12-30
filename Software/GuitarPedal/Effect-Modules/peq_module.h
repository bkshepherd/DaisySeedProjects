#pragma once
#ifndef PEQ_MODULE_H
#define PEQ_MODULE_H

#include <stdint.h>

#include "base_effect_module.h"

#ifdef __cplusplus

/** @file peq_module.h */

namespace bkshepherd {

class ParametricEQModule : public BaseEffectModule {
  public:
    ParametricEQModule();
    ~ParametricEQModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ParameterChanged(int parameter_id) override;
};
} // namespace bkshepherd
#endif
#endif
