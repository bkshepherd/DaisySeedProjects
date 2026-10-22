#include "effect_chain.h"
#include <cstdio>
#include <cstring>

using namespace bkshepherd;
using daisysp::CrossFade;

// EffectChain lets two or more effects be wired into a single BaseEffectModule
// and run serially, without any changes to the child modules or to the
// guitar_pedal / UI host code. That's possible because the host and UI only
// ever reach an effect through BaseEffectModule's public API, and this class
// forwards that API to its children: it synthesizes a combined
// ParameterMetaData array from the children's own metadata getters (see the
// constructor), and forwards every value change to the right child via the
// ParameterChanged hook (see below).
//
// Every slot also gets its own synthesized "<tag> On" Bool parameter (ids
// 0..slotCount-1, ahead of the children's own parameters), so each slot can
// be bypassed independently - from the menu, a knob, MIDI CC, or the
// alternate footswitch's toggle mode (see AlternateFootswitchPressed below).
// Turning a slot off crossfades it out over m_fadeTimeSamples and then stops
// calling its Process methods entirely: SetEnabled()/IsEnabled() alone
// don't silence a child (almost nothing in Effect-Modules checks
// IsEnabled() from inside Process), so the chain has to do the muting
// itself. This mirrors guitar_pedal.cpp's own global bypass crossfade, and
// has the same consequence: a delay or reverb tail is cut off at the end of
// the fade rather than being allowed to ring out. That tradeoff is what
// makes skipping the child's Process worthwhile - e.g. Dattorro's reverb
// engine alone runs 13 interpolating delay lines every sample it's called.
//
// Not every effect module is a good fit for chaining:
//
//  - Any module that keeps its DSP buffers in a DSY_SDRAM_BSS file-scope
//    global (most delays and reverbs) may only appear once across the whole
//    effectList, chains included, because every instance of that module type
//    aliases the same buffer. If you chain one, comment out its standalone
//    effectList entry. CloudSeedModule is a sharper case: its pool_index
//    bump allocator is sized for exactly one instance and is never reset, so
//    a second instance gets nullptr back from Init() and crashes. It's CPU
//    heavy enough that it should just stay standalone.
//  - A chain always draws its own name on the home screen, so children with
//    a custom DrawUI (Tuner, Looper, the EQs, AutoPan) lose their display.
//  - GetMappedParameterIDForKnob is resolved against the chain's combined
//    metadata, so a child's own "shift layer" knob bank (Delay, TapeDelay)
//    isn't reachable inside a chain - use the chain's own shift bank
//    instead (see the class comment in effect_chain.h).
//
// These aren't handled specially - an unsuitable combination is expected to
// just misbehave or crash rather than being defended against here.

namespace {

// Generous headroom for "<tag> <child parameter name>" and "<tag> On".
constexpr int kMaxCombinedNameLength = 32;

// The most characters a tagged parameter name may have: the menu (UI/fit_text_menu.h) draws a name in the biggest font that fits and cuts it with a "~" beyond the 22 characters that
// fit in the smallest (5 pixel wide) font, so a name up to this long is drawn in full, if small. (libDaisy's own menu draws nothing for a line wider than the display, which at the
// default 11 pixel font was anything over 10 characters: the limit used to be 10 here, and a longer tagged name fell back to the child's own untagged name.)
constexpr int kMaxSafeDisplayNameLength = 22;

// How long a slot takes to crossfade in/out when toggled, matching the
// pedal's own global bypass fade (see guitar_pedal.cpp).
constexpr float kSlotFadeTimeInSeconds = 0.1f;

} // namespace

