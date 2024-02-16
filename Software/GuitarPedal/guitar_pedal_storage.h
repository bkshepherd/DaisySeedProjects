#pragma once
#ifndef GUITAR_PEDAL_STORAGE_H
#define GUITAR_PEDAL_STORAGE_H

// Peristant Storage Settings
#define SETTINGS_FILE_FORMAT_VERSION 2


// Absolute maximum on current system, arbitrarily limiting this to 64KB
#define SETTINGS_ABSOLUTE_MAX_PARAM_COUNT 16000
#define ERR_VALUE_MAX 0xffffffff
// Save System Variables
struct Settings
{
    int fileFormatVersion;
    int globalActiveEffectID;
    bool globalMidiEnabled;
    bool globalMidiThrough;
    int globalMidiChannel;
    bool globalRelayBypassEnabled;
    bool globalSplitMonoInputToStereo;
    uint32_t *globalEffectsSettings;     // Set aside a block of memory for individual effect params

    bool operator==(const Settings &rhs)
    {
        if (fileFormatVersion != rhs.fileFormatVersion
            || globalActiveEffectID != rhs.globalActiveEffectID
            || globalMidiEnabled != rhs.globalMidiEnabled
            || globalMidiThrough != rhs.globalMidiThrough
            || globalMidiChannel != rhs.globalMidiChannel
            || globalRelayBypassEnabled != rhs.globalRelayBypassEnabled
            || globalSplitMonoInputToStereo != rhs.globalSplitMonoInputToStereo)
        {
            return false;
        }

        for (uint32_t i = 0; i < globalEffectsSettings[0]; i++)
        {
            if (globalEffectsSettings[i] != rhs.globalEffectsSettings[i])
            {
                return false;
            }
        }

        return true;
    }
    
    bool operator!=(const Settings &rhs)
    {
        return !operator==(rhs);
    }
};

void InitPersistantStorage();
void LoadEffectSettingsFromPersistantStorage();
void SaveEffectSettingsToPersitantStorageForEffectID(int effectID, uint32_t presetID);
uint32_t GetSettingsParameterValueForEffect(int effectID, int paramID);
void SetSettingsParameterValueForEffect(int effectID, int paramID, uint32_t paramValue, uint32_t startIdx);
void LoadPresetFromPersistentStorage(uint32_t effectID, uint32_t presetID);
void FactoryReset(void* context);

#endif
