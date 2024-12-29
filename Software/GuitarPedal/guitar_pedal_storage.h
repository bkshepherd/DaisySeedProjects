#pragma once
#ifndef GUITAR_PEDAL_STORAGE_H
#define GUITAR_PEDAL_STORAGE_H

// Peristant Storage Settings
#define SETTINGS_FILE_FORMAT_VERSION 4

// Arbitrarily limiting this to 4KB of stored presets since this sits in DTCMRAM which is limited to 128KB.
// TODO: In the future it would be better if this worked with the QSPI directly instead of using
// the PersistentStorage class as an abstraction since that only lets you store a fixed struct size.
// then it would be possible to not have to pre-allocate a fixed size in DTCMRAM for the preset save data.
#define SETTINGS_ABSOLUTE_MAX_PARAM_COUNT 1024
#define ERR_VALUE_MAX 0xffffffff

// Save System Variables
struct Settings {
    int fileFormatVersion;
    int globalActiveEffectID;
    bool globalMidiEnabled;
    bool globalMidiThrough;
    int globalMidiChannel;
    bool globalRelayBypassEnabled;
    bool globalSplitMonoInputToStereo;

    // Set aside a block of memory for individual effect params.
    // Please note this MUST be a fixed amount of memory in the struct and cannot be a pointer to dynamic memory!
    // If you try to use a pointer it will only save the pointer address to QSPI storage and not any of the contents
    // of that dynamic memory.  This is a limitation of the way the PersistantStorage helper class works.
    uint32_t globalEffectsSettings[SETTINGS_ABSOLUTE_MAX_PARAM_COUNT];

    bool operator==(const Settings &rhs) {
        if (fileFormatVersion != rhs.fileFormatVersion || globalActiveEffectID != rhs.globalActiveEffectID ||
            globalMidiEnabled != rhs.globalMidiEnabled || globalMidiThrough != rhs.globalMidiThrough ||
            globalMidiChannel != rhs.globalMidiChannel || globalRelayBypassEnabled != rhs.globalRelayBypassEnabled ||
            globalSplitMonoInputToStereo != rhs.globalSplitMonoInputToStereo) {
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

#endif