EffectChain::EffectChain(const char *name, std::initializer_list<ChainSlot> slots, std::initializer_list<ChainMapping> mappings,
                          int primarySlot, std::initializer_list<int> footswitchTogglesSlots)
    : BaseEffectModule(), m_slotCount(static_cast<int>(slots.size())), m_switchesParam(m_slotCount >= 2 ? m_slotCount : -1),
      m_childParamBase(m_slotCount + (m_slotCount >= 2 ? 1 : 0)), m_primarySlot(primarySlot), m_hasShiftBank(false),
      m_shiftModeActive(false), m_fadeTimeSamples(1), m_combinedMetaData(nullptr), m_combinedNames(nullptr),
      m_paramSlot(nullptr), m_paramChildId(nullptr) {
    m_slots = new ChainSlot[m_slotCount];
    int slotIdx = 0;
    for (const auto &slot : slots) {
        m_slots[slotIdx++] = slot;
    }

    m_footswitchToggleSlots = new int[footswitchTogglesSlots.size()];
    m_footswitchToggleSlotCount = 0;
    for (int slot : footswitchTogglesSlots) {
        if (slot < 0 || slot >= m_slotCount) {
            continue;
        }
        m_footswitchToggleSlots[m_footswitchToggleSlotCount++] = slot;
    }

    m_slotFadeLeft = new CrossFade[m_slotCount];
    m_slotFadeRight = new CrossFade[m_slotCount];
    m_slotFading = new bool[m_slotCount];
    m_slotFadingForward = new bool[m_slotCount];
    m_slotSamplesTilFadeComplete = new int[m_slotCount];
    m_slotFadePos = new float[m_slotCount];
    for (int s = 0; s < m_slotCount; s++) {
        m_slotFading[s] = false;
        m_slotFadingForward[s] = true;
        m_slotSamplesTilFadeComplete[s] = 0;
        m_slotFadePos[s] = 1.0f;
    }

    // Work out where each child's parameters start in the combined list,
    // after the m_slotCount enable parameters and Switches (m_childParamBase).
    int *slotParamOffset = new int[m_slotCount];
    int totalChildParamCount = 0;
    for (int s = 0; s < m_slotCount; s++) {
        slotParamOffset[s] = totalChildParamCount;
        totalChildParamCount += m_slots[s].effect->GetParameterCount();
    }
    const int totalParamCount = m_childParamBase + totalChildParamCount;

    m_combinedMetaData = new ParameterMetaData[totalParamCount];
    m_combinedNames = new char *[totalParamCount];
    m_paramSlot = new uint8_t[totalChildParamCount];
    m_paramChildId = new uint16_t[totalChildParamCount];

    // One enable parameter per slot, defaulting to on.
    for (int s = 0; s < m_slotCount; s++) {
        char *nameBuf = new char[kMaxCombinedNameLength];
        snprintf(nameBuf, kMaxCombinedNameLength, "%s On", m_slots[s].tag);
        m_combinedNames[s] = nameBuf;

        ParameterMetaData &md = m_combinedMetaData[s];
        md.name = nameBuf;
        md.valueType = ParameterValueType::Bool;
        md.valueCurve = ParameterValueCurve::Linear;
        md.valueBinCount = 0;
        md.valueBinNames = nullptr;
        md.defaultValue.uint_value = 1;
        md.knobMapping = -1;
        md.midiCCMapping = -1;
        md.minValue = 0;
        md.maxValue = 1;
        md.fineStepSize = 0.01f;
    }

    // "Switches": which two slots the footswitches switch on and off instead of working as usual (Util/chain_switches.h; the pedal reads it
    // through GetFootswitchSlots). A chain of two only offers Normal and 1+2.
    if (m_switchesParam >= 0) {
        char *nameBuf = new char[kMaxCombinedNameLength];
        snprintf(nameBuf, kMaxCombinedNameLength, "Switches");
        m_combinedNames[m_switchesParam] = nameBuf;

        ParameterMetaData &md = m_combinedMetaData[m_switchesParam];
        md.name = nameBuf;
        md.valueType = ParameterValueType::Binned;
        md.valueCurve = ParameterValueCurve::Linear;
        md.valueBinCount = m_slotCount >= 3 ? chainswitches::MODE_COUNT : chainswitches::SLOTS_1_2 + 1;
        md.valueBinNames = chainswitches::kModeNames;
        md.defaultValue.uint_value = chainswitches::NORMAL;
        md.knobMapping = -1;
        md.midiCCMapping = -1;
        md.minValue = 0;
        md.maxValue = 1;
        md.fineStepSize = 0.01f;
    }

    // Rebuild a ParameterMetaData entry for every child parameter from that
    // child's own public getters. This has to go through the public API:
    // m_paramMetaData is protected, so this chain can't reach into a sibling
    // instance's copy of it directly.
    for (int s = 0; s < m_slotCount; s++) {
        BaseEffectModule *child = m_slots[s].effect;
        const int childParamCount = child->GetParameterCount();

        for (int p = 0; p < childParamCount; p++) {
            const int childArrayIdx = slotParamOffset[s] + p;
            const int id = m_childParamBase + childArrayIdx;

            char *nameBuf = new char[kMaxCombinedNameLength];
            snprintf(nameBuf, kMaxCombinedNameLength, "%s %s", m_slots[s].tag, child->GetParameterName(p));
            if (strlen(nameBuf) > kMaxSafeDisplayNameLength) {
                // Tagged name would render blank (see kMaxSafeDisplayNameLength
                // above) - fall back to the child's own name, untagged, rather
                // than show nothing.
                snprintf(nameBuf, kMaxCombinedNameLength, "%s", child->GetParameterName(p));
            }
            m_combinedNames[id] = nameBuf;

            ParameterMetaData &md = m_combinedMetaData[id];
            md.name = nameBuf;
            md.valueType = child->GetParameterType(p);
            md.valueCurve = child->GetParameterValueCurve(p);
            md.valueBinCount = child->GetParameterBinCount(p);
            md.valueBinNames = child->GetParameterBinNames(p);
            // Raw default value bits are used for every type, Float included -
            // it's the same union storage SetParameterAsFloat writes into, so
            // this is bit exact and skips a float round-trip.
            md.defaultValue.uint_value = child->GetParameterDefaultValueRaw(p);
            md.knobMapping = -1;
            md.midiCCMapping = -1;
            md.minValue = child->GetParameterMin(p);
            md.maxValue = child->GetParameterMax(p);
            md.fineStepSize = child->GetParameterFineStepSize(p);
            md.unit = child->GetParameterUnit(p);
            md.decimals = child->GetParameterDecimals(p);
            md.showSign = child->GetParameterShowSign(p);
            md.displayFn = child->GetParameterDisplayFunction(p);

            m_paramSlot[childArrayIdx] = static_cast<uint8_t>(s);
            m_paramChildId[childArrayIdx] = static_cast<uint16_t>(p);
        }
    }

    // Only the mappings explicitly listed get wired to a knob or MIDI CC, so
    // two children (or a child and a slot's own enable parameter) can't
    // collide on the same knob by accident.
    for (const auto &mapping : mappings) {
        if (mapping.slot < 0 || mapping.slot >= m_slotCount) {
            continue;
        }

        int id;
        if (mapping.childParamId == EffectChain::SLOT_ENABLE) {
            id = mapping.slot;
        } else {
            BaseEffectModule *child = m_slots[mapping.slot].effect;
            if (mapping.childParamId < 0 || mapping.childParamId >= (int)child->GetParameterCount()) {
                continue;
            }
            id = m_childParamBase + slotParamOffset[mapping.slot] + mapping.childParamId;
        }

        m_combinedMetaData[id].knobMapping = mapping.knobMapping;
        m_combinedMetaData[id].midiCCMapping = mapping.midiCCMapping;

        if (mapping.knobMapping >= kShiftKnobOffset) {
            m_hasShiftBank = true;
        }
    }

    delete[] slotParamOffset;

    m_name = name;
    m_paramMetaData = m_combinedMetaData;

    // Seeds m_params from m_combinedMetaData's default values, which were
    // just captured from the children's own current (default) values above,
    // so the chain and its children agree from the start. This never calls
    // ParameterChanged, which is why the loop above wired up child/param ids
    // first - by the time anything can call SetParameterRaw on this chain,
    // the mapping tables it needs already exist.
    InitParams(totalParamCount);
}

