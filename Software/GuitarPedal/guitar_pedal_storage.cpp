#include "guitar_pedal_storage.h"
#include "Effect-Modules/base_effect_module.h"

using namespace bkshepherd;

extern PersistentStorage<Settings> storage;
extern int availableEffectsCount;
extern BaseEffectModule **availableEffects;
extern int activeEffectID;
extern BaseEffectModule *activeEffect;

uint32_t DSY_SDRAM_BSS globalEffectsSettings[SETTINGS_ABSOLUTE_MAX_PARAM_COUNT];

uint32_t GetDefaultTotalIdxOfGlobalSettingsBlock()
{
	uint32_t tempSize = 0;
	
	for (int effectID = 0; effectID < availableEffectsCount; effectID++)
    {
		int paramCount = availableEffects[effectID]->GetParameterCount();
		// Reserve 2 words for Num of Presets and Num of Parameters
		tempSize += 2 + paramCount;
	}
	return tempSize;
}
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
	defaultSettings.globalEffectsSettings = globalEffectsSettings;
    // All Effect Params in the settings should be zero'd
    for (int i = 0; i < SETTINGS_ABSOLUTE_MAX_PARAM_COUNT; i++)
    {
        defaultSettings.globalEffectsSettings[i] = 0;
    }
	
	uint32_t globalEffectsSettingMemIdx = 0U;
	defaultSettings.globalEffectsSettings[globalEffectsSettingMemIdx] = GetDefaultTotalIdxOfGlobalSettingsBlock();
	++globalEffectsSettingMemIdx;
    // Override any defaults with effect specific default settings
    for (int effectID = 0; effectID < availableEffectsCount; effectID++)
    {
        int paramCount = availableEffects[effectID]->GetParameterCount();
		// Change first word of each effect such that this is total number of presets
		// Default value: 1
		defaultSettings.globalEffectsSettings[globalEffectsSettingMemIdx] = 1U;
		++globalEffectsSettingMemIdx;
		// Next word is going to be the number of parameters
		defaultSettings.globalEffectsSettings[globalEffectsSettingMemIdx] = paramCount;
		++globalEffectsSettingMemIdx;

        for (int paramID = 0;  paramID < paramCount; paramID++)
        {
            defaultSettings.globalEffectsSettings[globalEffectsSettingMemIdx] = availableEffects[effectID]->GetParameterRaw(paramID);

			if(availableEffects[effectID]->GetParameterType(paramID) == ParameterValueType::FloatMagnitude)
			{
				uint32_t tmp;
				float f = defaultSettings.globalEffectsSettings[globalEffectsSettingMemIdx] = availableEffects[effectID]->GetParameterAsFloat(paramID);
				std::memcpy(&tmp, &f, sizeof(float));
				defaultSettings.globalEffectsSettings[globalEffectsSettingMemIdx] = tmp;
			}
			else
			{
				defaultSettings.globalEffectsSettings[globalEffectsSettingMemIdx] = availableEffects[effectID]->GetParameterRaw(paramID);
			}
			++globalEffectsSettingMemIdx;
        }
    } 
    
    storage.Init(defaultSettings);

    Settings &settings = storage.GetSettings();
    
    // If the stored data is not the current version do a factory reset
    if (settings.fileFormatVersion != SETTINGS_FILE_FORMAT_VERSION)
    {
        storage.RestoreDefaults();
    }

    // Make sure the settings active effect is within the proper range.
    if (settings.globalActiveEffectID < 0 || settings.globalActiveEffectID >= availableEffectsCount)
    {
        settings.globalActiveEffectID = 0;
    }
}

uint32_t ShiftSettingsToMatchCurrentParameters(uint32_t prev_params, uint32_t curr_params, uint32_t effectID, uint32_t presetsCount, uint32_t shiftStartIdx, uint32_t currentMaxIdx)
{
	uint32_t newMaxIdx = currentMaxIdx;
	Settings &settings = storage.GetSettings();

	newMaxIdx += presetsCount * (curr_params - prev_params);
	if(newMaxIdx <= SETTINGS_ABSOLUTE_MAX_PARAM_COUNT)
	{
		if(curr_params > prev_params)
		{
			uint32_t diff = newMaxIdx - currentMaxIdx;
			for(uint32_t i = newMaxIdx; i >= shiftStartIdx; --i)
			{
				settings.globalEffectsSettings[i] = settings.globalEffectsSettings[i-diff];
			}
		}
		else
		{
			uint32_t diff = currentMaxIdx - newMaxIdx;
			for(uint32_t i = shiftStartIdx; i < newMaxIdx; ++i)
			{
				settings.globalEffectsSettings[i] = settings.globalEffectsSettings[i + diff];
			}
			// Clear words up until the last maximum
			for(uint32_t i = newMaxIdx; i <= currentMaxIdx; ++i)
			{
				settings.globalEffectsSettings[i] = 0;
			}
		}
	}
	else
	{
		newMaxIdx = ERR_VALUE_MAX;
	}
	
	return newMaxIdx;
}


