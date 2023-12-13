#pragma once
#ifndef GUITAR_PEDAL_STORAGE_H
#define GUITAR_PEDAL_STORAGE_H

// Peristant Storage Settings
#define SETTINGS_FILE_FORMAT_VERSION 3
#define SETTINGS_MAX_EFFECT_COUNT 8
#define SETTINGS_MAX_EFFECT_PARAM_COUNT 20

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
    uint8_t globalEffectsSettings[SETTINGS_MAX_EFFECT_COUNT * SETTINGS_MAX_EFFECT_PARAM_COUNT];     // Set aside a block of memory for individual effect params

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

        for (int i = 0; i < SETTINGS_MAX_EFFECT_COUNT * SETTINGS_MAX_EFFECT_PARAM_COUNT; i++)
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
void SaveEffectSettingsToPersitantStorageForEffectID(int effectID);
uint8_t GetSettingsParameterValueForEffect(int effectID, int paramID);
void SetSettingsParameterValueForEffect(int effectID, int paramID, uint8_t paramValue);

#endif