EffectChain::~EffectChain() {
    for (int i = 0; i < m_paramCount; i++) {
        delete[] m_combinedNames[i];
    }
    delete[] m_combinedNames;
    delete[] m_combinedMetaData;
    delete[] m_paramSlot;
    delete[] m_paramChildId;

    delete[] m_slotFadeLeft;
    delete[] m_slotFadeRight;
    delete[] m_slotFading;
    delete[] m_slotFadingForward;
    delete[] m_slotSamplesTilFadeComplete;
    delete[] m_slotFadePos;
    delete[] m_footswitchToggleSlots;

    for (int s = 0; s < m_slotCount; s++) {
        if (m_slots[s].ownsEffect) {
            delete m_slots[s].effect;
        }
    }
    delete[] m_slots;
}

void EffectChain::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    m_fadeTimeSamples = static_cast<int>(kSlotFadeTimeInSeconds * sample_rate);
    if (m_fadeTimeSamples < 1) {
        m_fadeTimeSamples = 1;
    }

    for (int s = 0; s < m_slotCount; s++) {
        m_slots[s].effect->Init(sample_rate);
        m_slotFadeLeft[s].Init();
        m_slotFadeRight[s].Init();
        m_slotFading[s] = false;
        m_slotFadePos[s] = (m_params[s] != 0) ? 1.0f : 0.0f;
        m_slots[s].effect->SetEnabled(IsEnabled() && (m_params[s] != 0));
    }

    // Re-push every child parameter now that the children are initialized.
    // Right now this is a no-op - the chain's params were captured from the
    // children's own defaults at construction, and the host always calls
    // Init() before restoring settings from persistent storage - but it
    // keeps "child params match the chain's" true unconditionally rather
    // than relying on that ordering. Slot enable parameters and Switches (ids
    // below m_childParamBase) have no child to push to.
    for (int i = m_childParamBase; i < m_paramCount; i++) {
        const int childArrayIdx = i - m_childParamBase;
        m_slots[m_paramSlot[childArrayIdx]].effect->SetParameterRaw(m_paramChildId[childArrayIdx], m_params[i]);
    }
}

