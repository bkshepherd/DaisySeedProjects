#pragma once
#ifndef GUITAR_PEDAL_STORAGE_H
#define GUITAR_PEDAL_STORAGE_H

// Persistent Storage Settings
// Bump this whenever the shape of `Settings` below changes (fields added,
// removed, or reordered before `globalEffectsSettings`) - it's the only
// thing that catches a layout mismatch against whatever's already on flash,
// since PersistentStorage reads the raw bytes straight into this struct.
// A mismatch forces a one-time RestoreDefaults() (see InitPersistantStorage),
// which wipes all saved effect settings/presets, not just the new field.
#define SETTINGS_FILE_FORMAT_VERSION 11

// Arbitrarily limiting this to 4KB of stored presets since this sits in DTCMRAM which is limited to 128KB.
// TODO: In the future it would be better if this worked with the QSPI directly instead of using
// the PersistentStorage class as an abstraction since that only lets you store a fixed struct size.
// then it would be possible to not have to pre-allocate a fixed size in DTCMRAM for the preset save data.
#define SETTINGS_ABSOLUTE_MAX_PARAM_COUNT 1024
#define ERR_VALUE_MAX 0xffffffff

// Save System Variables
struct Settings {
    int fileFormatVersion;
    uint32_t globalEffectsLayoutHash;
    int globalActiveEffectID;
    bool globalMidiEnabled;
    bool globalMidiThrough;
    int globalMidiChannel;
    bool globalRelayBypassEnabled;
    bool globalSplitMonoInputToStereo;
    bool globalEffectOn; // Pedal's on/off (bypass) state, restored on boot.

    // Set aside a block of memory for individual effect params.
    // Please note this MUST be a fixed amount of memory in the struct and cannot be a pointer to dynamic memory!
    // If you try to use a pointer it will only save the pointer address to QSPI storage and not any of the contents
    // of that dynamic memory.  This is a limitation of the way the PersistantStorage helper class works.
    uint32_t globalEffectsSettings[SETTINGS_ABSOLUTE_MAX_PARAM_COUNT];

    bool operator==(const Settings &rhs) {
        if (fileFormatVersion != rhs.fileFormatVersion || globalEffectsLayoutHash != rhs.globalEffectsLayoutHash ||
            globalActiveEffectID != rhs.globalActiveEffectID ||
            globalMidiEnabled != rhs.globalMidiEnabled || globalMidiThrough != rhs.globalMidiThrough ||
            globalMidiChannel != rhs.globalMidiChannel || globalRelayBypassEnabled != rhs.globalRelayBypassEnabled ||
            globalSplitMonoInputToStereo != rhs.globalSplitMonoInputToStereo || globalEffectOn != rhs.globalEffectOn) {
            return false;
        }

        for (uint32_t i = 0; i < SETTINGS_ABSOLUTE_MAX_PARAM_COUNT; i++) {
            if (globalEffectsSettings[i] != rhs.globalEffectsSettings[i]) {
                return false;
            }
        }

        return true;
    }

    bool operator!=(const Settings &rhs) { return !operator==(rhs); }
};

void InitPersistantStorage();
void LoadEffectSettingsFromPersistantStorage();
void SaveEffectSettingsToPersitantStorageForEffectID(int effectID, uint32_t presetID);
void SetSettingsParameterValueForEffect(int effectID, int paramID, uint32_t paramValue, uint32_t startIdx);
void LoadPresetFromPersistentStorage(uint32_t effectID, uint32_t presetID);
void FactoryReset(void *context);
void RebootToBootloader(void *context);

#endif
