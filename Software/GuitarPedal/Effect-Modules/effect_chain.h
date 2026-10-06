#pragma once
#ifndef EFFECT_CHAIN_H
#define EFFECT_CHAIN_H

#include "base_effect_module.h"
#include "daisysp.h"
#include <cstdint>
#include <initializer_list>
#ifdef __cplusplus

/** @file effect_chain.h */

namespace bkshepherd {

/** One child effect inside an EffectChain.
 *
 * `tag` is a short (2 character) prefix prepended to every one of this
 * child's parameter names in the combined menu, e.g. "Tr Depth", so that two
 * children with the same parameter name (both have a "Mix") stay
 * distinguishable. It also names that slot's synthesized "<tag> On" enable
 * parameter (see EffectChain).
 *
 * By default `effect` is owned by the EffectChain from construction, and
 * deleted with it. Set `ownsEffect` to false to instead share an effect
 * instance that something else owns - e.g. the same single-instance-only
 * module (Dattorro, CloudSeed, ...) used standalone in the effect list and
 * as a slot in one or more chains. The chain will still read/write its
 * parameters and call its Process methods like any other slot; it just
 * won't delete it. Whatever does own it (the standalone effectList entry,
 * or one chain designated as the owner) must outlive every non-owning
 * reference to it.
 */
struct ChainSlot {
    const char *tag;
    BaseEffectModule *effect;
    bool ownsEffect = true;
};

/** Maps one child's parameter onto a physical knob and/or MIDI CC.
 *
 * Every child parameter is exposed in the combined menu regardless of
 * whether it appears here; this list only controls which parameters are
 * reachable from a knob or MIDI CC. Leaving a mapping out of this list (or
 * using -1) means that control isn't wired to anything.
 *
 * Use `EffectChain::SLOT_ENABLE` as `childParamId` to map a knob or MIDI CC
 * to that slot's own "<tag> On" enable parameter instead of one of the
 * child's parameters.
 */
struct ChainMapping {
    int slot;
    int childParamId;
    int knobMapping = -1;
    int midiCCMapping = -1;
};

/** Combines two or more effects into a single BaseEffectModule.
 *
 * Audio is processed serially through the child effects in slot order. Every
 * child's parameters are exposed together in one combined parameter list
 * (tagged with the slot's prefix), so the effect menu, persistent storage,
 * knobs and MIDI CC all work exactly as they do for a single effect - the
 * host and UI code have no idea a chain is involved.
 *
 * Every slot also gets a synthesized Bool parameter, "<tag> On", defaulting
 * to true, at the front of the combined parameter list. Turning one off
 * crossfades that slot out of the chain over a fraction of a second and then
 * stops processing it (see effect_chain.cpp), so it's just as much a "real"
 * parameter as anything a child declares: it shows up as a checkbox in the
 * menu, is saved with presets, and can be put on a knob or MIDI CC via
 * `ChainMapping`'s `SLOT_ENABLE` sentinel.
 *
 * One child, `primarySlot`, is the sole owner of the pedal's single-owner
 * resources: it is the only child whose LED brightness and alternate
 * footswitch events are forwarded (broadcasting a footswitch press to every
 * child would, for example, make a Looper child start recording and a Delay
 * child flip its shift layer on the same press). `primarySlot` defaults to
 * 0, matching the expected use of putting the modulation/tempo effect first
 * (e.g. Tremolo -> Reverb). The chain's own knob-shift bank (below) takes
 * over the "held for 1 second" gesture when it's in use, so `primarySlot`'s
 * own `AlternateFootswitchHeldFor1Second` is then never reached.
 *
 * `footswitchTogglesSlots` repurposes the alternate footswitch: instead of
 * forwarding to `primarySlot` (tap tempo, shift layers, ...), a press
 * toggles the "On" state of every listed slot together, as a group, and tap
 * tempo is given up (`AlternateFootswitchForTempo()` returns false). Use
 * this when the pedal's second footswitch is more useful as a bypass for
 * part of the chain than as that child's own alternate function - e.g.
 * toggling just a tremolo, or a tremolo and delay together, ahead of a
 * reverb that stays always-on.
 *
 * A chain has only 6 physical knobs to go around, which runs out fast once
 * two or three children are combined. Giving any `ChainMapping` a
 * `knobMapping` of `kShiftKnobOffset` (6) or higher puts that parameter on a
 * second bank, reachable by the same 6 knobs after holding the alternate
 * footswitch for a second: knob N normally reads whatever's mapped to knob
 * N, but while shifted it reads whatever's mapped to knob `N +
 * kShiftKnobOffset` instead (so "knob 7" through "knob 12" in mapping terms
 * are physical knobs 1-6, shifted). Holding again returns to the normal
 * bank. The shift bank only exists if some `ChainMapping` actually uses a
 * `knobMapping` of 6 or higher; otherwise (6 or fewer knobs, all in 0-5)
 * holding the footswitch keeps today's behavior of forwarding to
 * `primarySlot` instead (or is a no-op, under `footswitchTogglesSlots`).
 *
 * Not every effect module is a good fit for chaining - see the constraints
 * called out in the file comment for effect_chain.cpp.
 */
class EffectChain : public BaseEffectModule {
  public:
    /** Sentinel for `ChainMapping::childParamId`: map the knob/CC to the
     * slot's own enable parameter instead of one of the child's parameters.
     */
    static constexpr int SLOT_ENABLE = -1;