void EffectChain::ProcessMono(float in) {
    float sample = in;

    for (int s = 0; s < m_slotCount; s++) {
        const bool enabled = m_params[s] != 0;

        if (!enabled && !m_slotFading[s]) {
            // Fully bypassed and settled - skip the child's DSP entirely.
            continue;
        }

        float dry = sample;
        m_slots[s].effect->ProcessMono(dry);
        float wet = m_slots[s].effect->GetAudioLeft();

        m_slotFadeLeft[s].SetPos(AdvanceSlotFade(s, enabled));
        sample = m_slotFadeLeft[s].Process(dry, wet);
    }

    m_audioLeft = sample;
    m_audioRight = sample;
}

void EffectChain::ProcessStereo(float inL, float inR) {
    float left = inL;
    float right = inR;

    for (int s = 0; s < m_slotCount; s++) {
        const bool enabled = m_params[s] != 0;

        if (!enabled && !m_slotFading[s]) {
            continue;
        }

        float dryLeft = left;
        float dryRight = right;
        m_slots[s].effect->ProcessStereo(dryLeft, dryRight);
        float wetLeft = m_slots[s].effect->GetAudioLeft();
        float wetRight = m_slots[s].effect->GetAudioRight();

        const float pos = AdvanceSlotFade(s, enabled);
        m_slotFadeLeft[s].SetPos(pos);
        m_slotFadeRight[s].SetPos(pos);
        left = m_slotFadeLeft[s].Process(dryLeft, wetLeft);
        right = m_slotFadeRight[s].Process(dryRight, wetRight);
    }

    m_audioLeft = left;
    m_audioRight = right;
}

void EffectChain::SetEnabled(bool isEnabled) {
    BaseEffectModule::SetEnabled(isEnabled);

    for (int s = 0; s < m_slotCount; s++) {
        m_slots[s].effect->SetEnabled(isEnabled && (m_params[s] != 0));
    }
}