uint32_t ShiftSettingsToAddNewPreset(int effectID, uint32_t params, uint32_t shiftStartIdx, uint32_t currentMaxIdx)
{
	uint32_t newMaxIdx = currentMaxIdx;
	Settings &settings = storage.GetSettings();

	newMaxIdx += params;
	if(newMaxIdx <= SETTINGS_ABSOLUTE_MAX_PARAM_COUNT)
	{
		uint32_t diff = newMaxIdx - currentMaxIdx;
		for(uint32_t i = newMaxIdx; i >= shiftStartIdx; --i)
		{
			settings.globalEffectsSettings[i] = settings.globalEffectsSettings[i-diff];
		}
		for(uint32_t i = effectID + 1; i < (uint32_t) availableEffectsCount; ++i)
		{
			uint32_t tmp = availableEffects[i]->GetSettingsArrayStartIdx() + diff;
			availableEffects[i]->SetSettingsArrayStartIdx(tmp);
		}
	}
	else
	{
		newMaxIdx = ERR_VALUE_MAX;
	}
	
	return newMaxIdx;
}

void LoadPresetFromPersistentStorage(uint32_t effectID, uint32_t presetID)
{
	uint32_t presetCount = availableEffects[effectID]->GetPresetCount();
	// Get a handle to the persitance storage settings
    Settings &settings = storage.GetSettings();
    if (effectID >= 0 && effectID < (uint32_t) availableEffectsCount && presetID < presetCount)
    {
		int paramCount = availableEffects[effectID]->GetParameterCount();
		uint32_t startIdx;
		
		startIdx = availableEffects[effectID]->GetSettingsArrayStartIdx() + 2 + (paramCount * presetID);

		for (int paramID = 0; paramID < paramCount; paramID++)
		{
			if(availableEffects[effectID]->GetParameterType(paramID) == ParameterValueType::FloatMagnitude)
			{
				uint32_t tmp = settings.globalEffectsSettings[startIdx + paramID];
				float f;
				std::memcpy(&f, &tmp, sizeof(float));
				availableEffects[effectID]->SetParameterAsFloat(paramID, f);
			}
			else
			{
				availableEffects[effectID]->SetParameterRaw(paramID, settings.globalEffectsSettings[startIdx + paramID]);
			}
		}
    }
}

void LoadEffectSettingsFromPersistantStorage()
{
	uint32_t globalEffectsSettingMemIdx = 0U;
	Settings &settings = storage.GetSettings();
	uint32_t globalEffectsMaxIdx = settings.globalEffectsSettings[globalEffectsSettingMemIdx];
	++globalEffectsSettingMemIdx;
    // Load Preset 0 of each Effect Parameters, based on values from Persistant Storage
    for (int effectID = 0; effectID < availableEffectsCount; effectID++)
    {
		uint32_t presetsCount = settings.globalEffectsSettings[globalEffectsSettingMemIdx];
		availableEffects[effectID]->SetSettingsArrayStartIdx(globalEffectsSettingMemIdx);
		++globalEffectsSettingMemIdx;
        uint32_t paramCount = availableEffects[effectID]->GetParameterCount();
		uint32_t prevParamCount = settings.globalEffectsSettings[globalEffectsSettingMemIdx];
		++globalEffectsSettingMemIdx;	
		// If the two variables are the same, there's no problem, just load value from the global settings copy
		if(paramCount == prevParamCount)
		{
			for (uint32_t paramID = 0; paramID < paramCount; paramID++)
			{
				uint32_t value = settings.globalEffectsSettings[globalEffectsSettingMemIdx];
				if(availableEffects[effectID]->GetParameterType(paramID) == ParameterValueType::FloatMagnitude)
				{
					float tmp;
					std::memcpy(&tmp, &value, sizeof(float));
					availableEffects[effectID]->SetParameterAsFloat(paramID, tmp);
				}
				else
				{
					availableEffects[effectID]->SetParameterRaw(paramID, value);
				}
				
				++globalEffectsSettingMemIdx;
			}
		}
		// If we have added a new parameter since the last time settings were stored, we read values from persitent memory (but assume that a new param was added to the end)
		// This way we preserve all other effects and don't get into a scenario where we accidentally use a large value as the number of presets.
		else if(prevParamCount < paramCount)
		{
			for (uint32_t paramID = 0; paramID < prevParamCount; paramID++)
			{
				uint32_t value = settings.globalEffectsSettings[globalEffectsSettingMemIdx];
				if(availableEffects[effectID]->GetParameterType(paramID) == ParameterValueType::FloatMagnitude)
				{
					float tmp;
					std::memcpy(&tmp, &value, sizeof(float));
					availableEffects[effectID]->SetParameterAsFloat(paramID, tmp);
				}
				else
				{
					availableEffects[effectID]->SetParameterRaw(paramID, value);
				}
				++globalEffectsSettingMemIdx;
			}
			// Shift the array and then add the new values
			globalEffectsMaxIdx = ShiftSettingsToMatchCurrentParameters(prevParamCount, paramCount, effectID, presetsCount, globalEffectsSettingMemIdx, globalEffectsMaxIdx);
			for(uint32_t paramID = prevParamCount; paramID < paramCount; paramID++)
			{
				uint32_t value = availableEffects[effectID]->GetParameterRaw(paramID);
				settings.globalEffectsSettings[globalEffectsSettingMemIdx] =value;
				++globalEffectsSettingMemIdx;
			}
		}
		// Else, if we have removed a parameter since the last time settings were stored, read all the relevant parameters, (again assume that these were correct in the first place)
		// Then shift the effects after this one back by the appropriate amount of words
		else
		{
			for (uint32_t paramID = 0; paramID < paramCount; paramID++)
			{
				uint32_t value = settings.globalEffectsSettings[globalEffectsSettingMemIdx];
				availableEffects[effectID]->SetParameterRaw(paramID, value);
				++globalEffectsSettingMemIdx;
			}
			globalEffectsMaxIdx = ShiftSettingsToMatchCurrentParameters(prevParamCount, paramCount, effectID, presetsCount, globalEffectsSettingMemIdx, globalEffectsMaxIdx);
		}
		availableEffects[effectID]->SetPresetCount(presetsCount);
		/* Assume preset 0 for now */
		availableEffects[effectID]->SetCurrentPreset(0U);
		globalEffectsSettingMemIdx += paramCount * (presetsCount -1U);
    }
}

