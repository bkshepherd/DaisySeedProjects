#ifndef LOADED_EFFECTS_H
#define LOADED_EFFECTS_H
#pragma once

#include "Effect-Modules/base_effect_module.h"
#include "Effect-Modules/modulated_tremolo_module.h"
#include "Effect-Modules/chorus_module.h"

namespace bkshepherd {

void load_effects(int &availableEffectsCount, BaseEffectModule **&availableEffects) {
    static BaseEffectModule* effectList[] = {
        new ModulatedTremoloModule(),
        new ChorusModule(),
    };
    
    availableEffectsCount = sizeof(effectList) / sizeof(effectList[0]);
    availableEffects = effectList;
}

} // namespace bkshepherd

#endif