void EffectChain::StartSlotFade(int slot, bool enabling) {
    m_slotFading[slot] = true;
    m_slotFadingForward[slot] = enabling;
    m_slotSamplesTilFadeComplete[slot] = m_fadeTimeSamples;

    // Drive the child's own SetEnabled too, even though this chain no longer
    // relies on it to gate Process: TapeDelayModule resets its internal state
    // on a disable->enable edge, and that should still happen per slot.
    m_slots[slot].effect->SetEnabled(IsEnabled() && enabling);
}

float EffectChain::AdvanceSlotFade(int slot, bool enabled) {
    if (!m_slotFading[slot]) {
        m_slotFadePos[slot] = enabled ? 1.0f : 0.0f;
        return m_slotFadePos[slot];
    }

    float factor = (float)m_slotSamplesTilFadeComplete[slot] / (float)m_fadeTimeSamples;
    if (m_slotFadingForward[slot]) {
        factor = 1.0f - factor;
    }

    m_slotSamplesTilFadeComplete[slot] -= 1;
    if (m_slotSamplesTilFadeComplete[slot] < 0) {
        m_slotFading[slot] = false;
    }

    m_slotFadePos[slot] = factor;
    return factor;
}

void EffectChain::SetTempo(uint32_t bpm) {
    for (int s = 0; s < m_slotCount; s++) {
        m_slots[s].effect->SetTempo(bpm);
    }

    // SetTempo makes tempo-synced children rewrite their own parameters
    // (e.g. HarmonicTremoloModule::SetTempo calls SetParameterAsMagnitude on
    // itself), which the chain otherwise has no way to see.
    PullParametersFromChildren();
}

void EffectChain::UpdateUI(float elapsedTime) {
    for (int s = 0; s < m_slotCount; s++) {
        m_slots[s].effect->UpdateUI(elapsedTime);
    }
}

float EffectChain::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        if (m_footswitchToggleSlotCount > 0) {
            // In toggle mode LED 2 tracks the fade level of the toggled
            // group instead of a child's own LED opinion.
            value *= m_slotFadePos[m_footswitchToggleSlots[0]];
        } else {
            // LED 1 follows whichever child is the "primary" one (e.g. the
            // tremolo in Trem+Verb), rather than every child at once.
            value *= m_slots[m_primarySlot].effect->GetBrightnessForLED(1);
        }
    }

    return value;
}

int EffectChain::GetMappedParameterIDForKnob(int knob_id) const {
    if (m_shiftModeActive) {
        return BaseEffectModule::GetMappedParameterIDForKnob(knob_id + kShiftKnobOffset);
    }
    return BaseEffectModule::GetMappedParameterIDForKnob(knob_id);
}

void EffectChain::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                          bool isEditing) {
    BaseEffectModule::DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn, isEditing);

    if (m_shiftModeActive) {
        display.SetCursor(boundsToDrawIn.GetRight() - 40, 2);
        display.WriteString("SHIFT", Font_6x8, true);
    }
}

void EffectChain::OnNoteOn(float notenumber, float velocity) {
    for (int s = 0; s < m_slotCount; s++) {
        m_slots[s].effect->OnNoteOn(notenumber, velocity);
    }

    PullParametersFromChildren();
}

void EffectChain::OnNoteOff(float notenumber, float velocity) {
    for (int s = 0; s < m_slotCount; s++) {
        m_slots[s].effect->OnNoteOff(notenumber, velocity);
    }

    PullParametersFromChildren();
}

bool EffectChain::AlternateFootswitchForTempo() const {
    if (m_footswitchToggleSlotCount > 0) {
        // Toggle mode repurposes the alternate footswitch entirely, so tap
        // tempo is given up.
        return false;
    }
    return m_slots[m_primarySlot].effect->AlternateFootswitchForTempo();
}