void SaveEffectSettingsToPersitantStorageForEffectID(int effectID, uint32_t presetID)
{
	bool canWriteNewPreset = true;
	Settings &settings = storage.GetSettings();
	uint32_t globalEffectsMaxIdx = settings.globalEffectsSettings[0U];
    // Save Effect Parameters to Persistant Storage based on values from the specified active effect
    if (effectID >= 0 && effectID < availableEffectsCount)
    {
		int paramCount = availableEffects[effectID]->GetParameterCount();
		uint32_t presetCount = availableEffects[effectID]->GetPresetCount();
		uint32_t startIdx;
		
		startIdx = availableEffects[effectID]->GetSettingsArrayStartIdx() + 2 + (paramCount * presetID);
		
		if(presetID >= presetCount)
		{
			// Ignore whatever presetID was given and just increment by 1
			startIdx = availableEffects[effectID]->GetSettingsArrayStartIdx() + 2 + (paramCount * presetCount);
			globalEffectsMaxIdx = ShiftSettingsToAddNewPreset(effectID, paramCount, startIdx, globalEffectsMaxIdx);
			if(globalEffectsMaxIdx == ERR_VALUE_MAX)
			{
				// TODO: Log some error message, for now don't do anything and prevent the adding the new preset
				canWriteNewPreset = false;
			}
			else
			{
				presetCount +=1;
				availableEffects[effectID]->SetPresetCount(presetCount);
			}
		}

		if(canWriteNewPreset)
		{
			for (int paramID = 0; paramID < paramCount; paramID++)
			{
				if(availableEffects[effectID]->GetParameterType(paramID) == ParameterValueType::FloatMagnitude)
				{
					uint32_t tmp;
					float f = availableEffects[effectID]->GetParameterAsFloat(paramID);
					std::memcpy(&tmp, &f, sizeof(float));
					SetSettingsParameterValueForEffect(effectID, paramID, tmp , startIdx);
				}
				else
				{
					SetSettingsParameterValueForEffect(effectID, paramID, availableEffects[effectID]->GetParameterRaw(paramID), startIdx);
				}
			}
			// Set the new maximum
			settings.globalEffectsSettings[0U] = globalEffectsMaxIdx;
			// Set the index to number of Presets for this effect
			startIdx = availableEffects[effectID]->GetSettingsArrayStartIdx();
			settings.globalEffectsSettings[startIdx] = presetCount;
		}
    }
}

// Helpful Function for setting a parameter value for an effect from the Persistant Storage
void SetSettingsParameterValueForEffect(int effectID, int paramID, uint32_t paramValue, uint32_t startIdx)
{
    // Make sure the effect and param id are within valid ranges for the settings.
    if (effectID < 0 || effectID > availableEffectsCount - 1 || paramID < 0)
    {
        return;
    }
	if (paramID > availableEffects[effectID]->GetParameterCount())
	{
		return;
	}

    // Get a handle to the persitance storage settings
    Settings &settings = storage.GetSettings();
    settings.globalEffectsSettings[startIdx + paramID] = paramValue;
}

void FactoryReset(void* context)
{
    storage.RestoreDefaults();
}