    /** `ChainMapping::knobMapping` values at or above this select the
     * "shifted" bank - see the class comment.
     */
    static constexpr int kShiftKnobOffset = 6;

    EffectChain(const char *name, std::initializer_list<ChainSlot> slots, std::initializer_list<ChainMapping> mappings,
                int primarySlot = 0, std::initializer_list<int> footswitchTogglesSlots = {});
    ~EffectChain() override;

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void SetEnabled(bool isEnabled) override;
    void SetTempo(uint32_t bpm) override;
    void UpdateUI(float elapsedTime) override;
    void DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                bool isEditing) override;
    float GetBrightnessForLED(int led_id) const override;
    int GetMappedParameterIDForKnob(int knob_id) const override;
    void OnNoteOn(float notenumber, float velocity) override;
    void OnNoteOff(float notenumber, float velocity) override;
    bool AlternateFootswitchForTempo() const override;
    void AlternateFootswitchPressed() override;
    void AlternateFootswitchReleased() override;
    void AlternateFootswitchHeldFor1Second() override;

  protected:
    void ParameterChanged(int parameter_id) override;

  private:
    /** Copies every child's current parameter value onto the chain's own
     * m_params, without going through SetParameterRaw (so this never
     * re-enters ParameterChanged). Safe to call from the audio ISR - it's
     * just loads and stores, no allocation. Used after operations that make
     * children mutate their own parameters out from under the chain
     * (SetTempo, the alternate footswitch callbacks, note on/off). Slot
     * enable parameters aren't touched - they belong to the chain, not a
     * child.
     */
    void PullParametersFromChildren();

    /** Starts (or reverses, if already mid-fade) the crossfade for a slot
     * toward the given target state.
     */
    void StartSlotFade(int slot, bool enabling);

    /** Advances one slot's crossfade position by one sample and returns the
     * new position (0 = fully bypassed/dry, 1 = fully wet), also caching it
     * in m_slotFadePos for the const GetBrightnessForLED to read.
     */
    float AdvanceSlotFade(int slot, bool enabled);

    ChainSlot *m_slots;
    int m_slotCount;
    int m_primarySlot;

    int *m_footswitchToggleSlots;
    int m_footswitchToggleSlotCount;

    bool m_hasShiftBank;     // true if any mapping used a knob in the shifted bank
    bool m_shiftModeActive;  // true while the shifted bank is selected

    int m_fadeTimeSamples; // ~0.1s of samples at Init()'s sample rate, floor of 1

    daisysp::CrossFade *m_slotFadeLeft;  // one per slot
    daisysp::CrossFade *m_slotFadeRight; // one per slot; unused in mono
    bool *m_slotFading;
    bool *m_slotFadingForward; // true = fading toward enabled/wet
    int *m_slotSamplesTilFadeComplete;
    float *m_slotFadePos; // last computed fade position per slot, for LED readback

    ParameterMetaData *m_combinedMetaData; // owned; m_paramMetaData points here
    char **m_combinedNames;                // owned; one name string per parameter
    uint8_t *m_paramSlot;                  // child parameter array index -> slot index
    uint16_t *m_paramChildId;              // child parameter array index -> parameter id within that child
};

} // namespace bkshepherd
#endif
#endif