void EffectChain::AlternateFootswitchPressed() {
    if (m_footswitchToggleSlotCount > 0) {
        // If any toggled slot is currently on, turn the whole group off;
        // otherwise turn the whole group on. Going through SetParameterAsBool
        // (rather than touching m_params/StartSlotFade directly) means this
        // flows through the same ParameterChanged funnel as a menu edit or a
        // MIDI CC, so it's automatically saved, reflected in the menu, and
        // fades exactly like any other enable-parameter change.
        const bool newState = !(m_params[m_footswitchToggleSlots[0]] != 0);
        for (int i = 0; i < m_footswitchToggleSlotCount; i++) {
            SetParameterAsBool(m_footswitchToggleSlots[i], newState);
        }
        return;
    }

    // Only the primary child gets footswitch events - broadcasting a single
    // press to every child would, for example, make a Looper child start
    // recording and a Delay child flip its shift layer at the same time.
    m_slots[m_primarySlot].effect->AlternateFootswitchPressed();
    PullParametersFromChildren();
}

void EffectChain::AlternateFootswitchReleased() {
    if (m_footswitchToggleSlotCount > 0) {
        return;
    }
    m_slots[m_primarySlot].effect->AlternateFootswitchReleased();
    PullParametersFromChildren();
}

void EffectChain::AlternateFootswitchHeldFor1Second() {
    if (m_hasShiftBank) {
        // The shift bank takes over this gesture entirely - primarySlot's own
        // AlternateFootswitchHeldFor1Second is never reached once a chain has
        // enough mappings to need a second knob bank.
        m_shiftModeActive = !m_shiftModeActive;
        return;
    }
    if (m_footswitchToggleSlotCount > 0) {
        return;
    }
    m_slots[m_primarySlot].effect->AlternateFootswitchHeldFor1Second();
    PullParametersFromChildren();
}

// The slots for the footswitches (the "Switches" parameter): a slot is switched through its own "<tag> On" parameter, so a footswitch press is saved, shown in
// the menu and crossfaded like any other change of it (as in the alternate footswitch's toggle mode above).
bool EffectChain::IsSlotOn(int slot) const { return slot >= 0 && slot < m_slotCount && m_params[slot] != 0; }

void EffectChain::SetSlotOn(int slot, bool on) {
    if (slot >= 0 && slot < m_slotCount) {
        SetParameterAsBool(slot, on);
    }
}

float EffectChain::GetSlotLevel(int slot) const { return (slot >= 0 && slot < m_slotCount) ? m_slotFadePos[slot] : 0.0f; }

chainswitches::Assignment EffectChain::GetFootswitchSlots() const {
    return m_switchesParam >= 0 ? chainswitches::Resolve(static_cast<int>(m_params[m_switchesParam]), m_slotCount) : chainswitches::Assignment{};
}

void EffectChain::ParameterChanged(int parameter_id) {
    if (parameter_id < 0 || parameter_id >= m_paramCount) {
        return;
    }

    if (parameter_id < m_slotCount) {
        // A slot's own "<tag> On" parameter - there's no child to forward to,
        // just start (or reverse) that slot's crossfade.
        StartSlotFade(parameter_id, m_params[parameter_id] != 0);
        return;
    }
    if (parameter_id == m_switchesParam) {
        return; // the chain's own setting: read by the pedal through GetFootswitchSlots
    }

    const int childArrayIdx = parameter_id - m_childParamBase;
    BaseEffectModule *child = m_slots[m_paramSlot[childArrayIdx]].effect;
    const int childParamId = m_paramChildId[childArrayIdx];

    child->SetParameterRaw(childParamId, m_params[parameter_id]);

    // Read the value back rather than trusting the one just pushed: some
    // children clamp or otherwise rewrite a parameter from inside their own
    // ParameterChanged (CrusherModule does this for its RATE parameter).
    // SetParameterRaw only calls ParameterChanged when the value actually
    // changes, so if this chain kept its own pushed value instead, any
    // divergence from the child would become permanent - there would be no
    // way to ever push that parameter again.
    m_params[parameter_id] = child->GetParameterRaw(childParamId);
}

void EffectChain::PullParametersFromChildren() {
    for (int i = m_childParamBase; i < m_paramCount; i++) {
        const int childArrayIdx = i - m_childParamBase;
        SetParameterRaw(i, m_slots[m_paramSlot[childArrayIdx]].effect->GetParameterRaw(m_paramChildId[childArrayIdx]));
    }
}
