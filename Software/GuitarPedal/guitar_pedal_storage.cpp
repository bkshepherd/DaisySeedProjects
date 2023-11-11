#include "guitar_pedal_storage.h"
#include "Effect-Modules/base_effect_module.h"

using namespace bkshepherd;

extern PersistentStorage<Settings> storage;
extern int availableEffectsCount;
extern BaseEffectModule **availableEffects;
extern int activeEffectID;
extern BaseEffectModule *activeEffect;

void InitPersistantStorage()
{
    Settings defaultSettings;
    defaultSettings.fileFormatVersion = SETTINGS_FILE_FORMAT_VERSION;
    defaultSettings.globalActiveEffectID = 0;
    defaultSettings.globalMidiEnabled = true;
    defaultSettings.globalMidiChannel = 1;
    defaultSettings.globalMidiThrough = true;
    defaultSettings.globalRelayBypassEnabled = false;
    defaultSettings.globalSplitMonoInputToStereo = true;

    // All Effect Params in the settings sound be zero'd
    for (int i = 0; i < SETTINGS_MAX_EFFECT_COUNT * SETTINGS_MAX_EFFECT_PARAM_COUNT; i++)
    {
        defaultSettings.globalEffectsSettings[i] = 0;
    }

    // Override any defaults with effect specific default settings
    for (int effectID = 0; effectID < availableEffectsCount; effectID++)
    {
        int paramCount = availableEffects[effectID]->GetParameterCount();

        for (int paramID = 0;  paramID < paramCount; paramID++)
        {
            defaultSettings.globalEffectsSettings[(effectID * SETTINGS_MAX_EFFECT_PARAM_COUNT) + paramID] = availableEffects[effectID]->GetParameter(paramID);
        }
    } 
    
    storage.Init(defaultSettings);
}

void LoadEffectSettingsFromPersistantStorage()
{
    // Load Effect Parameters based on values from Persistant Storage
    for (int effectID = 0; effectID < availableEffectsCount; effectID++)
    {
        int paramCount = availableEffects[effectID]->GetParameterCount();

        for (int paramID = 0; paramID < paramCount; paramID++)
        {
            availableEffects[effectID]->SetParameter(paramID, GetSettingsParameterValueForEffect(effectID, paramID));
        }
    }
}

void SaveEffectSettingsToPersitantStorageForEffectID(int effectID)
{
    // Save Effect Parameters to Persistant Storage based on values from the specified active effect
    if (effectID >= 0 && effectID < availableEffectsCount)
    {
        int paramCount = availableEffects[effectID]->GetParameterCount();

        for (int paramID = 0; paramID < paramCount; paramID++)
        {
            SetSettingsParameterValueForEffect(effectID, paramID, availableEffects[effectID]->GetParameter(paramID));
        }
    }
}

// Helpful Function for getting a parameter value for an effect from the Persistant Storage
uint8_t GetSettingsParameterValueForEffect(int effectID, int paramID)
{
    // Make sure the effect and param id are within valid ranges for the settings.
    if (effectID < 0 || effectID > SETTINGS_MAX_EFFECT_COUNT - 1 || paramID < 0 || paramID > SETTINGS_MAX_EFFECT_PARAM_COUNT - 1)
    {
        return 0;
    }

    // Get a handle to the persitance storage settings
    Settings &settings = storage.GetSettings();
    return settings.globalEffectsSettings[(effectID * SETTINGS_MAX_EFFECT_PARAM_COUNT) + paramID];
}

// Helpful Function for setting a parameter value for an effect from the Persistant Storage
void SetSettingsParameterValueForEffect(int effectID, int paramID, uint8_t paramValue)
{
    // Make sure the effect and param id are within valid ranges for the settings.
    if (effectID < 0 || effectID > SETTINGS_MAX_EFFECT_COUNT - 1 || paramID < 0 || paramID > SETTINGS_MAX_EFFECT_PARAM_COUNT - 1)
    {
        return;
    }

    // Get a handle to the persitance storage settings
    Settings &settings = storage.GetSettings();
    settings.globalEffectsSettings[(effectID * SETTINGS_MAX_EFFECT_PARAM_COUNT) + paramID] = paramValue;
}
