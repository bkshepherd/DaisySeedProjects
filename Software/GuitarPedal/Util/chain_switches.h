#pragma once
// The "Switches" parameter of an effect chain (EffectChain, read by guitar_pedal.cpp through GetFootswitchSlots): the two footswitches can
// switch two of the chain's slots on and off instead of working as usual (the right one bypassing the whole effect, the left one the effect's
// own alternate function). The right footswitch gets the lower slot. The choice of slots is here so that it can be tested on the PC.
namespace chainswitches {

enum Mode { NORMAL = 0, SLOTS_1_2, SLOTS_1_3, SLOTS_2_3, MODE_COUNT };

inline const char *kModeNames[MODE_COUNT] = {"Normal", "1+2", "1+3", "2+3"}; // not const: libDaisy's MappedStringListValue takes a const char **

/** The slots the two footswitches switch (0-based), or -1 for both when the footswitches work as usual. */
struct Assignment {
    int bypassSlot = -1;    // the right (bypass) footswitch
    int alternateSlot = -1; // the left (alternate) footswitch
    bool Active() const { return bypassSlot >= 0; }
};

/** The assignment for a mode and an effect with slotCount slots (0 for an effect that is not a chain). A mode that needs a slot the effect does
 *  not have (1+3 or 2+3 on a chain of two), an unknown mode and a plain effect give the usual behaviour. */
inline Assignment Resolve(int mode, int slotCount) {
    Assignment a;
    switch (mode) {
    case SLOTS_1_2: a.bypassSlot = 0; a.alternateSlot = 1; break;
    case SLOTS_1_3: a.bypassSlot = 0; a.alternateSlot = 2; break;
    case SLOTS_2_3: a.bypassSlot = 1; a.alternateSlot = 2; break;
    default: return Assignment{};
    }
    if (a.alternateSlot >= slotCount) return Assignment{};
    return a;
}

} // namespace chainswitches
